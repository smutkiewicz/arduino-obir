#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>

RF24 radio (7,8);
RF24Network network(radio);

int THIS_NODE_ID = 0;
int PEER_NODE_ID = 1;

struct payload {
  unsigned short counter;
};

void setup() {
  Serial.begin(115200);
  SPI.begin();
  radio.begin();
  network.begin(47, THIS_NODE_ID);
}

void loop() {
  // put your main code here, to run repeatedly:
  struct payload p;
  String readString = "";

  network.update();

  while(network.available()){
    RF24NetworkHeader header(THIS_NODE_ID);
    bool success = network.read(header, &p, sizeof(p));

    if (success) {
      Serial.print("Received = ");
      Serial.println(p.counter);
    } else {
      Serial.print("Unable to process packet...");
    }

  }
    
  while (Serial.available()){ 
    delay(3);
    if (Serial.available() > 0) {
      char c = Serial.read();  //gets one byte from serial buffer
      readString += c; //makes the string readString
    } 
  }

  if (readString.length() >0) {
      p.counter = atoi(readString.c_str());
      RF24NetworkHeader header(PEER_NODE_ID);
      bool success = network.write(header, &p, sizeof(p));
      readString = "";
      
      if (success) {
        Serial.print("Send packet = ");// + p.counter);  
        Serial.println(p.counter);
      } else {
        Serial.println("Unable = " + p.counter);  
        }
  }
  
}

int convert_to_number(char c) {
  int num = c - '0';
  return num;
}

