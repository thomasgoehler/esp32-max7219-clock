# esp32-max7219-clock
Another fancy clock displayed on a 32x8 dot LED display with automatic DLS and timesync via ntp.

### Configuration
Please create a config.h file in the same path where esp32-max7219-clock.ino is located.
```
/**********  User Config Setting   ******************************/
#define OPEN_WEATHER_API_KEY "PASTE YOUR KEY HERE"
#define CITY "YOUR CITY"
#define COUNTRYCODE "ISO ALPHA COUNTRY CODE OF YOUR LOCATION"
#define BOT_TOKEN "PASTE YOUR TELEGRAM BOT TOKEN HERE"
#define TZ_DATA 3600 // please paste your time // Please enter the deviation of your time zone from GMT in seconds without considering DST. Example: GMT+1 is 3600
/***************************************************************
```
