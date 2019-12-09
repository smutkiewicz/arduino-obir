// Rafał Raczyński
// Michał Smutkiewicz
// Joanna Zalewska
//
// Zadanie 4

#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

// ustawiamy podany w poleceniu adres MAC
byte mac[]={0x00, 0xaa, 0xbb, 0xcc, 0xde, 0xf3};

// Zmienne i obiekty
EthernetUDP udp;
char packetBuffer[255];
int MAX_BUFFER = 255;

// w funkcji setup() inicjalizujemy serwer UDP na zadanym porcie, a także
// przygotowujemy pin 3 do sterowania lampką (tak, jak w zadaniu 3)
void setup() {
  Serial.begin(115200);
  Ethernet.begin(mac);
  //Log - wypisujemy adres
  Serial.println(Ethernet.localIP());
  short localPort = 2353;
  udp.begin(localPort);
  // do sterowania lampką
  pinMode(3, OUTPUT);
}

// w funkcji loop() nasłuchujemy nadchodzących poleceń. Jeśli takie nadejdzie,
// odczytujemy wartość OCT i konwertujemy ją do postaci dziesiętnej. Uzyskana
// w ten sposób liczba jest poziomem jasności lampki.
void loop() {
	
  // funkcja zwraca rozmiar pakietu
  int packetSize=udp.parsePacket();
  if(packetSize){
    int r = udp.read(packetBuffer, MAX_BUFFER);

	// W celach debugu wypisujemy otrzymaną wartość OCT
    Serial.print("packetBuffer=");
    Serial.println(packetBuffer);

    int dec = oct_to_dec();

	// wypisujemy też wartość przekonwertowaną na DEC
    Serial.print("dec=");
    Serial.println(dec);

	// Pin 3 steruje jasnością lampki
    analogWrite(3, dec);
  }
}

int oct_to_dec() {
  int ocet = atoi(packetBuffer);
  int dec = 1;
  int i = 0;
  int temp = 0;

  // Odczytana wartość modulo 10 (ostatnia cyfra) mnożona jest przez odpowiednią potęgę 8
  // i dodawana do sumy. Niestety, nie zdążyliśmy zabezpieczyć funkcji przed wprowadzeniem złych
  // cyfr (spoza formatu OCT), gdyż skupiliśmy się na dalszych zadaniach.
  while (ocet != 0) {
    temp = ocet % 10;
    ocet = ocet / 10;
    dec += temp * pow(8, i);
    i++;
  }

  return dec;
}
