#include "Arduino.h"
#include "NidayandHelper.h"

NidayandHelper::NidayandHelper() {
    this->_configfile = "/config.json";
    this->_mqttclientid = ("ESPClient-" + String(ESP_getChipId()));

}

boolean NidayandHelper::loadconfig() {
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

JsonVariant NidayandHelper::getconfig() {
    return this->_config;
}

boolean NidayandHelper::saveconfig(JsonVariant json) {
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

String NidayandHelper::mqtt_gettopic(String type) {
    return "/raw/esp8266/" + String(ESP_getChipId()) + "/" + type;
}


boolean NidayandHelper::mqtt_reconnect(PubSubClient &mqttClient) {
    return mqtt_reconnect(mqttClient, "", "");
}

boolean NidayandHelper::mqtt_reconnect(PubSubClient &mqttClient, std::list<const char *> topics) {
    return mqtt_reconnect(mqttClient, "", "", topics);
}

boolean NidayandHelper::mqtt_reconnect(PubSubClient &mqttClient, String uid, String pwd) {
    std::list<const char *> mylist;
    return mqtt_reconnect(mqttClient, uid, pwd, mylist);
}

boolean
NidayandHelper::mqtt_reconnect(PubSubClient &mqttClient, String uid, String pwd, std::list<const char *> topics) {
    boolean mqttLogon = false;

    if (!uid.isEmpty() and !pwd.isEmpty()) {
        mqttLogon = true;
    }
    if (!mqttClient.connected()) {
        Serial.printf("MQTT Login: '%s', pass: '%s'\n", uid.c_str(), pwd.c_str());
        // Attempt to connect
        if ((mqttLogon ? mqttClient.connect(this->_mqttclientid.c_str(), uid.c_str(), pwd.c_str()) : mqttClient.connect(
                this->_mqttclientid.c_str()))) {
            Serial.println("connected");

            //Send register MQTT message with JSON of chipid and ip-address
            this->mqtt_publish(mqttClient, "/raw/esp8266/register",
                               "{ \"id\": \"" + String(ESP_getChipId()) + "\", \"ip\":\"" + WiFi.localIP().toString() +
                               "\"}");

            //HA autodiscovery
            this->sendAutoDiscovery(mqttClient);

            //Setup subscription
            if (!topics.empty()) {
                for (const char *t : topics) {
                    mqttClient.subscribe(t);
                    Serial.println("Subscribed to " + String(t));
                }
            }

            return true;
        }
    }

    return false;
}

//Can be use by HomeAssistant to automatically confugure 
void NidayandHelper::sendAutoDiscovery(PubSubClient &mqttClient) {
    String haConfig;
    uint32_t chipId = ESP_getChipId();

    for (int i = 1; i <= 3; i++) {
        haConfig = "{\"~\": \"/raw/esp8266/" + String(chipId) + "\", \"name\": \"Rollerblind " + String(i) +
                   "\", \"cmd_t\": \"~/in" + String(i) + "\", \"set_pos_t\": \"~/in" + String(i) +
                   "\", \"pos_t\": \"~/out" + String(i) +
                   "\", \"payload_open\": 0, \"payload_close\": 100, \"pos_open\": 0, \"pos_clsd\": 100, \"val_tpl\": \"{{ value | int }}\", \"opt\": false }";
        haConfig = "{\"~\": \"/raw/esp8266/" + String(chipId) + "\", \"name\": \"Rollerblind " + String(i) +
                   "\", \"cmd_t\": \"~/in" + String(i) + "\", \"set_pos_t\": \"~/in" + String(i) +
                   "\", \"pos_t\": \"~/out" + String(i) +
                   "\", \"pos_open\": 0, \"pos_clsd\": 100, \"val_tpl\": \"{{ value | int }}\" }";
        this->mqtt_publish(mqttClient, "homeassistant/cover/" + String(chipId) + "/blind" + String(i) + "/config",
                           haConfig);
    }
}


void NidayandHelper::mqtt_publish(PubSubClient &mqttClient, String topic, String payload) {
    Serial.println("Trying to send msg to the '" + topic + "': " + payload);
    //Send status to MQTT bus if connected
    if (mqttClient.connected()) {
        if (!mqttClient.publish(topic.c_str(), payload.c_str())) {
            Serial.println(F("Cannot send message to the MQTT server."));
        }
    } else {
        Serial.println("PubSub client is not connected...");
    }
}

void NidayandHelper::resetsettings() {
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
