#include "coap-simple.h"
#include "Arduino.h"

#define LOGGING

// funkcja dodająca opcję do pakietu CoAP
void CoapPacket::addOption(uint8_t number, uint8_t length, uint8_t *opt_payload)
{
    options[optionnum].number = number;       // numer opcji
    options[optionnum].length = length;       // długosć bitowa bufora
    options[optionnum].buffer = opt_payload;  // bufor, zawartosć opcji
    ++optionnum;
}

// konstruktor
Coap::Coap(
    UDP& udp
) {
    this->_udp = &udp;
}

// funkcja inicjująca pracę serwera na defaultowym porcie UDP
bool Coap::start() {
    this->start(COAP_DEFAULT_PORT);
    return true;
}

// funkcja inicjująca pracę serwera na wybranym porcie UDP
bool Coap::start(int port) {
    this->_udp->begin(port);
    return true;
}

// wysyłanie pakietu
uint16_t Coap::sendPacket(CoapPacket &packet, IPAddress ip, int port) {
    uint8_t buffer[BUF_MAX_SIZE];
    uint8_t *p = buffer; // wskaźnik na początek bufora pakietu
    uint16_t running_delta = 0;
    uint16_t packetSize = 0;

    // tworzenie podstawowego nagłówka pakietu:
    // Ver - 2 bity, Type - 2 bity, TokenLength - 4 bity, Code - 8 bitów, MID - 16 bitów
    *p = 0x01 << 6;
    *p |= (packet.type & 0x03) << 4;
    *p++ |= (packet.tokenlen & 0x0F);
    *p++ = packet.code;
    *p++ = (packet.messageid >> 8);   // wpisanie pierwszych 8 bitów 16 bitowego MID
    *p++ = (packet.messageid & 0xFF); // drugie 8 bitów bitowe AND z 0xFF
    p = buffer + COAP_HEADER_SIZE;
    packetSize += 4;

    // dodanie opcjonalnego tokenu, jeśli istnieje
    if (packet.token != NULL && packet.tokenlen <= 0x0F) {
        memcpy(p, packet.token, packet.tokenlen);
        p += packet.tokenlen;
        packetSize += packet.tokenlen;
    }

    // nagłówek opcji
    for (int i = 0; i < packet.optionnum; i++)  {
        uint32_t optdelta;
        uint8_t len, delta;

        // sprawdzenie, czy nagłówek opcji i jego zawartosć mieszczą się w buforze
        if (packetSize + 5 + packet.options[i].length >= BUF_MAX_SIZE) {
            return 0;
        }
        
        optdelta = packet.options[i].number - running_delta;
        COAP_OPTION_DELTA(optdelta, &delta);
        COAP_OPTION_DELTA((uint32_t)packet.options[i].length, &len);

        // w bloku opcji pierwsze 4 bity to delta, a kolejne 4 długość
        *p++ = (0xFF & (delta << 4 | len)); // przesunięcie delty o 4 bity w lewo
        if (delta == 13) {
            *p++ = (optdelta - 13);
            packetSize++;
        } else if (delta == 14) {
            *p++ = ((optdelta - 269) >> 8);
            *p++ = (0xFF & (optdelta - 269));
            packetSize+=2;
        } if (len == 13) {
            *p++ = (packet.options[i].length - 13);
            packetSize++;
        } else if (len == 14) {
            *p++ = (packet.options[i].length >> 8);
            *p++ = (0xFF & (packet.options[i].length - 269));
            packetSize+=2;
        }

        memcpy(p, packet.options[i].buffer, packet.options[i].length);
        p += packet.options[i].length;
        packetSize += packet.options[i].length + 1;
        running_delta = packet.options[i].number;
    }

    // stwórz payload
    if (packet.payloadlen > 0) {
        // jeśli pakiet nie jest pusty (rozmiar + marker + długosć payloadu)
        if ((packetSize + 1 + packet.payloadlen) >= BUF_MAX_SIZE) {
            return 0;
        }
        *p++ = 0xFF; // marker
        memcpy(p, packet.payload, packet.payloadlen);
        packetSize += 1 + packet.payloadlen;
    }

    // wyślij
    _udp->beginPacket(ip, port);
    _udp->write(buffer, packetSize);
    _udp->endPacket();

    return packet.messageid;
}

uint16_t Coap::send(IPAddress ip, int port, char *url, COAP_TYPE type, COAP_METHOD method, uint8_t *token, uint8_t tokenlen, uint8_t *payload, uint32_t payloadlen, COAP_CONTENT_TYPE content_type) {

    // make packet
    CoapPacket packet;

    packet.type = type;
    packet.code = method;
    packet.token = token;
    packet.tokenlen = tokenlen;
    packet.payload = payload;
    packet.payloadlen = payloadlen;
    packet.optionnum = 0;
    packet.messageid = rand();

    // dodaj opcję URI_HOST
    String ipaddress = String(ip[0]) + String(".") + String(ip[1]) + String(".") + String(ip[2]) + String(".") + String(ip[3]); 
    packet.addOption(COAP_URI_HOST, ipaddress.length(), (uint8_t *)ipaddress.c_str());

    // parsowanie URL
    int idx = 0;
    for (int i = 0; i < strlen(url); i++) {
        if (url[i] == '/') {
          // dodaj opcję URI_PATH
			    packet.addOption(COAP_URI_PATH, i-idx, (uint8_t *)(url + idx));
          idx = i + 1;
        }
    }

    // dodaj opcję URI_PATH
    if (idx <= strlen(url)) {
		  packet.addOption(COAP_URI_PATH, strlen(url)-idx, (uint8_t *)(url + idx));
    }

  	// dodaj opcję Content-Format option
  	uint8_t optionBuffer[2] {0};
  	if (content_type != COAP_NONE) {
  		optionBuffer[0] = ((uint16_t)content_type & 0xFF00) >> 8;
  		optionBuffer[1] = ((uint16_t)content_type & 0x00FF) ;
  		packet.addOption(COAP_CONTENT_FORMAT, 2, optionBuffer);
  	}

    // wyslij pakiet
    return this->sendPacket(packet, ip, port);
}

int Coap::parseOption(CoapOption *option, uint16_t *running_delta, uint8_t **buf, size_t buflen) {
    uint8_t *p = *buf;
    uint8_t headlen = 1;
    uint16_t len, delta;

    if (buflen < headlen) return -1; // bufor o długosci 0 jest pusty

    delta = (p[0] & 0xF0) >> 4; // pierwsze 4 bity to delta
    len = p[0] & 0x0F; // kolejne 4 bity to długosć

    // delta to pierwszy bajt opcji
    // D to finalna Option Delta
    // e0 to pierwszy bajt Option Delta Extended
    // e1 to drugi bajt Option Delta Extended
    if (delta == 13) { // D = 13 + e0 (brak e1)
        headlen++;
        if (buflen < headlen) return -1;
        delta = p[1] + 13;
        p++;
    } else if (delta == 14) { // D = 269 + e0*256 + e1 (D >= 269)
        headlen += 2;
        if (buflen < headlen) return -1;
        delta = ((p[1] << 8) | p[2]) + 269;
        p+=2;
    } else if (delta == 15) return -1;

    if (len == 13) {
        headlen++;
        if (buflen < headlen) return -1;
        len = p[1] + 13;
        p++;
    } else if (len == 14) {
        headlen += 2;
        if (buflen < headlen) return -1;
        len = ((p[1] << 8) | p[2]) + 269;
        p+=2;
    } else if (len == 15)
        return -1;

    if ((p + 1 + len) > (*buf + buflen))  return -1;
    option->number = delta + *running_delta;
    option->buffer = p+1;
    option->length = len;
    *buf = p + 1 + len;
    *running_delta += delta;

    return 0;
}

bool Coap::loop() {

    uint8_t buffer[BUF_MAX_SIZE];
    int32_t packetlen = _udp->parsePacket(); // długosć pakietu

    // jeśli długość następnego pakietu jest większa niż 0, przetwarzaj
    while (packetlen > 0) {
        packetlen = _udp->read(buffer, packetlen >= BUF_MAX_SIZE ? BUF_MAX_SIZE : packetlen);

        CoapPacket packet;

        // sprawdza czy nagłówek został już przeczytany,
        // parsuje pakiet i oblicza jego długosć
        if (packetlen < COAP_HEADER_SIZE || (((buffer[0] & 0xC0) >> 6) != 1)) {
            packetlen = _udp->parsePacket();
            continue;
        }

        packet.type = (buffer[0] & 0x30) >> 4;
        packet.tokenlen = buffer[0] & 0x0F;
        packet.code = buffer[1];
        packet.messageid = 0xFF00 & (buffer[2] << 8); // AND przesunięcia w lewo pierwszych 8 bitów MID
        packet.messageid |= 0x00FF & buffer[3]; // AND kolejnych 8 bitów MID

        if (packet.tokenlen == 0)  packet.token = NULL;
        else if (packet.tokenlen <= 8)  packet.token = buffer + 4;
        else {
            packetlen = _udp->parsePacket();
            continue;
        }

        // parsuj opcje i payload pakietu
        if (COAP_HEADER_SIZE + packet.tokenlen < packetlen) {
            int optionIndex = 0;
            uint16_t delta = 0;
            uint8_t *end = buffer + packetlen;
            uint8_t *p = buffer + COAP_HEADER_SIZE + packet.tokenlen;

            // powtarzaj dopóki wszystkie opcje nie zostaną odczytane
            // lub nie dotrzemy do markera lub do końca pakietu
            while(optionIndex < MAX_OPTION_NUM && *p != 0xFF && p < end) {
                packet.options[optionIndex];
                if (0 != parseOption(&packet.options[optionIndex], &delta, &p, end-p))
                    return false;
                optionIndex++;
            }
            packet.optionnum = optionIndex;

            if (p+1 < end && *p == 0xFF) { // jeśli jest co najmniej bit payloadu
                packet.payload = p+1;
                packet.payloadlen = end-(p+1);
            } else {
                packet.payload = NULL;
                packet.payloadlen= 0;
            }
        }

        if (packet.type == COAP_ACK) {
            // odpowiedz pakietem ACK
            resp(packet, _udp->remoteIP(), _udp->remotePort());
        } else {
            
            String url = "";
            // dopasuj endpoint do funkcji
            for (int i = 0; i < packet.optionnum; i++) {
                if (packet.options[i].number == COAP_URI_PATH && packet.options[i].length > 0) {
                    char urlname[packet.options[i].length + 1];
                    memcpy(urlname, packet.options[i].buffer, packet.options[i].length);
                    urlname[packet.options[i].length] = NULL;
                    if(url.length() > 0)
                      url += "/";
                    url += urlname;
                }
            }        

            // jesli nie znaleziono funkcji obsługującej żądany URI, zwróć NOT_FOUND
            if (!uri.find(url)) {
                sendResponse(_udp->remoteIP(), _udp->remotePort(), packet.messageid, NULL, 0,
                        COAP_NOT_FOUND, COAP_NONE, NULL, 0);
            } else {
                uri.find(url)(packet, _udp->remoteIP(), _udp->remotePort());
            }
        }

        // obsłuż następny pakiet
        packetlen = _udp->parsePacket();
    }

    return true;
}

//===========================================================================================================
// funkcje do odsyłania odpowiedzi na żądania klienta
//===========================================================================================================

// funkcja do wysyłania odpowiedzi ACK na pakiety typu Confirmable
uint16_t Coap::sendAck(IPAddress ip, int port, uint16_t messageid, uint8_t *token, int tokenlen) {
    this->sendResponse(ip, port, messageid, NULL, 0, COAP_ACK, COAP_VALID, COAP_TEXT_PLAIN, token, tokenlen);
}

uint16_t Coap::sendResponse(IPAddress ip, int port, uint16_t messageid, char *payload, int payloadlen,
                COAP_RESPONSE_CODE code, COAP_CONTENT_TYPE contentType, uint8_t *token, int tokenlen) {
    this->sendResponse(ip, port, messageid, payload, payloadlen, COAP_NONCON, code, contentType, token, tokenlen);            
}

uint16_t Coap::sendResponse(IPAddress ip, int port, uint16_t messageid, char *payload, int payloadlen,
                            COAP_TYPE type, COAP_RESPONSE_CODE code, COAP_CONTENT_TYPE contentType, 
                            uint8_t *token, int tokenlen) {
    CoapPacket packet;

    // złożenie pakietu CoAP
    packet.type = type;
    packet.code = code;
    packet.token = token;
    packet.tokenlen = tokenlen;
    packet.payload = (uint8_t*) payload;
    packet.payloadlen = payloadlen;
    packet.optionnum = 0;
    packet.messageid = messageid;

    // dodanie opcji content format
    uint8_t optionBuffer[2] = {0};
    optionBuffer[0] = ((uint16_t) contentType & 0xFF00) >> 8;
    optionBuffer[1] = ((uint16_t) contentType & 0x00FF);
	  packet.addOption(COAP_CONTENT_FORMAT, 2, optionBuffer);

    return this->sendPacket(packet, ip, port);
}

// funkcja do powiadamiania obserwatora o zmianach zasobu
uint16_t Coap::notifyObserver(IPAddress ip, int port, uint8_t obs, char *payload, 
                              COAP_TYPE type, COAP_RESPONSE_CODE code, COAP_CONTENT_TYPE contentType, 
                              uint8_t *token, uint8_t tokenlen)
{
    CoapPacket packet;

    // złożenie pakietu CoAP
    packet.type = type;
    packet.code = COAP_CONTENT;
    packet.token = token;
    packet.tokenlen = tokenlen;
    packet.payload = (uint8_t*) payload;
    packet.payloadlen = strlen(payload);
    packet.optionnum = 0;
    packet.messageid = NULL;

    // dodanie numeru sekwencyjnego opcji Observe
    packet.options[packet.optionnum].buffer = &obs;
    packet.options[packet.optionnum].length = 1;
    packet.options[packet.optionnum].number = COAP_OBSERVE;
    packet.optionnum++;

    // dodanie opcji content format
    char optionBuffer[2];
    optionBuffer[0] = ((uint16_t) contentType & 0xFF00) >> 8;
    optionBuffer[1] = ((uint16_t) contentType & 0x00FF);
    packet.options[packet.optionnum].buffer = (uint8_t*) optionBuffer;
    packet.options[packet.optionnum].length = 2;
    packet.options[packet.optionnum].number = COAP_CONTENT_FORMAT;
    packet.optionnum++;

    return this->sendPacket(packet, ip, port);
}
