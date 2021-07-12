#pragma once

#ifndef ConfigHelper_h
#define ConfigHelper_h

#include "Arduino.h"
#include <ArduinoJson.h>
#include "FS.h"

#ifdef ESP32
#include "SPIFFS.h"
#include <WebServer.h>
#include "esp_task_wdt.h"

#define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())
#else

#include <LittleFS.h>
#include <ESP8266WebServer.h>

#define ESP_getChipId()   (ESP.getChipId())
#endif

class ConfigHelper {
public:
    ConfigHelper();

    boolean loadconfig();

    JsonVariant getconfig();

    boolean saveconfig(JsonVariant json);

    void resetsettings();

private:
    JsonVariant _config;
    String _configfile;
};

#endif
