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
  int type; // typ resource'u
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

// zasoby
int last_pressed = 35;
int led_level = 950;

// to poniżej to do konwersji
// how bright the lamp is - 0 is off, 1000 is max brightness
char lamp[4] = "500";
// what button was last pressed on keyboard
char keyboard = '3';

// radio
int THIS_NODE_ID = 0;
int PEER_NODE_ID = 1;

RF24 radio(7, 8);
RF24Network network(radio);

// id zasobów dla radia
const int ALL = 0;
const int GET_KEYBOARD = 1;
const int GET_LED = 2;
const int SET_LED = 3;

// ethernet
int MAX_BUFFER_IN = 255;
int MAX_BUFFER_OUT = 1;
byte mac[] = {0x00, 0xaa, 0xbb, 0xcc, 0xde, 0xf3};
byte ip[] = {169, 254, 243, 41};
short localPort = 5683;

char packet_buffer[255];
char send_buffer[1];
char reply_buffer[] = "acknowledged";

EthernetUDP udp;
Coap server(udp);
observer observers;

//coap endpoints
String light = "light";
String keybrd = "keyboard";
String general = ".well-known/core";
String statistics = "statistics";

void setup()
{
  Serial.begin(115200);
  Serial.println("OBIR - Arduino UNO CoAP Server");

  //ethernet connection
  Ethernet.begin(mac, ip);
  Serial.print("ip: ");
  Serial.println(Ethernet.localIP());
  udp.begin(localPort);

  //add urls for stuff
  server.server(light_callback, light);
  server.server(keyboard_callback, keybrd);
  server.server(statistics_callback, statistics);
  server.server(general_callback, general);
  server.start(); //start the coap server

  //radio connection
  SPI.begin();
  radio.begin();
  network.begin(47, THIS_NODE_ID);
}

void loop()
{
  server.loop();
  radio_loop();
}

bool radio_send_msg(int type, int value)
{
  // update statistics
  Serial.print("Radio send message of type ");
  Serial.println(type);

  payload_t payload{millis(), type, value};
  RF24NetworkHeader header(PEER_NODE_ID);

  bool success = network.write(header, &payload, sizeof(payload));
  if (success)
  {
    Serial.println("Radio send message success.");
  }
  else
  {
    Serial.println("Radio send message failure.");
  }

  return success;
}

bool radio_receive_msg(payload_t* p)
{
  RF24NetworkHeader header(THIS_NODE_ID);
  return network.read(header, &p, sizeof(p));
}

void radio_loop()
{
  while (network.available()) {
    payload_t payload;
    bool success = radio_receive_msg(&payload);
    if (success) {
      Serial.print("Received some stuff from peer with timestamp ");
      Serial.println(payload.timestamp);
      radio_handle_payload(payload);
    }else {
      Serial.println("Ooopsie whoopsie, you've failed successfully to receive stuff");
    }
  }
}

void radio_handle_payload(payload_t payload) {
  switch (payload.type) {
    case GET_LED:
      Serial.print("LED level = ");
      Serial.println(payload.value);

      led_level = payload.value;
      break;

    case GET_KEYBOARD:
      Serial.print("Last key pressed was \"");
      Serial.print(payload.value);
      Serial.println("\"");

      last_pressed = payload.value;

      //updateObserver();

      break;

    default:
      Serial.println("Unknown radio message");
      break;
  }
}

//these are for radio
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

//in here for test reasons
//void ethernet_parse()
//{
//  int size = udp.parsePacket();
//
//  if (size)
//  {
//    int r = udp.read(packet_buffer, MAX_BUFFER_IN);
//
//    Serial.print("packetBuffer=");
//    for (int i = 5; i < r - 1; i++)
//    {
//      Serial.print(packet_buffer[i]);
//    }
//
//    Serial.println(packet_buffer[r - 1]);
//    udp.beginPacket(udp.remoteIP(), udp.remotePort());
//    udp.write(reply_buffer);
//    udp.endPacket();
//  }
//}

//these are for COAP
void light_callback(CoapPacket &packet, IPAddress ip, int port)
{
  if (packet.code == COAP_GET)
  {
    //get current value over the radio
    get_led();
    itoclamp(led_level);
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
    //call this with p as value
    set_led(atoi(p));
    Serial.println(message);
  }
}

void keyboard_callback(CoapPacket &packet, IPAddress ip, int port)
{
  if (packet.code == COAP_GET)
  {
    //get current value over the radio
    get_keyboard();
    keyboard = last_pressed;
    for (int i = 0; i < sizeof(packet.options); i++)
    {
      if (packet.options[i].number == 2)
      {
        if (*(packet.options[i].buffer) == 88)
        {
          observers.ip = ip;
          observers.port = port;
          observers.counter = 2;
          memcpy(observers.token, packet.token, packet.tokenlen);
          observers.tokenlen = packet.tokenlen;
          observers.counter++;
          server.notifyObserver(observers.ip, observers.port, observers.counter, &keyboard, observers.token, observers.tokenlen);
          break;
        }
        else
        {
          observers.counter = -1;
          break;
        }
      }
    }
    server.sendResponse(ip, port, packet.messageid, &keyboard);
  }
}

void statistics_callback(CoapPacket &packet, IPAddress ip, int port)
{

}

void general_callback(CoapPacket &packet, IPAddress ip, int port)
{

}

void itoclamp(int i)
{
  //  int char_array_length=1;
  //  if (i < 10) {
  //    char_array_length = 1;
  //  } else if (i > 9 && i < 100) {
  //    char_array_length = 2;
  //  } else if ( i > 99 && i < 1000) {
  //    char_array_length = 3;
  //  } else if (i == 1000) {
  //    char_array_length = 4;
  //  } else {
  //    Serial.println("zbyt duża wartosc");
  //  }
  //  char str[char_array_length];
  sprintf(lamp, "%d", i);
  //  return str;
}
