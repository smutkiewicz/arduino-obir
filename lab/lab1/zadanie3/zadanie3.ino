/*
  Autorzy:
    Rafał Raczyński 283528
    Michał Smutkiewicz 283538
    Joanna Zalewska 283557

  Program odczytuje napięcie na podłączonym do płytki potencjometrze i wyświetla go w formacie HEX w pętli. Wartość odczytywana jest z pinu A0.
*/


int sensorValue = 0;
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
}

void loop() {
  // put your main code here, to run repeatedly:
  sensorValue = analogRead(A0);
  Serial.println(sensorValue, HEX);
  delay(2);
}
