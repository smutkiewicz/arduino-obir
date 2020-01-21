//  ARDUINO MINI PRO
//  Urzadzenie zarzadzajace lampka i klawiatura, odbierajace polecenia droga radiowa
//
//  Rafał Raczyński
//  Michał Smutkiewicz
//  Joanna Zalewska
//
//==========================================================================================================
//                                                                                                   includy
//==========================================================================================================

#include <SPI.h>          // radio
#include <RF24Network.h>  // radio
#include <RF24.h>         // radio
#include <Keypad.h>       // klawiatura

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

// Pin sluzacy do sterowania poziomem jasnosci lampki
int LAMP_PIN = 3;

// Poczatkowy poziom jasnosci lampki - 255 na pinie jest odpowiednikiem poziomu "0".
int lampLightLevel = 255; // czyli nie swieci

// Radio podlaczone pod piny 10 i A2
RF24 radio(A2, 10);
RF24Network network(radio);

// Numery wezlow przypisane zgodnie z konwencja za laboratorium 2
const short THIS_NODE_ID = 1;
const short PEER_NODE_ID = 0;

// Przyjmujemy kanal 38
int OUR_CHANNEL = 38;

// Payload radiowy
struct payload_t
{
  unsigned long timestamp; // stempel czasowy
                           // Wykorzystywany m.in. w celu obliczania czasu propagacji wiadomosci
  short type; // typ zasobu
              // Na podstawie zdefiniowanych nizej stalych wartosci
              
  int value; // przenoszona wartość
             // Uzywana do przesylania dodatkowych danych, poza zadaniami
};

// Numery zadan wymienianych miedzy urzadzeniami
const short ALL = 0;          // Nieuzywane
                              // Zadanie zwrocenia wartosci lampki i klawiatury
                     
const short GET_KEYBOARD = 1; // Klawiatura
                              // Zadanie zwrocenia wartosci aktualnie przyciskanego klawisza klawiatury
                              
const short GET_LED = 2;      // Lampka
                              // Zadanie zwrocenia stanu lampki (poziom jasnosci)
                          
const short SET_LED = 3;      // Lampka
                              // Zadanie ustawienia stanu lampki (poziom jasnosci)
                          
const short STATS = 4;        // Nieuzywane
                              // Zadanie zwrocenia pustej odpowiedzi w celu obliczenia czasu propagacji

//==========================================================================================================
//                                                                                                     setup
//==========================================================================================================

void setup() {

  // Port serial inicjalizowany dla debugu i sledzenia dzialania urzadzenia
  Serial.begin(115200);

  // Radio wywolywane na ustalonych pinach i przy ustalonych parametrach (kanal, ID)
  SPI.begin();
  radio.begin();
  network.begin(OUR_CHANNEL, THIS_NODE_ID);

  // Output do sterowania poziomem jasnosci lampki, na pinie zdefiniowanym wczesniej
  pinMode(LAMP_PIN, OUTPUT);

  // Setup poziomu jasnosci na poczatkowy - zeby lampka nie swiecila po uruchomieniu urzadzenia
  setLightLevel(lampLightLevel);
  
}

//===========================================================================================================
//                                                                                               inne funkcje
//===========================================================================================================

//============================================================================ Konwersja 0-1000 na 255-0 ====
// Lampka przyjmuje wartosci jasnosci od 0-1000 (ciemny-jasny), natomiast trzeba to
// przekonwertowac na wartosci 0-255 (jasny-ciemny) przed puszczeniem na pin
int levelToShort(int longLevel){
  int shortLevel =(int) ((float)longLevel/(1000.0/255.0));
  return 255-shortLevel;
}

//============================================================================ Konwersja 255-0 na 0-1000 ====
// Poziom lampki jest przechowywany w notacji "na pin" (0-255, jasny-ciemny),
// przed wyslaniem do serwera trzeba go przekonwertowac na 0-1000 (ciemny-jasny)
int levelToLong(int shortLevel){
  int longLevel = (int) ((float)shortLevel*(1000.0/255.0));
  return 1000-longLevel;
}

//============================================ Pobieranie klawisza z klawiatury i wyslanie go przez radio ====
char getKeyAndNotify() {

  // Odczyt klawisza
  char k = pad.getKey();

  // Jesli jakis klawisz jesy naciskany - wyslij wiadomosc do serwera
  if(k != NO_KEY) {
    Serial.print("SEND KEYPAD: ");
    Serial.println(k);

    // Ponizej - standardowa procedura wysylania payloadu
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

  // Poziom jasnosci lampki jest odczytywanty i wysylany do serwera
  RF24NetworkHeader header(PEER_NODE_ID);
  payload_t p;
  p.type = GET_LED;

  // wartosc 0-255 jest konwertowana na 0-1000
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

  // Pin LAMP_PIN sluzy do sterowania poziomem jasnosci lampki
  analogWrite(LAMP_PIN, level);

  // Aktualny poziom jasnosci jest przechowywany w zmiennej
  lampLightLevel = level;
}

//====================================================================================== Obsluga zapytan ====
bool getReq(payload_t *payload){

  // Pobieranie z radia zadania wyslanego przez serwer,
  // wartosci przypisywane sa do pol przekazanego payloadu
  RF24NetworkHeader header(THIS_NODE_ID);
  bool result = network.read(header, payload, sizeof(*payload));
  return result;
}

//==========================================================================================================
//                                                                                                      loop
//==========================================================================================================

void loop() {

  // Przygotowanie do sprawdzenia radia
  bool getReqSuccess = false;
  network.update();
  payload_t payload;

  // Jesli serwer wysyla polecenie - odczytaj
  while (network.available()){
    getReqSuccess = getReq(&payload);
  }

  // Jesli odebrano polecenie - pobierz wartosci payloadu i odpowiednio zareaguj
  if(getReqSuccess){
    short reqType = payload.type;
    int value = payload.value;
    unsigned long timestamp = payload.timestamp;

    // Reakcja na odpowiedni typ zapytania
    if(reqType == GET_LED){
      getLightLevelAndNotify(); // Odpowiedz z poziomem jasnosci lampki
    }
    else if(reqType == SET_LED){
      setLightLevel(levelToShort(value)); // Ustaw poziom lampki, po konwersji formatu 0-1000 na 0-255
    }
    else if(reqType == GET_KEYBOARD){
      getKeyAndNotify(); // Jesli naciskany jest jakis klawisz - wyslij jaki
    }
    else {
      Serial.println("Odebrano zapytanie nieznanego/nieobslugiwanego typu, ignoruje.");
    }
  }

  // Jesli radio milczy - sprawdz, czy nie klika ktos przycisku.
  getKeyAndNotify(); // Jesli tak - powiadom serwer.

  //getKeyAndLog(); // Debug na wypadek testowania klawiatury bez radia
  
}
