
//----------------------------------------------------Dichiarazione librerie
#include <NTPClient.h> // usare libreria NTPClient-master
#include <WiFi.h>
#include <WiFiUdp.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <DHTesp.h>
//----------------------------------------------------//

//----------------------------------------------------Definizione conf Display
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 24
#define NUM_ZONES   3
#define CLK_PIN   18    //Clock pin
#define DATA_PIN  23    //Data pin
#define CS_PIN    5     //CS pin
#define SPEED_TIME  0
#define PAUSE_TIME  0
//----------------------------------------------------//

//----------------------------------------------------Debug
//#define DEBUG // togliere il commento per attivare debug su seriale
//----------------------------------------------------//

//----------------------------------------------------Files accessori
#include "myfont.h"     // font I've modified from the standard one
#include "secret.h"     // user/passwords for wifi
//----------------------------------------------------//

DHTesp dht;
MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES); //Alias per prefisso libreria MD_Parola
WiFiUDP ntpUDP;

String  hmsCET, hmsUTC, hour, minute, sec;
String  Formatted_date;
String  dayStamp;
String  timeStamp;

long    currentMillis = 0;
long    previousMillis = 0;
int     timeinterval = 1000;      //  refresh interval
int     wifiinterval = 10000;     //  WiFi refresh interval
int     dhtinterval = 2000;       //  DHT refresh interval
int     dateinterval = 3600000;  //  date refresh interval

//----------------------------------------------------Simboli personalizzati
uint8_t degC[] = { 6, 3, 3, 56, 68, 68, 68 };                 // Degree symbol for temperature in °C
uint8_t hum[] = { 5, 99, 19, 8, 100, 99 };                    // % symbol for humidity
uint8_t deg[] = {3, 2, 5, 2};                                 // ° symbol for degree
uint8_t full[] = {8, 255, 255, 255, 255, 255, 255, 255, 255,}; // test segmento completo
//----------------------------------------------------//

//----------------------------------------------------Time zone const
const int CET = 3600;         // in minutes
const int CEST = 7200;        // in minutes
const int UTC = 0;            // in minutes
//----------------------------------------------------//

NTPClient timeClient(ntpUDP, "192.168.88.1");

//----------------------------------------------------display zone const
const int zDOWN = 0;
const int zCENTER = 1;
const int zUP = 2;
//----------------------------------------------------//

void setup() {
  delay(50);
#ifdef DEBUG
  Serial.begin(57600);
#endif
  delay(50);
  dht.setup(21, DHTesp::DHT22); //Inizializzazione DHT (PIN, Modello)

  //----------------------------------------------------Inizializzazione display
  P.begin(NUM_ZONES); //Inizializzo la libreria passando il numero di zone definito ad inizio codice

  //----------------------------------------------------definizione delle zone
  P.setZone(zDOWN, 0, 7);     //zona 0 da 0 a 7
  P.setZone(zCENTER, 8, 15);  //zone 2 da 8 a 15
  P.setZone(zUP, 16, 23);     //zona 3 da 16 a 23
  //----------------------------------------------------//

  P.displayReset();
  P.setIntensity(0);
  //P.setInvert(false); // Inverte il display (true/false)
  delay(50);

  //----------------------------------------------------banner accensione
  P.displayClear();
  //P.setIntensity(0);
  P.displayZoneText(zUP, "IU2GLR", PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
  P.displayZoneText(zDOWN, "LED, NTP, DHT", PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
  P.displayZoneText(zCENTER, "clock rev: 1.4", PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
  P.displayAnimate();
  //----------------------------------------------------//

  delay(2000);

  //----------------------------------------------------Inizializzazione WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
#ifdef DEBUG
    Serial.println("Connecting.");
#endif
    ////P.setIntensity(0);
    P.displayClear();
    P.displayZoneText(zCENTER, "Connecting...", PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
    P.displayAnimate();
  }
  //----------------------------------------------------//
#ifdef DEBUG
  Serial.println("WiFi connected!");
#endif
  //P.setIntensity(0);
  P.displayZoneText(zUP, "Connected", PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
  P.displayZoneText(zCENTER, "to:", PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
  P.displayZoneText(zDOWN, ssid, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
  P.displayAnimate();
  //P.displayReset();

  //----------------------------------------------------Inizializzazione Time
  timeClient.begin();
  //----------------------------------------------------//
  delay(2000);
  P.displayClear();

}

void loop()
{
  checkWiFi();
  obtainTime();

  int rotate;
  rotate = sec.toInt();

  //----------------------------------------------------Visualizzazione alternata Data / Temp&Umidità
  if ((rotate >= 0) && (rotate <= 19) || (rotate >= 30) && (rotate <= 49)) { //se 0<=t<=19 o 30<=t<=49 visualizza info
    obtainInfo();
  }
  else {
    obtainDate();
  }
  //----------------------------------------------------//
  delay(100);
}

void checkWiFi() {
  unsigned long currentMillis = millis();
  // if WiFi is down, try reconnecting every CHECK_WIFI_TIME seconds
  //Serial.println("Connection status: " + WiFi.status());
  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= wifiinterval)) {
    //P.setIntensity(9);
    P.displayZoneText(zCENTER, "wifi lost", PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
#ifdef DEBUG
    Serial.println("WiFi Lost");
#endif
    P.displayZoneText(zDOWN, "Reconnecting", PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
#ifdef DEBUG
    Serial.println("Reconnecting");
#endif
    P.displayAnimate();
    //Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    //WiFi.begin(ssid, password);
    WiFi.reconnect();
    previousMillis = currentMillis;
  }
}

void obtainInfo() {
  //if (currentMillis - previousMillis > dhtinterval)  {
  //previousMillis = millis();
  //----------------------------------------------------Rilevazione umidità
  float humi = dht.getHumidity();
  char beta[5];
  dtostrf(humi, 3, 1, beta);
  String strhumi = "";     // empty temp string
  strhumi.concat(beta);
  P.addChar('%', hum);
  strhumi += '%';
  //----------------------------------------------------//

  //----------------------------------------------------Rilevazione temperatura
  float temp = dht.getTemperature();
  //convert float to string
  char alfa[5];
  dtostrf(temp, 3, 1, alfa);
  String strtemp = "";     // empty temp string
  strtemp.concat(alfa);
  P.addChar('$', deg);
  strtemp += '$';
  //----------------------------------------------------//

  //----------------------------------------------------Creo char array per stampa info su display
  String info = strtemp + " - " + strhumi;  //Creo una stringa contenente tutte le info
  int info_len = info.length() + 1;         // calcolo la lughezza della strina e assegno il valore ad una variabile
  char all[info_len];                       // creo una variabile tipo char array della dimansione di info +1
  info.toCharArray(all, info_len);          //e gli assgno il valore di info
  //----------------------------------------------------//

  //P.setIntensity(0);
  P.displayClear(zUP);
  P.displayZoneText(zUP, all, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);        //Zona per Temp / Date / etc.
  P.displayAnimate();
#ifdef DEBUG
  Serial.println (all);
#endif
  //}
}

void obtainTime() {
  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }

  currentMillis = millis();

  if (currentMillis - previousMillis > timeinterval)  {
    previousMillis = millis();

    //----------------------------------------------------Orario UTC
    timeClient.setTimeOffset(UTC);
    Formatted_date = timeClient.getFormattedDate();
#ifdef DEBUG
    Serial.println(Formatted_date);
#endif DEBUG
    hour = Formatted_date.substring(11, 13);
    minute = Formatted_date.substring(14, 16);
    sec = Formatted_date.substring(17, 19);

    hmsUTC = "UTC " + hour + ":" + minute + ":" + sec;  //Costruisco la stringa con i valori da visualizzare
    //Converte stringa in char array
    int hmsUTC_len = hmsUTC.length() + 1;
    char myhmsUTC[hmsUTC_len];
    hmsUTC.toCharArray(myhmsUTC, hmsUTC_len);
    P.displayZoneText(zCENTER, myhmsUTC, PA_LEFT, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);        //Zona per -UTC
    //----------------------------------------------------//


    //----------------------------------------------------Orario CET/CEST
    timeClient.setTimeOffset(CEST);  //Sostituire con CET,CEST
    Formatted_date = timeClient.getFormattedDate();

    hour = Formatted_date.substring(11, 13);
    minute = Formatted_date.substring(14, 16);
    sec = Formatted_date.substring(17, 19);

    hmsCET = "CET " + hour + ":" + minute + ":" + sec;   //Costruisco la stringa con i valori da visualizzare
    //Converte stringa in char array
    int hmsCET_len = hmsCET.length() + 1;
    char myhmsCET[hmsCET_len];
    hmsCET.toCharArray(myhmsCET, hmsCET_len);
    P.displayZoneText(zDOWN, myhmsCET, PA_LEFT, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);        //Zona per -CET
    //----------------------------------------------------//

    P.displayAnimate();
  }
}

void obtainDate() {
  //Extract date
  //Formatted_date = timeClient.getFormattedDate();
  //if (currentMillis - previousMillis > dateinterval)  {
  //previousMillis = millis();
  int splitT = Formatted_date.indexOf("T");
  dayStamp = Formatted_date.substring(0, splitT);
#ifdef DEBUG
  Serial.println (dayStamp);
#endif
  int dayStamp_len = dayStamp.length() + 1;
  char mydayStamp[dayStamp_len];
  dayStamp.toCharArray(mydayStamp, dayStamp_len);
  P.displayZoneText(zUP, mydayStamp, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);        //Zona per -CET
  P.displayAnimate();
#ifdef DEBUG
  Serial.println (mydayStamp);
#endif
}
/*
  void testDisplay() {
  P.addChar('#', full);
  P.displayZoneText(zUP, '#', PA_LEFT, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);        //Zona per -CET
  P.displayAnimate();
  }
*/
