#ifndef WATCHY_H
#define WATCHY_H

#include <Arduino.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Arduino_JSON.h>
#include <GxEPD2_BW.h>
#include <Wire.h>
#include "Display.h"
#include "BLE.h"
#include "bma.h"
#include "config.h"
#include "esp_chip_info.h"
#include "MoonPhase.h"
#include "TimezonesGMT.h"
#ifdef ARDUINO_ESP32S3_DEV
  #include "Watchy32KRTC.h"
  #include "soc/rtc.h"
  #include "soc/rtc_io_reg.h"
  #include "soc/sens_reg.h"
  #include "esp_sleep.h"
  #include "rom/rtc.h"
  #include "soc/soc.h"
  #include "soc/rtc_cntl_reg.h"
  #include "time.h"
  #include "esp_sntp.h"
  #include "hal/rtc_io_types.h"
  #include "driver/rtc_io.h"
  #define uS_TO_S_FACTOR 1000000ULL  //Conversion factor for micro seconds to seconds
  #define ADC_VOLTAGE_DIVIDER ((360.0f+100.0f)/360.0f) //Voltage divider at battery ADC  
#else
  #include "WatchyRTC.h"
#endif

typedef struct weatherData {
  int8_t temperature;
  int16_t weatherConditionCode;
  bool isMetric;
  String weatherDescription;
  bool external;
  tmElements_t sunrise;
  tmElements_t sunset;
} weatherData;

typedef struct watchySettings {
  // Weather Settings
  String cityID;
  String lat;
  String lon;
  String weatherAPIKey;
  String weatherURL;
  String weatherUnit;
  String weatherLang;
  int8_t weatherUpdateInterval;
  // NTP Settings
  String ntpServer;
  long gmtOffset;
  //
  bool vibrateOClock;
} watchySettings;

//Headers for functions in main.ino

void drawWatchFace();
void drawTime();
void drawDate();
void drawSteps();
void drawWeather();
void drawBattery();
float getBatteryVoltage();

weatherData _getWeatherData(String cityID, String lat, String lon, String units, String lang,
                                   String url, String apiKey,
                                   uint8_t updateInterval);
weatherData getWeatherData();
void setAlarm();
void showMenu();
void showWatchFace();
void setupWifi();
bool syncNTP(long gmt, String ntpServer);
void showSyncNTP();
void showMoonPhase();
void showWeatherData();
void vibMotor(uint8_t intervalMs, uint8_t length);
void drawWeather();
void drawBattery(int x, int y);
void drawWeather(int x, int y);
void showDateTime();
void handleButtonPress(int buttonPin);
void soundAlarm();
uint16_t _readRegister(uint8_t address, uint8_t reg, uint8_t *data,
                               uint16_t len);
uint16_t _writeRegister(uint8_t address, uint8_t reg, uint8_t *data,
                                uint16_t len);
#endif
