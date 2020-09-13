#pragma once

#ifndef NidayandHelper_h
#define NidayandHelper_h

#include "Arduino.h"
#include <ArduinoJson.h>
#include "FS.h"
#include <PubSubClient.h>
#include <list>

#ifdef ESP32
  #include "SPIFFS.h"
  #include <esp_wifi.h>
  #include <WiFi.h>
  #include <WiFiClient.h>
  #include <WebServer.h>
  #include "esp_task_wdt.h"

  #define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())

  #define LED_ON      HIGH
  #define LED_OFF     LOW
#else
  #include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
  //needed for library
  #include <DNSServer.h>
  #include <ESP8266WebServer.h>

  #define ESP_getChipId()   (ESP.getChipId())

  #define LED_ON      LOW
  #define LED_OFF     HIGH
#endif

#include <ESP_WiFiManager.h>

class NidayandHelper {
  public:
    NidayandHelper();
    boolean loadconfig();
    JsonVariant getconfig();
    boolean saveconfig(JsonVariant json);

    String mqtt_gettopic(String type);

    void mqtt_reconnect(PubSubClient& psclient);
    void mqtt_reconnect(PubSubClient& psclient, std::list<const char*> topics);
    void mqtt_reconnect(PubSubClient& psclient, String uid, String pwd);
    void mqtt_reconnect(PubSubClient& psclient, String uid, String pwd, std::list<const char*> topics);

    void mqtt_publish(PubSubClient& psclient, String topic, String payload);

    void resetsettings(ESP_WiFiManager& wifim);

  private:
    JsonVariant _config;
    String _configfile;
    String _mqttclientid;
};

#endif
