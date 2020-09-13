#define _WIFIMGR_LOGLEVEL_ 4
#define USE_AVAILABLE_PAGES true

#include <CheapStepper.h>
//#include <ESP8266mDNS.h>
#include <WiFiUdp.h>

#include <WebSocketsServer.h>
#include <ArduinoOTA.h>
#include "NidayandHelper.h"
#include "index_html.h"
#include <string>

//--------------- CHANGE PARAMETERS ------------------
//Configure Default Settings for Access Point logon
String APid = "BlindsConnectAP";    //Name of access point
String APpw = "nidayand";           //Hardcoded password for access point
// SSID and PW for your Router
String Router_SSID;
String Router_Pass;

//----------------------------------------------------

// Version number for checking if there are new code releases and notifying the user
String version = "1.4.0-egor";

NidayandHelper helper = NidayandHelper();

//Fixed settings for WIFI
WiFiClient espClient;
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
int path2 = 0;                       //Direction of blind (1 = down, 0 = stop, -1 = up)
int path3 = 0;                       //Direction of blind (1 = down, 0 = stop, -1 = up)

int currentPosition1 = 0;
int targetPosition1 = 0;                     //The set position 0-100% by the client
int maxPosition1 = 100000;

int currentPosition2 = 0;
int targetPosition2 = 0;
int maxPosition2 = 100000;

int currentPosition3 = 0;
int targetPosition3 = 0;
int maxPosition3 = 100000;

boolean loadDataSuccess = false;
boolean saveItNow = false;          //If true will store positions to SPIFFS
bool shouldSaveConfig = false;      //Used for WIFI Manager callback to save parameters
boolean initLoop = true;            //To enable actions first time the loop is run
boolean ccw = true;                 //Turns counter clockwise to lower the curtain

#define M1_1 25
#define M1_2 26
#define M1_3 32
#define M1_4 33
CheapStepper Stepper1(M1_1, M1_2, M1_3, M1_4); //Initiate stepper driver

#define M2_1 27
#define M2_2 14
#define M2_3 12
#define M2_4 13
CheapStepper Stepper2(M2_1, M2_2, M2_3, M2_4);

#define M3_1 17
#define M3_2 5
#define M3_3 18
#define M3_4 19
CheapStepper Stepper3(M3_1, M3_2, M3_3, M3_4);

#ifdef ESP32
  WebServer server(80);              // TCP server at port 80 will respond to HTTP requests
#else
  ESP8266WebServer server(80);              // TCP server at port 80 will respond to HTTP requests
#endif

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
  set1 = (targetPosition1 * 100)/maxPosition1;
  pos1 = (currentPosition1 * 100)/maxPosition1;
  set2 = (targetPosition2 * 100)/maxPosition2;
  pos2 = (currentPosition2 * 100)/maxPosition2;
  set3 = (targetPosition3 * 100)/maxPosition3;
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

  } else if ((command == "manual" && value == "0") || value == "STOP") {
    /*
       Stop
    */
    if (motor_num == 1) {
      path1 = 0;
      saveItNow = true;
      action1 = "";
    }
    else if (motor_num == 2)
    {
      path2 = 0;
      saveItNow = true;
      action2 = "";
    }
    else if (motor_num == 3)
    {
      path3 = 0;
      saveItNow = true;
      action3 = "";
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
      targetPosition1 = path1; //Copy path for responding to updates
      action1 = "auto";

      set1 = (targetPosition1 * 100)/maxPosition1;
      pos1 = (currentPosition1 * 100)/maxPosition1;

      Serial.printf("Starting movement of Stepper1. currentPosition %i; targetPosition %i \n", currentPosition1, targetPosition1);
      Stepper1.newMove(currentPosition1 < targetPosition1, abs(currentPosition1 - targetPosition1));

      //Send the instruction to all connected devices
      sendmsg(outputTopic);
    }
    else if (motor_num == 2)
    {
      path2 = maxPosition2 * value.toInt() / 100;
      targetPosition2 = path2; //Copy path for responding to updates
      action2 = "auto";

      set2 = (targetPosition2 * 100)/maxPosition2;
      pos2 = (currentPosition2 * 100)/maxPosition2;

      Serial.printf("Starting movement of Stepper2. currentPosition %i; targetPosition %i \n", currentPosition2, targetPosition2);
      Stepper2.newMove(currentPosition2 < targetPosition2, abs(currentPosition2 - targetPosition2));

      //Send the instruction to all connected devices
      sendmsg(outputTopic);
    }
    else if (motor_num == 3)
    {
      path3 = maxPosition3 * value.toInt() / 100;
      targetPosition3 = path3; //Copy path for responding to updates
      action3 = "auto";

      set3 = (targetPosition3 * 100)/maxPosition3;
      pos3 = (currentPosition3 * 100)/maxPosition3;

      Serial.printf("Starting movement of Stepper3. currentPosition %i; targetPosition %i \n", currentPosition3, targetPosition3);
      Stepper3.newMove(currentPosition3 < targetPosition3, abs(currentPosition3 - targetPosition3));

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
  
  Serial.println("Starting now...");

//Load config upon start
  Serial.println("Initializing SPIFFS...");
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  // Serial.println("Formatting spiffs now...");
  // SPIFFS.format();
  // Serial.println("done format.");

  //Setup WIFI Manager
  ESP_WiFiManager wifiManager;
  wifiManager.setDebugOutput(true);
  wifiManager.setConfigPortalTimeout(1);

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
#ifdef ESP32
  WiFi.setHostname(config_name);
#else //ESP8266
  WiFi.hostname(config_name);
#endif

  //Define customer parameters for WIFI Manager
  Serial.println("Configuring WiFi-manager parameters...");
  ESP_WMParameter custom_config_name("Name", "Bonjour name", config_name, 40);
  ESP_WMParameter custom_rotation("Rotation", "Clockwise rotation", config_rotation, 40);
  ESP_WMParameter custom_text("<p><b>Optional MQTT server parameters:</b></p>");
  ESP_WMParameter custom_mqtt_server("server", "MQTT server", mqtt_server, 40);
  ESP_WMParameter custom_mqtt_port("port", "MQTT port", mqtt_port, 6);
  ESP_WMParameter custom_mqtt_uid("uid", "MQTT username", mqtt_server, 40);
  ESP_WMParameter custom_mqtt_pwd("pwd", "MQTT password", mqtt_server, 40);
  ESP_WMParameter custom_text2("<script>t = document.createElement('div');t2 = document.createElement('input');t2.setAttribute('type', 'checkbox');t2.setAttribute('id', 'tmpcheck');t2.setAttribute('style', 'width:10%');t2.setAttribute('onclick', \"if(document.getElementById('Rotation').value == 'false'){document.getElementById('Rotation').value = 'true'} else {document.getElementById('Rotation').value = 'false'}\");t3 = document.createElement('label');tn = document.createTextNode('Clockwise rotation');t3.appendChild(t2);t3.appendChild(tn);t.appendChild(t3);document.getElementById('Rotation').style.display='none';document.getElementById(\"Rotation\").parentNode.insertBefore(t, document.getElementById(\"Rotation\"));</script>");

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

  
  Router_SSID = wifiManager.WiFi_SSID();
  Router_Pass = wifiManager.WiFi_Pass();
  //Remove this line if you do not want to see WiFi password printed
  Serial.println("Stored: SSID = " + Router_SSID + ", Pass = " + Router_Pass);
  if (Router_SSID != "") {
    Serial.println("Got stored Credentials.");
  } else {
    Serial.println("No stored Credentials. No timeout");
  }

  wifiManager.autoConnect(APid.c_str(), APpw.c_str());

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
  // if (MDNS.begin(config_name)) {
  //   Serial.println("MDNS responder started");
  //   MDNS.addService("http", "tcp", 80);
  //   MDNS.addService("ws", "tcp", 81);

  // } else {
  //   Serial.println("Error setting up MDNS responder!");
  //   while(1) {
  //     delay(1000);
  //   }
  // }
  Serial.print("Connect to http://"+String(config_name)+".local or http://");
  Serial.println(WiFi.localIP());

  //Start HTTP server
  server.on("/", handleRoot);
  server.on("/reset", [&]{ helper.resetsettings(wifiManager); });
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

  Stepper1.setRpm(30);
  Stepper2.setRpm(30);
  Stepper3.setRpm(30);

#ifdef ESP32
  esp_task_wdt_init(10, true);
#else
  ESP.wdtDisable();
#endif
}

/**
  Turn of power to coils whenever the blind
  is not moving
*/
void stopPowerToCoils() {
  digitalWrite(M1_1, LOW);
  digitalWrite(M1_2, LOW);
  digitalWrite(M1_3, LOW);
  digitalWrite(M1_4, LOW);

  digitalWrite(M2_1, LOW);
  digitalWrite(M2_2, LOW);
  digitalWrite(M2_3, LOW);
  digitalWrite(M2_4, LOW);

  digitalWrite(M3_1, LOW);
  digitalWrite(M3_2, LOW);
  digitalWrite(M3_3, LOW);
  digitalWrite(M3_4, LOW);

//  digitalWrite(D9, LOW);
}

void loop(void)
{
  //OTA client code
  ArduinoOTA.handle();

  //Websocket listner
  webSocket.loop();
#ifdef ESP32
  esp_task_wdt_reset();
#else //ESP8266
  ESP.wdtFeed();
#endif


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
    Stepper1.run();
    currentPosition1 = targetPosition1 - Stepper1.getStepsLeft();
    if (currentPosition1 == targetPosition1) {
      path1 = 0;
      action1 = "";
      set1 = (targetPosition1 * 100)/maxPosition1;
      pos1 = (currentPosition1 * 100)/maxPosition1;
      sendmsg(outputTopic);
      Serial.println("Stepper 1 has reached target position.");
      saveItNow = true;
    }
  } else if (action1 == "manual" && path1 != 0) {
    Stepper1.move(path1 > 0, abs(path1));
    currentPosition1 = currentPosition1 + path1;
  }


  if (action2 == "auto") {
    Stepper2.run();
    currentPosition2 = targetPosition2 - Stepper2.getStepsLeft();
    if (currentPosition2 == targetPosition2) {
      path2 = 0;
      action2 = "";
      set2 = (targetPosition2 * 100)/maxPosition2;
      pos2 = (currentPosition2 * 100)/maxPosition2;
      sendmsg(outputTopic);
      Serial.println("Stepper 2 has reached target position.");
      saveItNow = true;
    }
  } else if (action2 == "manual" && path2 != 0) {
    Stepper2.move(path2 > 0, abs(path2));
    currentPosition2 = currentPosition2 + path2;
  }

  if (action3 == "auto") {
    Stepper3.run();
    currentPosition3 = targetPosition3 - Stepper3.getStepsLeft();
    if (currentPosition3 == targetPosition3) {
      path3 = 0;
      action3 = "";
      set3 = (targetPosition3 * 100)/maxPosition3;
      pos3 = (currentPosition3 * 100)/maxPosition3;
      sendmsg(outputTopic);
      Serial.println("Stepper 3 has reached target position.");
      saveItNow = true;
    }
  } else if (action3 == "manual" && path3 != 0) {
    Stepper3.move(path3 > 0, abs(path3));
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