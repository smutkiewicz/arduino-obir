
//==========================================================================================================
//                                                                                                   includy
//==========================================================================================================

#include <SPI.h>
#include <RF24Network.h>
#include <RF24.h>
#include <Keypad.h>

//==========================================================================================================
//                                                                                                deklaracje
//==========================================================================================================

// Oznaczenia literowe przycisków klawiatury
char keys[4][3] = {
  {'*', '0', '#'},
  {'7', '8', '9'},
  {'4', '5', '6'},
  {'1', '2', '3'}
};


// Piny na rzędy i kolumny klawiatury:
byte row[4] = {7, 9, A3, 5};
byte col[3] = {6, 4, 8};

// Klawiatura
Keypad pad = Keypad(makeKeymap(keys), row, col, 4, 3);

// Lampka
int LAMP_PIN = 3;
int lampLightLevel = 255; // czyli nie swieci

// Radio
RF24 radio(A2, 10);
RF24Network network(radio);
const short THIS_NODE_ID = 1;
const short PEER_NODE_ID = 0;
int OUR_CHANNEL = 47;

// Payload radiowy
struct payload_t
{
  unsigned long timestamp; // stempel czasowy
  short type; // typ zasobu
  int value; // przenoszona wartość
};

// id zasobów do komunikacji z radiem
const short ALL = 0; // nieuzywane?
const short GET_KEYBOARD = 1;
const short GET_LED = 2;
const short SET_LED = 3;
const short STATS = 4; // nieuzywane?

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

  Serial.println("SETUP done");
}

//===========================================================================================================
//                                                                                               inne funkcje
//===========================================================================================================

//============================================================================ Konwersja 0-1000 na 255-0 ====
int levelToShort(int longLevel){
  int shortLevel =(int) ((float)longLevel/(1000.0/255.0));
  return 255-shortLevel;
}

//============================================================================ Konwersja 255-0 na 0-1000 ====
int levelToLong(int shortLevel){
  int longLevel = (int) ((float)shortLevel*(1000.0/255.0));
  return 1000-longLevel;
}

//============================================================== Pobieranie klawisza z klawiatury (+log) ====
//==============================================================                  CZASEM PRZYDATNY DEBUG ====
//==============================================================              DO WYWALENIA JAK SKONCZYMY ====

// Ustawia poziom lampki jako klawisz*51 (w notacji 0-255)
// + Wypisuje klawisz
// + * Wypisuje poziom
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
      Serial.println(levelToLong(lampLightLevel));
    }
  }

  return k;
}

//============================================ Pobieranie klawisza z klawiatury i wyslanie go przez radio ====
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
    if(success){
      Serial.println("KEY sent");
    }
    else {
      Serial.println("KEY fail");
    }
    
  }

  return k;
}

//====================================================== Pobieranie stanu lampy i wyslanie go przez radio ====
void getLightLevelAndNotify() {

  RF24NetworkHeader header(PEER_NODE_ID);
  payload_t p;
  p.type = GET_LED;
  p.value = levelToLong(lampLightLevel);
  p.timestamp = millis();
    
  bool success = network.write(header, &p, sizeof(p));
  if(success){
    Serial.println("LIGHT sent");
  }
  else {
    Serial.println("LIGHT fail");
  }

}

//=================================================================== Ustawianie stanu (jasnosci) lampki ====
void setLightLevel(int level) {
  Serial.print("SET LIGHT INTERNAL: ");
  Serial.println(level);
  analogWrite(LAMP_PIN, level);
  
  lampLightLevel = level;
}

//====================================================================================== Obsluga zapytan ====
bool getReq(payload_t *payload){

  RF24NetworkHeader header(THIS_NODE_ID);
  bool result = network.read(header, payload, sizeof(*payload));
  return result;
}

//==========================================================================================================
//                                                                                                      loop
//==========================================================================================================

void loop() {

  bool getReqSuccess = false;
  network.update();
  payload_t payload;
  
  while (network.available()){
    getReqSuccess = getReq(&payload);
  }

  if(getReqSuccess){
    short reqType = payload.type;
    int value = payload.value;
    unsigned long timestamp = payload.timestamp;

    // Reakcja na odpowiedni typ zapytania
    if(reqType == GET_LED){
      getLightLevelAndNotify();
    }
    else if(reqType == SET_LED){
      setLightLevel(levelToShort(value));
    }
    else if(reqType == GET_KEYBOARD){
      getKeyAndNotify();
    }
    else {
      Serial.println("Odebrano zapytanie nieznanego/nieobslugiwanego typu, ignoruje.");
    }
  }

  // Jesli radio milczy - sprawdz, czy nie klika ktos przycisku.
  getKeyAndNotify();

  //getKeyAndLog(); // Debug na wypadek testowania klawiatury bez radia
  
}

//===========================================================================================================
//                                                                                            K O S T N I C A
//===========================================================================================================
