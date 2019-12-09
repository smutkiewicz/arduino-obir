// Rafał Raczyński
// Michał Smutkiewicz
// Joanna Zalewska
//
// Zadanie 4

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <Keypad.h>

byte mac[]={0x00, 0xaa, 0xbb, 0xcc, 0xde, 0xf3};
EthernetUDP udp;

char packetBuffer[255];
char sendBuffer[1];
int dec;

int MAX_BUFFER_IN = 255;
int MAX_BUFFER_OUT = 1;
byte ip[] = { 10, 0, 0, 177 };

// Oznaczenia literowe przycisków klawiatury
char keys[4][3] = {
  {'*', '0', '#'},
  {'7', '8', '9'},
  {'4', '5', '6'},
  {'1', '2', '3'}
};

// Przyjęto następujące piny na rzędy i kolumny klawiatury:
byte row[4] = {A4, A5, A0, 2};
byte col[3] = {A2, A1, A3};
Keypad pad = Keypad(makeKeymap(keys), row, col, 4, 3);

// w funkcji setup() inicjalizujemy serwer UDP na zadanym porcie, a także
// przygotowujemy pin 3 do sterowania lampką (tak, jak w zadaniu 3 i 4)
void setup() {
  Serial.begin(115200);
  Serial.println("test1");
  //Ethernet.begin(mac);
  Ethernet.begin(mac, ip);
  Serial.println(Ethernet.localIP());
  short localPort = 2353;
  udp.begin(localPort);
  Serial.println("test1");
  pinMode(3, OUTPUT);
}

// w funkcji loop() nasłuchujemy nadchodzących poleceń. Jeśli takie nadejdzie,
// odczytujemy wartość OCT i konwertujemy ją do postaci dziesiętnej. Uzyskana
// w ten sposób liczba jest poziomem jasności lampki.
// 
// Nowym elementem jest tu próba odczytu wartości z klawiatury numerycznej, co niestety nie udało się.
// Dopiero w czystym, testowym kodzie i przy zmianie wybranych portów odczytaliśmy poprawnie
// naciskany klawisz.
void loop() {
  
  int size = udp.parsePacket();

  if (size) {
    Serial.println("test");
    int r = udp.read(packetBuffer, MAX_BUFFER_IN);

    Serial.print("packetBuffer=");
    Serial.println(packetBuffer);

    dec = oct_to_dec();

    Serial.print("dec=");
    Serial.println(dec);

    analogWrite(3, dec);
  }

  char k = pad.getKey();

  if (k != NO_KEY) {
    Serial.println(k);
    sendBuffer[0] = k;

    udp.beginPacket(udp.remoteIP(), udp.remotePort());
    int l = udp.write(sendBuffer, MAX_BUFFER_OUT);
    udp.endPacket();

    Serial.print("dec=");
    Serial.println(dec);

    analogWrite(3, (k-'0')*28);
  }
}

int oct_to_dec() {
  int oc = atoi(packetBuffer);
  int dec = 1;
  int i = 0;
  int temp = 0;

  // Odczytana wartość modulo 10 (ostatnia cyfra) mnożona jest przez odpowiednią potęgę 8
  // i dodawana do sumy. Niestety, nie zdążyliśmy zabezpieczyć funkcji przed wprowadzeniem złych
  // cyfr (spoza formatu OCT).
  while (oc != 0) {
    temp = oc % 10;
    oc /=10;
    dec += temp * pow(8, i);
    i++;
  }

  return dec;
}
