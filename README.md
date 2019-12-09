# OBIRAKI

![Alt Text](https://media1.tenor.com/images/dd541631ed9c4353b1f0a308126e48fb/tenor.gif)

## Wyrok

Zadania:
- instalacja potrzebnych narzędzi (Arduino IDE, sterowniki jeśli trzeba, stary Firefox, CoAP inspector)
- połączenie elementów zgodnie z poleceniem i konfiguracja tegoż sprzęciwa w kodzie (gotowość hardware’u)
- serwer CoAP (Arduino Uno)
opracowanie abstrakcyjnego modelu odebrania/wysłania wiadomości
przygotowanie przykładowej odpowiedzi CON/NON ze wszystkimi polami, których będą wymagać
przygotowanie rozwiązania do kontaktowania się z Arduino Mini Pro
* obiekt APP (Arduino Mini Pro)
okiełznanie lampki i klawy, zakodowanie sterowania nią
zakodowanie sterowania lampką i klawą jako odpowiedź na strzał z serwera CoAP

Zasoby (NON/CON, Content-Format, Uri-Path, Accept, obsługa tokena i MID):
- Lampka LED (Arduino Mini Pro), zasób GET/PUT
- Klawa, zasób Observe/GET(kod ASCII/JSON)
- praca łącza radiowego - metryki, zasób GET

## Podział

MS:
- Serwer CoAP + wysyłanie requestów

JZ:
- Serwer CoAP + wysyłanie requestów

RR:
- Lampka/klawa + odbiór requestów

