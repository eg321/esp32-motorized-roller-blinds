#define LONG_PRESS_MS 1000      // How much ms you should hold button to switch to "long press" mode (tune blinds position)
#define MAX_STEPPERS_COUNT 4    // This limited by available pins usually
/*
Usually your home automation can extract needed data from single JSON with info about all steppers.
These separate updates for specific steppers will additionally stuck all steppers for some milliseconds.
Do you really want this?
*/
#define MQTT_UPDATES_PER_STEPPER false

#define NUMBER_OF_CLICKS_TO_RESTART 5 //How many clicks of Up/Down button needed to restart controller

#define WIFI_FAILURE_TIMEOUT 15 * 60 // How many seconds to wait for connection restoration before restart (affects captive portal too)

#include <CheapStepper.h>
//#include <ESP8266mDNS.h>
//#include <WiFiUdp.h>

#include <WebSocketsServer.h>
#include <ArduinoOTA.h>
#include <ArduinoJson.h>
#include <WiFiSettings.h>
#include "Helpers/ConfigHelper.h"
#include "Helpers/MqttHelper.h"
#include "Helpers/ButtonsHelper.h"
#include "Helpers/StepperHelper.h"
#include "index_html.h"
#include <string>

//----------------------------------------------------

// Version number for checking if there are new code releases and notifying the user
String version = "2.0.1";

ConfigHelper helper = ConfigHelper();
MqttHelper mqttHelper = MqttHelper();
ButtonsHelper buttonsHelper = ButtonsHelper();
StepperHelper stepperHelpers[MAX_STEPPERS_COUNT];

String deviceHostname;             //WIFI config: Bonjour name of device
int steppersRPM;                   //WIFI config
int state = 0;
long lastPublish = 0;

boolean loadDataSuccess = false;
boolean saveItNow = false;          //If true will store positions to filesystem
boolean initLoop = true;            //To enable actions first time the loop is run

#ifdef ESP32
WebServer server(80);              // TCP server at port 80 will respond to HTTP requests
#else
ESP8266WebServer server(80);              // TCP server at port 80 will respond to HTTP requests
#endif

WebSocketsServer webSocket = WebSocketsServer(81);  // WebSockets will respond on port 81

bool loadConfig() {
    if (!helper.loadconfig()) {
        return false;
    }
    JsonObject &root = helper.getconfig();

    root.printTo(Serial);
    Serial.println();

    //Store variables locally
    uint8_t num = 0;
    for (StepperHelper &stepperHelper : stepperHelpers) {
        num++;
        if (stepperHelper.isConnected()) {
            stepperHelper.currentPosition = root["currentPosition" + String(num)]; // 400
            stepperHelper.maxPosition = root["maxPosition"+ String(num)]; // 20000
        }
    }

    return true;
}

/**
   Save configuration data to a JSON file
*/
bool saveConfig() {
    const size_t capacity = JSON_OBJECT_SIZE(MAX_STEPPERS_COUNT * 2);
    DynamicJsonBuffer jsonBuffer(capacity);
    JsonObject &json = jsonBuffer.createObject();
    uint8_t num = 0;
    for (StepperHelper stepperHelper : stepperHelpers) {
        num++;
        if (stepperHelper.isConnected()) {
            json["currentPosition" + String(num)] = stepperHelper.currentPosition;
            json["maxPosition" + String(num)] = stepperHelper.maxPosition;
        }
    }

    return helper.saveconfig(json);
}

void sendUpdate() {
    uint8_t num = 0;
    DynamicJsonBuffer jsonBuffer(200);
    JsonObject& root = jsonBuffer.createObject();
    for (StepperHelper &stepperHelper : stepperHelpers) {
        num++;
        if (stepperHelper.isConnected()) {
            if (stepperHelper.maxPosition != 0) {
                stepperHelper.set = (stepperHelper.targetPosition * 100) / stepperHelper.maxPosition;
                stepperHelper.pos = (stepperHelper.currentPosition * 100) / stepperHelper.maxPosition;
            }
            root["set" + String(num)] = stepperHelper.set;
            root["position" + String(num)] = stepperHelper.pos;

#if MQTT_UPDATES_PER_STEPPER
            if (mqttHelper.isMqttEnabled && mqttHelper.getClient().connected()) {
                mqttHelper.publishMsg(mqttHelper.getTopicPath("out" + String(num)),
                                    String(stepperHelper.pos));
            }
#endif
        }
    }

    char jsonOutput[128]; //@TODO: Calculate capacity for JSON
    root.printTo(jsonOutput);
    Serial.println("Broadcasting JSON:" + String(jsonOutput));

    if (mqttHelper.isMqttEnabled && mqttHelper.getClient().connected()) {
        mqttHelper.publishMsg(mqttHelper.outputTopic, jsonOutput);
    }

    webSocket.broadcastTXT(jsonOutput);
}

/****************************************************************************************
*/
void processCommand(const String& command, const String& value, int stepperNum, uint8_t clientId) {
    if (command == "update") { //Send position details to the newly connected client
        sendUpdate();
        return;
    } else if (command == "ping") {
        //Do nothing
        return;
    };

    if (!stepperHelpers[stepperNum - 1].isConnected()) {
        Serial.printf("Stepper %i is not connected. Ignoring command '%s' from client %i...\r\n",
                      stepperNum,
                      command.c_str(),
                      clientId);
        Serial.println();
        return;
    }

    StepperHelper* stepperHelper = &stepperHelpers[stepperNum - 1];

    /*
       Below are actions based on inbound MQTT payload
    */
    if (command == "start") { // Store the current position as the start position
        stepperHelper->currentPosition = 0;
        stepperHelper->route = 0;
        stepperHelper->action = "manual";
        saveItNow = true;
    } else if (command == "max") { // Store the max position of a closed blind
        stepperHelper->maxPosition = stepperHelper->currentPosition;
        stepperHelper->route = 0;
        stepperHelper->action = "manual";
        saveItNow = true;
    } else if (command == "stop") { // STOP!
        stepperHelper->route = 0;
        stepperHelper->action = "";
        saveItNow = true;
    } else if (command == "manual" && value == "1") { // Move down without limit to max position
        stepperHelper->route = 1;
        stepperHelper->action = "manual";
    } else if (command == "manual" && value == "-1") { // Move up without limit to top position
        stepperHelper->route = -1;
        stepperHelper->action = "manual";
    } else {
        /*
           Any other message will take the blind to a position
           Incoming value = 0-100
        */
        Serial.println("New position command: " + value + "%");

        stepperHelper->targetPosition = stepperHelper->maxPosition * value.toInt() / 100;
        stepperHelper->route = stepperHelper->currentPosition < stepperHelper->targetPosition ? 1 : -1;
        stepperHelper->action = "auto";

        stepperHelper->set = value.toInt();
        stepperHelper->pos = (stepperHelper->currentPosition * 100) / stepperHelper->maxPosition;

        Serial.printf("Starting movement of Stepper %i (current position = %i, targetPosition = %i)...\r\n",
                      stepperNum,
                      stepperHelper->currentPosition,
                      stepperHelper->targetPosition);
        stepperHelper->getStepper()->newMove(stepperHelper->route == 1,
                                            abs(stepperHelper->currentPosition - stepperHelper->targetPosition));

        //Send the instruction to all connected devices
        sendUpdate();
    }
}

void processRequest(String &payload, uint8_t client_id = 0) {
    StaticJsonBuffer<100> jsonBuffer; //@TODO: Check capacity for JSON object
    JsonObject &root = jsonBuffer.parseObject(payload);
    if (root.success()) {
        const int stepperNum = root["num"];
        String command = root["action"];
        String value = root["value"];
        //Send to common MQTT and websocket function
        processCommand(command, value, stepperNum, client_id);
    } else {
        Serial.println("parseObject() failed");
    }
}

void webSocketEvent(uint8_t clientId, WStype_t type, uint8_t *payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            Serial.printf("[client %u] connected...\r\n", clientId);
            break;
        case WStype_DISCONNECTED:
            Serial.printf("[client %u] disconnected.\r\n", clientId);
            break;
        case WStype_TEXT:
            Serial.printf("[client %u] payload: %s\r\n", clientId, payload);
            String request = (char *) payload;
            processRequest(request, clientId);
            break;
    }
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
    Serial.print("MQTT message received [");
    Serial.print(topic);
    Serial.print("] ");
    String jsonRequest = "";
    for (auto i = 0; i < length; i++) {
        jsonRequest += String((char) payload[i]);
    }
    processRequest(jsonRequest);
}

void handleRoot() {
    server.send(200, "text/html", INDEX_HTML);
}

void handleNotFound() {
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";
    for (uint8_t i = 0; i < server.args(); i++) {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(404, "text/plain", message);
}

void setupOTA() {
    // Authentication to avoid unauthorized updates
    //ArduinoOTA.setPassword(OTA_PWD); //@TODO: make it configurable

    ArduinoOTA.setHostname(deviceHostname.c_str());

    ArduinoOTA.onStart([]() {
        Serial.println("Start OTA");
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\nEnd OTA");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("OTA Progress: %u%%\r\n", (progress / (total / 100)));
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

void restartDevice() {
    Serial.println("Restarting...");
    delay(500);

#ifdef ESP8266
    ESP.reset();
#else   //ESP32
    ESP.restart();
#endif
}

void onPressHandler(Button2 &btn) {
    Serial.println("onPressHandler");
    String newValue;
    boolean isRestartRequested = false;

    if (btn == buttonsHelper.buttonUp) {
        Serial.println("Up button clicked");
        newValue = "0";
    } else if (btn == buttonsHelper.buttonDown) {
        Serial.println("Down button clicked");
        newValue = "100";
    }

    if (btn.getNumberOfClicks() == NUMBER_OF_CLICKS_TO_RESTART) {
        Serial.print(btn.getNumberOfClicks());
        Serial.println(" times clicked.");
        isRestartRequested = true;
    }

    uint8_t num = 0;
    for (StepperHelper &stepperHelper : stepperHelpers) {
        num++;
        if (stepperHelper.isConnected()) {
            if (isRestartRequested || (stepperHelper.route == -1 && newValue == "0") || (stepperHelper.route == 1 && newValue == "100")) {
                // Another click to the same direction will cause stopping
                processCommand("stop", "", num, BUTTONS_CLIENT_ID);
            } else {
                processCommand("auto", newValue, num, BUTTONS_CLIENT_ID);
            }
        }
    }

    if (isRestartRequested) {
        restartDevice();
    }
}

void onReleaseHandler(Button2 &btn) {
    Serial.print("onReleaseHandler. Button released after (ms): ");
    Serial.println(btn.wasPressedFor());
    if (btn.wasPressedFor() > LONG_PRESS_MS) {
        uint8_t num = 0;
        for (StepperHelper &stepperHelper : stepperHelpers) {
            num++;
            if (stepperHelper.isConnected()) {
                processCommand("stop", "", num, BUTTONS_CLIENT_ID);
            }
        }
    }
}

void checkWiFiConnection() {
    static unsigned long lastCheckTime = 0;
    static unsigned int failureTime = 0;
    unsigned long now = millis();

    if (WiFi.status() != WL_CONNECTED && now > lastCheckTime + (30 * 1000)) {
        if (failureTime == 0) {
            Serial.println("WiFi connection failure. Starting timer to restart device...");
            failureTime = now;
        }

        Serial.printf("%i seconds until restart.\n", abs((int)(now - failureTime - WIFI_FAILURE_TIMEOUT * 1000) / 1000));
        if (now > failureTime + WIFI_FAILURE_TIMEOUT * 1000) {
            Serial.println("Restarting device...");
            restartDevice();
        }

        lastCheckTime = now;
    } else if (failureTime != 0 && WiFi.status() == WL_CONNECTED) {
        Serial.println("WiFi connection is restored.");
        failureTime = 0;
    }
}

void setup(void) {
    Serial.begin(115200);
    delay(100);

    Serial.println("Starting now...");

//Load config upon start
    Serial.println("Initializing filesystem...");
#ifdef ESP32
    if (!SPIFFS.begin(true)) {
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
    for (StepperHelper &stepperHelper : stepperHelpers) {
        if (stepperHelper.isConnected()) {
            stepperHelper.action = "";
        }
    }

    //Set the WIFI hostname
#ifdef ESP32
    WiFi.setHostname(deviceHostname.c_str());
#else //ESP8266
    WiFi.hostname(deviceHostname);
#endif

    //Define customer parameters for WIFI Manager
    Serial.println("Configuring WiFi-manager parameters...");

    buttonsHelper.useButtons = WiFiSettings.checkbox("Enable buttons Up / Down", true);
    buttonsHelper.pinButtonUp = WiFiSettings.integer("'Up' button pin", 0, 32, 22);
    buttonsHelper.pinButtonDown = WiFiSettings.integer("'Down' button pin", 0, 32, 23);

    steppersRPM = WiFiSettings.integer("Steppers speed (RPM)", 30);
    for (int i = 0; i < MAX_STEPPERS_COUNT; i++) {
        stepperHelpers[i].pinsStr = WiFiSettings.string("Stepper " + String(i + 1) + " pins (if connected)");
    }

    deviceHostname = WiFiSettings.string("Name", 40, "");
    mqttHelper.mqttServer = WiFiSettings.string("MQTT server", 40);
    mqttHelper.mqttPort = WiFiSettings.integer("MQTT port", 0, 65535, 1883);
    mqttHelper.mqttUser = WiFiSettings.string("MQTT username", 40);
    mqttHelper.mqttPwd = WiFiSettings.string("MQTT password", 40);

    //reset settings - for testing
    //clean FS, for testing
    // helper.resetsettings();

    WiFiSettings.onPortal = []() {
        setupOTA();
    };
    WiFiSettings.onPortalWaitLoop = []() {
        ArduinoOTA.handle();
        checkWiFiConnection();
    };
    WiFiSettings.onSuccess = []() {
        buttonsHelper.setupButtons();

        buttonsHelper.buttonUp.setPressedHandler(onPressHandler);
        buttonsHelper.buttonDown.setPressedHandler(onPressHandler);
        buttonsHelper.buttonUp.setReleasedHandler(onReleaseHandler);
        buttonsHelper.buttonDown.setReleasedHandler(onReleaseHandler);

        mqttHelper.loop();
    };
    WiFiSettings.onFailure = []() {
        Serial.println(F("Failure occurred during WiFi connection."));
    };

    WiFiSettings.connect();

    /*
       Try to load FS data configuration every time when
       booting up. If loading does not work, set the default
       positions
    */
    Serial.println("Trying to load config...");
    loadDataSuccess = loadConfig();
    if (loadDataSuccess) {
        Serial.println("Config is loaded.");
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
    Serial.print("Connect to http://");
    Serial.println(WiFi.localIP());

    //Start HTTP server
    server.on("/", handleRoot);
    server.on("/reset", [&] { helper.resetsettings(); });
    server.onNotFound(handleNotFound);
    server.begin();

    //Start websocket
    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    setupOTA();

    uint8_t num = 0;
    String connectedSteppers;
    for (StepperHelper &stepperHelper : stepperHelpers) {
        num++;
        if (stepperHelper.isConnected()) {
            connectedSteppers += String(num) + ',';
            Serial.printf("Stepper %i is connected. Setting RPM to %i...\r\n", num, steppersRPM);
            stepperHelper.getStepper()->setRpm(steppersRPM);
        }
    }

    mqttHelper.setup(mqttCallback);

    //Update webpage
    INDEX_HTML.replace("{VERSION}", "v" + version);
    INDEX_HTML.replace("{NAME}", String(deviceHostname));
    INDEX_HTML.replace("{CONNECTED_STEPPERS}", connectedSteppers);

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
    for (StepperHelper stepperHelper : stepperHelpers) {
        if (stepperHelper.isConnected()) {
            stepperHelper.disablePowerToCoils();
        }
    }
}

void loop(void) {
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

    mqttHelper.loop();

    checkWiFiConnection();

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
    uint8_t num = 0;
    bool isTimeToSendUpdate = false;
    for (StepperHelper &stepperHelper : stepperHelpers) {
        num++;
        if (!stepperHelper.isConnected()) {
            continue;
        }
        if (stepperHelper.action == "auto") {
            stepperHelper.getStepper()->run();
            stepperHelper.currentPosition = stepperHelper.targetPosition - stepperHelper.getStepper()->getStepsLeft();
            if (stepperHelper.currentPosition == stepperHelper.targetPosition) {
                stepperHelper.route = 0;
                stepperHelper.action = "";
                stepperHelper.set = (stepperHelper.targetPosition * 100) / stepperHelper.maxPosition;
                stepperHelper.pos = (stepperHelper.currentPosition * 100) / stepperHelper.maxPosition;
                sendUpdate();
                Serial.printf("Stepper %i has reached target position.\r\n", num);
                saveItNow = true;
                isTimeToSendUpdate = true;
            }
        } else if (stepperHelper.action == "manual" && stepperHelper.route != 0) {
            stepperHelper.getStepper()->move(stepperHelper.route > 0, abs(stepperHelper.route));
            stepperHelper.currentPosition += stepperHelper.route;
        }

        if (stepperHelper.action != "") {
            isTimeToSendUpdate = true;
        }
    }

    if (isTimeToSendUpdate) {
        long now = millis();
        if (now - lastPublish > 3000) { // Update state
            lastPublish = now;
            sendUpdate();
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