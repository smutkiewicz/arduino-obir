#include <coap-simple.h>
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <stdio.h>

int THIS_NODE_ID = 0;
int PEER_NODE_ID = 1;
//ethernet
byte mac[] = {0x00, 0xaa, 0xbb, 0xcc, 0xde, 0xf3};
byte ip[] = {169, 254, 243, 41};
EthernetUDP udp;
short localPort = 5683;
Coap server(udp);
char packetBuffer[255];
char sendBuffer[1];
char ReplyBuffer[] = "acknowledged";
int MAX_BUFFER_IN = 255;
int MAX_BUFFER_OUT = 1;
//coap
String lightEndPoint = "light";
String keyboardEndPoint = "keyboard";
String generalEndPoint = ".well-known/core";
String statisticsEndPoint = "statistics";

//how bright the lamp is - 0 is off, 1000 is max brightness
char lamp[4] = "500";
//what button was last pressed on keyboard
char keyboard[1] = "3";

//struct payload {
//  unsigned short counter;
//};

struct Observer {
  IPAddress ip;
  int port;
  uint8_t token[2];
  uint8_t tokenlen;
  uint8_t counter;
};

Observer observers;

void setup() {
  Serial.begin(115200);
  Serial.println("hello");
  //SPI.begin();
  //radio.begin();
  //network.begin(47, THIS_NODE_ID);
  //ethernet connection
  Ethernet.begin(mac, ip);
  Serial.println("mac");
  Serial.println(Ethernet.localIP());
  Serial.println("ip");
  udp.begin(localPort);

  //create a COAP server
  //add urls for stuff
  server.server(lightCallback, lightEndPoint);
  server.server(keyboardCallback, keyboardEndPoint);
  server.server(generalCallback, generalEndPoint);
  //start the coap server
  server.start();
}

void loop() {
  server.loop();
}

void ethernetParse() {
  int size = udp.parsePacket();

  if (size) {
    int r = udp.read(packetBuffer, MAX_BUFFER_IN);

    Serial.print("packetBuffer=");
    for (int i = 5; i < r - 1; i++) {
      Serial.print(packetBuffer[i]);
    }
    Serial.println(packetBuffer[r - 1]);
    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    udp.write(ReplyBuffer);
    udp.endPacket();
  }
}

//these are for radio
void getLamp() {}
void getKeyboard() {}
void setLamp() {}

//these are for COAP
void lightCallback(CoapPacket &packet, IPAddress ip, int port) {
  if (packet.code == COAP_GET) {
    //get current value over the radio
    getLamp();
    server.sendResponse(ip, port, packet.messageid, lamp);
  }
  if (packet.code == COAP_PUT) {
    //copy payload to array
    char p[packet.payloadlen + 1];
    memcpy(p, packet.payload, packet.payloadlen);
    //put null -> makes string
    p[packet.payloadlen] = NULL;
    String message(p);
    //call this with p as value
    setLamp();
    Serial.println(message);
  }
}

void keyboardCallback(CoapPacket &packet, IPAddress ip, int port) {
  if (packet.code == COAP_GET) {
    for (int i = 0; i < sizeof(packet.options); i++) {
      if (packet.options[i].number == 2) {
        if (*(packet.options[i].buffer) == 88) {
          observers.ip = ip;
          observers.port = port;
          observers.counter = 2;
          memcpy(observers.token, packet.token, packet.tokenlen);
          observers.tokenlen = packet.tokenlen;
          observers.counter++;
          server.notifyObserver(observers.ip, observers.port, observers.counter, keyboard, observers.token, observers.tokenlen);
          break;
        } else {
          observers.counter = -1;
          break;
        }
      }
      server.sendResponse(ip, port, packet.messageid, keyboard);
      //get current value over the radio
      getKeyboard();
    }
  }
}

void generalCallback() {}
