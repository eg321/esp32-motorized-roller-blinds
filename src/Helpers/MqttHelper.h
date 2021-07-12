#ifndef ESP32_MOTORIZED_ROLLER_BLINDS_MQTTHELPER_H
#define ESP32_MOTORIZED_ROLLER_BLINDS_MQTTHELPER_H

#include <WString.h>
#include <list>
#include "Arduino.h"
#include <PubSubClient.h>
#include "Helpers/ConfigHelper.h"

#ifdef ESP32
    #include <esp_wifi.h>
    #include <WiFi.h>
#else
    #include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
    //needed for library
    #include <DNSServer.h>
    #include <ESP8266WebServer.h>
#endif

#define MQTT_RECONNECT_DELAY 30000 //Delay in ms between reconnections

class MqttHelper {
private:
    unsigned long mqttLastConnectAttempt = 0;
    PubSubClient* client = nullptr;
    String prefix = "ESP_Blinds/"; //root path for mqtt messages

public:
    String mqttServer;           //WIFI config: MQTT server config (optional)
    int mqttPort = 1883;         //WIFI config: MQTT port config (optional)
    String mqttUser;             //WIFI config: MQTT server username (optional)
    String mqttPwd;             //WIFI config: MQTT server password (optional)
    boolean isMqttEnabled = true;
    std::list<String> topicsToSubscribe;
    String outputTopic;               //MQTT topic for sending messages

    WiFiClient espClient;

    void loop();

    void setup(MQTT_CALLBACK_SIGNATURE);

    PubSubClient& getClient();

    String getTopicPath(const String& suffix);

    boolean reconnect();

    void sendHAAutoDiscovery();

    void publishMsg(String topic, String payload);
};

#endif //ESP32_MOTORIZED_ROLLER_BLINDS_MQTTHELPER_H
