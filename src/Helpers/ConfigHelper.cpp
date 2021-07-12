#include "Arduino.h"
#include "ConfigHelper.h"

ConfigHelper::ConfigHelper() {
    this->_configfile = "/config.json";
}

boolean ConfigHelper::loadconfig() {
    // Serial.println("Before open config");
    // if (SPIFFS.remove(this->_configfile)) {
    //   Serial.println("Config deleted");
    // }
#ifdef ESP32
    File configFile = SPIFFS.open(this->_configfile, "r");
#else
    File configFile = LittleFS.open(this->_configfile, "r");
#endif
    if (!configFile) {
        Serial.println("Failed to open config file");
        return false;
    }

    Serial.println("Config opened");
    size_t size = configFile.size();
    if (size > 1024) {
        Serial.println("Config file size is too large");
        return false;
    }

    String configData;
    while (configFile.available()) {
        configData += char(configFile.read());
    }

    const size_t capacity = JSON_OBJECT_SIZE(12) + 230;
    DynamicJsonBuffer jsonBuffer(capacity);
    this->_config = jsonBuffer.parseObject(configData);

    if (!this->_config.success()) {
        Serial.println("Failed to parse config file");
        return false;
    }
    return true;
}

JsonVariant ConfigHelper::getconfig() {
    return this->_config;
}

boolean ConfigHelper::saveconfig(JsonVariant json) {
#ifdef ESP32
    File configFile = SPIFFS.open(this->_configfile, "w");
#else
    File configFile = LittleFS.open(this->_configfile, "w");
#endif
    if (!configFile) {
        Serial.println("Failed to open config file for writing");
        return false;
    }

    json.printTo(configFile);

    Serial.println("Saved JSON to the config file");
    json.printTo(Serial);
    Serial.println();
    return true;
}

void ConfigHelper::resetsettings() {
    delay(500);
    Serial.println(F("Clearing settings..."));
#ifdef ESP8266
    LittleFS.format();
    ESP.reset();
#else   //ESP32
    SPIFFS.format();
    ESP.restart();
#endif
    Serial.println(F("Settings cleared!"));
    delay(1000);
}
