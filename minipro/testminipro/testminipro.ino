//===========================================================================================================
// kod testowy, uruchamiany na wypadek potrzeby doraźnej diagnostyki
//===========================================================================================================

#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>

RF24 radio (7,8);
RF24Network network(radio);

int THIS_NODE_ID = 1;
int PEER_NODE_ID = 0;

unsigned long time = 0;

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
  p.counter = 0;


  network.update();

  while(network.available()){
    Serial.println("network available");
    RF24NetworkHeader header(THIS_NODE_ID);
    network.read(header, &p, sizeof(p));
    int temp = analogRead(A1);
    Serial.println("Payload: " + p.counter);
  }

  if (p.counter != 0 && 1/p.counter * 1000 > time) {
    RF24NetworkHeader header(PEER_NODE_ID);
    bool success = network.write(header, &p, sizeof(p));
        
    if (success) {
      Serial.print("Send packet = ");// + p.counter);  
      Serial.println(p.counter);
    } else {
      Serial.println("Unable = " + p.counter);  
    }

    time = 0;
  }
  
}
