#include "coap-simple.h"
#include <SPI.h>
#include <RF24Network.h>
#include <RF24.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <stdio.h>

// budowa wiadomości w komunikacji radiowej pomiędzy komponentami Uno i Mini
struct payload_t
{
  unsigned long timestamp; // stempel czasowy
  short type; // typ zasobu
  int value; // przenoszona wartość
};

// budowa pojedynczego obserwatora
struct observer
{
  IPAddress ip;
  int port;
  uint8_t token[2];
  uint8_t tokenlen;
  int counter; // numer sekwencji
  uint8_t content_type; // typ (plaintext lub json)
};

//===========================================================================================================
// zasoby
//===========================================================================================================
int last_pressed = 35; // kod ascii ostatnio wciśniętego znaku
int led_level = 0; // poziom światła lampki led (0-1000)

// zasób opisujący pozostałe zasoby w formacie CoRE Link Format
const String well_known = "</light>;ct=0,</keyboard>;ct=0;rt=\"obs\",</statistics>;ct=0";

// łańcuchy dla ułatwienia konwersji dla payloadu
char lamp[4] = "0"; // 0 - lampka wyłaczona, 1000 - max poziom światła
char keyboard = '3'; // ostatni wcisniety znak na klawiaturze

observer our_observer; // obserwator klawiatury
bool first_loop = true; // czy musimy pobrać aktualny stan zasobów

//===========================================================================================================
// komponenty dla potrzeb radia
//===========================================================================================================
const short THIS_NODE_ID = 0; // id Arduino UNO związane z radiem
const short PEER_NODE_ID = 1; // id Arduino Mini Pro związane z radiem

// komponenty radiowe
RF24 radio(7, 8); 
RF24Network network(radio);

// id zasobów do komunikacji z radiem
const short GET_KEYBOARD = 1;
const short GET_LED = 2;
const short SET_LED = 3;

//===========================================================================================================
// komponenty dla potrzeb Ethernetu i CoAP
//===========================================================================================================
const byte mac[] = {0x00, 0xaa, 0xbb, 0xcc, 0xde, 0xf3};
//const byte ip[] = {169, 254, 243, 41};
const byte ip[] = {192, 168, 137, 2};
const short local_port = 5683;

// komponenty CoAP/Ethernet
EthernetUDP udp;
Coap server(udp);

//===========================================================================================================
// statystyki radia
//===========================================================================================================
unsigned long last_timestamp; // stempel czasowy zapisany przy wysyłaniu wiadomosci przez radio
unsigned long last_rtt = 0; // rtt ostatnio wysłanej wiadomosci
int sent = 0; // ilosć wszystkich wysłanych wiadomosci
int received = 0; // ilosć wszystkich odebranych wiadomosci

//===========================================================================================================
// konfiguracja i główna pętla programu
//===========================================================================================================

void setup()
{
  Serial.begin(115200);

  // konfiguracja połączenia Ethernet
  Ethernet.begin(mac, ip);
  Serial.print("ip: ");
  Serial.println(Ethernet.localIP());
  udp.begin(local_port);

  // dodanie endpointów CoAP
  server.server(light_callback, "light");
  server.server(keyboard_callback, "keyboard");
  server.server(statistics_callback, "statistics");
  server.server(general_callback, ".well-known/core");
  server.response(callback_response);
  server.start(); // uruchom serwer coap

  // konfiguracja połączenia radiowego
  SPI.begin();
  radio.begin();
  network.begin(47, THIS_NODE_ID);
  Serial.println("Setup done");
}

void loop()
{
  network.update(); // odbierz nowe wiadomosci, funkcja niezbędna do działania powłoki RF24Network
  radio_loop();
  server.loop();

  get_all(); // pobierz wartosci zasobów na starcie
}

// reakcja na wiadomosć ack 
void callback_response(CoapPacket &packet, IPAddress ip, int port) 
{
  Serial.println("resp");
}

//===========================================================================================================
// radio
//===========================================================================================================

// ogólna funkcja do wysyłania wiadomości radiowych do Arduino Mini
bool radio_send_msg(short type, int value)
{
  Serial.print("Radio send ");
  Serial.println(type);

  payload_t payload{millis(), type, value};
  RF24NetworkHeader header(PEER_NODE_ID); // nagłówek wiadomości radiowej, nadanej do Mini

  bool success = false; // czy wysłanie wiadomości przez radio się powiodło
  short retries = 0; // ilość prób wysłania wiadomości przez radio, maks. 5

  while (!success && retries < 5) // próbuj maks. 5 razy
  {
    success = network.write(header, &payload, sizeof(payload)); // spróbuj wysłać wiadomosć
    if (success) 
    {
      Serial.println("Success.");
      sent++; // dodaj pomyslne wysłanie do statystyk
      last_timestamp = millis();
    }
    else retries++;
  }

  if (!success) Serial.println("Failure.");
  return success;
}

// ogólna funkcja do odbierania wiadomosci od Arduino Mini
bool radio_read_msg(payload_t* p)
{
  RF24NetworkHeader header(THIS_NODE_ID); // nagłówek wiadomości radiowej, nadanej do Mini
  return network.read(header, p, sizeof(*p)); // spróbuj odebrać wiadomość
}

// główna pętla radia
void radio_loop()
{
  bool success = false;
  payload_t payload;
  
  while (network.available()) // sprawdź, czy jest dostępna jakaś nowa wiadomosć
  {
    success = radio_read_msg(&payload); // spróbuj odczytać wiadomosć
    if (success)
    {
      Serial.print("Radio msg, timestamp ");
      Serial.println(payload.timestamp);
      received++; // dodaj poprawnie odebraną wiadomosć do statystyk
    }
    else
    {
      Serial.println("Radio msg failure.");
    }
  }

  // zareaguj na poprawnie odebraną wiadomosć
  if (success) radio_handle_payload(payload);
}

// ogólna funkcja do reakcji na poprawnie odebraną wiadomosć przez radio
void radio_handle_payload(payload_t payload)
{
  switch (payload.type)
  {
    case GET_LED:
      Serial.print("LED lvl = ");
      Serial.println(payload.value);

      led_level = payload.value; // aktualizacja wartosci
      sprintf(lamp, "%d", led_level); // konwertuj pomocniczo wartosć
      
      break;

    case GET_KEYBOARD:
      Serial.print("Last key = ");
      Serial.println(payload.value);

      last_pressed = payload.value; // aktualizacja wartosci
      keyboard = (char) last_pressed; // konwertuj pomocniczo wartosć
      notify(); // powiadom obserwatora, jeśli istnieje

      break;

    default:
      Serial.print("Unknown radio msg ");
      Serial.println(payload.type);
      
      break;
  }

  handle_stats();
}

// funkcja do aktualizacji statystyk Round Trip Time
void handle_stats()
{
  // jesli ostatni timestamp jest = 0, to znaczy że dostalismy wiadomosć dla obserwatora
  // i nie liczymy RTT
  if (last_timestamp != 0) 
  {
    last_rtt = millis() - last_timestamp;
    last_timestamp = 0;
  }
}

//===========================================================================================================
// Żądania zasobów przez radio
//===========================================================================================================

void get_all()
{
  // pobierz wartosci zasobów na starcie
  if (first_loop)
  {
    get_led();
    get_keyboard();
    first_loop = false;
  }
}

void get_led()
{
  radio_send_msg(GET_LED, 0);
}

void get_keyboard()
{
  radio_send_msg(GET_KEYBOARD, 0);
}

void set_led(int value)
{
  Serial.print("Send SET_LED to ");
  Serial.println(value);
  radio_send_msg(SET_LED, value);
}

//===========================================================================================================
// Serwer CoAP
//===========================================================================================================

// funkcja do odpowiadania pakietem ACK na pakiety typu CON
// funkcja wspiera też CoAP ping - puste wiadomosci CON
void conack_callback(CoapPacket &packet, IPAddress ip, int port)
{
  if (packet.type == COAP_CON) 
  {
    server.sendAck(ip, port, packet.messageid, packet.token, packet.tokenlen);
  }
}

// callback obsługujący żądania .well-known/core i zwracający zasób opisujący pozostałe zasoby w formacie CoRE Link Format
void general_callback(CoapPacket &packet, IPAddress ip, int port)
{
  conack_callback(packet, ip, port); // obsługa CON/CoAP Ping

  // obsługa żądania w zależnosci od kodu wiadomosci
  if (packet.code == COAP_GET)
  {
    server.sendResponse(ip, port, packet.messageid, well_known.c_str(), strlen(well_known.c_str()), 
                        packet.type, COAP_CONTENT, COAP_APPLICATION_LINK_FORMAT, packet.token, packet.tokenlen);
  }
}

// callback obsługujący żądania dotyczące lampki LED
void light_callback(CoapPacket &packet, IPAddress ip, int port)
{
  conack_callback(packet, ip, port); // obsługa CON/CoAP Ping
  
  // obsługa żądania w zależnosci od kodu wiadomosci
  if (packet.code == COAP_GET)
  {
    get_led(); // pobierz aktualną wartosć przez radio
    server.sendResponse(ip, port, packet.messageid, lamp, strlen(lamp), 
                        packet.type, COAP_CONTENT, COAP_TEXT_PLAIN, packet.token, packet.tokenlen); // odeslij odpowiedź
  }
  else if (packet.code == COAP_PUT)
  {
    char p[packet.payloadlen + 1];
    memcpy(p, packet.payload, packet.payloadlen);
    p[packet.payloadlen] = NULL; // put null -> tworzy stringa
    String message(p);

    int value = atoi(p);
    if (0 <= value && value <= 1000) // sprawdź czy wartosć żądana jest z zakresu 
    {
      led_level = value; // uaktualnij wartosć
      sprintf(lamp, "%d", value);
      set_led(atoi(p)); // ustaw lampkę led na żądaną wartosć
    }
    else // wartosć jest spoza zakresu, odeslij wiadomosć o niepowodzeniu
    {
      server.sendResponse(ip, port, packet.messageid, message.c_str(), strlen(message.c_str()), 
                          packet.type, COAP_BAD_REQUEST, COAP_TEXT_PLAIN, packet.token, packet.tokenlen);
    }
  }
}

// callback obsługujący żądania dotyczące klawiatury
// klient, aby stać się obserwatorem, musi wysłać pakiet z opcją OBSERVE o wartości 0, 
// co sprawia iż zostaje wpisany na listę obserwatorów. 
// Aby przestać obserwować zasób może wysłać wiadomość RST lub wiadomość z opcją Observe o wartości 1. 
// Wiadomość od serwera zostaje wysłana przy zmianie wartości zasobu, 
// OBSERVE przyjmuje kolejne wartości sekwencji, token jest zawsze taki sam.
void keyboard_callback(CoapPacket &packet, IPAddress ip, int port)
{
  String msg; // treść wiadomosci odpowiedzi
  int observe = 0; // flaga pomocnicza (0 - no option; 1 - observe; 2 - unobserve)
  int json = 0; // flaga pomocnicza opcji żądania w formacie JSON
  COAP_CONTENT_TYPE content_type = COAP_TEXT_PLAIN; // żądany typ reprezentacji zasobu

  conack_callback(packet, ip, port);
  
  if (packet.code == COAP_GET)
  {
    get_keyboard(); // pobierz aktualną wartosć przez radio

    // dla każdej z opcji pakietu
    for (int i = 0; i < sizeof(packet.options); i++)
    { 
      if (packet.options[i].number == COAP_OBSERVE) // jeśli opcja observe
      {
        if (*(packet.options[i].buffer) == 88) observe = 1; // obserwuj
        else if (*(packet.options[i].buffer) == 1) observe = 2; // przestań obserwować
      } 
      
      if (packet.options[i].number == COAP_ACCEPT) // jeśli opcja accept
      {
        if (*(packet.options[i].buffer) == COAP_APPLICATION_JSON) // jeśli opcja accept z opcją formatu JSON
        {
          json = 1;
          content_type = COAP_APPLICATION_JSON; // odeślij w wiadomości content-type json
        }
      }
    }
    
    if (observe == 0) // zwykły pakiet GET
    {
      // konwersja wiadomości w zależności od content-type
      if (json == 0) 
      {
        // text/plain
        msg = String(keyboard);
      } 
      else if (json == 1) 
      {
        // application/json
        msg = "{\"keyboard\": {\"key\": \"";
        msg += keyboard;
        msg += "\"}}";
      }
    
      server.sendResponse(ip, port, packet.messageid, msg.c_str(), strlen(msg.c_str()), 
                          packet.type, COAP_CONTENT, content_type, packet.token, packet.tokenlen);
    }
    else if (observe == 1) // pakiet GET z funkcją Observe = 0
    {
      Serial.println("observe");

      // dodaj nowego obserwatora
      our_observer.ip = ip;
      our_observer.port = port;
      our_observer.counter = 2; // nadaj startowy numer sekwencyjny
      our_observer.content_type = (uint8_t) content_type;
      memcpy(our_observer.token, packet.token, packet.tokenlen);
      our_observer.tokenlen = packet.tokenlen;
      notify(); // powiadom nowego obserwatora
    }
    else if (observe == 2) // pakiet GET z funkcją Observe = 1
    {
      // przestań obserwować zasób
      Serial.println("stop observing");
      our_observer.counter = -1;
    }  
  } 
  else if (packet.code == COAP_RESET) // pakiet RST cofający obserwowanie zasobu
  {
    // przestań obserwować zasób
    Serial.println("stop observing");
    our_observer.counter = -1;
  }
}

// callback obsługujący żądania dotyczące statystyk pracy radia
// metryki: średnie RTT, utrata pakietów
void statistics_callback(CoapPacket &packet, IPAddress ip, int port)
{
  conack_callback(packet, ip, port);
  
  if (packet.code == COAP_GET)
  {
    String payload; // payload wiadomości dla klienta
    //payload_t return_msg; // uchwyt na wiadomosc zwrotną
  
    Serial.println("Radio stats");
  
    //double avg_rtt = rtt_sum / sent; // srednie RTT
    
    payload = String(sent) + " snd, " + String(received) + " rcv, " 
              + " last rtt " + String(last_rtt);
    int payload_length = payload.length();

    // odpowiedź na żądanie o statystyki
    server.sendResponse(ip, port, packet.messageid, payload.c_str(), strlen(payload.c_str()), 
                        packet.type, COAP_CONTENT, COAP_TEXT_PLAIN, packet.token, packet.tokenlen);
  }
}

// funkcja powiadamiająca obserwatora o zmianie stanu zasobu klawiatury
void notify()
{
  if (our_observer.counter != -1) // sprawdż, czy ktoś jest zarejestrowany jako obserwator
  {
    String msg; // tresć wiadomosci odpowiedzi

    // w zależności od preferowanego przez obserwatora typu, wyślij mu powiadomienie w odpowiedniej reprezentacji
    if (our_observer.content_type == COAP_APPLICATION_JSON)
    {
      // application/json
      msg = "{\"keyboard\": {\"key\": \"";
      msg += keyboard;
      msg += "\"}}";
    }
    else
    {
      // text/plain
      msg = String(keyboard);  
    }

    server.notifyObserver(our_observer.ip, our_observer.port, our_observer.counter, msg.c_str(), 
                          COAP_NONCON, COAP_CONTENT, our_observer.content_type,
                          our_observer.token, our_observer.tokenlen); // wysłanie powiadomienia
    our_observer.counter++; // zwiększ numer sekwencyjny dla kolejnego powiadomienia
  }
}

//===========================================================================================================
// funkcje użytkowe
//===========================================================================================================

// funkcja służąca do zaglądania w opcje pakietu celem debuggowania
/*void print_options(CoapPacket &packet)
{
  Serial.print(String(packet.options[i].number));
  Serial.print(" = ");
  Serial.println(String(*(packet.options[i].buffer)));
}*/
