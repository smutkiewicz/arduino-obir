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
      Serial.print("Last key pressed was \"");
      Serial.print(payload.value);
      Serial.println("\"");

      last_pressed = payload.value;
      keyboard = (char) last_pressed;
      server.notifyObserver(our_observer.ip, our_observer.port, our_observer.counter, &keyboard, our_observer.token, our_observer.tokenlen);

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

void light_callback(CoapPacket &packet, IPAddress ip, int port)
{
  if (packet.code == COAP_GET)
  {
    //get current value over the radio
    get_led();
    sprintf(lamp, "%d", led_level);

    server.sendResponse(ip, port, packet.messageid, lamp);
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

void keyboard_callback(CoapPacket &packet, IPAddress ip, int port)
{
  int json = 0;
  if (packet.code == COAP_GET)
  {
    get_keyboard(); //get current value over the radio
    keyboard = (char) last_pressed;
    for (int i = 0; i < sizeof(packet.options); i++)
    { 
      if (packet.options[i].number == 2) //observer
      {
        if (*(packet.options[i].buffer) == 88)
        {
          Serial.println("observer");
          our_observer.ip = ip;
          our_observer.port = port;
          our_observer.counter = 2;
          memcpy(our_observer.token, packet.token, packet.tokenlen);
          our_observer.tokenlen = packet.tokenlen;
          our_observer.counter++;
          server.notifyObserver(our_observer.ip, our_observer.port, our_observer.counter, &keyboard, our_observer.token, our_observer.tokenlen);
          
          break;
        }
        else
        {
          our_observer.counter = -1;
          break;
        }
      } 
      
      if (packet.options[i].number == 17) 
      {
        if (*(packet.options[i].buffer) == 50) json = 1;
      }
    }
    
    if (json == 0) 
    {
      server.sendResponse(ip, port, packet.messageid, &keyboard);
    } 
    else if (json == 1) 
    {
      String message = "{\"keyboard\": {\"key\": \"";
      message += keyboard;
      message += "\"}}";
      server.sendResponse(ip, port, packet.messageid, message.c_str());
    }
  }
}

void statistics_callback(CoapPacket &packet, IPAddress ip, int port)
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

  server.sendResponse(ip, port, packet.messageid, payload.c_str());
  
  delete return_msg;
}

void general_callback(CoapPacket &packet, IPAddress ip, int port)
{
  if (packet.code == COAP_GET)
  {
    server.sendResponse(ip, port, packet.messageid, well_known.c_str(), strlen(well_known.c_str()), COAP_CONTENT, COAP_APPLICATION_LINK_FORMAT, packet.token, packet.tokenlen);
  }
}

//===========================================================================================================
// funkcje użytkowe
//===========================================================================================================
