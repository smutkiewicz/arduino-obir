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
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <Keypad.h>

//==========================================================================================================
//                                                                                                deklaracje
//==========================================================================================================

// Obsluga sieci
byte mac[]={0x00, 0xaa, 0xbb, 0xcc, 0xde, 0xf3};
EthernetUDP udp;
char packetBuffer[255];
int MAX_BUFFER_IN = 255;
byte ip[] = { 10, 0, 0, 177 };


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

//==========================================================================================================
//                                                                                                     setup
//==========================================================================================================

void setup() {

  // Serial dla debugu, do wywalenia w ostatecznej wersji
  Serial.begin(115200);

  // Ethernet musi byc wywolany z danym IP, bo siec niekoniecznie jest
  // w stanie podac nam je automatycznie (do weryfikacji?)
  Ethernet.begin(mac, ip);
  Serial.println(Ethernet.localIP());
  short localPort = 2353;
  udp.begin(localPort);

  // Output do sterowania poziomem lampki
  pinMode(LAMP_PIN, OUTPUT);

  // Setup poziomu jasnosci na poczatkowy
  setLightLevel(lampLightLevel);
}

//==========================================================================================================
//                                                                                                      loop
//==========================================================================================================

void loop() {
  
  //getReqPickReaction();
  getKeyAndLog(); // temp dla debugu
  
}

//===========================================================================================================
//                                                                                               inne funkcje
//===========================================================================================================


//====================================================================================== Obsluga zapytan ====
void getReqPickReaction(){
  
  int size = udp.parsePacket();
  Serial.println("req loop");
  if (size) {

    Serial.println("req loop size");
    // Wstepne pobranie informacji o pakiecie
    udp.read(packetBuffer, MAX_BUFFER_IN);
    const char * reqType = getRequestType();
    const char * device = getDeviceFromRequest();

    // Reakcja na odpowiedni typ zapytania
    if(reqType == "GET"){
      // bla bla
    }
    else if(reqType ==  "PUT"){

      // PUT musi zawierac jakas wartosc, raczej
      const char * value = getValueFromRequest();

      // Tylko LAMPKA obsluguje PUT (?)
      if(device == "LAMPKA"){
        int decValue = convertOctToDec(value);
        setLightLevel(decValue);
      }
      else{
        Serial.println("Nielegalne zadanie PUT, ignoruje.");
        returnError();
      }
      
    }
    else {
      Serial.println("Odebrano zapytanie nieznanego/nieobslugiwanego typu, ignoruje.");
      returnError();
    }
  }
  // else size=0, ignore, nie bede logowal zerowych
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

//============================================================================ Pobieranie typu zapytania ====
const char * getRequestType(){
  // todo GET czy PUT, wyluskiwanie z req
  return "PUT";
}

//==================================================================== Pobieranie czystej wartosci z PUT ====
const char * getValueFromRequest(){
  // obsluga wyluskiwania z pakietu CoAP czystej wartosci, jesli to PUT
  return "100";
}

//================================== Pobieranie identyfikatora urzadzenia docelowego (klawiatura/lampka) ====
const char * getDeviceFromRequest() {
  //todo, odczyt czy lampka czy klawa
  return "LAMPKA";
}

//=================================================================== Ustawianie stanu (jasnosci) lampki ====
void setLightLevel(int level) {
  analogWrite(LAMP_PIN, level);
}

//======================================================================= Zwroc blad przed lacze radiowe ====
void returnError(){
  // zwroc odpowiedni error code w zaleznosci od parametrow, todo
}


//===========================================================================================================
//                                                                                            K O S T N I C A
//===========================================================================================================



        // potencjalnie uzyteczne
        //
        //udp.beginPacket(udp.remoteIP(), udp.remotePort());
        //int l = udp.write(sendBuffer, MAX_BUFFER_OUT);
        //udp.endPacket();
