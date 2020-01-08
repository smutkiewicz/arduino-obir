// Mini Pro
// Lampka + klawiatura
//
// z pobudek religijnych odrzucam snejka
// nie jestem w stanie tego przetestowac poza kompilacja
//
// (a ze guzik z C pamietam, pewnie eksploduje)
//
// ni w zab nie ogarniam tych char arrayow w funkcjach w C,
// chca jakichs cost char pointerow
// bo niby z C++ niekompatybilne...
//
//==========================================================================================================
//                                                                                                   includy
//==========================================================================================================

#include <SPI.h>
#include <RF24Network.h>
#include <RF24.h>
//#include <Ethernet.h>
//#include <EthernetUdp.h>
#include <Keypad.h>

//==========================================================================================================
//                                                                                                deklaracje
//==========================================================================================================

// Obsluga sieci
//byte mac[]={0x00, 0xaa, 0xbb, 0xcc, 0xde, 0xf3};
//EthernetUDP udp;
char packetBuffer[255];
int MAX_BUFFER_IN = 255;
//byte ip[] = { 10, 0, 0, 177 };


// Oznaczenia literowe przycisków klawiatury
char keys[4][3] = {
  {'*', '0', '#'},
  {'7', '8', '9'},
  {'4', '5', '6'},
  {'1', '2', '3'}
};


// Piny na rzędy i kolumny klawiatury:
byte row[4] = {4, 5, A0, 2};
byte col[3] = {A2, A1, A3};

// Klawiatura
Keypad pad = Keypad(makeKeymap(keys), row, col, 4, 3);

// Lampka
int LAMP_PIN = 3;
int lampLightLevel = 255; // czyli nie swieci

// Radio
RF24 radio(9, 6);
RF24Network network(radio);
int THIS_NODE_ID = 1;
int PEER_NODE_ID = 0;
int OUR_CHANNEL = 47; // Do sprawdzenia?
int currentMessageID = 1;

// z uno
struct payload_t
{
  unsigned long timestamp; // stempel czasowy
  short type; // typ zasobu
  int value; // przenoszona wartość
};

// id zasobów do komunikacji z radiem
const short ALL = 0;
const short GET_KEYBOARD = 1;
const short GET_LED = 2;
const short SET_LED = 3;
const short STATS = 4;

//==========================================================================================================
//                                                                                                     setup
//==========================================================================================================

void setup() {

  // Serial dla debugu, do wywalenia w ostatecznej wersji
  Serial.begin(115200);

  // Radio wywolywane na ustalonych pinach
  SPI.begin();
  radio.begin();
  network.begin(OUR_CHANNEL, THIS_NODE_ID);

  // Output do sterowania poziomem lampki
  pinMode(LAMP_PIN, OUTPUT);

  // Setup poziomu jasnosci na poczatkowy
  setLightLevel(lampLightLevel);
}

//===========================================================================================================
//                                                                                               inne funkcje
//===========================================================================================================

//============================================================================ Konwersja 0-1000 na 255-0 ====
int levelToShort(int longLevel){
  int shortLevel = longLevel/(1000/255);
  return 255-shortLevel;
}

//============================================================================ Konwersja 255-0 na 0-1000 ====
int levelToLong(int shortLevel){
  int longLevel = shortLevel*(1000/255);
  return 1000-longLevel;
}

//============================================================== Pobieranie klawisza z klawiatury (+log) ====

char getKeyAndLog() {
  
  char k = pad.getKey();

  if(k != NO_KEY) {
    Serial.print("KEYPAD: ");
    Serial.println(k);
    if(k == '*') {
      Serial.print("DEBUG current light level: ");
      Serial.println(lampLightLevel);
    }
    else {
      int tempLightLevel = (k-'0')*51;
      setLightLevel(tempLightLevel);
      lampLightLevel = tempLightLevel;
      Serial.print("DEBUG set light level: ");
      Serial.println(lampLightLevel);
    }
  }

  return k;
}

//============================================ Pobieranie klawisza z klawiatury i wyslanie go przz radio ====
char getKeyAndNotify() {
  
  char k = pad.getKey();

  if(k != NO_KEY) {
    Serial.print("SEND KEYPAD: ");
    Serial.println(k);
    RF24NetworkHeader header(PEER_NODE_ID);

    struct payload_t p;
    p.type = GET_KEYBOARD;
    p.value = k;
    p.timestamp = millis();
    
    bool success = network.write(header, &p, sizeof(p));
    
  }

  return k;
}

//====================================================== Pobieranie stanu lampy i wyslanie go przez radio ====
void getLightLevelAndNotify() {

  RF24NetworkHeader header(PEER_NODE_ID);
  struct payload_t p;
  p.type = GET_LED;
  p.value = levelToLong(lampLightLevel);
  p.timestamp = millis();
    
  bool success = network.write(header, &p, sizeof(p));

}

//======================================================================= Konwersja z formatu OCT na DEC ====
int convertOctToDec(const char value[]) {
  
  int octValue = atoi(value);
  int decValue = 1;
  int i = 0;
  int temp = 0;

  while (octValue != 0) {
    temp = octValue % 10;
    octValue /= 10;
    decValue += temp * pow(8, i);
    i++;
  }

  return decValue;
}

//=================================================================== Ustawianie stanu (jasnosci) lampki ====
void setLightLevel(int level) {
  Serial.println(level);
  analogWrite(LAMP_PIN, level);
  lampLightLevel = level;
}

//======================================================================= Zwroc blad przed lacze radiowe ====
void returnError(){
  // walic radio, niech na ten moment po prostu wiem.
  Serial.println("Nieobslugiwany typ requesta, odrzucam.");
}

//====================================================================================== Obsluga zapytan ====
void getReqPickReaction(){

  payload_t payload;
  RF24NetworkHeader header(THIS_NODE_ID);
  bool result = network.read(header, &payload, sizeof(payload));
  
  if (result) {

    short reqType = payload.type;
    int value = payload.value;
    unsigned long timestamp = payload.timestamp;

    // Reakcja na odpowiedni typ zapytania
    if(reqType == GET_LED){
      getLightLevelAndNotify();
    }
    else if(reqType == SET_LED){
      int decValue = convertOctToDec(value);
      setLightLevel(levelToShort(decValue));
    }
    else if(reqType == GET_KEYBOARD){
      getKeyAndNotify();
    }
    else if(reqType == STATS){
      payload.timestamp = millis();
      bool success = network.write(header, &payload, sizeof(payload));
    }
    else {
      Serial.println("Odebrano zapytanie nieznanego/nieobslugiwanego typu, ignoruje.");
      returnError();
    }
  }
}

//==========================================================================================================
//                                                                                                      loop
//==========================================================================================================

void loop() {

  network.update();
  while (network.available()){
    Serial.println("DEBUG avail");
    getReqPickReaction();
  }
  
  getKeyAndLog(); // temp dla debugu
  
}

//===========================================================================================================
//                                                                                            K O S T N I C A
//===========================================================================================================
