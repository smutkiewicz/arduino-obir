#include <SPI.h>
#include <RF24Network.h> 
#include <RF24.h>
#include <Dhcp.h>
#include <Dns.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include "coap-simple.h"

#define LEDP 9

/*
if you change LED, req/res test with coap-client(libcoap), run following.
coap-client -m get coap://(arduino ip addr)/light
coap-client -e "1" -m put coap://(arduino ip addr)/light
coap-client -e "0" -m put coap://(arduino ip addr)/light
*/

byte mac[] = { 0x00, 0xAA, 0xBB, 0xCC, 0xDE, 0x02 };
IPAddress dev_ip(192, 168, 0, 13);

// CoAP client response callback
void callback_response(CoapPacket &packet, IPAddress ip, int port);

// CoAP server endpoint url callback
void callback_light(CoapPacket &packet, IPAddress ip, int port);

// UDP and CoAP class
EthernetUDP Udp;
Coap coap(Udp);

// Radio
RF24 radio(7, 8);
RF24Network network(radio);

int CHANNEL = 47;
int THIS_NODE_ID = 0;
int PEER_NODE_ID = 1;

struct payload {  
  unsigned long timestamp; // stempel czasowy  
  int type;
  int value;
};

// LED STATE
bool LEDSTATE;

// CoAP server endpoint URL
void callback_light(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("[Light] ON/OFF");
  
  // send response
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;
  
  String message(p);

  if (message.equals("0"))
    LEDSTATE = false;
  else if(message.equals("1"))
    LEDSTATE = true;
      
  if (LEDSTATE) {
    //digitalWrite(LEDP, HIGH) ; 
    coap.sendResponse(ip, port, packet.messageid, "1");
  } else { 
    //digitalWrite(LEDP, LOW) ; 
    coap.sendResponse(ip, port, packet.messageid, "0");
  }
}

// CoAP client response callback
void callback_response(CoapPacket &packet, IPAddress ip, int port) {
  Serial.println("[Coap Response got]");
  
  char p[packet.payloadlen + 1];
  memcpy(p, packet.payload, packet.payloadlen);
  p[packet.payloadlen] = NULL;
  
  Serial.println(p);
}

void setup() {
  Serial.begin(115200);

  // Radio
  SPI.begin(); 
  radio.begin();
  network.begin(CHANNEL, THIS_NODE_ID);

  // Internet
  Ethernet.begin(mac, dev_ip);
  
  Serial.print("My IP address: ");
  Serial.print(Ethernet.localIP());
  Serial.println();

  // LED State
  //pinMode(LEDP, OUTPUT);
  //digitalWrite(LEDP, HIGH);
  LEDSTATE = true;
  
  // add server url endpoints.
  // can add multiple endpoint urls.
  // exp) coap.server(callback_switch, "switch");
  //      coap.server(callback_env, "env/temp");
  //      coap.server(callback_env, "env/humidity");
  Serial.println("Setup Callback Light");
  coap.server(callback_light, "light");

  // client response callback.
  // this endpoint is single callback.
  Serial.println("Setup Response Callback");
  coap.response(callback_response);

  // start coap server/client
  coap.start();
}

void loop() {
  // send GET or PUT coap request to CoAP server.
  // To test, use libcoap, microcoap server...etc
  //int msgid = coap.put(IPAddress(10, 0, 0, 177), 5683, "light", "1");
  //Serial.println("Send Request");
  //int msgid = coap.get(IPAddress(192, 168, 0, 1), 5683, "time");

  //delay(1000);
  coap.loop();
  network_loop();
}

bool send_radio_msg(int type, int value) {
  struct payload payload;
  payload.timestamp = 1;
  payload.type = type;
  payload.value = value;
  RF24NetworkHeader header(PEER_NODE_ID);
  return network.write(header, &payload, sizeof(payload));
}

bool receive_radio_msg(struct payload* p) {
  RF24NetworkHeader header(THIS_NODE_ID);
  return network.read(header, &p, sizeof(p));
}

void network_loop() {
  network.update(); 
  while(network.available()) {
    struct payload payload;
    bool success = receive_radio_msg(&payload);
    if (success) {
      Serial.println("Received some stuff from peer");  
    } else {
      Serial.println("Ooopsie whoopsie, you've failed successfully");  
    }
  }
}
