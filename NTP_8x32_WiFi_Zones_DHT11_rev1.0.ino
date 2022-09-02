
/*
   01/settembre/2022
   agguinta libreria DHTesp per sensore DHT11
*/
/*
   31/agosto/2022
   ottimizzazione memoria rimuovendo animazioni non necessarie dal file:
   C:\Users\marmi01\OneDrive\Documenti\Arduino\libraries\arduino_667928\src\MD_Parola.h
*/



#include <NTPClient.h> // usare libreria NTPClient-master
#include <WiFi.h>
#include <WiFiUdp.h>
#include <MD_Parola.h>
#include <MD_MAX72xx.h>
//#include <SPI.h>
#include <DHTesp.h>


#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 20
#define NUM_ZONES   3
#define CLK_PIN   18
#define DATA_PIN  23
#define CS_PIN    5
#define SPEED_TIME  0
#define PAUSE_TIME  0

DHTesp dht;

MD_Parola Display = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES); //Alias per prefisso

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Configurazione WiFi
const char* ssid       = "MARTINET-OUT";
const char* password   = "asdfghjkl";
//

String  hmCET, hmUTC, hour, minute, sec;
String Formatted_date;
String formattedDate;
String dayStamp;
String timeStamp;

long currentMillis = 0;
long previousMillis = 0;
int timeinterval = 1000;      //  refresh interval
int wifiinterval = 60000;     //  WiFi refresh interval
int dhtinterval = 2000;       //  DHT refresh interval

// Time zone const
const int CET = 3600;
const int CEST = 7200;
const int UTC = 0;

// display zone const
const int zDOWN = 0;
const int zCENTER = 1;
const int zUP = 2;

void setup() {
  Serial.begin(115200);
  dht.setup(21, DHTesp::DHT22); //Inizializzazione DHT (PIN, Modello)
  Display.begin(NUM_ZONES); //Inizializzo la libreria passando il numero di zone definito ad inizio codice
  Display.setIntensity(0);
  Display.displayClear();
  //Display.setInvert(false); // Inverte il display (true/false)

  // definizione delle zone
  Display.setZone(zDOWN, 0, 7);   //zona 0 da 0 a 2       //
  Display.setZone(zCENTER, 8, 15);  //zone 2 da 8 a 10
  Display.setZone(zUP, 16, 19); //zona 3 da 11 a 15
  //Display.setIntensity(0);
  Display.displayZoneText(zCENTER, "World clock", PA_CENTER, SPEED_TIME, 0, PA_NO_EFFECT, PA_NO_EFFECT);
  Display.displayZoneText(zDOWN, "rev: 2.5", PA_CENTER, SPEED_TIME, 0, PA_NO_EFFECT, PA_NO_EFFECT);
  Display.displayAnimate();

  delay(3000);
  Display.displayReset();


  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    //Serial.print("Connecting.");
    //Display.setIntensity(0);
    Display.displayClear();
    Display.displayZoneText(zCENTER, "Connecting...", PA_CENTER, SPEED_TIME, 0, PA_NO_EFFECT, PA_NO_EFFECT);
    Display.displayAnimate();
  }
  //Serial.print("WiFi connected!");
  //Display.setIntensity(0);
  Display.displayClear();
  Display.displayZoneText(zCENTER, "Connected to", PA_CENTER, SPEED_TIME, 0, PA_NO_EFFECT, PA_NO_EFFECT);
  Display.displayZoneText(zDOWN, ssid, PA_CENTER, SPEED_TIME, 0, PA_NO_EFFECT, PA_NO_EFFECT);
  Display.displayAnimate();
  Display.displayReset();

  timeClient.begin();
  delay(2000);
}

void loop()
{
  unsigned long currentMillis = millis();
  // if WiFi is down, try reconnecting every CHECK_WIFI_TIME seconds

  if ((WiFi.status() != WL_CONNECTED) && (currentMillis - previousMillis >= wifiinterval)) {
    Display.displayZoneText(zCENTER, "wifi lost", PA_CENTER, SPEED_TIME, 0, PA_NO_EFFECT, PA_NO_EFFECT);
    Display.displayZoneText(zDOWN, "Reconnecting", PA_CENTER, SPEED_TIME, 0, PA_NO_EFFECT, PA_NO_EFFECT);
    Display.displayAnimate();
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    WiFi.reconnect();
    previousMillis = currentMillis;
  }

  obtainTime();
  if ((currentMillis - previousMillis >= dhtinterval)) {
    obtainTemp();
  }
  delay(200);
}

void obtainTemp() {

  float temp = dht.getTemperature();

  //convert float to string
  String strtemp = "";     // empty temp string
  strtemp.concat(temp);
  String strtemp2 = strtemp + "°";

  //convert string to char array
  int temp_len = strtemp.length() + 1; //calculate string length
  char temperature[temp_len];              //creation of a char array
  strtemp2.toCharArray(temperature, temp_len);
  Display.setIntensity(0);
  Display.displayZoneText(zUP, temperature, PA_LEFT, SPEED_TIME, 0, PA_PRINT, PA_NO_EFFECT);        //Zona per Temp / Date / etc.
  Display.displayAnimate();
  //Serial.println(temperature);
  //Serial.println(temp);
}
/*
  void obtainHumidity() {
  float humi = dht.getHumidity();

  String strhumi = "";     // empty temp string
  strhumi.concat(humi);

  int humi_len = strhumi.length() + 1; //calculate string length
  char humidity[humi_len];              //creation of a char array
  strhumi.toCharArray(humidity, humi_len);

  Display.setIntensity(0);
  Display.displayZoneText(zUP, humidity, PA_LEFT, SPEED_TIME, 0, PA_PRINT, PA_NO_EFFECT);        //Zona per Temp / Date / etc.
  Display.displayAnimate();

  }
*/
void obtainTime() {
  Display.setIntensity(0);

  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }

  currentMillis = millis();

  if (currentMillis - previousMillis > timeinterval)  {
    previousMillis = millis();
    timeClient.setTimeOffset(UTC);  //Sostituire con UTC,CET,CEST
    Formatted_date = timeClient.getFormattedDate();
    //   Serial.println(Formatted_date);

    hour = Formatted_date.substring(11, 13);
    minute = Formatted_date.substring(14, 16);
    sec = Formatted_date.substring(17, 19);

    hmUTC = "UTC " + hour + ":" + minute; // + ":" + sec;  //Costruisco la stringa con i valori da visualizzare
    //Converte stringa in char array
    int hmUTC_len = hmUTC.length() + 1;
    char myhmUTC[hmUTC_len];
    hmUTC.toCharArray(myhmUTC, hmUTC_len);
    Display.displayZoneText(zCENTER, myhmUTC, PA_LEFT, SPEED_TIME, 0, PA_PRINT, PA_NO_EFFECT);        //Zona per -UTC


    timeClient.setTimeOffset(CEST);  //Sostituire con UTC,CET,CEST
    Formatted_date = timeClient.getFormattedDate();
    // Serial.println(Formatted_date);

    hour = Formatted_date.substring(11, 13);
    minute = Formatted_date.substring(14, 16);
    sec = Formatted_date.substring(17, 19);

    hmCET = "CET " + hour + ":" + minute + ":" + sec;   //Costruisco la stringa con i valori da visualizzare
    //Converte stringa in char array
    int hmCET_len = hmCET.length() + 1;
    char myhmCET[hmCET_len];
    hmCET.toCharArray(myhmCET, hmCET_len);
    Display.displayZoneText(zDOWN, myhmCET, PA_LEFT, SPEED_TIME, 0, PA_PRINT, PA_NO_EFFECT);        //Zona per -CET

    Display.displayAnimate();
  }
}
