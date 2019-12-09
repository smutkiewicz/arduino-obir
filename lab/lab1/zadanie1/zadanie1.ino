/*
  Autorzy:
    Rafał Raczyński 283528
    Michał Smutkiewicz 283538
    Joanna Zalewska 283557

  Program zmusza diodę na płytce Arduino do migania wg określonej sekwencji, podanej
  na karcie z poleceniem.

  Funkcje "turn_(on/off)_and_delay odpowiednio włączają i wyłączają diodę, a następnie
  opóźniają wykonywanie programu o podaną liczbę ms.
*/


void setup() {
  // put your setup code here, to run once:
  pinMode(13, OUTPUT);
}

void loop() {
  // put your main code here, to run repeatedly:
  turn_on_and_delay(10);
  turn_off_and_delay(100);
  
  turn_on_and_delay(100);
  turn_off_and_delay(100);
  
  turn_on_and_delay(100);
  turn_off_and_delay(100);
  
  turn_on_and_delay(100);
  turn_off_and_delay(100);
  turn_off_and_delay(1000);
}

void turn_on_and_delay(int time) {
  digitalWrite(13, HIGH);
  delay(time);
}

void turn_off_and_delay(int time) {
  digitalWrite(13, LOW);
  delay(time);  
}
