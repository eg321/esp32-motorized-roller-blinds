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
#define AVAILABILITY_MSG_INTERVAL 300 //seconds between availability messages

class MqttHelper {
private:
    unsigned long mqttLastConnectAttempt = 0;
    PubSubClient* client = nullptr;
    unsigned long lastAvailableMsgTime = 0;

public:
    typedef std::function<void(void)> TCallback;

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

    TCallback onConnect;

    PubSubClient& getClient();

    String getTopicPath(const String& suffix);

    boolean reconnect();

    void publishMsg(String topic, String payload);

    String prefix = "ESP_Blinds";

    void sendAvailabilityMessage();
};

#endif //ESP32_MOTORIZED_ROLLER_BLINDS_MQTTHELPER_H
