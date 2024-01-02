// Header file includes
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <time.h>
#include <MD_Parola.h>
#include <SPI.h>

#include "Font_Data.h"

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4

#define CLK_PIN   18 // or SCK
#define DATA_PIN  19 // or MOSI
#define CS_PIN    5 // or SS

// Arbitrary output pins
MD_Parola P = MD_Parola(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

#define SPEED_TIME  75
#define PAUSE_TIME  0
#define MAX_MESG  20

/**********  User Config Setting   ******************************/
const int timezoneinSeconds = 3600;
/***************************************************************/
int dst = 0;
uint16_t h, m, s;
uint8_t dow;
int day;
uint8_t month;
String year;
// Global variables
char szTime[9];    // mm:ss\0
char szsecond[4];    // ss
char szMesg[MAX_MESG+1] = "";

void getsec(char *psz) {
  sprintf(psz, "%02d", s);
}

void getTime(char *psz, bool f = true) {
  time_t now = time(nullptr);
  struct tm* p_tm = localtime(&now);
  h = p_tm->tm_hour;
  m = p_tm->tm_min;
  s = p_tm->tm_sec;
  sprintf(psz, "%02d%c%02d", h, (f ? ':' : ' '), m);
  Serial.println(psz);
}

void setup(void) {

  // WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    // it is a good practice to make sure your code sets wifi mode how you want it.

    // put your setup code here, to run once:
    Serial.begin(115200);
    
    //WiFiManager, Local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wm;

    // reset settings - wipe stored credentials for testing
    // these are stored by the esp library
    // wm.resetSettings();

    // Automatically connect using saved credentials,
    // if connection fails, it starts an access point with the specified name ( "AutoConnectAP"),
    // if empty will auto generate SSID, if password is blank it will be anonymous AP (wm.autoConnect())
    // then goes into a blocking loop awaiting configuration and will return success result

    bool res;
    // res = wm.autoConnect(); // auto generated AP name from chipid
    res = wm.autoConnect("Matrix Clock AP"); // anonymous ap
    // res = wm.autoConnect("AutoConnectAP","password"); // password protected ap

    if(!res) {
        Serial.println("Failed to connect");
        // ESP.restart();
    } 
    else {
        //if you get here you have connected to the WiFi    
        Serial.println("connected...yeey :)");
    } 


  delay(3000);
  getTimentp();

  P.begin(3);
  P.setInvert(false);

  P.setZone(0, 0, 0);
  P.setZone(1, 1, 3);
  P.setFont(0, numeric7Seg);
  P.setFont(1, numeric7Se);
  P.displayZoneText(0, szsecond, PA_LEFT, SPEED_TIME, 0, PA_PRINT, PA_NO_EFFECT);
  P.displayZoneText(1, szTime, PA_CENTER, SPEED_TIME, PAUSE_TIME, PA_PRINT, PA_NO_EFFECT);
  P.setIntensity(0);

  getTime(szTime);
}

void loop(void) {
  static uint32_t lastTime = 0; // millis() memory
  static uint32_t lastSyncTime = 0; // time of last synchronization
  static uint8_t display = 0;  // current display mode
  static bool flasher = false;  // seconds passing flasher

  P.displayAnimate();

  if (millis() - lastTime >= 1000) {
    lastTime = millis();
    getTime(szTime, flasher);
    flasher = !flasher;

    // Now getsec is called after getTime
    getsec(szsecond);

    P.displayReset(0);
    P.displayReset(1);
  }

  // Check the time once per hour and resynchronize
  if (millis() - lastSyncTime >= 3600000)  // 3600000 milliseconds = 1 hour
  {
    lastSyncTime = millis();
    checkAndSyncTime();
  }
}

void checkAndSyncTime() {
  Serial.println("Checking and syncing time...");
  // Check if Daylight Saving Time is active
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
  configTime(timezoneinSeconds, dst, "de.pool.ntp.org", "time.nist.gov");
  while (!time(nullptr)) {
    delay(500);
    Serial.print(".");
  }

  // Check if Daylight Saving Time is active
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }

  dst = timeinfo.tm_isdst;

  Serial.print("Time Update, DST: ");
  Serial.println(dst);
}
