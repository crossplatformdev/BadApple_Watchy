#ifndef SETTINGS_H
#define SETTINGS_H

#include "Watchy.h"

//Weather Settings
#define CITY_ID "3117735" //New York City https://openweathermap.org/current#cityid
#define OPENWEATHERMAP_APIKEY "25819493fef88b03ed48312e7436448b" //use your own API key :)
#define OPENWEATHERMAP_URL "http://api.openweathermap.org/data/2.5/weather?id={cityID}&appid={apiKey}" //open weather api
#define TEMP_UNIT "metric" //metric = Celsius , imperial = Fahrenheit
#define TEMP_LANG "en"
#define WEATHER_UPDATE_INTERVAL 5 //must be greater than 5, measured in minutes
//NTP Settings
#define NTP_SERVER "pool.ntp.org"
#define GMT_OFFSET_SEC 3600 * 2 //New York is UTC -5 EST, -4 EDT, will be overwritten by weather data

watchySettings settings{
    .cityID = CITY_ID,
    .weatherAPIKey = OPENWEATHERMAP_APIKEY,
    .weatherURL = OPENWEATHERMAP_URL,
    .weatherUnit = TEMP_UNIT,
    .weatherLang = TEMP_LANG,
    .weatherUpdateInterval = WEATHER_UPDATE_INTERVAL,
    .ntpServer = NTP_SERVER,
    .gmtOffset = GMT_OFFSET_SEC,
    .vibrateOClock = true,
};

#endif
