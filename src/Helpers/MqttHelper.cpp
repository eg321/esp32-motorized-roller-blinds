#include "MqttHelper.h"

boolean MqttHelper::reconnect() {
    boolean isLoginNeeded = false;

    if (!mqttUser.isEmpty() and !mqttPwd.isEmpty()) {
        isLoginNeeded = true;
    }
    if (!getClient().connected()) {
        String clientId = "ESP-Blinds-" + String(ESP_getChipId());
        Serial.printf("MQTT connecting (login: '%s', pass: '%s')...\r\n", mqttUser.c_str(), mqttPwd.c_str());
        // Attempt to connect
        if ((isLoginNeeded ? getClient().connect(clientId.c_str(), mqttUser.c_str(), mqttPwd.c_str())
                           : getClient().connect(clientId.c_str()))) {
            Serial.println("MQTT connected.");

            //Setup subscription
            if (!topicsToSubscribe.empty()) {
                for (const String &topic : topicsToSubscribe) {
                    getClient().subscribe(topic.c_str());
                    Serial.println("Subscribed to " + topic);
                }
            }

            if (onConnect) onConnect();

            sendAvailabilityMessage();

            return true;
        } else {
            Serial.println("MQTT connection failure.");
        }
    }

    return false;
}

void MqttHelper::sendAvailabilityMessage() {
    publishMsg(prefix + "/" + String(ESP_getChipId()) + "/available", "online");
}

void MqttHelper::loop() {
    if (!isMqttEnabled) {
        return;
    }
    unsigned long now = millis();

    if (getClient().connected()) {
        getClient().loop();

        if (abs(now - lastAvailableMsgTime) > AVAILABILITY_MSG_INTERVAL * 1000) {
            sendAvailabilityMessage();
            lastAvailableMsgTime = now;
        }
    } else {
        if (abs(now - mqttLastConnectAttempt) > MQTT_RECONNECT_DELAY) { //Attempt to reconnect once per 30 sec
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
        Serial.println("Configuring connection to the MQTT server...");
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
        client->setBufferSize(1024); // increased size needed for HA Autodiscovery packets
    }

    return *client;
}

String MqttHelper::getTopicPath(const String &suffix) {
    return prefix + "/" + String(ESP_getChipId()) + "/" + suffix;
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
