// Rafał Raczyński
// Michał Smutkiewicz
// Joanna Zalewska
//
// Zadanie 3

// poziom jasności
unsigned char level=0;

// Inicjalizacja pinu 3 jako wyjścia na lampkę
void setup() {
  pinMode(3, OUTPUT);
}

// W 30-milisekundowych interwałach inkrementujemy poziom jasności lampki. Maksymalna jego wartość to 255.
// Przekroczenie tego poziomu powoduje powrót do wartości 0.
void loop() {
  analogWrite(3, level);
  level++;
  delay(30);
}
