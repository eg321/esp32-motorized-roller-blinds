#include "Arduino.h"
#include "NidayandHelper.h"

NidayandHelper::NidayandHelper(){
  this->_configfile = "/config.json";
  this->_mqttclientid = ("ESPClient-" + String(ESP.getChipId()));

}

boolean NidayandHelper::loadconfig(){
  // Serial.println("Before open config");
  // if (SPIFFS.remove(this->_configfile)) {
  //   Serial.println("Config deleted");
  // }
  File configFile = SPIFFS.open(this->_configfile, "r");
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
  while (configFile.available()){
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

JsonVariant NidayandHelper::getconfig(){
  return this->_config;
}

boolean NidayandHelper::saveconfig(JsonVariant json){
  File configFile = SPIFFS.open(this->_configfile, "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(configFile);

  Serial.println("Saved JSON to SPIFFS");
  json.printTo(Serial);
  Serial.println();
  return true;
}

String NidayandHelper::mqtt_gettopic(String type) {
  return "/raw/esp8266/" + String(ESP.getChipId()) + "/" + type;
}


void NidayandHelper::mqtt_reconnect(PubSubClient& psclient){
  return mqtt_reconnect(psclient, String(NULL), String(NULL));
}
void NidayandHelper::mqtt_reconnect(PubSubClient& psclient, std::list<const char*> topics){
  return mqtt_reconnect(psclient, String(NULL), String(NULL), topics);
}
void NidayandHelper::mqtt_reconnect(PubSubClient& psclient, String uid, String pwd){
  std::list<const char*> mylist;
  return mqtt_reconnect(psclient, uid, pwd, mylist);
}
void NidayandHelper::mqtt_reconnect(PubSubClient& psclient, String uid, String pwd, std::list<const char*> topics){
  // Loop until we're reconnected
  boolean mqttLogon = false;
  if (uid!=NULL and pwd != NULL){
    mqttLogon = true;
  }
  while (!psclient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if ((mqttLogon ? psclient.connect(this->_mqttclientid.c_str(), uid.c_str(), pwd.c_str()) : psclient.connect(this->_mqttclientid.c_str()))) {
      Serial.println("connected");

      //Send register MQTT message with JSON of chipid and ip-address
      this->mqtt_publish(psclient, "/raw/esp8266/register", "{ \"id\": \"" + String(ESP.getChipId()) + "\", \"ip\":\"" + WiFi.localIP().toString() +"\"}");

      //Setup subscription
      if (!topics.empty()){
        for (const char* t : topics){
           psclient.subscribe(t);
           Serial.println("Subscribed to "+String(t));
        }
      }

    } else {
      Serial.print("failed, rc=");
      Serial.print(psclient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      ESP.wdtFeed();
      delay(5000);
    }
  }
  if (psclient.connected()){
    psclient.loop();
  }
}

void NidayandHelper::mqtt_publish(PubSubClient& psclient, String topic, String payload){
  Serial.println("Trying to send msg..."+topic+":"+payload);
  //Send status to MQTT bus if connected
  if (psclient.connected()) {
    psclient.publish(topic.c_str(), payload.c_str());
  } else {
    Serial.println("PubSub client is not connected...");
  }
}

void NidayandHelper::resetsettings(WiFiManager& wifim){
  wifim.resetSettings();
  delay(500);
  SPIFFS.format();
  Serial.println("Settings cleared");
  ESP.reset();
}
