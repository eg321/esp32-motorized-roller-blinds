#include "MqttHelper.h"

boolean MqttHelper::reconnect() {
    boolean isLoginNeeded = false;

    if (!mqttUser.isEmpty() and !mqttPwd.isEmpty()) {
        isLoginNeeded = true;
    }
    if (!getClient().connected()) {
        String clientId = "ESP-Blinds-" + String(ESP_getChipId());
        Serial.printf("MQTT Login: '%s', pass: '%s'\r\n", mqttUser.c_str(), mqttPwd.c_str());
        // Attempt to connect
        if ((isLoginNeeded ? getClient().connect(clientId.c_str(), mqttUser.c_str(), mqttPwd.c_str())
                           : getClient().connect(clientId.c_str()))) {
            Serial.println("MQTT connected.");

            //Send register MQTT message with JSON of chipid and ip-address
            publishMsg(prefix + "register",
                       "{ \"id\": \"" + String(ESP_getChipId()) + "\", \"ip\":\"" + WiFi.localIP().toString() +
                       "\"}");

            //HA autodiscovery
            sendHAAutoDiscovery();

            //Setup subscription
            if (!topicsToSubscribe.empty()) {
                for (const String &topic : topicsToSubscribe) {
                    getClient().subscribe(topic.c_str());
                    Serial.println("Subscribed to " + topic);
                }
            }

            return true;
        }
    }

    return false;
}

void MqttHelper::loop() {
    if (!isMqttEnabled) {
        return;
    }

    if (getClient().connected()) {
        getClient().loop();
    } else {
        unsigned long now = millis();
        if (now - mqttLastConnectAttempt > MQTT_RECONNECT_DELAY) { //Attempt to reconnect once per 30 sec
            mqttLastConnectAttempt = now;
            if (reconnect()) {
                mqttLastConnectAttempt = 0;
            }
        }
    }
}

void MqttHelper::setup(MQTT_CALLBACK_SIGNATURE) {
    /* Setup connection for MQTT and for subscribed
      messages IF a server address has been entered
    */
    if (String(mqttServer) != "") {
        Serial.println("Registering MQTT server...");
        getClient().setServer(mqttServer.c_str(), mqttPort);
        getClient().setCallback(callback);
        outputTopic = getTopicPath("out");
        topicsToSubscribe.push_back(getTopicPath("in"));
    } else {
        isMqttEnabled = false;
        Serial.println("NOTE: No MQTT server address has been configured. Using websockets only...");
    }
}

PubSubClient &MqttHelper::getClient() {
    if (client == nullptr) {
        client = new PubSubClient(espClient);
    }

    return *client;
}

String MqttHelper::getTopicPath(const String &suffix) {
    return prefix + String(ESP_getChipId()) + "/" + suffix;
}

void MqttHelper::publishMsg(String topic, String payload) {
    Serial.println("Trying to send msg to the '" + topic + "': " + payload);
    //Send status to MQTT bus if connected
    if (getClient().connected()) {
        if (!getClient().publish(topic.c_str(), payload.c_str())) {
            Serial.println(F("Cannot send message to the MQTT server."));
        }
    } else {
        Serial.println("Cannot send message - MQTT client is not connected.");
    }
}

//Can be use by HomeAssistant to automatically discover blinds
void MqttHelper::sendHAAutoDiscovery() {
    return; //@TODO: fix autodiscover
    String haConfig;
    uint32_t chipId = ESP_getChipId();

    for (int i = 1; i <= 3; i++) {
        haConfig = "{\"~\": \"/raw/esp8266/" + String(chipId) + "\", \"name\": \"Rollerblind " + String(i) +
                   "\", \"cmd_t\": \"~/in" + String(i) + "\", \"set_pos_t\": \"~/in" + String(i) +
                   "\", \"pos_t\": \"~/out" + String(i) +
                   "\", \"payload_open\": 0, \"payload_close\": 100, \"pos_open\": 0, \"pos_clsd\": 100, \"val_tpl\": \"{{ value | int }}\", \"opt\": false }";
        publishMsg("homeassistant/cover/" + String(chipId) + "/blind" + String(i) + "/config", haConfig);
    }
}