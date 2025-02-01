#include "Arduino.h"
#include "GxEPD2_BW.h"
#include "Display.h"
#include "BadApple.h"
#include "config.h"
#include "settings.h"
#include "Watchy.h"
#include "Fonts/FreeMonoBold9pt7b.h"
#include "Seven_Segment10pt7b.h"
#include "DSEG7_Classic_Regular_15.h"
#include "DSEG7_Classic_Bold_25.h"
#include "DSEG7_Classic_Regular_39.h"
#include "DSEG7_Classic_Bold_53.h"
#include "icons.h"


bool vibrateOClock = true;

GxEPD2_BW<WatchyDisplay, 200> display = GxEPD2_BW<WatchyDisplay, 200>(WatchyDisplay());
WiFiManager wifiManager;
BMA423 sensor;  
MoonPhase mp;
#ifdef ARDUINO_ESP32S3_DEV
  Watchy32KRTC RTC;
  #define ACTIVE_LOW 0
  #pragma "ACTIVE_LOW is 0"
#else
  WatchyRTC RTC;
  #define ACTIVE_LOW 1
#endif

int frameCounter = 0;
uint16_t frame = 0;


bool WIFI_CONFIGURED;
bool BLE_CONFIGURED;
bool USB_PLUGGED_IN = false;

bool DARKMODE = true;

IPAddress lastIPAddress;
char lastSSID[30];

int weatherIntervalCounter = -1;
weatherData currentWeather;

tmElements_t currentTime;
tmElements_t alarmTime;
tmElements_t bootTime;
time_t currentMillis = 0;
bool alarmCancelled = false;
char dateTime[30];

const uint8_t BATTERY_SEGMENT_WIDTH = 7;
const uint8_t BATTERY_SEGMENT_HEIGHT = 11;
const uint8_t BATTERY_SEGMENT_SPACING = 9;
const uint8_t WEATHER_ICON_WIDTH = 48;
const uint8_t WEATHER_ICON_HEIGHT = 32;

#define DEBOUNCE_DELAY 150  // 150 ms debounce time
#define MAX_HOLD_TIME 500 // Máximo tiempo para mantener el botón presionado

unsigned long lastButtonPressTime = 0;  // Última vez que se presionó el botón
unsigned long buttonHoldStartTime = 0;  // Tiempo en que comenzó a mantenerse el botón
bool buttonHeld = false;        // Estado para detectar si el botón está mantenido
unsigned int buttonPressed = -1;
unsigned long elapsedMillis = 0;    // Control para manejo de tiempo transcurrido

void drawTime(){
  display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  display.setFont(&DSEG7_Classic_Bold_53);
  display.setCursor(5, 53+5);
  int displayHour;
  if(HOUR_12_24==12){
    displayHour = ((currentTime.Hour)%12);
  } else {
    displayHour = currentTime.Hour;
  }
  if(displayHour < 10){
    display.print("0");
  }
  display.print(displayHour);

  bool blink = 1 - (currentTime.Second % 2); // 1 -> visible, 0 -> invisible
  if(blink){
    display.print(":");
  } else {
    display.print(" ");
  }
  
  if(currentTime.Minute < 10){
    display.print("0");
  }
  display.print(currentTime.Minute);
}

void drawDate(){
  display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  display.setFont(&Seven_Segment10pt7b);
  int16_t  x1, y1;
  uint16_t w, h;

  String dayOfWeek = dayStr(currentTime.Wday);
  display.getTextBounds(dayOfWeek, 5, 85, &x1, &y1, &w, &h);
  if(currentTime.Wday == 4){
    w = w - 5;
  }
  display.setCursor(85 - w, 85);
  display.println(dayOfWeek);

  String month = monthShortStr(currentTime.Month);
  display.getTextBounds(month, 60, 110, &x1, &y1, &w, &h);
  display.setCursor(85 - w, 110);
  display.println(month);
  display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  display.setFont(&DSEG7_Classic_Bold_25);
  display.setCursor(5, 120);
  if(currentTime.Day < 10){
    display.print("0");
  }
  display.println(currentTime.Day);
  display.setCursor(5, 150);
  display.println(tmYearToCalendar(currentTime.Year));// offset from 1970, since year is stored in uint8_t
}
void drawSteps(){
  // reset step counter at midnight
  if (currentTime.Hour == 0 && currentTime.Minute == 0){
    sensor.resetStepCounter();
  }
  uint32_t stepCount = sensor.getCounter();
  display.drawBitmap(10, 165, steps, 19, 23, DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  display.setCursor(35, 190);
  display.println(stepCount);
}
void drawBattery(){
  display.drawBitmap(158, 73, battery, 37, 21, DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  display.fillRect(163, 78, 27, BATTERY_SEGMENT_HEIGHT, DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);//clear battery segments
  int8_t batteryLevel = 0;
  float VBAT = getBatteryVoltage();
    #ifndef ARDUINO_ESP32S3_DEV
  if(VBAT > 4.1){
    batteryLevel = 3;
  }
  else if(VBAT > 3.95 && VBAT <= 4.1){
    batteryLevel = 2;
  }
  else if(VBAT > 3.80 && VBAT <= 3.95){
    batteryLevel = 1;
  }
  else if(VBAT <= 3.80){
    batteryLevel = 0;
  }
  #else
   if (VBAT > 3.90) {
    batteryLevel = 3;
  }
  else if (VBAT > 3.75 && VBAT <= 3.90) {
    batteryLevel = 2;
  }
  else if (VBAT > 3.60 && VBAT <= 3.75) {
    batteryLevel = 1;
  }
  else if (VBAT <= 3.60) {
    batteryLevel = 0;
  }
  #endif

  for(int8_t batterySegments = 0; batterySegments < batteryLevel; batterySegments++){
    display.fillRect(163 + (batterySegments * BATTERY_SEGMENT_SPACING), 78, BATTERY_SEGMENT_WIDTH, BATTERY_SEGMENT_HEIGHT, DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  }
}

void drawWeather(){

  weatherData currentWeather = getWeatherData();

  int8_t temperature = currentWeather.temperature;
  int16_t weatherConditionCode = currentWeather.weatherConditionCode;

  display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  display.setFont(&DSEG7_Classic_Regular_39);
  int16_t  x1, y1;
  uint16_t w, h;
  display.getTextBounds(String(temperature), 0, 0, &x1, &y1, &w, &h);
  if(159 - w - x1 > 87){
    display.setCursor(159 - w - x1, 150);
  }else{
    display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
    display.setFont(&DSEG7_Classic_Bold_25);
    display.getTextBounds(String(temperature), 0, 0, &x1, &y1, &w, &h);
    display.setCursor(159 - w - x1, 136);
  }
  display.println(temperature);
  display.drawBitmap(165, 110, currentWeather.isMetric ? celsius : fahrenheit, 26, 20, DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  const unsigned char* weatherIcon;

  if(WIFI_CONFIGURED){
    //https://openweathermap.org/weather-conditions
    if(weatherConditionCode > 801){//Cloudy
      weatherIcon = cloudy;
    }else if(weatherConditionCode == 801){//Few Clouds
      weatherIcon = cloudsun;
    }else if(weatherConditionCode == 800){//Clear
      weatherIcon = sunny;
    }else if(weatherConditionCode >=700){//Atmosphere
      weatherIcon = atmosphere;
    }else if(weatherConditionCode >=600){//Snow
      weatherIcon = snow;
    }else if(weatherConditionCode >=500){//Rain
      weatherIcon = rain;
    }else if(weatherConditionCode >=300){//Drizzle
      weatherIcon = drizzle;
    }else if(weatherConditionCode >=200){//Thunderstorm
      weatherIcon = thunderstorm;
    }else{
      return;
    }
  }else{
    weatherIcon = chip;
  }
  
  display.drawBitmap(145, 158, weatherIcon, WEATHER_ICON_WIDTH, WEATHER_ICON_HEIGHT, DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
}

float getBatteryVoltage() {
  #ifdef ARDUINO_ESP32S3_DEV
  return analogReadMilliVolts(BATT_ADC_PIN) / 1000.0f * ADC_VOLTAGE_DIVIDER;
  #else
  if (RTC.rtcType == DS3231) {
  return analogReadMilliVolts(BATT_ADC_PIN) / 1000.0f *
       2.0f; // Battery voltage goes through a 1/2 divider.
  } else {
  return analogReadMilliVolts(BATT_ADC_PIN) / 1000.0f * 2.0f;
  }
  #endif
}

void showWatchFace() {
  display.setFullWindow(); 
  display.clearScreen( DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.fillRect(0, 0, 200, 200, DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  display.display(false); // full refresh 

  RTC.read(currentTime);
  time_t epoch = makeTime(currentTime);
  mp.calculate(epoch);

  buttonPressed = -1;
  
  while(1) {
    display.fillRect(0, 0, 200, 200, DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
    display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
    display.setCursor(0, 0);

    if(millis() - lastButtonPressTime > DEBOUNCE_DELAY){
      if (digitalRead(DOWN_BTN_PIN) == ACTIVE_LOW) {
        buttonPressed = DOWN_BTN_PIN;
      } else if (digitalRead(UP_BTN_PIN) == ACTIVE_LOW) {
        buttonPressed = UP_BTN_PIN;    
      } else if (digitalRead(BACK_BTN_PIN) == ACTIVE_LOW) {
        buttonPressed = BACK_BTN_PIN;
      } else if (digitalRead(MENU_BTN_PIN) == ACTIVE_LOW) {
        buttonPressed = MENU_BTN_PIN;
      }

      if(buttonPressed != -1){
        lastButtonPressTime = millis();
      }
    }

    if (buttonPressed == BACK_BTN_PIN) {
      break;
    }

    drawTime();
    drawDate();
    drawSteps();
    drawWeather();
    drawBattery();

    display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
    display.drawBitmap(116, 75, WIFI_CONFIGURED ? wifi : wifioff, 26, 18, DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
    if(BLE_CONFIGURED){
      display.drawBitmap(100, 73, bluetooth, 13, 21, DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
    }
    #ifdef ARDUINO_ESP32S3_DEV
    if(USB_PLUGGED_IN){
      display.drawBitmap(140, 75, charge, 16, 18, DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
    }
    #endif
    display.display(true); // partial refresh
  }

  display.clearScreen( DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.display(false); // full refresh
}

weatherData _getWeatherData(String cityID, String lat, String lon, String units, String lang,
                   String url, String apiKey,
                   uint8_t updateInterval) {
  currentWeather.isMetric = units == String("metric");
  if (weatherIntervalCounter < 0) { //-1 on first run, set to updateInterval
  weatherIntervalCounter = updateInterval;
  }
  if (weatherIntervalCounter >=
    updateInterval) { // only update if WEATHER_UPDATE_INTERVAL has elapsed
            // i.e. 30 minutes
  if (connectWiFi()) {
    HTTPClient http; // Use Weather API for live data if WiFi is connected
    http.setConnectTimeout(3000); // 3 second max timeout
    String weatherQueryURL = url;
    if(cityID != ""){
    weatherQueryURL.replace("{cityID}", cityID);
    }else{
    weatherQueryURL.replace("{lat}", lat);
    weatherQueryURL.replace("{lon}", lon);
    }
    weatherQueryURL.replace("{units}", units);
    weatherQueryURL.replace("{lang}", lang);
    weatherQueryURL.replace("{apiKey}", apiKey);
    http.begin(weatherQueryURL.c_str());
    int httpResponseCode = http.GET();
    if (httpResponseCode == 200) {
    String payload       = http.getString();
    JSONVar responseObject   = JSON.parse(payload);
    currentWeather.temperature = int(responseObject["main"]["temp"]);
    currentWeather.weatherConditionCode =
      int(responseObject["weather"][0]["id"]);
    currentWeather.weatherDescription =
      JSONVar::stringify(responseObject["weather"][0]["main"]);
    currentWeather.external = true;
    // use timezone of lat & lon
    settings.gmtOffset = int(responseObject["timezone"]);
    // adjust timezone of sunrise & sunset
    breakTime((time_t)(int)responseObject["sys"]["sunrise"] + settings.gmtOffset, currentWeather.sunrise);
    breakTime((time_t)(int)responseObject["sys"]["sunset"] + settings.gmtOffset, currentWeather.sunset);
    // sync NTP during weather API call using timezone
    syncNTP(settings.gmtOffset, NTP_SERVER);
    } else {
    // http error
    }
    http.end();
    // turn off radios
    WiFi.mode(WIFI_OFF);
    btStop();
  } else { // No WiFi, use internal temperature sensor
    uint8_t temperature = sensor.readTemperature(); // celsius
    if (!currentWeather.isMetric) {
    temperature = temperature * 9. / 5. + 32.; // fahrenheit
    }
    currentWeather.temperature      = temperature;
    currentWeather.weatherConditionCode = 800;
    currentWeather.external       = false;
  }
  weatherIntervalCounter = 0;
  } else {
  weatherIntervalCounter++;
  }
  return currentWeather;
}

weatherData getWeatherData() {
  return _getWeatherData(settings.cityID, settings.lat, settings.lon,
  settings.weatherUnit, settings.weatherLang, settings.weatherURL,
  settings.weatherAPIKey, settings.weatherUpdateInterval);
}

void setAlarm() {
  RTC.read(currentTime);
  #ifdef ARDUINO_ESP32S3_DEV
  uint8_t minute = alarmTime.Minute;
  uint8_t hour   = alarmTime.Hour;
  #else
  int8_t minute = alarmTime.Minute;
  int8_t hour   = alarmTime.Hour;
  #endif

  int8_t setIndex = SET_HOUR;

  int8_t blink = 0;

  pinMode(DOWN_BTN_PIN, INPUT);
  pinMode(UP_BTN_PIN, INPUT);
  pinMode(MENU_BTN_PIN, INPUT);
  pinMode(BACK_BTN_PIN, INPUT);

  display.setFullWindow(); 
  display.clearScreen( DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.fillRect(0, 0, 200, 200, DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  display.display(false); // full refresh 
  buttonPressed = -1;
  while(1){
    if(millis() - lastButtonPressTime > DEBOUNCE_DELAY){
      if (digitalRead(DOWN_BTN_PIN) == ACTIVE_LOW) {
        buttonPressed = DOWN_BTN_PIN;
      } else if (digitalRead(UP_BTN_PIN) == ACTIVE_LOW) {
        buttonPressed = UP_BTN_PIN;    
      } else if (digitalRead(BACK_BTN_PIN) == ACTIVE_LOW) {
        buttonPressed = BACK_BTN_PIN;
      } else if (digitalRead(MENU_BTN_PIN) == ACTIVE_LOW) {
        buttonPressed = MENU_BTN_PIN;
      }

      if(buttonPressed != -1){
        lastButtonPressTime = millis();
      }
    }
    
    if (buttonPressed == MENU_BTN_PIN) {
      setIndex++;
      if (setIndex > 1) {
        break;
      }
    }
    if (buttonPressed == BACK_BTN_PIN) {
      if (setIndex != 0) {
        setIndex--;
      }
    }

    blink = 1 - blink;

    if (buttonPressed == UP_BTN_PIN) {
      blink = 1;
      switch (setIndex) {
      case SET_HOUR:
        hour == 23 ? (hour = 0) : hour++;
        break;
      case SET_MINUTE:
        minute == 59 ? (minute = 0) : minute++;
        break;
      default:
        break;
      }
    }

    if (buttonPressed == DOWN_BTN_PIN) {
      blink = 1;
      switch (setIndex) {
      case SET_HOUR:
        hour == 0 ? (hour = 23) : hour--;
        break;
      case SET_MINUTE:
        minute == 0 ? (minute = 59) : minute--;
        break;
      default:
        break;
      }
    }

    display.fillRect(0, 0, 200, 200, DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
    display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextSize(3);
    display.setCursor(15, 80);
    display.print("ALARM");
    
    display.setCursor(15, 160);
    if (setIndex == SET_HOUR) { // blink hour digits
      if(DARKMODE){
        display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
      } else {
        display.setTextColor(!blink ? GxEPD_WHITE : GxEPD_BLACK);
      }
    }
    if (hour < 10) {
      display.print("0");
    }
    display.print(hour);

    display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
    display.print(":");

    if (setIndex == SET_MINUTE) { // blink minute digits
      if(DARKMODE){
        display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
      } else {
        display.setTextColor(!blink ? GxEPD_WHITE : GxEPD_BLACK);
      }
    }
    if (minute < 10) {
      display.print("0");
    }
    display.print(minute);
    
    //display.refresh(false);

    display.display(true); // partial refresh
  display.clearScreen( DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  }

  alarmTime.Hour   = hour;
  alarmTime.Minute = minute;
  alarmTime.Second = 0;
  setIndex = -1; 
}

unsigned int setIndex = 0;

void showChrono(){

  pinMode(DOWN_BTN_PIN, INPUT);
  pinMode(UP_BTN_PIN, INPUT);
  pinMode(MENU_BTN_PIN, INPUT);
  pinMode(BACK_BTN_PIN, INPUT);

  int8_t blink = 0;

  bool chronoRunning = false;
  bool chronoLap = false;
  bool chronoReset = false;
  bool chronoExit = false;

  int8_t lapCount = 0;
  
  tmElements_t chronoTime;
  chronoTime.Hour = 0;
  chronoTime.Minute = 0;
  chronoTime.Second = 0;

  tmElements_t chronoLapTimes[5];
  unsigned long chronoLapMillis[5];
  for(int i = 0; i<5; i++){
    chronoLapTimes[i].Hour = 0;
    chronoLapTimes[i].Minute = 0;
    chronoLapTimes[i].Second = 0;
    chronoLapMillis[i] = 0;
  }

  unsigned long debounceStart = millis();
  unsigned long previousPress = millis();
  time_t chronoStartTime = millis();
  unsigned long chronoElapsedMillis = 0;
  //bool buttonHeld = false;
  buttonPressed = -1;
    display.clearScreen( DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);

  delay(DEBOUNCE_DELAY);  
  display.setFullWindow(); 
  display.clearScreen( DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.fillRect(0, 0, 200, 200, DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  display.display(false); // full refresh 
  while(1){
    if(millis() - lastButtonPressTime > DEBOUNCE_DELAY){
      if (digitalRead(DOWN_BTN_PIN) == ACTIVE_LOW) {
        buttonPressed = DOWN_BTN_PIN;
      } else if (digitalRead(UP_BTN_PIN) == ACTIVE_LOW) {
        buttonPressed = UP_BTN_PIN;    
      } else if (digitalRead(BACK_BTN_PIN) == ACTIVE_LOW) {
        buttonPressed = BACK_BTN_PIN;
      } else if (digitalRead(MENU_BTN_PIN) == ACTIVE_LOW) {
        buttonPressed = MENU_BTN_PIN;
      }

      if(buttonPressed != -1){
        lastButtonPressTime = millis();
      }
    }

    if (BACK_BTN_PIN == buttonPressed) {
      buttonPressed = -1;
      break;
    }

    if (MENU_BTN_PIN == buttonPressed) {
      chronoLap = true;
    }

    if (DOWN_BTN_PIN == buttonPressed) {
      chronoRunning = !chronoRunning;
    }

    if (UP_BTN_PIN == buttonPressed) {
      chronoReset = true;
    }
    
    buttonPressed = -1;
    //}

    if(chronoRunning){
      chronoElapsedMillis = millis() - chronoStartTime;
      chronoTime.Hour = chronoElapsedMillis / 3600000;
      chronoTime.Minute = (chronoElapsedMillis % 3600000) / 60000;
      chronoTime.Second = (chronoElapsedMillis % 60000) / 1000;
    }else{
      chronoStartTime = millis() - chronoElapsedMillis;
    }

    if(chronoReset){
      chronoElapsedMillis = 0;
      
      for(int i = 0; i<5; i++){
        chronoLapTimes[i].Hour = 0;
        chronoLapTimes[i].Minute = 0;
        chronoLapTimes[i].Second = 0;
        chronoLapMillis[i] = 0;
      }

      chronoTime.Hour = 0;
      chronoTime.Minute = 0;
      chronoTime.Second = 0;
      
      lapCount = 0;

      chronoReset = false;
    }

    if(chronoLap){
      chronoLapTimes[lapCount].Hour = chronoTime.Hour;
      chronoLapTimes[lapCount].Minute = chronoTime.Minute;
      chronoLapTimes[lapCount].Second = chronoTime.Second;
      chronoLapMillis[lapCount] = chronoElapsedMillis % 1000;
      lapCount++;
      chronoLap = false;

      if(lapCount > 4){
        lapCount = 0;
      }
    }

    display.fillRect(0, 0, 200, 200, DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
    display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextSize(1);
    display.setCursor(20, 20);
    //Paint chrono screen
    display.setCursor(20, 20);
    display.print("BACK");
    display.setCursor(20, 180);
    display.print("LAP");
    display.setCursor(80, 180);
    display.print("START/STOP");
    display.setCursor(120, 20);
    display.print("RESET");


    display.setTextSize(1);
    display.setCursor(10, 50);
    //display time: "00:00:00.000"
    char chronoTimeStr[18];
    sprintf(chronoTimeStr, "CUR(%d) %02d:%02d.%03d", lapCount+1, chronoTime.Minute, chronoTime.Second, chronoElapsedMillis%1000);
    display.print(chronoTimeStr);

    for(int i = 0; i<5; i++){
      display.setCursor(10, 70+(i*20));
      
      char lapTimeStr[18];
      sprintf(lapTimeStr, "LAP(%d) %02d:%02d.%03d", i+1, chronoLapTimes[i].Minute, chronoLapTimes[i].Second, chronoLapMillis[i]%1000);
      display.print(lapTimeStr);
    }

  //display.refresh(false);
    yield();
    display.display(true);
  }
}

void showMenu() {
  RTC.read(currentTime);

  unsigned long currentMillis = millis();

  int8_t blink = 0;

  pinMode(DOWN_BTN_PIN, INPUT);
  pinMode(UP_BTN_PIN, INPUT);
  pinMode(MENU_BTN_PIN, INPUT);
  pinMode(BACK_BTN_PIN, INPUT);



  buttonHeld = false;
  buttonPressed = -1;
  display.setFullWindow(); 
  display.clearScreen( DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.fillRect(0, 0, 200, 200, DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  display.display(false); // full refresh 
  time_t lastButtonPressTime = millis();
  bool selected = false;
  while(1) {
   

    display.setFont(&FreeMonoBold9pt7b);
    display.setTextSize(1);
    display.setCursor(20, 20);

    if(millis() - lastButtonPressTime > DEBOUNCE_DELAY){
      if (digitalRead(DOWN_BTN_PIN) == ACTIVE_LOW) {
        buttonPressed = DOWN_BTN_PIN;
      } else if (digitalRead(UP_BTN_PIN) == ACTIVE_LOW) {
        buttonPressed = UP_BTN_PIN;    
      } else if (digitalRead(BACK_BTN_PIN) == ACTIVE_LOW) {
        buttonPressed = BACK_BTN_PIN;
      } else if (digitalRead(MENU_BTN_PIN) == ACTIVE_LOW) {
        buttonPressed = MENU_BTN_PIN;
      }

      if(buttonPressed != -1){
        lastButtonPressTime = millis();
      }
    }

    if (buttonPressed == BACK_BTN_PIN) {
      selected = false;
      break;
    } else if (buttonPressed == DOWN_BTN_PIN) {
      setIndex++;
      if (setIndex > 6) {
        setIndex = 0;
      }
    } else if (buttonPressed == UP_BTN_PIN) {
      setIndex--;
      if (setIndex < 0) {
        setIndex = 6;
      }
    } else if (buttonPressed == MENU_BTN_PIN) {
      selected = true;
    }

    buttonPressed = -1;
    blink = 1 - blink;

    if (setIndex == 0) { // blink hour digits
      if(DARKMODE){
        display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
      } else {
        display.setTextColor(!blink ? GxEPD_WHITE : GxEPD_BLACK);
      }
    } else {
      display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
    }
    display.setCursor(20, 50);
    display.println("Show Time");

    if (setIndex == 1) { // blink minute digits
      if(DARKMODE){
        display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
      } else {
        display.setTextColor(!blink ? GxEPD_WHITE : GxEPD_BLACK);
      }
    } else {
      display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
    }
    display.setCursor(20, 70);
    display.println("Setup WiFi");
    
    if (setIndex == 2) { // blink minute digits
      if(DARKMODE){
        display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
      } else {
        display.setTextColor(!blink ? GxEPD_WHITE : GxEPD_BLACK);
      }
    } else {
      display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
    }
    display.setCursor(20, 90);
    display.println("Sync NTP");
    
    if (setIndex == 3) { // blink minute digits
      if(DARKMODE){
        display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
      } else {
        display.setTextColor(!blink ? GxEPD_WHITE : GxEPD_BLACK);
      }
    } else {
      display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
    }
    display.setCursor(20, 110);
    display.println("Set Alarm");
    
    if (setIndex == 4) { // blink minute digits
      if(DARKMODE){
        display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
      } else {
        display.setTextColor(!blink ? GxEPD_WHITE : GxEPD_BLACK);
      }
    } else {
      display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
    }
    display.setCursor(20, 130);
    display.println("Chrono");
    
    if (setIndex == 5) { // blink minute digits
      if(DARKMODE){
        display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
      } else {
        display.setTextColor(!blink ? GxEPD_WHITE : GxEPD_BLACK);
      }
    } else {
      display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
    }
    display.setCursor(20, 150);
    display.println("Moon Phase");
    
    if (setIndex == 6) { // blink minute digits
      if(DARKMODE){
        display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
      } else {
        display.setTextColor(!blink ? GxEPD_WHITE : GxEPD_BLACK);
      }
    } else {
      display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
    }
    display.setCursor(20, 170);
    display.println("Weather");

    if(selected){
      selected = false;
      blink = 0;      
      delay(DEBOUNCE_DELAY);
          
      switch (setIndex) {
        case 0:
          Serial.println("TIME_BTN_PIN");
          showWatchFace(); 
          break;
        case 1:
          Serial.println("WIFI_BTN_PIN");
          setupWifi();   
          break;
        case 2:
          Serial.println("NTP_BTN_PIN");
          showSyncNTP();
          break;
        case 3:
          Serial.println("ALARM_BTN_PIN");
          setAlarm();
          break;
        case 4:
          Serial.println("CHRONO_BTN_PIN");
          showChrono();
          break;
        case 5:
          Serial.println("MOON_BTN_PIN");
          showMoonPhase();
          break;
        case 6:
          Serial.println("WEATHER_BTN_PIN");
          showWeatherData();
          break;
        default:
          break;
      }
    }
    display.display(true); // partial refresh
  }
  display.setFullWindow(); 
  display.clearScreen( DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.fillRect(0, 0, 200, 200, DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  display.display(false); // full refresh 

}

void setupWifi() {
  display.epd2.setBusyCallback(0); // temporarily disable lightsleep on busy
  wifiManager.resetSettings();
  wifiManager.setTimeout(WIFI_AP_TIMEOUT);
  wifiManager.setAPCallback(_configModeCallback);
  //display.setFullWindow();
    display.setFullWindow(); 
  display.clearScreen( DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.fillRect(0, 0, 200, 200, DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextSize(1);
  display.display(false); // full refresh
  if (!wifiManager.autoConnect(WIFI_AP_SSID)) { // WiFi setup failed
  display.println("Setup failed &");
  display.println("timed out!");
  } else {
  display.println("Connected to:");
  display.println(WiFi.SSID());
  display.println("Local IP:");
  display.println(WiFi.localIP());
  weatherIntervalCounter = -1; // Reset to force weather to be read again
  lastIPAddress = WiFi.localIP();
  WiFi.SSID().toCharArray(lastSSID, 30);
  getWeatherData(); // force weather update
  }
  
  // turn off radios
  WiFi.mode(WIFI_OFF);
  
  btStop();
  // enable lightsleep on busy
  display.epd2.setBusyCallback(WatchyDisplay::busyCallback);
  //display.refresh(false);

display.display(true); // full refresh
  sleep(1);
}

void _configModeCallback(WiFiManager *myWiFiManager) {
  //display.setFullWindow();
  display.fillScreen(GxEPD_BLACK);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_WHITE);
  display.setCursor(0, 30);
  display.println("Connect to");
  display.print("SSID: ");
  display.println(WIFI_AP_SSID);
  display.print("IP: ");
  display.println(WiFi.softAPIP());
  display.println("MAC address:");
  display.println(WiFi.softAPmacAddress().c_str());
  ////display.refresh(false);

//display.display(false); // full refresh
}
bool connectWiFi() {
  //display.setFullWindow();
  display.clearScreen( DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.fillRect(0, 0, 200, 200, DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextSize(1);
  display.display(false); // full refresh
  if (WL_CONNECT_FAILED ==
  WiFi.begin()) { // WiFi not setup, you can also use hard coded credentials
          // with WiFi.begin(SSID,PASS);
  WIFI_CONFIGURED = false;
  } else {
  if (WL_CONNECTED ==
    WiFi.waitForConnectResult() || WiFi.isConnected()) { // attempt to connect for 10s
    lastIPAddress = WiFi.localIP();
    WiFi.SSID().toCharArray(lastSSID, 30);
    WIFI_CONFIGURED = true;
  } else { // connection failed, time out
    WIFI_CONFIGURED = false;
    // turn off radios
    //WiFi.mode(WIFI_OFF);
    WiFi.begin();
    display.print("WiFi Not Connected");
    display.print("IP:");
    display.println(WiFi.softAPIP());
    display.print("MAC:");
    display.println(WiFi.softAPmacAddress().c_str());
    display.print("SSID:");
    display.println(WIFI_AP_SSID);
    btStop();
  }
  }
  return WIFI_CONFIGURED;
}

bool syncNTP(long gmt, String ntpServer) {
  // NTP sync - call after connecting to
  // WiFi and remember to turn it back off
  WiFiUDP ntpUDP;
  NTPClient timeClient(ntpUDP, NTP_SERVER, gmt);
  timeClient.setTimeOffset(gmt);
  timeClient.begin();
  if (!timeClient.forceUpdate()) {
  return false; // NTP sync failed
  }
  tmElements_t tm;
  breakTime((time_t)timeClient.getEpochTime(), tm);
  RTC.set(tm);
  //Update also moon calendar.
  RTC.read(currentTime);
  time_t epoch = makeTime(currentTime);
  mp.calculate(epoch);
  return true;
}

void showSyncNTP() {
  display.setFullWindow(); 
  display.clearScreen( DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.fillRect(0, 0, 200, 200, DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  display.display(false); // full refresh 
  display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextSize(1);
  display.setCursor(10, 30);
  display.println("Syncing NTP... ");
  display.print("GMT offset: ");
  display.println(settings.gmtOffset);
  ////display.refresh(false);

//display.display(false); // full refresh
  if (connectWiFi()) {
    if (syncNTP(settings.gmtOffset, NTP_SERVER)) {
      display.println("NTP Sync Success\n");
      display.println("Current Time Is:");

      RTC.read(currentTime);

      display.print(tmYearToCalendar(currentTime.Year));
      display.print("/");
      display.print(currentTime.Month);
      display.print("/");
      display.print(currentTime.Day);
      display.print(" - ");

        if (currentTime.Hour < 10) {
          display.print("0");
        }
        display.print(currentTime.Hour);
        display.print(":");
        if (currentTime.Minute < 10) {
          display.print("0");
        }
        display.println(currentTime.Minute);
      } else {
        display.println("NTP Sync Failed");
      }
    } else {
      display.println("WiFi Not Configured");
      setupWifi();
      showSyncNTP();
    }
  //display.refresh(false);

  display.display(true); // full refresh
  buttonPressed = -1;
  sleep(1);
  
//  showMenu(menuIndex, true);
}

void showMoonPhase() {
  display.setFullWindow(); 
  display.clearScreen( DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.fillRect(0, 0, 200, 200, DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  display.display(false); // full refresh 

  while(1){
    if(millis() - lastButtonPressTime > DEBOUNCE_DELAY){
      if (digitalRead(DOWN_BTN_PIN) == ACTIVE_LOW) {
        buttonPressed = DOWN_BTN_PIN;
      } else if (digitalRead(UP_BTN_PIN) == ACTIVE_LOW) {
        buttonPressed = UP_BTN_PIN;    
      } else if (digitalRead(BACK_BTN_PIN) == ACTIVE_LOW) {
        buttonPressed = BACK_BTN_PIN;
      } else if (digitalRead(MENU_BTN_PIN) == ACTIVE_LOW) {
        buttonPressed = MENU_BTN_PIN;
      }

      if(buttonPressed != -1){
        lastButtonPressTime = millis();
      }
    }

    if(BACK_BTN_PIN == buttonPressed){
      buttonPressed = -1;
      lastButtonPressTime = millis();
      break;
    }

    RTC.read(currentTime);
    mp.calculate(makeTime(currentTime));


    display.fillRect(0, 0, 200, 200, DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
    display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextSize(1);
    display.setCursor(0, 10);
    
    display.setCursor(0, MENU_HEIGHT);

    display.print("Date: ");
    display.println(mp.jDate);
    
    display.print("Phase: ");
    display.println(mp.phase);

    display.print("Age: ");
    display.print(mp.age);
    display.println(" days");

    display.print("Visibility: ");
    display.print(mp.fraction);
    display.println("%");

    display.print("Distance: ");
    display.print(mp.distance);
    display.println(" er");

    display.print("Latitude: ");
    display.print(mp.latitude);
    display.println("°");

    display.print("Longitude: ");
    display.print(mp.longitude);
    display.println("°");

    display.print("Ph.: ");
    display.println(mp.phaseName);

    display.print("Zodiac: ");
    display.println(mp.zodiacName);
    
    //display.refresh(false);

    display.display(true); // partial refresh
  display.clearScreen( DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);

  }
}

void showWeatherData(){
  display.setFullWindow(); 
  display.clearScreen( DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.fillRect(0, 0, 200, 200, DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  display.display(false); // full refresh 
  
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextSize(1);
  while(1){
    display.fillRect(0, 0, 200, 200, DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
    display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);

    if(millis() - lastButtonPressTime > DEBOUNCE_DELAY){
      if (digitalRead(DOWN_BTN_PIN) == ACTIVE_LOW) {
        buttonPressed = DOWN_BTN_PIN;
      } else if (digitalRead(UP_BTN_PIN) == ACTIVE_LOW) {
        buttonPressed = UP_BTN_PIN;    
      } else if (digitalRead(BACK_BTN_PIN) == ACTIVE_LOW) {
        buttonPressed = BACK_BTN_PIN;
      } else if (digitalRead(MENU_BTN_PIN) == ACTIVE_LOW) {
        buttonPressed = MENU_BTN_PIN;
      }

      if(buttonPressed != -1){
        lastButtonPressTime = millis();
      }
    }

    if (buttonPressed == BACK_BTN_PIN) {
      buttonPressed = -1;
      lastButtonPressTime = millis();
      break;
    }
    
    display.setCursor(10, 10);
    display.print("Weather: ");
    display.print(currentWeather.temperature);
    display.print("C ");
    display.println(currentWeather.weatherDescription);
    display.print("Sunrise: ");
    display.print(currentWeather.sunrise.Hour);
    display.print(":");
    display.println(currentWeather.sunrise.Minute);
    display.print("Sunset: ");
    display.print(currentWeather.sunset.Hour);
    display.print(":");
    display.println(currentWeather.sunset.Minute);
    //display.refresh(false);

    display.display(true); // partial refresh
  display.clearScreen( DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  }
}

uint16_t _readRegister(uint8_t address, uint8_t reg, uint8_t *data,
                 uint16_t len) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.endTransmission();
  Wire.requestFrom((uint8_t)address, (uint8_t)len);
  uint8_t i = 0;
  while(Wire.available()) {
  data[i++] = Wire.read();
  }
  return 0;
}

uint16_t _writeRegister(uint8_t address, uint8_t reg, uint8_t *data,
                uint16_t len) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(data, len);
  return (0 != Wire.endTransmission());
}

void _bmaConfig() {

  if (sensor.begin(_readRegister, _writeRegister, delay) == false) {
  // fail to init BMA
  return;
  }

  // Accel parameter structure
  Acfg cfg;
  /*!
    Output data rate in Hz, Optional parameters:
      - BMA4_OUTPUT_DATA_RATE_0_78HZ
      - BMA4_OUTPUT_DATA_RATE_1_56HZ
      - BMA4_OUTPUT_DATA_RATE_3_12HZ
      - BMA4_OUTPUT_DATA_RATE_6_25HZ
      - BMA4_OUTPUT_DATA_RATE_12_5HZ
      - BMA4_OUTPUT_DATA_RATE_25HZ
      - BMA4_OUTPUT_DATA_RATE_50HZ
      - BMA4_OUTPUT_DATA_RATE_100HZ
      - BMA4_OUTPUT_DATA_RATE_200HZ
      - BMA4_OUTPUT_DATA_RATE_400HZ
      - BMA4_OUTPUT_DATA_RATE_800HZ
      - BMA4_OUTPUT_DATA_RATE_1600HZ
  */
  cfg.odr = BMA4_OUTPUT_DATA_RATE_100HZ;
  /*!
    G-range, Optional parameters:
      - BMA4_ACCEL_RANGE_2G
      - BMA4_ACCEL_RANGE_4G
      - BMA4_ACCEL_RANGE_8G
      - BMA4_ACCEL_RANGE_16G
  */
  cfg.range = BMA4_ACCEL_RANGE_2G;
  /*!
    Bandwidth parameter, determines filter configuration, Optional parameters:
      - BMA4_ACCEL_OSR4_AVG1
      - BMA4_ACCEL_OSR2_AVG2
      - BMA4_ACCEL_NORMAL_AVG4
      - BMA4_ACCEL_CIC_AVG8
      - BMA4_ACCEL_RES_AVG16
      - BMA4_ACCEL_RES_AVG32
      - BMA4_ACCEL_RES_AVG64
      - BMA4_ACCEL_RES_AVG128
  */
  cfg.bandwidth = BMA4_ACCEL_NORMAL_AVG4;

  /*! Filter performance mode , Optional parameters:
    - BMA4_CIC_AVG_MODE
    - BMA4_CONTINUOUS_MODE
  */
  cfg.perf_mode = BMA4_CONTINUOUS_MODE;

  // Configure the BMA423 accelerometer
  sensor.setAccelConfig(cfg);

  // Enable BMA423 accelerometer
  // Warning : Need to use feature, you must first enable the accelerometer
  // Warning : Need to use feature, you must first enable the accelerometer
  sensor.enableAccel();

  struct bma4_int_pin_config config;
  config.edge_ctrl = BMA4_LEVEL_TRIGGER;
  config.lvl     = BMA4_ACTIVE_HIGH;
  config.od    = BMA4_PUSH_PULL;
  config.output_en = BMA4_OUTPUT_ENABLE;
  config.input_en  = BMA4_INPUT_DISABLE;
  // The correct trigger interrupt needs to be configured as needed
  sensor.setINTPinConfig(config, BMA4_INTR1_MAP);

  struct bma423_axes_remap remap_data;
  remap_data.x_axis    = 1;
  remap_data.x_axis_sign = 0xFF;
  remap_data.y_axis    = 0;
  remap_data.y_axis_sign = 0xFF;
  remap_data.z_axis    = 2;
  remap_data.z_axis_sign = 0xFF;
  // Need to raise the wrist function, need to set the correct axis
  sensor.setRemapAxes(&remap_data);

  // Enable BMA423 isStepCounter feature
  sensor.enableFeature(BMA423_STEP_CNTR, true);
  // Enable BMA423 isTilt feature
  sensor.enableFeature(BMA423_TILT, true);
  // Enable BMA423 isDoubleClick feature
  sensor.enableFeature(BMA423_WAKEUP, true);

  // Reset steps
  sensor.resetStepCounter();

  // Turn on feature interrupt
  sensor.enableStepCountInterrupt();
  sensor.enableTiltInterrupt();
  // It corresponds to isDoubleClick interrupt
  sensor.enableWakeupInterrupt();
}

void vibMotor(uint8_t intervalMs, uint8_t length) {
  pinMode(VIB_MOTOR_PIN, OUTPUT);
  bool motorOn = false;
  for (int i = 0; i < length; i++) {
    motorOn = !motorOn;
    digitalWrite(VIB_MOTOR_PIN, motorOn);
    delay(intervalMs);
  }
}

void setup(){
  Serial.begin(115200);
  pinMode(MENU_BTN_PIN, INPUT);
  pinMode(BACK_BTN_PIN, INPUT);
  pinMode(UP_BTN_PIN, INPUT);
  pinMode(DOWN_BTN_PIN, INPUT);
  //esp_sleep_wakeup_cause_t wakeup_reason;
  // wakeup_reason = esp_sleep_get_wakeup_cause(); // get wake up reason
  #ifdef ARDUINO_ESP32S3_DEV
  Wire.begin(WATCHY_V3_SDA, WATCHY_V3_SCL);   // init i2c
  #else
  Wire.begin(SDA, SCL);             // init i2c
  #endif

  RTC.init();
  // Init the display since is almost sure we will use it
  display.epd2.initWatchy();
  mp = MoonPhase();

  _bmaConfig();
  #ifdef ARDUINO_ESP32S3_DEV
  pinMode(USB_DET_PIN, INPUT);
  bool USB_PLUGGED_IN = (digitalRead(USB_DET_PIN) == 1);
  #endif  
  //gmtOffset = settings.gmtOffset;  
  RTC.read(currentTime);
  RTC.read(bootTime);
  // For some reason, seems to be enabled on first boot
//  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);

  time_t epoch = makeTime(currentTime);
  mp.calculate(epoch);

  //display.setFullWindow();
  display.fillScreen(GxEPD_BLACK);
  ////display.refresh(false);

//display.display(false);

  #ifdef ARDUINO_ESP32S3_DEV
  esp_sleep_enable_ext0_wakeup((gpio_num_t)USB_DET_PIN, USB_PLUGGED_IN ? LOW : HIGH); //// enable deep sleep wake on USB plug in/out
  rtc_gpio_set_direction((gpio_num_t)USB_DET_PIN, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en((gpio_num_t)USB_DET_PIN);

/*   esp_sleep_enable_ext1_wakeup(
    BTN_PIN_MASK,
    ESP_EXT1_WAKEUP_ANY_LOW); // enable deep sleep wake on button press
  */
  rtc_gpio_set_direction((gpio_num_t)UP_BTN_PIN, RTC_GPIO_MODE_INPUT_ONLY);
  //rtc_gpio_set_direction((gpio_num_t)DOWN_BTN_PIN, RTC_GPIO_MODE_INPUT_ONLY);
  //rtc_gpio_set_direction((gpio_num_t)MENU_BTN_PIN, RTC_GPIO_MODE_INPUT_ONLY);
  //rtc_gpio_set_direction((gpio_num_t)BACK_BTN_PIN, RTC_GPIO_MODE_INPUT_ONLY);
  
  rtc_gpio_pullup_en((gpio_num_t)UP_BTN_PIN);
  //rtc_gpio_pullup_en((gpio_num_t)DOWN_BTN_PIN);
  //rtc_gpio_pullup_en((gpio_num_t)MENU_BTN_PIN);
  //rtc_gpio_pullup_en((gpio_num_t)BACK_BTN_PIN);

  rtc_clk_32k_enable(true);
  rtc_clk_slow_freq_set(RTC_SLOW_FREQ_32K_XTAL);
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  int secToNextMin = 60 - timeinfo.tm_sec;
  //esp_sleep_enable_timer_wakeup(secToNextMin * uS_TO_S_FACTOR);
  #else
  // Set GPIOs 0-39 to input to avoid power leaking out
  const uint64_t ignore = 0b11110001000000110000100111000010; // Ignore some GPIOs due to resets
  for (int i = 0; i < GPIO_NUM_MAX; i++) {
  if ((ignore >> i) & 0b1)
    continue;
  pinMode(i, INPUT);
  }
  esp_sleep_enable_ext0_wakeup((gpio_num_t)RTC_INT_PIN,
                0); // enable deep sleep wake on RTC interrupt
  esp_sleep_enable_ext1_wakeup(
    BTN_PIN_MASK,
    ESP_EXT1_WAKEUP_ANY_HIGH); // enable deep sleep wake on button press
  #endif
  display.setFullWindow(); 
  display.clearScreen( DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.fillRect(0, 0, 200, 200, DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  display.display(false); // full refresh 

  showSyncNTP();
  
  display.setFullWindow(); 
  display.clearScreen( DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.fillRect(0, 0, 200, 200, DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  display.display(false); // full refresh 
  
  RTC.read(currentTime);
  getWeatherData();
  
  display.setFullWindow(); 
  display.clearScreen( DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.fillRect(0, 0, 200, 200, DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  display.display(false); // full refresh 

}

void handleButtonPress(int buttonPin) {
  Serial.print("Botón Presionado: ");
  Serial.println(buttonPin);

  elapsedMillis = 0; // Reiniciar el contador de tiempo transcurrido

  display.setFullWindow(); 
  display.clearScreen( DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.fillRect(0, 0, 200, 200, DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  display.display(false); // full refresh 
  
  // Operaciones al presionar un botón
  switch (buttonPin) {
    case BACK_BTN_PIN:
      Serial.println("BACK_BTN_PIN");
      break;
    case MENU_BTN_PIN:
      showMenu();  
      //buttonHeld = false;
      break;
    default:
      break;
  }
  buttonHeld = false;
  display.setFullWindow(); 
  display.clearScreen( DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.fillRect(0, 0, 200, 200, DARKMODE ? GxEPD_BLACK : GxEPD_WHITE);
  display.setTextColor(DARKMODE ? GxEPD_WHITE : GxEPD_BLACK);
  display.display(false); // full refresh 

}

void soundAlarm(){
  display.fillScreen(GxEPD_BLACK);

  while(1)
  {
  if(digitalRead(BACK_BTN_PIN) == ACTIVE_LOW){
    break;
  }

  display.setTextColor(GxEPD_WHITE);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextSize(3);
  display.setCursor(10, 80);
  display.print("ALARM");
  display.setCursor(10, 160);
  if (alarmTime.Hour < 10) {
    display.print("0");
  }
  display.print(alarmTime.Hour);

  display.print(":");

  if (alarmTime.Minute < 10) {
    display.print("0");
  }
  display.print(alarmTime.Minute);

  ////display.refresh(false);

//display.display(false); // partial refresh

  vibMotor(4*35, 2);
  vibMotor(4*35, 2);
  vibMotor(4*35, 2);
  vibMotor(2*35, 4);
  vibMotor(2*35, 4);
  
  vibMotor(4*35, 2);
  vibMotor(4*35, 2);
  vibMotor(4*35, 2);
  vibMotor(2*35, 4);
  vibMotor(2*35, 4);

  vibMotor(4*35, 2);
  vibMotor(4*35, 2);
  vibMotor(2*35, 4);
  vibMotor(2*35, 4);

  vibMotor(4*35, 2);
  vibMotor(4*35, 2);
  vibMotor(4*35, 2);
  vibMotor(2*35, 4);
  vibMotor(2*35, 4);
  
  vibMotor(4*35, 2);
  vibMotor(4*35, 2);
  vibMotor(4*35, 2);
  vibMotor(2*35, 4);
  vibMotor(2*35, 4);

  vibMotor(4*35, 2);
  vibMotor(4*35, 2);
  vibMotor(2*35, 4);
  vibMotor(2*35, 4);
  }   
}



void loop() {
  currentMillis = millis();
  USB_PLUGGED_IN = (digitalRead(USB_DET_PIN) == 1);

  RTC.read(currentTime);
  RTC.read(bootTime);
  
  

  if(buttonHeld){
      handleButtonPress(buttonPressed);
  } else {
    if (digitalRead(DOWN_BTN_PIN) == ACTIVE_LOW) {
      if (currentMillis - lastButtonPressTime > DEBOUNCE_DELAY) {
        buttonHeld = true;
        buttonPressed = DOWN_BTN_PIN;
      }
    } else if (digitalRead(UP_BTN_PIN) == ACTIVE_LOW) {
      if (currentMillis - lastButtonPressTime > DEBOUNCE_DELAY) {
        buttonHeld = true;
        buttonPressed = UP_BTN_PIN;    
      }
    } else if (digitalRead(BACK_BTN_PIN) == ACTIVE_LOW) {
      if (currentMillis - lastButtonPressTime > DEBOUNCE_DELAY) {
        buttonHeld = true;
        buttonPressed = BACK_BTN_PIN;
      }
    } else if (digitalRead(MENU_BTN_PIN) == ACTIVE_LOW) {
      if (currentMillis - lastButtonPressTime > DEBOUNCE_DELAY) {
        buttonHeld = true;
        buttonPressed = MENU_BTN_PIN;
      }
    } else {

      // Continuar actualizando la pantalla si es necesario
      display.drawImage(myBitmapallArray[frame++], 0, 0, 200, 200);
    
      if (frame == 1316) {
        frame = 0;
      }

      //esp_sleep_enable_timer_wakeup(WatchyDisplay::partial_refresh_time * 1000);
      //esp_light_sleep_start();

    }
  }


  if(buttonHeld){
    buttonHoldStartTime = currentMillis;
    lastButtonPressTime = currentMillis;
  }

  // Verificar si es la hora en punto para activar la vibración
  if (vibrateOClock) {
    if (currentTime.Minute == 0) {
      if (currentTime.Second < 5) {      
        vibMotor(75, 4);  // Vibrar en cada hora
      }
    }
  }

  if(currentTime.Hour == alarmTime.Hour){
    if(currentTime.Minute == alarmTime.Minute){
      buttonHeld = true;
      
      if(digitalRead(BACK_BTN_PIN) == ACTIVE_LOW || currentTime.Second > 10){
        buttonHeld = false;
      }

      if(buttonHeld){
        soundAlarm();
        buttonHeld = false;
      }
    }
  }


  yield();  // Alimentar el watchdog para evitar reinicios
}
