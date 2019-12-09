#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>

// Zadanie 3, wersja na płytkę UNO - wysyłanie poleceń do płytki MINI i odbieranie odpowiedzi.

// Uwagi ogólne:
// Z powodu braku czasu (mieliśmy problemy z podłączeniem płytki) nie udało nam się
// doprowadzić zadania do końca. Płytka poprawnie wysyła do MINI polecenia, ale ta nie
// reagowała na nie. Potencjometr został podłączony, ale kod odczytujący z niego wartość
// nie został dokończony.

// Zainicjowano radio na podanym kanale
RF24 radio (7,8);
RF24Network network(radio);

// Przypisano ID zgodnie z zaleceniami instrukcji - 0 i 1
int THIS_NODE_ID = 0;
int PEER_NODE_ID = 1;

// Nasz system przesyłał jedynie jedną liczbę, w związku z czym struktura paczki danych
// zawiera jedną zmienną typu short. Zmienna ta zajmuje 16b, co jest zgodne z wymaganiami
// zadania.
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
  // Inicjalizujemy payload oraz pusty String, który posłuży nam do zbudowania polecenia
  // na podstawie inputu użytkownika.
  struct payload p;
  String readString = "";

  network.update();

  // Odbiór odpowiedzi od płytki MINI. W pierwszych testach działał poprawnie, później
  // (po przełączeniu płytki MINI w "tryb" przesyłania wartości potencjometru) nie dostawaliśmy
  // poprawnego odczytu - płytka nie czytała poprawnie wartości.
  while(network.available()){
    RF24NetworkHeader header(THIS_NODE_ID);
    bool success = network.read(header, &p, sizeof(p));

	//Na tym etapie używamy wielokrotnie Serial.println w celu debugowania całego procesu.
    if (success) {
      Serial.print("Received = ");
      Serial.println(p.counter);
    } else {
      Serial.print("Unable to process packet...");
    }

  }
    
  // Poniższa sekcja kodu ma za zadanie pobrać input od użytkownika. Zastosowano delay
  // ze względu na stosowanie odczytu serial.
  // Odczytujemy wartości jeden bajt na raz, aż do uzyskania całej liczby. Na tym etapie
  // prac nie sprawdzamy jeszcze, czy zmieści się ona poprawnie w typie short.
  while (Serial.available()){ 
    delay(3);
    if (Serial.available() > 0) {
      char c = Serial.read();  //gets one byte from serial buffer
      readString += c; //makes the string readString
    } 
  }

  // Poniższa sekcja kodu wysyła odczytany input użytkownika w postaci pojedynczej liczby.
  // Jeśli input jest pusty, wysyłanie jest zaniechane.
  if (readString.length() >0) {
      p.counter = atoi(readString.c_str());
      RF24NetworkHeader header(PEER_NODE_ID);
      bool success = network.write(header, &p, sizeof(p));
      readString = "";
      
	  // Na bazie wartości zmiennej success wypisywaliśmy na ekran odpowiedni komunikat,
	  // by dowiedzieć się, czy pakiet został przez płytkę MINI poprawnie odebrany.
      if (success) {
		// Serial.println zachowywał się dziwnie gdy używaliśmy składni takiej, jak w sekcji else.
	    // Ostatecznie rozbiliśmy wypisywanie na dwie komendy.
        Serial.print("Send packet = ");// + p.counter);  
        Serial.println(p.counter);
      } else {
        Serial.println("Unable = " + p.counter);  
        }
  }
  
}

// funkcja nie została ostatecznie wykorzystana.
int convert_to_number(char c) {
  int num = c - '0';
  return num;
}

