#include <RF24Network.h>
#include <RF24.h>
#include <SPI.h>

// Zadanie 3, wersja na płytkę MINI - odczyt wartości z potencjometru, przesył
// odczytu drogą radiową, reagowanie na polecenia płytki UNO

// Uwagi ogólne:
// Z powodu braku czasu (mieliśmy problemy z podłączeniem płytki) nie udało nam się
// doprowadzić zadania do końca. Płytka poprawnie odbiera od UNO polecenia, ale nie
// reaguje na nie. Potencjometr został podłączony, ale kod odczytujący z niego wartość
// nie został dokończony.


// Zainicjowano radio na podanym kanale
RF24 radio (7,8);
RF24Network network(radio);

// Przypisano ID zgodnie z zaleceniami instrukcji - 0 i 1
int THIS_NODE_ID = 1;
int PEER_NODE_ID = 0;

// Zmienna ta miała służyć do regulowania częstotliwości pomiaru wartości potencjometru
unsigned long time = 0;

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
	
  // Inicjalizujemy payload z wartością zero. Wartości zerowej użyjemy później do testowania
  // poprawności odczytu.
  struct payload p;
  p.counter = 0;

  
  network.update();

  // Poniższa sekcja kodu miała za zadanie odczytać z połączenia radiowego przychodzący
  // pakiet z poleceniem i zareagować na niego (zmienić częstotliwość). Na tym etapie
  // używamy wielokrotnie Serial.println w celu debugowania całego procesu.
  while(network.available()){
    Serial.println("network available");
    RF24NetworkHeader header(THIS_NODE_ID);
    network.read(header, &p, sizeof(p));
    int temp = analogRead(A1); // w retrospekcji odczyt powinien mieć miejsce w sekcji wysyłania.
    Serial.println("Payload: " + p.counter);
  }

  // Poniższa sekcja miała za zadanie wysłanie do płytki UNO wartości potencjometru, lub
  // (na tym etapie) - przekazanie wartości odebranej w celu debugu.
  // Mechanizm wysyłania działał poprawnie, otrzymywaliśmy potwierdzenie od płtki UNO.
  if (p.counter != 0 && 1/p.counter * 1000 > time) {
    RF24NetworkHeader header(PEER_NODE_ID);
    bool success = network.write(header, &p, sizeof(p));
    
	// Na bazie wartości zmiennej success wypisywaliśmy na ekran odpowiedni komunikat,
	// by dowiedzieć się, czy pakiet został przez płtkę UNO poprawnie odebrany.
    if (success) {
	  // Serial.println zachowywał się dziwnie gdy używaliśmy składni takiej, jak w sekcji else.
	  // Ostatecznie rozbiliśmy wypisywanie na dwie komendy.
      Serial.print("Send packet = ");
      Serial.println(p.counter);
    } else {
      Serial.println("Unable = " + p.counter);  
    }

    time = 0;
  }
  
}
