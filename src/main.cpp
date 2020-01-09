#include <Stepper_28BYJ_48.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include <WiFiUdp.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include "FS.h"
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoOTA.h>
#include "NidayandHelper.h"
#include "index_html.h"
#include <string>

//--------------- CHANGE PARAMETERS ------------------
//Configure Default Settings for Access Point logon
String APid = "BlindsConnectAP";    //Name of access point
String APpw = "nidayand";           //Hardcoded password for access point

//----------------------------------------------------

// Version number for checking if there are new code releases and notifying the user
String version = "1.3.3-egor";

NidayandHelper helper = NidayandHelper();

//Fixed settings for WIFI
WiFiClient espClient;
//Setup WIFI Manager
WiFiManager wifiManager;
PubSubClient psclient(espClient);   //MQTT client
char mqtt_server[40];             //WIFI config: MQTT server config (optional)
char mqtt_port[6] = "1883";       //WIFI config: MQTT port config (optional)
char mqtt_uid[40];             //WIFI config: MQTT server username (optional)
char mqtt_pwd[40];             //WIFI config: MQTT server password (optional)

String outputTopic;               //MQTT topic for sending messages
String inputTopic1;                //MQTT topic for listening
String inputTopic2;
String inputTopic3;
boolean mqttActive = true;
char config_name[40];             //WIFI config: Bonjour name of device
char config_rotation[40] = "false"; //WIFI config: Detault rotation is CCW
unsigned long lastBlink = 0;
int state = 0;
long lastPublish = 0;

String action1;                      //Action manual/auto
String action2;
String action3;
String msg;

int set1;
int pos1;
int set2;
int pos2;
int set3;
int pos3;

int path1 = 0;                       //Direction of blind (1 = down, 0 = stop, -1 = up)
int setPos1 = 0;                     //The set position 0-100% by the client

int path2 = 0;                       //Direction of blind (1 = down, 0 = stop, -1 = up)
int setPos2 = 0;

int path3 = 0;                       //Direction of blind (1 = down, 0 = stop, -1 = up)
int setPos3 = 0;

long currentPosition1 = 0;
long maxPosition1 = 100000;

long currentPosition2 = 0;
long maxPosition2 = 100000;

long currentPosition3 = 0;
long maxPosition3 = 100000;

boolean loadDataSuccess = false;
boolean saveItNow = false;          //If true will store positions to SPIFFS
bool shouldSaveConfig = false;      //Used for WIFI Manager callback to save parameters
boolean initLoop = true;            //To enable actions first time the loop is run
boolean ccw = true;                 //Turns counter clockwise to lower the curtain

Stepper_28BYJ_48 Stepper1(D1, D2, D3, D4); //Initiate stepper driver
Stepper_28BYJ_48 Stepper2(D5, D6, D7, D8);
Stepper_28BYJ_48 Stepper3(D9, D9, D9, D9); // not used by me now

ESP8266WebServer server(80);              // TCP server at port 80 will respond to HTTP requests
WebSocketsServer webSocket = WebSocketsServer(81);  // WebSockets will respond on port 81

bool loadConfig() {
  if (!helper.loadconfig()){
    return false;
  }
  JsonObject& root = helper.getconfig();

  root.printTo(Serial);
  Serial.println();

  //Store variables locally
  currentPosition1 = root["currentPosition1"]; // 400
  maxPosition1 = root["maxPosition1"]; // 20000
  currentPosition2 = root["currentPosition2"]; // 40000
  maxPosition2 = root["maxPosition2"]; // 40000
  currentPosition3 = root["currentPosition3"]; // 60000
  maxPosition3 = root["maxPosition3"]; // 60000
  strcpy(config_name, root["config_name"]);
  strcpy(mqtt_server, root["mqtt_server"]);
  strcpy(mqtt_port, root["mqtt_port"]);
  strcpy(mqtt_uid, root["mqtt_uid"]);
  strcpy(mqtt_pwd, root["mqtt_pwd"]);
  strcpy(config_rotation, root["config_rotation"]);
  return true;
}

/**
   Save configuration data to a JSON file
   on SPIFFS
*/
bool saveConfig() {
  const size_t capacity = JSON_OBJECT_SIZE(12);
  DynamicJsonBuffer jsonBuffer(capacity);
  JsonObject& json = jsonBuffer.createObject();
  json["currentPosition1"] = currentPosition1;
  json["maxPosition1"] = maxPosition1;
  json["currentPosition2"] = currentPosition2;
  json["maxPosition2"] = maxPosition2;
  json["currentPosition3"] = currentPosition3;
  json["maxPosition3"] = maxPosition3;
  json["config_name"] = config_name;
  json["mqtt_server"] = mqtt_server;
  json["mqtt_port"] = mqtt_port;
  json["mqtt_uid"] = mqtt_uid;
  json["mqtt_pwd"] = mqtt_pwd;
  json["config_rotation"] = config_rotation;

  return helper.saveconfig(json);
}

/*
   Connect to MQTT server and publish a message on the bus.
   Finally, close down the connection and radio
*/
void sendmsg(String topic) {
  set1 = (setPos1 * 100)/maxPosition1;
  pos1 = (currentPosition1 * 100)/maxPosition1;
  set2 = (setPos2 * 100)/maxPosition2;
  pos2 = (currentPosition2 * 100)/maxPosition2;
  set3 = (setPos3 * 100)/maxPosition3;
  pos3 = (currentPosition3 * 100)/maxPosition3;
    
  msg = "{ \"set1\":"+String(set1)+", \"position1\":"+String(pos1)+", ";
  msg += "\"set2\":"+String(set2)+", \"position2\":"+String(pos2)+", ";
  msg += "\"set3\":"+String(set3)+", \"position3\":"+String(pos3)+" }";
  // Serial.println(msg);
  if (!mqttActive)
    return;

  helper.mqtt_publish(psclient, topic, msg);
  webSocket.broadcastTXT(msg);
}
/****************************************************************************************
*/
void processMsg(String command, String value, int motor_num, uint8_t clientnum){
  /*
     Below are actions based on inbound MQTT payload
  */
  if (command == "start") {
    /*
       Store the current position as the start position
    */
    if (motor_num == 1) {
      currentPosition1 = 0;
      path1 = 0;
      saveItNow = true;
      action1 = "manual";
    }
    else if (motor_num == 2)
    {
      currentPosition2 = 0;
      path2 = 0;
      saveItNow = true;
      action2 = "manual";
    }
    else if (motor_num == 3)
    {
      currentPosition3 = 0;
      path3 = 0;
      saveItNow = true;
      action3 = "manual";
    }
    
  } else if (command == "max") {
    /*
       Store the max position of a closed blind
    */
    if (motor_num == 1) {
      maxPosition1 = currentPosition1;
      path1 = 0;
      saveItNow = true;
      action1 = "manual";
    }
    else if (motor_num == 2)
    {
      maxPosition2 = currentPosition2;
      path2 = 0;
      saveItNow = true;
      action2 = "manual";
    }
    else if (motor_num == 3)
    {
      maxPosition3 = currentPosition3;
      path3 = 0;
      saveItNow = true;
      action3 = "manual";
    }

  } else if (command == "manual" && value == "0") {
    /*
       Stop
    */
   
    if (motor_num == 1) {
      path1 = 0;
      saveItNow = true;
      action1 = "manual";
    }
    else if (motor_num == 2)
    {
      path2 = 0;
      saveItNow = true;
      action2 = "manual";
    }
    else if (motor_num == 3)
    {
      path3 = 0;
      saveItNow = true;
      action3 = "manual";
    }

  } else if (command == "manual" && value == "1") {
    /*
       Move down without limit to max position
    */
    if (motor_num == 1) {
      path1 = 1;
      action1 = "manual";
    }
    else if (motor_num == 2)
    {
      path2 = 1;
      action2 = "manual";
    }
    else if (motor_num == 3)
    {
      path3 = 1;
      action3 = "manual";
    }

  } else if (command == "manual" && value == "-1") {
    /*
       Move up without limit to top position
    */
    if (motor_num == 1) {
      path1 = -1;
      action1 = "manual";
    }
    else if (motor_num == 2)
    {
      path2 = -1;
      action2 = "manual";
    }
    else if (motor_num == 3)
    {
      path3 = -1;
      action3 = "manual";
    }

  } else if (command == "update") {
    //Send position details to client
    sendmsg(outputTopic);
    webSocket.sendTXT(clientnum, msg);

  } else if (command == "ping") {
    //Do nothing
  } else {
    /*
       Any other message will take the blind to a position
       Incoming value = 0-100
       path is now the position
    */
    
    Serial.println("Received position " + value);
    if (motor_num == 1) {
      path1 = maxPosition1 * value.toInt() / 100;
      setPos1 = path1; //Copy path for responding to updates
      action1 = "auto";

      set1 = (setPos1 * 100)/maxPosition1;
      pos1 = (currentPosition1 * 100)/maxPosition1;

      //Send the instruction to all connected devices
      sendmsg(outputTopic);
    }
    else if (motor_num == 2)
    {
      path2 = maxPosition2 * value.toInt() / 100;
      setPos2 = path2; //Copy path for responding to updates
      action2 = "auto";

      set2 = (setPos2 * 100)/maxPosition2;
      pos2 = (currentPosition2 * 100)/maxPosition2;

      //Send the instruction to all connected devices
      sendmsg(outputTopic);
    }
    else if (motor_num == 3)
    {
      path3 = maxPosition3 * value.toInt() / 100;
      setPos3 = path3; //Copy path for responding to updates
      action3 = "auto";

      set3 = (setPos3 * 100)/maxPosition3;
      pos3 = (currentPosition3 * 100)/maxPosition3;

      //Send the instruction to all connected devices
      sendmsg(outputTopic);
    }

  }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case WStype_TEXT:
            Serial.printf("[%u] get Text: %s\n", num, payload);

            String res = (char*)payload;

            StaticJsonBuffer<100> jsonBuffer1;
            JsonObject& root = jsonBuffer1.parseObject(payload);
            if (root.success()) {
              const int motor_id = root["id"];
              String command = root["action"];
              String value = root["value"];
              //Send to common MQTT and websocket function
              processMsg(command, value, motor_id, num);
            break;
            }
            else {
              Serial.println("parseObject() failed");
            }
    }
}
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String res = "";
  for (int i = 0; i < length; i++) {
    res += String((char) payload[i]);
  }
  String motor_str = topic;
  int motor_id = motor_str.charAt(motor_str.length() - 1) - 0x30;

  if (res == "update" || res == "ping") {
    processMsg(res, "", NULL, NULL);
  } else
    processMsg("auto", res, motor_id, NULL);
}

/*
   Callback from WIFI Manager for saving configuration
*/
void saveConfigCallback () {
  shouldSaveConfig = true;
}

void handleRoot() {
  server.send(200, "text/html", INDEX_HTML);
}

void handleResetSettings() {
  helper.resetsettings(wifiManager);
}

void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void setup(void)
{
  Serial.begin(115200);
  delay(100);
  Serial.print("Starting now\n");

  //Reset the action
  action1 = "";
  action2 = "";
  action3 = "";

  //Set MQTT properties
  outputTopic = helper.mqtt_gettopic("out");
  inputTopic1 = helper.mqtt_gettopic("in1");
  inputTopic2 = helper.mqtt_gettopic("in2");
  inputTopic3 = helper.mqtt_gettopic("in3");

  //Set the WIFI hostname
  WiFi.hostname(config_name);

  //Define customer parameters for WIFI Manager
  WiFiManagerParameter custom_config_name("Name", "Bonjour name", config_name, 40);
  WiFiManagerParameter custom_rotation("Rotation", "Clockwise rotation", config_rotation, 40);
  WiFiManagerParameter custom_text("<p><b>Optional MQTT server parameters:</b></p>");
  WiFiManagerParameter custom_mqtt_server("server", "MQTT server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "MQTT port", mqtt_port, 6);
  WiFiManagerParameter custom_mqtt_uid("uid", "MQTT username", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_pwd("pwd", "MQTT password", mqtt_server, 40);
  WiFiManagerParameter custom_text2("<script>t = document.createElement('div');t2 = document.createElement('input');t2.setAttribute('type', 'checkbox');t2.setAttribute('id', 'tmpcheck');t2.setAttribute('style', 'width:10%');t2.setAttribute('onclick', \"if(document.getElementById('Rotation').value == 'false'){document.getElementById('Rotation').value = 'true'} else {document.getElementById('Rotation').value = 'false'}\");t3 = document.createElement('label');tn = document.createTextNode('Clockwise rotation');t3.appendChild(t2);t3.appendChild(tn);t.appendChild(t3);document.getElementById('Rotation').style.display='none';document.getElementById(\"Rotation\").parentNode.insertBefore(t, document.getElementById(\"Rotation\"));</script>");

  //reset settings - for testing
  //clean FS, for testing
  // helper.resetsettings(wifiManager);


  wifiManager.setSaveConfigCallback(saveConfigCallback);
  //add all your parameters here
  wifiManager.addParameter(&custom_config_name);
  wifiManager.addParameter(&custom_rotation);
  wifiManager.addParameter(&custom_text);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_mqtt_uid);
  wifiManager.addParameter(&custom_mqtt_pwd);
  wifiManager.addParameter(&custom_text2);

  wifiManager.autoConnect(APid.c_str(), APpw.c_str());

  //Load config upon start
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  /* Save the config back from WIFI Manager.
      This is only called after configuration
      when in AP mode
  */
  if (shouldSaveConfig) {
    //read updated parameters
    strcpy(config_name, custom_config_name.getValue());
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(mqtt_port, custom_mqtt_port.getValue());
    strcpy(mqtt_uid, custom_mqtt_uid.getValue());
    strcpy(mqtt_pwd, custom_mqtt_pwd.getValue());
    strcpy(config_rotation, custom_rotation.getValue());

    //Save the data
    saveConfig();
  }

  /*
     Try to load FS data configuration every time when
     booting up. If loading does not work, set the default
     positions
  */
  Serial.println("Trying to load config");
  loadDataSuccess = loadConfig();
  Serial.println("Config loaded");
  if (!loadDataSuccess) {
    currentPosition1 = 0;
    maxPosition1 = 100000;
    currentPosition2 = 0;
    maxPosition2 = 100000;
    currentPosition3 = 0;
    maxPosition3 = 100000;
  }

  /*
    Setup multi DNS (Bonjour)
    */
  if (MDNS.begin(config_name)) {
    Serial.println("MDNS responder started");
    MDNS.addService("http", "tcp", 80);
    MDNS.addService("ws", "tcp", 81);

  } else {
    Serial.println("Error setting up MDNS responder!");
    while(1) {
      delay(1000);
    }
  }
  Serial.print("Connect to http://"+String(config_name)+".local or http://");
  Serial.println(WiFi.localIP());

  //Start HTTP server
  server.on("/", handleRoot);
  server.on("/reset", handleResetSettings);
  server.onNotFound(handleNotFound);
  server.begin();

  //Start websocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  /* Setup connection for MQTT and for subscribed
    messages IF a server address has been entered
  */
  if (String(mqtt_server) != ""){
    Serial.println("Registering MQTT server");
    psclient.setServer(mqtt_server, String(mqtt_port).toInt());
    psclient.setCallback(mqttCallback);

  } else {
    mqttActive = false;
    Serial.println("NOTE: No MQTT server address has been registered. Only using websockets");
  }

  /* Set rotation direction of the blinds */
  if (String(config_rotation) == "false"){
    ccw = true;
  } else {
    ccw = false;
  }

  //Update webpage
  INDEX_HTML.replace("{VERSION}","V"+version);
  INDEX_HTML.replace("{NAME}",String(config_name));


  //Setup OTA
  //helper.ota_setup(config_name);
  {
    // Authentication to avoid unauthorized updates
    //ArduinoOTA.setPassword(OTA_PWD);

    ArduinoOTA.setHostname(config_name);

    ArduinoOTA.onStart([]() {
      Serial.println("Start");
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.begin();
  }
  ESP.wdtDisable();
}

/**
  Turn of power to coils whenever the blind
  is not moving
*/
void stopPowerToCoils() {
  digitalWrite(D1, LOW);
  digitalWrite(D2, LOW);
  digitalWrite(D3, LOW);
  digitalWrite(D4, LOW);

  digitalWrite(D5, LOW);
  digitalWrite(D6, LOW);
  digitalWrite(D7, LOW);
  digitalWrite(D8, LOW);

  digitalWrite(D9, LOW);
}

void loop(void)
{
  //OTA client code
  ArduinoOTA.handle();

  //Websocket listner
  webSocket.loop();

  ESP.wdtFeed();

  /**
    Serving the webpage
  */
  server.handleClient();

  //MQTT client
  if (mqttActive){
    helper.mqtt_reconnect(psclient, mqtt_uid, mqtt_pwd, { inputTopic1.c_str(), inputTopic2.c_str(), inputTopic3.c_str() });
  }


  /**
    Storing positioning data and turns off the power to the coils
  */
  if (saveItNow) {
    saveConfig();
    saveItNow = false;

    stopPowerToCoils();
  }

  /**
    Manage actions. Steering of the blind
  */
  if (action1 == "auto") {
    /*
       Automatically open or close blind
    */
    if (currentPosition1 > path1){
      Stepper1.step(ccw ? -1: 1);
      currentPosition1 = currentPosition1 - 1;
    } else if (currentPosition1 < path1){
      Stepper1.step(ccw ? 1 : -1);
      currentPosition1 = currentPosition1 + 1;
    } else {
      path1 = 0;
      action1 = "";
      set1 = (setPos1 * 100)/maxPosition1;
      pos1 = (currentPosition1 * 100)/maxPosition1;
      sendmsg(outputTopic);
      Serial.println("Stopped 1. Reached wanted position");
      saveItNow = true;
    }

  } else if (action1 == "manual" && path1 != 0) {
    /*
       Manually running the blind
    */
    Stepper1.step(ccw ? path1 : -path1);
    currentPosition1 = currentPosition1 + path1;
  }

  if (action2 == "auto") {
    /*
       Automatically open or close blind
    */
    if (currentPosition2 > path2){
      Stepper2.step(ccw ? -1: 1);
      currentPosition2 = currentPosition2 - 1;
    } else if (currentPosition2 < path2){
      Stepper2.step(ccw ? 1 : -1);
      currentPosition2 = currentPosition2 + 1;
    } else {
      path2 = 0;
      action2 = "";
      set2 = (setPos2 * 100)/maxPosition2;
      pos2 = (currentPosition2 * 100)/maxPosition2;
      sendmsg(outputTopic);
      Serial.println("Stopped 2. Reached wanted position");
      saveItNow = true;
    }

 } else if (action2 == "manual" && path2 != 0) {
    /*
       Manually running the blind
    */
    Stepper2.step(ccw ? path2 : -path2);
    currentPosition2 = currentPosition2 + path2;
  }

  if (action3 == "auto") {
    /*
       Automatically open or close blind
    */
    if (currentPosition3 > path3){
      Stepper3.step(ccw ? -1: 1);
      currentPosition3 = currentPosition3 - 1;
    } else if (currentPosition3 < path3){
      Stepper3.step(ccw ? 1 : -1);
      currentPosition3 = currentPosition3 + 1;
    } else {
      path3 = 0;
      action3 = "";
      set3 = (setPos3 * 100)/maxPosition3;
      pos3 = (currentPosition3 * 100)/maxPosition3;
      sendmsg(outputTopic);
      Serial.println("Stopped 3. Reached wanted position");
      saveItNow = true;
    }

  } else if (action3 == "manual" && path3 != 0) {
    /*
       Manually running the blind
    */
    Stepper3.step(ccw ? path3 : -path3);
    currentPosition3 = currentPosition3 + path3;
  }

  /*
     After running setup() the motor might still have
     power on some of the coils. This is making sure that
     power is off the first time loop() has been executed
     to avoid heating the stepper motor draining
     unnecessary current
  */
  if (action1 != "" || action2 != "" || action3 != "") {
    // Running mode
    long now = millis();
    if (now - lastPublish > 1000) {
      lastPublish = now;
      sendmsg(outputTopic);
    }
  }

  /*
     After running setup() the motor might still have
     power on some of the coils. This is making sure that
     power is off the first time loop() has been executed
     to avoid heating the stepper motor draining
     unnecessary current
  */
  if (initLoop) {
    initLoop = false;
    stopPowerToCoils();
  }
}
