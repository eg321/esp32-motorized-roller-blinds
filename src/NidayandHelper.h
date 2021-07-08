#pragma once

#ifndef NidayandHelper_h
#define NidayandHelper_h

#include "Arduino.h"
#include <ArduinoJson.h>
#include "FS.h"
#include <PubSubClient.h>
#include <list>
#include <WiFiSettings.h>

#ifdef ESP32
#include "SPIFFS.h"
#include <esp_wifi.h>
#include <WiFi.h>
#include <WebServer.h>
#include "esp_task_wdt.h"

#define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())

#define LED_ON      HIGH
#define LED_OFF     LOW
#else

#include <LittleFS.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>

#define ESP_getChipId()   (ESP.getChipId())

#define LED_ON      LOW
#define LED_OFF     HIGH
#endif

class NidayandHelper {
public:
    NidayandHelper();

    boolean loadconfig();

    JsonVariant getconfig();

    boolean saveconfig(JsonVariant json);

    String mqtt_gettopic(String type);

    boolean mqtt_reconnect(PubSubClient &mqttClient);

    boolean mqtt_reconnect(PubSubClient &mqttClient, std::list<const char *> topics);

    boolean mqtt_reconnect(PubSubClient &mqttClient, String uid, String pwd);

    boolean mqtt_reconnect(PubSubClient &mqttClient, String uid, String pwd, std::list<const char *> topics);

    void mqtt_publish(PubSubClient &mqttClient, String topic, String payload);

    void resetsettings();

private:
    JsonVariant _config;
    String _configfile;
    String _mqttclientid;

    void sendAutoDiscovery(PubSubClient &mqttClient);
};

#endif
