#define LONG_PRESS_MS 1000      //How much ms you should press button to switch to "long press" mode (tune blinds position)

#define _WIFIMGR_LOGLEVEL_ 4
#define USE_AVAILABLE_PAGES true

#include <CheapStepper.h>
//#include <ESP8266mDNS.h>
#include <WiFiUdp.h>

#include <WebSocketsServer.h>
#include <ArduinoOTA.h>
#include "NidayandHelper.h"
#include "ButtonsHelper.h"
#include "index_html.h"
#include <string>

//----------------------------------------------------

// Version number for checking if there are new code releases and notifying the user
String version = "1.4.2";

NidayandHelper helper = NidayandHelper();
ButtonsHelper buttonsHelper = ButtonsHelper();

//Fixed settings for WIFI
WiFiClient espClient;
PubSubClient mqttClient(espClient);   //MQTT client
String mqttServer;           //WIFI config: MQTT server config (optional)
int mqttPort = 1883;         //WIFI config: MQTT port config (optional)
String mqttUser;             //WIFI config: MQTT server username (optional)
String mqttPwd;             //WIFI config: MQTT server password (optional)

String outputTopic;               //MQTT topic for sending messages
String inputTopic1;                //MQTT topic for listening
String inputTopic2;
String inputTopic3;
boolean isMqttEnabled = true;
unsigned long mqttLastConnectAttempt = 0;
String deviceHostname;             //WIFI config: Bonjour name of device
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
boolean saveItNow = false;          //If true will store positions to filesystem
boolean initLoop = true;            //To enable actions first time the loop is run

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

  return true;
}

/**
   Save configuration data to a JSON file
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

  if (isMqttEnabled && mqttClient.connected()) {
    helper.mqtt_publish(mqttClient, topic, msg);

    helper.mqtt_publish(mqttClient, helper.mqtt_gettopic("out1"), String(pos1));
    helper.mqtt_publish(mqttClient, helper.mqtt_gettopic("out2"), String(pos2));
    helper.mqtt_publish(mqttClient, helper.mqtt_gettopic("out3"), String(pos3));
  }

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

void setupOTA() {
  // Authentication to avoid unauthorized updates
  //ArduinoOTA.setPassword(OTA_PWD);

  ArduinoOTA.setHostname(deviceHostname.c_str());

  ArduinoOTA.onStart([]() {
    Serial.println("Start OTA");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd OTA");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  
  ArduinoOTA.begin();
}

void onPressHandler(Button2& btn) {
    Serial.println("onPressHandler");
    if (btn == buttonsHelper.buttonUp) {
      Serial.println("Up button clicked");
      processMsg("auto", "0", 1, 0);
      processMsg("auto", "0", 2, 0);
      processMsg("auto", "0", 3, 0);
    } else if (btn == buttonsHelper.buttonDown) {
      Serial.println("Down button clicked");
      processMsg("auto", "100", 1, 0);
      processMsg("auto", "100", 2, 0);
      processMsg("auto", "100", 3, 0);
    }
}

void onReleaseHandler(Button2& btn) {
    Serial.print("onReleaseHandler. Button released after (ms): ");
    Serial.println(btn.wasPressedFor());
    if (btn.wasPressedFor() > LONG_PRESS_MS) {
      processMsg("manual", "STOP", 1, 0);
      processMsg("manual", "STOP", 2, 0);
      processMsg("manual", "STOP", 3, 0);
    }
}

void setup(void) {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("Starting now...");

//Load config upon start
  Serial.println("Initializing filesystem...");
#ifdef ESP32  
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }
#else //ESP8266
  if (!LittleFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }
#endif

  // Serial.println("Formatting spiffs now...");
  // SPIFFS.format();
  // Serial.println("done format.");

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
  WiFi.setHostname(deviceHostname.c_str());
#else //ESP8266
  WiFi.hostname(deviceHostname);
#endif

  //Define customer parameters for WIFI Manager
  Serial.println("Configuring WiFi-manager parameters...");

  buttonsHelper.useButtons = WiFiSettings.checkbox("Use buttons Up / Down", true);
  buttonsHelper.pinButtonUp = WiFiSettings.integer("'Up' button pin", 0, 32, 22);
  buttonsHelper.pinButtonDown = WiFiSettings.integer("'Down' button pin", 0, 32, 23);

  deviceHostname = WiFiSettings.string("Name", 40, "");
  mqttServer = WiFiSettings.string("MQTT server", 40);
  mqttPort = WiFiSettings.integer("MQTT port", 0, 65535, 1883);
  mqttUser = WiFiSettings.string("MQTT username", 40);
  mqttPwd = WiFiSettings.string("MQTT password", 40);

  //reset settings - for testing
  //clean FS, for testing
  // helper.resetsettings();

  WiFiSettings.onPortal = []() {
      setupOTA();
  };
  WiFiSettings.onPortalWaitLoop = []() {
        ArduinoOTA.handle();
  };
  WiFiSettings.onSuccess = []() {
      buttonsHelper.setupButtons();

      buttonsHelper.buttonUp.setPressedHandler(onPressHandler);
      buttonsHelper.buttonDown.setPressedHandler(onPressHandler);
      buttonsHelper.buttonUp.setReleasedHandler(onReleaseHandler);
      buttonsHelper.buttonDown.setReleasedHandler(onReleaseHandler);
  };

  WiFiSettings.connect();

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
  // if (MDNS.begin(deviceHostname)) {
  //   Serial.println("MDNS responder started");
  //   MDNS.addService("http", "tcp", 80);
  //   MDNS.addService("ws", "tcp", 81);

  // } else {
  //   Serial.println("Error setting up MDNS responder!");
  //   while(1) {
  //     delay(1000);
  //   }
  // }
  Serial.print("Connect to http://"+String(deviceHostname)+".local or http://");
  Serial.println(WiFi.localIP());

  //Start HTTP server
  server.on("/", handleRoot);
  server.on("/reset", [&]{ helper.resetsettings(); });
  server.onNotFound(handleNotFound);
  server.begin();

  //Start websocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  /* Setup connection for MQTT and for subscribed
    messages IF a server address has been entered
  */
  if (String(mqttServer) != ""){
    Serial.println("Registering MQTT server");
    mqttClient.setServer(mqttServer.c_str(), mqttPort);
    mqttClient.setCallback(mqttCallback);
  } else {
    isMqttEnabled = false;
    Serial.println("NOTE: No MQTT server address has been registered. Only using websockets");
  }

  //Update webpage
  INDEX_HTML.replace("{VERSION}","V"+version);
  INDEX_HTML.replace("{NAME}",String(deviceHostname));

  setupOTA();  

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

  if (buttonsHelper.useButtons) {
      buttonsHelper.processButtons();
  }

  /**
    Serving the webpage
  */
  server.handleClient();

  //MQTT client
  if (isMqttEnabled) {
    if (mqttClient.connected()) {
      mqttClient.loop();
    } else {
      unsigned long now = millis();
      if (now - mqttLastConnectAttempt > 60000) { //once per 60 sec
          mqttLastConnectAttempt = now;
          // Attempt to reconnect
          if (helper.mqtt_reconnect(mqttClient, mqttUser, mqttPwd, { inputTopic1.c_str(), inputTopic2.c_str(), inputTopic3.c_str() })) {
            mqttLastConnectAttempt = 0;
          }
      }
    }
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
    if (now - lastPublish > 3000) {
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