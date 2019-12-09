/*
  Autorzy:
    Rafał Raczyński 283528
    Michał Smutkiewicz 283538
    Joanna Zalewska 283557

  Program zmusza diodę na płytce Arduino do migania wg określonej sekwencji, podanej
  na karcie z poleceniem z okreslonym przez uzytkownika delayem.
*/

// aktualnie wczytany znak
char c;

// ilosc cyfr wczytanych do parsowania liczby, max to 3 cyfry
int numbers = 0;

// czy input został przedzielony spacją
bool received_space = false;

// pomocnicza zmienna do budowania pełnej liczby
int current_num = 0;

// tablica do zapisu podanych przekonwertowanych liczb
int delays[2];

// pomocnicza zmienna do indeksowania tablicy do zapisu podanych przekonwertowanych liczb
int delay_index = 0;

void setup() {
  // ustawienie zalecanej szybkości transmisji danych
  Serial.begin(115200);

  // ustawienie pinu od lampki LED
  pinMode(13, OUTPUT);
}

void loop() {

   // Sprawdzenie, czy bufor posiada znaki.
   if (Serial.available() > 0){ 
      // Odczytanie znaku z bufora.
      c = Serial.read();

      if (delay_index > 1) {
        delay_index = 0;
      }

      // logika parsowania cyfr i budowania z nich numeru
      if (input_is_a_number(c)) {
        numbers++;
        current_num = 10 * current_num + convertToNumber(c);
      } else if (input_is_space(c) && is_input_has_enough_digits()) {
        received_space = true;
        numbers = 0;
        delays[delay_index] = current_num;
        current_num = 0;
        delay_index++;
      } else if (is_input_eol(c) && is_input_has_enough_digits() && delay_index == 1) {
        numbers = 0;
        delays[delay_index] = current_num;
        current_num = 0;
        delay_index++;
        received_space = false;
      } else if (is_input_eol(c)) {
        numbers = 0;
        current_num = 0;
        delay_index = 0;
        received_space = false;

        Serial.println("Nieprawidlowy input. Podaj inne liczby.");
      }
      
   } else if (delay_index == 2) {

      // bufor jest pusty, więc printujemy tylko zawartość pamieci
      Serial.print(delays[0]);
      Serial.print(' ');
      Serial.print(delays[1]);
      Serial.print('\n');

      // własciwe właczanie/wylaczanie diody LED
      turn_on_and_delay(delays[0]);
      turn_off_and_delay(delays[1]);
   }
}

bool input_is_a_number(char c) {
  return numbers < 3 && c > 47 && c < 58;
}

bool is_input_has_enough_digits() {
  return numbers == 3;
}

bool input_is_space(char c) {
  return c == ' ' && !received_space;
}

bool is_input_eol(char c) {
  return c == '\n';
}

void flush_buffer() {
  while(Serial.available() > 0) {
     char t = Serial.read();
  }
}

int convertToNumber(char c) {
  int num = c - '0';
  return num;
}

void turn_on_and_delay(int time) {
  digitalWrite(13, HIGH);
  delay(time);
}

void turn_off_and_delay(int time) {
  digitalWrite(13, LOW);
  delay(time);  
}

