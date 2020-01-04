#include "coap-simple.h"
#include <SPI.h>
#include <RF24Network.h>
#include <RF24.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <stdio.h>

// wiadomość w komunikacji radiowej pomiędzy Uno a Mini
struct payload_t
{
  unsigned long timestamp; // stempel czasowy
  short type; // typ zasobu
  int value; // przenoszona wartość
};

// pojedynczy obserwator
struct observer
{
  IPAddress ip;
  int port;
  uint8_t token[2];
  uint8_t tokenlen;
  uint8_t counter; // numer sekwencji
  uint8_t content_type; // typ (plaintext lub json)
};

//===========================================================================================================
// zasoby
//===========================================================================================================
int last_pressed = 35; // kod ascii ostatnio wciśniętego znaku
int led_level = 950; // poziom światła lampki led (0-1000)

// zasób opisujący pozostałe zasoby w formacie CoRE Link Format
const String well_known = "</light>;ct=0,</keyboard>;ct=0;rt=\"obs\",</statistics>;ct=0";

// łańcuchy dla ułatwienia konwersji dla payloadu
char lamp[4] = "500"; // 0 - lampka wyłaczona, 1000 - max poziom światła
char keyboard = '3'; // ostatni wcisniety znak na klawiaturze

observer our_observer; // obserwator zasobu klawiatury

//===========================================================================================================
// komponenty dla potrzeb radia
//===========================================================================================================
const short THIS_NODE_ID = 0; // radiowe id Arduino UNO
const short PEER_NODE_ID = 1; // radiowe id Arduino Mini Pro

// komponenty radiowe
RF24 radio(7, 8); 
RF24Network network(radio);

// id zasobów do komunikacji z radiem
const short ALL = 0;
const short GET_KEYBOARD = 1;
const short GET_LED = 2;
const short SET_LED = 3;
const short STATS = 4;

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
  server.start(); // uruchom serwer coap

  // konfiguracja połączenia radiowego
  SPI.begin();
  radio.begin();
  network.begin(47, THIS_NODE_ID);
}

void loop()
{
  server.loop();
  radio_loop();
}

//===========================================================================================================
// radio
//===========================================================================================================

// ogólna funkcja do wysyłania wiadomosci do Arduino Mini
bool radio_send_msg(short type, int value)
{
  Serial.print("Radio send msg type ");
  Serial.println(type);

  payload_t payload{millis(), type, value};
  RF24NetworkHeader header(PEER_NODE_ID); // nagłówek wiadomosci przez radio, adresowanie do Mini

  bool success = false; // czy wysłanie wiadomosci przez radio sie powiodlo
  short retries = 1; // ilosc prób, max = 5

  while (!success && retries < 6) // próbuj max 5 razy
  {
    success = network.write(header, &payload, sizeof(payload)); // spróbuj wysłać wiadomosć
    if (success) Serial.println("Success.");
    else retries++;
  }

  if (!success) Serial.println("Failure.");
  return success;
}

// ogólna funkcja do odbierania wiadomosci od Arduino Mini
bool radio_receive_msg(payload_t* p)
{
  RF24NetworkHeader header(THIS_NODE_ID); // nagłówek wiadomosci przez radio, adresowanie do Uno
  return network.read(header, &p, sizeof(p)); // spróbuj odebrać wiadomosć
}

// główna pętla radia
void radio_loop()
{
  while (network.available()) // sprawdź, czy nadeszła nowa wiadomosć
  {
    payload_t payload;
    bool success = radio_receive_msg(&payload); // spróbuj odebrać wiadomosć
    if (success)
    {
      Serial.print("Radio msg, timestamp ");
      Serial.println(payload.timestamp);
      radio_handle_payload(payload); // zareaguj na poprawnie odebraną wiadomosć
    }
    else
    {
      Serial.println("Radio msg failure.");
    }
  }
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
      Serial.print("Last key pressed was ");
      Serial.println(payload.value);

      last_pressed = payload.value; // aktualizacja wartosci
      keyboard = (char) last_pressed; // konwertuj pomocniczo wartosć
      notify(); // powiadom obserwatora, jeśli istnieje

      break;

    default:
      Serial.println("Unknown radio msg");
      break;
  }
}

//===========================================================================================================
// Żądania zasobów przez radio
//===========================================================================================================

void get_led()
{
  Serial.println("Send GET_LED");
  radio_send_msg(GET_LED, 0);
}

void get_keyboard()
{
  Serial.println("Send GET_KEYBOARD");
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
// funkcja wspiera też coap ping - puste wiadomosci CON
void conack_callback(CoapPacket &packet, IPAddress ip, int port)
{
  if (packet.type == COAP_CON) server.sendAck(ip, port, packet.messageid, packet.token, packet.tokenlen);
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
    p[packet.payloadlen] = NULL; // put null -> makes string
    String message(p);
    set_led(atoi(p)); // ustaw lampkę led na żądaną wartosć
  }
}

// callback obsługujący żądania dotyczące klawiatury
// Klient, aby stać się obserwatorem musi wysłać pakiet z opcją OBSERVE o wartości 0 i tak zostaje wpisany na listę obserwatorów, 
// aby przestać obserwować zasób może wysłać wiadomość RST lub wiadomość z opcją Observe o wartości 1. 
// Odpowiedź serwera następuje przy zmianie wartości zasobu, 
// OBSERVE przyjmuje wartości sekwencji, token jest zawsze taki sam.
void keyboard_callback(CoapPacket &packet, IPAddress ip, int port)
{
  String msg; // tresć wiadomosci odpowiedzi
  int observe = 0; // flaga pomocnicza (0 - no option; 1 - observe; 2 - unobserve)
  int json = 0; // flaga pomocnicza opcji json
  COAP_CONTENT_TYPE content_type = COAP_TEXT_PLAIN; // żądany typ reprezentacji zasobu

  conack_callback(packet, ip, port);
  
  if (packet.code == COAP_GET)
  {
    get_keyboard(); // pobierz aktualną wartosć przez radio

    // dla każdej z opcji pakietu
    for (int i = 0; i < sizeof(packet.options); i++)
    { 
      if (packet.options[i].number == COAP_OBSERVE) // sprawdź opcję observe
      {
        if (*(packet.options[i].buffer) == 88) observe = 1; // obserwuj
        else if (*(packet.options[i].buffer) == 1) observe = 2; // przestań obserwować
      } 
      
      if (packet.options[i].number == COAP_ACCEPT) // sprawdź opcję accept
      {
        if (*(packet.options[i].buffer) == COAP_APPLICATION_JSON) 
        {
          json = 1;
          content_type = COAP_APPLICATION_JSON; // odsyłaj wiadomosci o content-type json
        }
      }
    }
    
    if (observe == 0) // zwykły pakiet GET
    {
      // konwersja wiadomosci w zależnosci od content-type
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
    payload_t* return_msg; // uchwyt na wiadomosc zwrotną
  
    unsigned long start_time, rtt, rtt_sum = 0;
    short received = 0; // ilość przekazanych z sukcesem wiadomosci
  
    Serial.println("Radio stats");
  
    for (uint8_t i = 0; i < 5; i++)
    {
      bool success_send = false;
      bool success_receive = false;
  
      start_time = millis(); //rozpoczecie pomiaru czasu
      success_send = radio_send_msg(STATS, 0); // spróbuj wysłać wiadomosć przez radio
  
      if (success_send)
      {
        success_receive = radio_receive_msg(return_msg); // spróbuj odebrać odpowiedź przez radio
        if (success_receive) received++;
      }
  
      rtt = millis() - start_time; // obliczenie RTT
      rtt_sum += rtt; // obliczenie sumarycznego RTT
    }
  
    double packet_loss = ((5 - received) / 5) * 100; // procentowa utrata pakietów
    double avg_rtt = rtt_sum / 5; // srednie RTT
    
    payload = String(received) + "/5 received, " + String(packet_loss) + "% packet loss, avg rtt " + String(avg_rtt);
    int payload_length = payload.length();

    // odpowiedź na żądanie o statystyki
    server.sendResponse(ip, port, packet.messageid, payload.c_str(), strlen(payload.c_str()), 
                        packet.type, COAP_CONTENT, COAP_TEXT_PLAIN, packet.token, packet.tokenlen);
    
    delete return_msg; // zwalnianie pamięci
  }
}

// funkcja powiadamiająca obserwatora o zmianie stanu zasobu klawiatury
void notify()
{
  if (our_observer.counter != -1) // sprawdż, czy ktos jest zarejestrowany jako obserwator
  {
    String msg; // tresć wiadomosci odpowiedzi

    // w zależnosci od preferowanego przez obserwatora typu, wyslij mu powiadomienie w odpowiedniej reprezentacji
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
