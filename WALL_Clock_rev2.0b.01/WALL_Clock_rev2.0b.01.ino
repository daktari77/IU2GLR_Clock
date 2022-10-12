//---------------------------------------------------- Revisione
char rev[10] = "rev. 2.0b";
//----------------------------------------------------//

//----------------------------------------------------Dichiarazione librerie
#include <arduino-timer.h>    // Simple non-blocking timer library for calling functions in / at / every specified units of time. Supports millis, micros, time rollover, and compile time configurable number of tasks.
#include <NTPClient.h>        // Usare libreria NTPClient-master
#include <WiFi.h>             // WiFi
#include <WiFiUdp.h>          // NTPClient
#include <MD_Parola.h>        // MAX_72xx
#include <MD_MAX72xx.h>       // MAX_72xx
#include <SPI.h>              // BMP280, MAX_72xx
#include <DHTesp.h>           // DHT22 optimizzato per ESP32
#include <TimeLib.h>          //
#include <time.h>             //
#include <Timezone.h>         // Cambio automatico TimeZone https://github.com/JChristensen/Timezone
#include <AsyncMqttClient.h>  // MQTT
#include <Wire.h>
extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
}
//----------------------------------------------------//

//----------------------------------------------------Files accessori
#include "secret.h"  // user/passwords for wifi
//----------------------------------------------------//

//----------------------------------------------------MQTT


//---------------------------------------------------- Raspberry Pi Mosquitto MQTT Broker
#define MQTT_HOST IPAddress(17, 6, 77, 196)
#define MQTT_PORT 1883
//----------------------------------------------------//

//---------------------------------------------------- MQTT Topics
#define MQTT_PUB_TEMP "CASA/Studio/temperatura"
#define MQTT_PUB_HUM "CASA/Studio/umidità"
#define MQTT_PUB_HIDX "CASA/Studio/heatindex"
//----------------------------------------------------//

//---------------------------------------------------- Definizione pin DHT22
#define DHT_PIN 21
//----------------------------------------------------//

//---------------------------------------------------- Definizione conf Display
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW  //Definizione hardware
#define MAX_DEVICES 24                     //  Numero display
#define MAX_ZONES 3                        //  Numero zone
#define MAX_CLK_PIN 18                     //  Clock pin
#define MAX_DATA_PIN 23                    //  Data pin
#define MAX_CS_PIN 5                       //  CS pin
#define SPEED_TIME 0                       // MD_PAROLA
#define PAUSE_TIME 0                       // MD_PAROLA
//----------------------------------------------------//


//----------------------------------------------------Debug
//#define DEBUG  //  togliere il commento per attivare debug su seriale
//#define DEBUGtime  //  togliere il commento per attivare debug su seriale
//#define DEBUGdate  //  togliere il commento per attivare debug su seriale
//----------------------------------------------------//

//----------------------------------------------------
//auto timer = timer_create_default();
Timer<4> timer;
//----------------------------------------------------//

DHTesp dht;                                                                                  // Sensore DHT22
MD_Parola P = MD_Parola(HARDWARE_TYPE, MAX_DATA_PIN, MAX_CLK_PIN, MAX_CS_PIN, MAX_DEVICES);  // Alias per prefisso libreria MD_Parola
WiFiUDP ntpUDP;

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

String hmsCET, hmsUTC, sec, dayStamp;

//---------------------------------------------------- Variabili per DHT
float humidity;
float temperature;
float heatindex;
//----------------------------------------------------//

//---------------------------------------------------- Dichiarazione costanti di tempo
int STARTinterval = 100;
int INFOinterval = 3000;    //  DATE refresh interval  1h
int DATEinterval = 6000;    //  DATE refresh interval  1h
int TIMEinterval = 1000;    //  refresh interval       1s
int WIFiinterval = 10000;   //  WiFi refresh interval  10s
int MQTTinterval = 300000;  //  MQTT refresh interval  5min
//int DSPLinterval = 3600000;  // DIPLAY reset interval   1h
//----------------------------------------------------//

//---------------------------------------------------- Simboli personalizzati display MD_PAROLA
//uint8_t hum[] = { 5, 99, 19, 8, 100, 99 };  //  % symbol for humidity
//uint8_t deg[] = { 3, 2, 5, 2 };             //  ° symbol for degree
//----------------------------------------------------//

//----------------------------------------------------Calcolo TimeZone
/**
   Input time in epoch format and return tm time format
   by Renzo Mischianti <www.mischianti.org>
*/
static tm getDateTimeByParams(long time) {
  struct tm *newtime;
  const time_t tim = time;
  newtime = localtime(&tim);
  return *newtime;
}
/**
   Input tm time format and return String with format pattern
   by Renzo Mischianti <www.mischianti.org>
*/
static String getDateTimeStringByParams(tm *newtime, char *pattern = (char *)"%d/%m/%Y %H:%M:%S") {
  char buffer[30];
  strftime(buffer, 30, pattern, newtime);
  return buffer;
}

/**
   Input time in epoch format format and return String with format pattern
   by Renzo Mischianti <www.mischianti.org>
*/
static String getEpochStringByParams(long time, char *pattern = (char *)"%d/%m/%Y %H:%M:%S") {
  //    struct tm *newtime;
  tm newtime;
  newtime = getDateTimeByParams(time);
  return getDateTimeStringByParams(&newtime, pattern);
}
//----------------------------------------------------//

int GTMOffset = 0;  // SET TO UTC TIME
NTPClient timeClient(ntpUDP, "europe.pool.ntp.org", GTMOffset * 60 * 60, 60 * 60 * 1000);
//NTPClient timeClient(ntpUDP, "17.6.77.254", GTMOffset * 60 * 60, 60 * 60 * 1000);

//----------------------------------------------------Central European Time (Frankfurt, Paris)
TimeChangeRule CEST = { "CEST", Last, Sun, Mar, 2, 120 };  // Central European Summer Time
TimeChangeRule CET = { "CET ", Last, Sun, Oct, 3, 60 };    // Central European Standard Time
Timezone CE(CEST, CET);
//----------------------------------------------------//

//----------------------------------------------------display zone const
const int zDN = 0;
const int zCN = 1;
const int zUP = 2;
//----------------------------------------------------//

void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event) {
  Serial.printf("[WiFi-event] event: %d\n", event);
  switch (event) {
    case SYSTEM_EVENT_STA_GOT_IP:
      Serial.println("WiFi connected");
      Serial.println("IP address: ");
      Serial.println(WiFi.localIP());
      connectToMqtt();
      break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
      Serial.println("WiFi lost connection");
      xTimerStop(mqttReconnectTimer, 0);  // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
      xTimerStart(wifiReconnectTimer, 0);
      break;
  }
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");
  if (WiFi.isConnected()) {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

/*void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}
void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}*/

void onMqttPublish(uint16_t packetId) {
  Serial.print("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void setup() {
  delay(50);
  Serial.begin(115200);
  delay(50);
  dht.setup(DHT_PIN, DHTesp::DHT22);  //Inizializzazione DHT (PIN, Modello)

  //----------------------------------------------------
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

  WiFi.onEvent(WiFiEvent);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  //mqttClient.onSubscribe(onMqttSubscribe);
  //mqttClient.onUnsubscribe(onMqttUnsubscribe);
  mqttClient.onPublish(onMqttPublish);
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  // If your broker requires authentication (username and password), set them below
  //mqttClient.setCredentials("REPlACE_WITH_YOUR_USER", "REPLACE_WITH_YOUR_PASSWORD");
  //----------------------------------------------------//
  connectToWifi();

  //----------------------------------------------------Inizializzazione display
  P.begin(MAX_ZONES);  //Inizializzo la libreria passando il numero di zone definito ad inizio codice
  //----------------------------------------------------//

  //----------------------------------------------------definizione delle zone
  P.setZone(zDN, 0, 7);    //zona 0 da 0 a 7
  P.setZone(zCN, 8, 15);   //zone 2 da 8 a 15
  P.setZone(zUP, 16, 23);  //zona 3 da 16 a 23
  //----------------------------------------------------//

  //----------------------------------------------------
  P.displayReset();
  P.setIntensity(0);
  P.setInvert(false);  // Inverte il display (true/false)
  delay(50);
  //----------------------------------------------------//

  //----------------------------------------------------banner accensione
  P.displayClear();
  P.displayZoneText(zUP, "IU2GLR", PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
  P.displayZoneText(zCN, "Wall CLOCK", PA_LEFT, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
  P.displayZoneText(zDN, rev, PA_RIGHT, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
  P.displayAnimate();
  //----------------------------------------------------//

  delay(2000);

#ifdef DEBUG
  Serial.println("WiFi connected!");
#endif

  P.displayZoneText(zUP, "Connected", PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
  P.displayZoneText(zCN, "to:", PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
  P.displayZoneText(zDN, WIFI_SSID, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
  P.displayClear();
  P.displayAnimate();


  //----------------------------------------------------Inizializzazione Time
  timeClient.begin();
  delay(1000);
  if (timeClient.update()) {
    Serial.print("Adjust local clock");
    unsigned long epoch = timeClient.getEpochTime();
    setTime(epoch);
  } else {
    Serial.print("NTP Update not WORK!!");
  }
  //----------------------------------------------------//
  delay(2000);





  //---------------------------------------------------- Dichiarazione timers
  timer.every(TIMEinterval, showTime);     // Visualizzazione orario UTC e CET
  timer.in(STARTinterval, showInfo);       // Intervallo inizio visualizzazione Info
  timer.every(MQTTinterval, publishMQTT);  // publicazione su MQTT ogni 5 min
  //timer.every(DSPLinterval, resetDisplay);  // reset display ongi ora
  //----------------------------------------------------//
}

void loop() {

  timer.tick();  // tick the timer

  delay(100);
}

/*
bool resetDisplay(void *) {
  P.displayReset();
  return true;  // keep timer active? true
}
*/


bool publishMQTT(void *) {
  //----------------------------------------------------Publicazione su MQTT

  //-------------------------------------------------- New DHT sensor readings
  humidity = dht.getHumidity();
  temperature = dht.getTemperature();
  heatindex = dht.computeHeatIndex(temperature, humidity, false);
  // Read temperature as Fahrenheit (isFahrenheit = true)
  //temp = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).

  // Publish an MQTT message on topic CASA/Studio/temperature
  uint16_t packetIdPub21 = mqttClient.publish(MQTT_PUB_TEMP, 1, true, String(temperature).c_str());
  Serial.printf("Publishing on topic %s at QoS 1, packetId: %i", MQTT_PUB_TEMP, packetIdPub21);
  Serial.printf("Message: %.2f \n", temperature);

  // Publish an MQTT message on topic CASA/Studio/humidity
  uint16_t packetIdPub22 = mqttClient.publish(MQTT_PUB_HUM, 1, true, String(humidity).c_str());
  Serial.printf("Publishing on topic %s at QoS 1, packetId %i: ", MQTT_PUB_HUM, packetIdPub22);
  Serial.printf("Message: %.2f \n", humidity);

  // Publish an MQTT message on topic CASA/Studio/heatindex
  uint16_t packetIdPub23 = mqttClient.publish(MQTT_PUB_HIDX, 1, true, String(heatindex).c_str());
  Serial.printf("Publishing on topic %s at QoS 1, packetId %i: ", MQTT_PUB_HIDX, packetIdPub23);
  Serial.printf("Message: %.2f \n", heatindex);
  //---------------------------------------------------//
  return true;  // keep timer active? true
}

bool showTime(void *) {

  while (!timeClient.update()) {
    timeClient.forceUpdate();
  }

  //----------------------------------------------------Orario UTC
  String strtUTC = (getEpochStringByParams(now()));
  hmsUTC = "UTC " + strtUTC.substring(11, 19);
  String strCET = (getEpochStringByParams(CE.toLocal(now())));
  hmsCET = "CET " + strCET.substring(11, 19);

#ifdef DEBUGtime
  Serial.print("UTC: ");
  Serial.println(hmsUTC);
  Serial.print("CET: ");
  Serial.println(hmsCET);
#endif

  //Converte stringa in char array
  int hmsUTC_len = hmsUTC.length() + 1;
  char myhmsUTC[hmsUTC_len];
  hmsUTC.toCharArray(myhmsUTC, hmsUTC_len);
  P.displayZoneText(zCN, myhmsUTC, PA_LEFT, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);  //Zona per -UTC
  //----------------------------------------------------//

  //----------------------------------------------------Orario CET/CEST

  //Converte stringa in char array
  int hmsCET_len = hmsCET.length() + 1;
  char myhmsCET[hmsCET_len];
  hmsCET.toCharArray(myhmsCET, hmsCET_len);
  P.displayZoneText(zDN, myhmsCET, PA_LEFT, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);  //Zona per -CET
  P.displayClear(zDN);
  P.displayClear(zCN);
  P.displayAnimate();
  return true;  // keep timer active? true
}

bool showDate(void *) {
  String strtDATA = (getEpochStringByParams(CE.toLocal(now())));
  //Serial.println (text);
  dayStamp = strtDATA.substring(0, 10);

#ifdef DEBUGdate
  Serial.println("Data: " + dayStamp);
  Serial.println(getEpochStringByParams(CE.toLocal(now())));
  //Serial.println(CE);
#endif
  int dayStamp_len = dayStamp.length() + 1;
  char mydayStamp[dayStamp_len];
  dayStamp.toCharArray(mydayStamp, dayStamp_len);
  //P.displayClear(zUP);
  P.displayZoneText(zUP, mydayStamp, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);  //Zona per -CET
  P.displayAnimate();
  timer.in(INFOinterval, showInfo);
  return true;  // keep timer active? true
}

bool showInfo(void *) {
  humidity = dht.getHumidity();                                    // Rilevazione umidità
  temperature = dht.getTemperature();                              // Rilevazione temperatura
  heatindex = dht.computeHeatIndex(temperature, humidity, false);  // Calcolo temperatura percepita

#ifdef DEBUG
  Serial.print("Umidita: ");
  Serial.print(dht.getHumidity());
  Serial.println("%");
  Serial.print("Temeratura: ");
  Serial.print(dht.getTemperature());
  Serial.println("°C");
#endif

  //----------------------------------------------------Conversione
  char info[14];
  char chartemperature[6];
  char charhumidity[6];
  dtostrf(humidity, 3, 1, charhumidity);        //prendo 3 caratteri di cui uno decimale
  dtostrf(temperature, 3, 1, chartemperature);  //prendo 3 caratteri di cui uno decimale
  strcat(charhumidity, "%");
  strcat(chartemperature, "c");
  strcpy(info, chartemperature);
  strcat(info, " / ");
  strcat(info, charhumidity);

  //P.displayClear(zUP);
  P.displayZoneText(zUP, info, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);  //Zona per Temp / Date / etc.
  P.displayAnimate();
  timer.in(DATEinterval, showDate);
  return true;
}