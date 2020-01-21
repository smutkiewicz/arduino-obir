//===========================================================================================================
// kod testowy, uruchamiany na wypadek potrzeby doraźnej diagnostyki
//===========================================================================================================

#include <SPI.h>
#include <RF24Network.h>
#include <RF24.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <stdio.h>

const short THIS_NODE_ID = 0; // id Arduino UNO związane z radiem
const short PEER_NODE_ID = 1; // id Arduino Mini Pro związane z radiem

// komponenty radiowe
RF24 radio(7, 8); 
RF24Network network(radio);

void setup() {
  Serial.begin(115200);
  SPI.begin();
  radio.begin();
  network.begin(47, THIS_NODE_ID);
  Serial.println("Setup done");
}

void loop() {
  int tempss = 5;
  network.update();
  while (network.available()) // sprawdź, czy nadeszła nowa wiadomosć
  {
    RF24NetworkHeader header; // nagłówek wiadomości radiowej, nadanej do Mini
    bool success = network.read(header, &tempss, sizeof(tempss)); // spróbuj odebrać wiadomość

    if (success)
    {
      Serial.println("rcvd");
      Serial.println(tempss);
    }
    else
    {
      Serial.println("Radio msg failure.");
    }
  }

}
