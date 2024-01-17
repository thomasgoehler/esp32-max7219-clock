#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <time.h>
#include <MD_Parola.h>
#include <SPI.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <EEPROM.h>

#include "Font_Data.h"
#include "config.h"

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CLK_PIN   18
#define DATA_PIN  19
#define CS_PIN    5

// for saving city and countrycode
#define CITY_ADDRESS 0
#define COUNTRY_CODE_ADDRESS 20

MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

#define SPEED_TIME  75
#define PAUSE_TIME  0
#define MAX_MESG  20

// Telegram settings
const unsigned long BOT_MTBS = 1000;
String botToken = BOT_TOKEN;
WiFiClientSecure secured_client;
UniversalTelegramBot bot(botToken, secured_client);
unsigned long bot_lasttime;
unsigned long lastBotUpdate = 0;
const unsigned long botUpdateInterval = BOT_MTBS;
char telegramText[256] = {'\0'};

uint8_t scrollSpeed = 10;
textEffect_t scrollEffect = PA_SCROLL_LEFT;
textPosition_t scrollAlign = PA_LEFT;
uint16_t scrollPause = 10;

const int timezoneInSeconds = TZ_DATA;
String openWeatherMapApiKey = OPEN_WEATHER_API_KEY;
String city;
String countryCode;
uint8_t degC[] = { 6, 3, 3, 56, 68, 68, 68 }; // Deg C

unsigned long weatherLastTime = 0;
unsigned long weatherTimerDelay = 10;
String jsonBuffer;

int dst = 0;
int lastShown = 0;
uint16_t h, m, s;
char szTime[9];    // mm:ss\0
char szSecond[4];  // ss
char szMesg[MAX_MESG + 1] = "";

void getSec(char *psz) {
  sprintf(psz, "%02d", s);
}

void getTime(char *psz, bool f = true) {
  time_t now = time(nullptr);
  struct tm *p_tm = localtime(&now);
  h = p_tm->tm_hour;
  m = p_tm->tm_min;
  s = p_tm->tm_sec;
  sprintf(psz, "%02d%c%02d", h, (f ? ':' : ' '), m);
  Serial.println(psz);
}

void showText(char *text) {
  int counter = 0;
  P.begin();
  P.setIntensity(0);
  P.displayClear();
  P.displayText(text, scrollAlign, scrollSpeed, scrollPause, scrollEffect, scrollEffect);

  while (counter != 1) {
    int speed;
    if (speed != P.getSpeed())
      P.setSpeed(60);

    if (P.displayAnimate()) {
      P.displayReset();
      counter++;
    }
  }
}

void showDate() {
  int counter = 0;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

  char szDate[11];
  sprintf(szDate, "%02d.%02d.%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);

  P.begin();
  P.setIntensity(0);
  P.displayClear();
  P.displayText(szDate, scrollAlign, scrollSpeed, scrollPause, scrollEffect, scrollEffect);

  while (counter != 1) {
    int speed;
    if (speed != P.getSpeed())
      P.setSpeed(60);

    if (P.displayAnimate()) {
      P.displayReset();
      counter++;
    }
  }
}

void showTelegramText() {
  int counter = 0;
  P.begin();
  P.addChar('$', degC);
  P.setIntensity(0);
  P.displayClear();
  P.displayText(telegramText, scrollAlign, scrollSpeed, scrollPause, scrollEffect, scrollEffect);

  while (counter != 1) {
    int speed;
    if (speed != P.getSpeed())
      P.setSpeed(60);

    if (P.displayAnimate()) {
      P.displayReset();
      counter++;
    }
  }
  for (int i = 0; i < sizeof(telegramText); ++i) {
    telegramText[i] = '\0';
}
char telegramText[256] = {'\0'};
}

void updateCity(String newCity) {
  city = newCity;
  saveStringToEEPROM(CITY_ADDRESS, city);
}

void updateCountryCode(String newCountryCode) {
  countryCode = newCountryCode;
  saveStringToEEPROM(COUNTRY_CODE_ADDRESS, countryCode);
}

void showWeather() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi Disconnected");
    return;
  }

  if ((millis() - weatherLastTime) > weatherTimerDelay) {
    String serverPath = "http://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode +
                        "&units=metric" + "&lang=de" + "&APPID=" + openWeatherMapApiKey;

    jsonBuffer = httpGETRequest(serverPath.c_str());

    JSONVar myObject = JSON.parse(jsonBuffer);
    if (JSON.typeof(myObject) == "undefined") {
      Serial.println("Parsing input failed!");
      return;
    }

    char temperatureStr[10], pressureStr[10], humidityStr[10], windSpeedStr[10], descriptionStr[50];
    dtostrf(myObject["main"]["temp"], 4, 2, temperatureStr);
    dtostrf(myObject["main"]["pressure"], 4, 2, pressureStr);
    dtostrf(myObject["main"]["humidity"], 4, 2, humidityStr);
    dtostrf(myObject["wind"]["speed"], 4, 2, windSpeedStr);
    strcpy(descriptionStr, myObject["weather"][0]["description"]);

    // replace german umlauts in description
    String cleanedDescription = replaceUmlaute(descriptionStr);

    Serial.print("City, Country: ");
    Serial.print(city);
    Serial.print(", ");
    Serial.println(countryCode);
    Serial.print("Temperature: ");
    Serial.println(temperatureStr);
    Serial.print("Pressure: ");
    Serial.println(pressureStr);
    Serial.print("Humidity: ");
    Serial.println(humidityStr);
    Serial.print("Wind Speed: ");
    Serial.println(windSpeedStr);
    Serial.print("Description: ");
    Serial.println(cleanedDescription);

    char alles[200];
    sprintf(alles, "Das aktuelle Wetter fuer %s, %s: %s, Temperatur: %s \xB0""C, Luftdruck: %s hPa, Luftfeuchtigkeit: %s %%, Wind: %s m/s", city, countryCode, cleanedDescription.c_str(), temperatureStr, pressureStr, humidityStr, windSpeedStr);

showText(alles);
  }

  weatherLastTime = millis();
}


void updateTelegram(void *pvParameters) {
  while (true) {
    if (millis() - lastBotUpdate > botUpdateInterval) {
      int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

      while (numNewMessages) {
        Serial.println("got response");
        handleNewMessages(numNewMessages);
        numNewMessages = bot.getUpdates(bot.last_message_received + 1);
      }

      lastBotUpdate = millis();
    }
    delay(1000);
  }
}

void saveStringToEEPROM(int address, const String& data) {
  int length = data.length() + 1; // inklusive Nullzeichen
  for (int i = 0; i < length; ++i) {
    EEPROM.write(address + i, data[i]);
  }
  EEPROM.commit();
}

String loadStringFromEEPROM(int address) {
  String result;
  char c;
  while ((c = EEPROM.read(address++)) != 0) {
    result += c;
  }
  return result;
}


void setup(void) {
  WiFi.mode(WIFI_STA);
  Serial.begin(115200);

  showText("verbinde mit WLAN ...");

  WiFiManager wm;
  bool res = wm.autoConnect("Matrix Clock AP");

  if (!res) {
    Serial.println("Failed to connect");
    showText("WLAN konnte nicht verbunden werden!");
  } else {
    Serial.println("connected...");
    showText("WLAN verbunden...");
  }

  delay(3000);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);
  showText("synchronisiere mit NTP Server");
  getTimentp();

  // Überprüfe, ob Werte im EEPROM vorhanden sind
  String storedCity = loadStringFromEEPROM(CITY_ADDRESS);
  String storedCountryCode = loadStringFromEEPROM(COUNTRY_CODE_ADDRESS);

  // Verwende Standardwerte, wenn im EEPROM keine Werte vorhanden sind
  city = (storedCity.length() > 0) ? storedCity : "Dresden";
  countryCode = (storedCountryCode.length() > 0) ? storedCountryCode : "DE";

  P.begin(3);
  P.setInvert(false);
  P.setZone(0, 0, 0);
  P.setZone(1, 1, 3);
  P.setFont(0, numeric7Seg);
  P.setFont(1, numeric7Se);
  P.displayZoneText(0, szSecond, PA_LEFT, SPEED_TIME, 0, PA_PRINT, PA_NO_EFFECT);
  //P.displayZoneText(0, szSecond, PA_LEFT, SPEED_TIME, 0, PA_GROW_DOWN, PA_GROW_UP); //PA_GROW_DOWN
  P.displayZoneText(1, szTime, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT); 
  P.setIntensity(0);

  getTime(szTime);

  // Start the Telegram update background thread
  xTaskCreatePinnedToCore(
    updateTelegram,  // Thread function
    "TelegramUpdate", // Thread name
    10000,            // Thread stacksize
    NULL,             // n/a
    1,                // Thread priority
    NULL,             // n/a
    0                 // Core on which the task is to be run (0 or 1)
  );
}

void loop(void) {

  static uint32_t lastTime = 0;
  static uint32_t lastSyncTime = 0;
  static uint32_t lastShowDateOrWeather = 0;
  static uint8_t display = 0;
  static bool flasher = false;

  P.displayAnimate();

  if (millis() - lastTime >= 1000) {
    lastTime = millis();
    getTime(szTime, flasher);
    flasher = !flasher;
    getSec(szSecond);
    P.displayReset(0);
    P.displayReset(1);
  }

  if (millis() - lastSyncTime >= 3600000) {
    lastSyncTime = millis();
    checkAndSyncTime();
  }

  if (millis() - lastShowDateOrWeather >= 60000) {
    lastShowDateOrWeather = millis();
    
    if (lastShown == 0)
    {
      showDate();
      lastShown = 1;
      lastShowDateOrWeather = millis(); // set 60 seconds timer to zero
    }
    else if (lastShown == 1)
    {
      showWeather();
      lastShown = 0;
      lastShowDateOrWeather = millis(); // set 60 seconds timer to zero
    }
      
    P.begin(3);
    P.setInvert(false);
    P.setZone(0, 0, 0);
    P.setZone(1, 1, 3);
    P.setFont(0, numeric7Seg);
    P.setFont(1, numeric7Se);
    P.displayZoneText(0, szSecond, PA_LEFT, SPEED_TIME, 0, PA_PRINT, PA_NO_EFFECT);
    P.displayZoneText(1, szTime, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
    P.setIntensity(0);
  }
  else
  {
    if (telegramText[0] != '\0')
    {
      showTelegramText();
      P.begin(3);
      P.setInvert(false);
      P.setZone(0, 0, 0);
      P.setZone(1, 1, 3);
      P.setFont(0, numeric7Seg);
      P.setFont(1, numeric7Se);
      P.displayZoneText(0, szSecond, PA_LEFT, SPEED_TIME, 0, PA_PRINT, PA_NO_EFFECT);
      P.displayZoneText(1, szTime, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
      P.setIntensity(0);
      lastShowDateOrWeather = millis(); // after showing telegram message set 60 seconds timer to zero
    }
  }
}

void checkAndSyncTime() {
  Serial.println("Checking and syncing time...");
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

  dst = timeinfo.tm_isdst;

  Serial.print("Time Update, DST: ");
  Serial.println(dst);
}

void getTimentp() {
  configTime(timezoneInSeconds, dst, "de.pool.ntp.org", "time.nist.gov");
  while (!time(nullptr)) {
    delay(500);
    Serial.print(".");
  }

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

  dst = timeinfo.tm_isdst;

  Serial.print("Time Update, DST: ");
  Serial.println(dst);
}

String httpGETRequest(const char *serverName) {
  WiFiClient client;
  HTTPClient http;

  http.begin(client, serverName);
  int httpResponseCode = http.GET();
  String payload = "{}";

  if (httpResponseCode > 0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
    payload = http.getString();
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }

  http.end();
  return payload;
}

String replaceUmlaute(String input) {
  input.replace("Ä", "Ae");
  input.replace("ä", "ae");
  input.replace("Ö", "Oe");
  input.replace("ö", "oe");
  input.replace("Ü", "Ue");
  input.replace("ü", "ue");
  input.replace("ß", "ss");
  return input;
}

void handleNewMessages(int numNewMessages) {
  String messageContent;
  for (int i = 0; i < numNewMessages; i++) {
    bot.sendMessage(bot.messages[i].chat_id, "Nachricht empfangen", "");

    // Befehl zum Aktualisieren der Stadt
    if (bot.messages[i].text.startsWith("/setcity")) {
      String newCity = bot.messages[i].text.substring(8); // Annahme: /setcity Stadtname
      updateCity(newCity);
      bot.sendMessage(bot.messages[i].chat_id, "Stadt aktualisiert", "");
    }

    // Befehl zum Aktualisieren des Landes
    else if (bot.messages[i].text.startsWith("/setcountry")) {
      String newCountryCode = bot.messages[i].text.substring(11); // Annahme: /setcountry Ländercode
      updateCountryCode(newCountryCode);
      bot.sendMessage(bot.messages[i].chat_id, "Land aktualisiert", "");
    }

    // Weitere Befehle können hier hinzugefügt werden.

    messageContent += bot.messages[i].text;
  }
  strcpy(telegramText, messageContent.c_str());
}
