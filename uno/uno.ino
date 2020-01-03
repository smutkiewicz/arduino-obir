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
  short type; // typ resource'u
  int value; // przenoszona wartość
};

// pojedynczy obserwator
struct observer
{
  IPAddress ip;
  int port;
  uint8_t token[2];
  uint8_t tokenlen;
  uint8_t counter;
  uint8_t content_type;
};

//===========================================================================================================
// zasoby
//===========================================================================================================
int last_pressed = 35;
int led_level = 950;
const String well_known = "</light>;ct=0,</keyboard>;ct=0;rt=\"obs\",</statistics>;ct=0";

// dla ułatwienia konwersji dla payloadu
char lamp[4] = "500"; // how bright the lamp is - 0 is off, 1000 is max brightness
char keyboard = '3'; // what button was last pressed on keyboard

// obserwator zasobu klawiatury
observer our_observer;

//===========================================================================================================
// komponenty dla potrzeb radia
//===========================================================================================================
const short THIS_NODE_ID = 0;
const short PEER_NODE_ID = 1;

RF24 radio(7, 8);
RF24Network network(radio);

// id zasobów dla radia
const short ALL = 0;
const short GET_KEYBOARD = 1;
const short GET_LED = 2;
const short SET_LED = 3;
const short STATS = 4;

//===========================================================================================================
// komponenty dla potrzeb Ethernetu i CoAP
//===========================================================================================================
const short MAX_BUFFER_IN = 255;
const byte mac[] = {0x00, 0xaa, 0xbb, 0xcc, 0xde, 0xf3};
//const byte ip[] = {169, 254, 243, 41};
const byte ip[] = {192, 168, 137, 2};
const short localPort = 5683;

EthernetUDP udp;
Coap server(udp);

//===========================================================================================================
// konfiguracja i główna pętla programu
//===========================================================================================================

void setup()
{
  Serial.begin(115200);

  // ethernet connection
  Ethernet.begin(mac, ip);
  Serial.print("ip: ");
  Serial.println(Ethernet.localIP());
  udp.begin(localPort);

  // endpointy CoAP
  server.server(light_callback, "light");
  server.server(keyboard_callback, "keyboard");
  server.server(statistics_callback, "statistics");
  server.server(general_callback, ".well-known/core");
  server.start(); //start the coap server

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

bool radio_send_msg(short type, int value)
{
  // update statistics
  Serial.print("Radio send msg type ");
  Serial.println(type);

  payload_t payload{millis(), type, value};
  RF24NetworkHeader header(PEER_NODE_ID);

  bool success = false;
  short retries = 1;

  while (!success && retries < 6)
  {
    success = network.write(header, &payload, sizeof(payload));
    if (success) Serial.println("Success.");
    else
    {
      retries++;
    }
  }

  if (!success) Serial.println("Failure.");
  return success;
}

bool radio_receive_msg(payload_t* p)
{
  RF24NetworkHeader header(THIS_NODE_ID);
  return network.read(header, &p, sizeof(p));
}

void radio_loop()
{
  while (network.available())
  {
    payload_t payload;
    bool success = radio_receive_msg(&payload);
    if (success)
    {
      Serial.print("Radio msg, timestamp ");
      Serial.println(payload.timestamp);
      radio_handle_payload(payload);
    }
    else
    {
      Serial.println("Radio msg failure.");
    }
  }
}

void radio_handle_payload(payload_t payload)
{
  switch (payload.type)
  {
    case GET_LED:
      Serial.print("LED lvl = ");
      Serial.println(payload.value);

      led_level = payload.value;
      break;

    case GET_KEYBOARD:
      Serial.print("Last key pressed was ");
      Serial.println(payload.value);

      last_pressed = payload.value;
      keyboard = (char) last_pressed;
      notify();

      break;

    default:
      Serial.println("Unknown radio msg");
      break;
  }
}

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

void conack_callback(CoapPacket &packet, IPAddress ip, int port)
{
  if (packet.type == COAP_CON) server.sendAck(ip, port, packet.messageid, packet.token, packet.tokenlen);
}

void light_callback(CoapPacket &packet, IPAddress ip, int port)
{
  conack_callback(packet, ip, port);
  
  if (packet.code == COAP_GET)
  {
    //get current value over the radio
    get_led();
    sprintf(lamp, "%d", led_level);
    server.sendResponse(ip, port, packet.messageid, lamp, strlen(lamp), packet.type, COAP_CONTENT, COAP_TEXT_PLAIN, packet.token, packet.tokenlen);
  }
  else if (packet.code == COAP_PUT)
  {
    //copy payload to array
    char p[packet.payloadlen + 1];
    memcpy(p, packet.payload, packet.payloadlen);
    //put null -> makes string
    p[packet.payloadlen] = NULL;
    String message(p);
    set_led(atoi(p));
  }
}

// Klient, aby stać się obserwatorem musi wysłać pakiet z opcją OBSERVE o wartości 0 i tak zostaje wpisany na listę obserwatorów, 
// aby przestać obserwować zasób może wysłać wiadomość RST lub wiadomość z opcją Observe o wartości 1. 
// Odpowiedź serwera następuje przy zmianie wartości zasobu, 
// OBSERVE przyjmuje wartości sekwencji, token jest zawsze taki sam.
void keyboard_callback(CoapPacket &packet, IPAddress ip, int port)
{
  String msg;
  int observe = 0; // 0 - no option; 1 - observe; 2 - unobserve
  int json = 0;
  COAP_CONTENT_TYPE content_type = COAP_TEXT_PLAIN;

  conack_callback(packet, ip, port);
  
  if (packet.code == COAP_GET)
  {
    get_keyboard(); //get current value over the radio
    keyboard = (char) last_pressed;
    
    for (int i = 0; i < sizeof(packet.options); i++)
    { 
      if (packet.options[i].number == COAP_OBSERVE) //observer
      {
        if (*(packet.options[i].buffer) == 88) observe = 1;
        else if (*(packet.options[i].buffer) == 1) observe = 2;
      } 
      
      if (packet.options[i].number == COAP_ACCEPT) // json
      {
        if (*(packet.options[i].buffer) == COAP_APPLICATION_JSON) 
        {
          json = 1;
          content_type = COAP_APPLICATION_JSON;
        }
      }
    }
    
    if (observe == 0)
    {
      if (json == 0) 
      {
        msg = String(keyboard);
      } 
      else if (json == 1) 
      {
        msg = "{\"keyboard\": {\"key\": \"";
        msg += keyboard;
        msg += "\"}}";
      }
    
      server.sendResponse(ip, port, packet.messageid, msg.c_str(), strlen(msg.c_str()), 
                          packet.type, COAP_CONTENT, content_type, packet.token, packet.tokenlen);
    }
    else if (observe == 1)
    {
      Serial.println("observe");
      our_observer.ip = ip;
      our_observer.port = port;
      our_observer.counter = 2;
      our_observer.content_type = (uint8_t) content_type;
      memcpy(our_observer.token, packet.token, packet.tokenlen);
      our_observer.tokenlen = packet.tokenlen;
      notify();
    }
    else if (observe == 2) 
    {
      // przestań obserwować zasób
      Serial.println("stop observing");
      our_observer.counter = -1;
    }  
  } 
  else if (packet.code == COAP_RESET || observe == 2) 
  {
    // przestań obserwować zasób
    Serial.println("stop observing");
    our_observer.counter = -1;
  }
}

void statistics_callback(CoapPacket &packet, IPAddress ip, int port)
{
  conack_callback(packet, ip, port);
  
  if (packet.code == COAP_GET)
  {
    String payload; // payload wiadomości dla klienta
    payload_t* return_msg; // uchwyt na wiadomosc zwrotną
  
    unsigned long start_time, rtt, rtt_sum = 0;
    short received = 0;
  
    Serial.println("Radio stats");
  
    for (uint8_t i = 0; i < 5; i++)
    {
      bool success_send = false;
      bool success_receive = false;
  
      start_time = millis(); //rozpoczecie pomiaru czasu
      success_send = radio_send_msg(STATS, 0);
  
      if (success_send)
      {
        success_receive = radio_receive_msg(return_msg);
        if (success_receive) received++;
      }
  
      rtt = millis() - start_time; // obliczenie RTT
      rtt_sum += rtt;
    }
  
    double packet_loss = ((5 - received) / 5) * 100;
    double avg_rtt = rtt_sum / 5;
    
    payload = String(received) + "/5 received, " + String(packet_loss) + "% packet loss, avg rtt " + String(avg_rtt);
    int payload_length = payload.length();
    
    server.sendResponse(ip, port, packet.messageid, payload.c_str(), strlen(payload.c_str()), 
                        packet.type, COAP_CONTENT, COAP_TEXT_PLAIN, packet.token, packet.tokenlen);
    
    delete return_msg;
  }
}

void general_callback(CoapPacket &packet, IPAddress ip, int port)
{
  conack_callback(packet, ip, port);
  
  if (packet.code == COAP_GET)
  {
    server.sendResponse(ip, port, packet.messageid, well_known.c_str(), strlen(well_known.c_str()), 
                        packet.type, COAP_CONTENT, COAP_APPLICATION_LINK_FORMAT, packet.token, packet.tokenlen);
  }
}

void notify()
{
  if (our_observer.counter != -1)
  {
    String msg;
    if (our_observer.content_type == COAP_APPLICATION_JSON)
    {
       msg = "{\"keyboard\": {\"key\": \"";
       msg += keyboard;
       msg += "\"}}";
    }
    else
    {
       msg = String(keyboard);  
    }
        
    server.notifyObserver(our_observer.ip, our_observer.port, our_observer.counter, msg.c_str(), 
                          COAP_NONCON, COAP_CONTENT, our_observer.content_type,
                          our_observer.token, our_observer.tokenlen);
    our_observer.counter++;
  }
}

//===========================================================================================================
// funkcje użytkowe
//===========================================================================================================

/*void print_options(CoapPacket &packet)
{
  Serial.print(String(packet.options[i].number));
  Serial.print(" = ");
  Serial.println(String(*(packet.options[i].buffer)));
}*/
