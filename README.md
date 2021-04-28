3d parts for printing are available id `3d_parts` folder. Available from Thingiverse also: https://www.thingiverse.com/thing:4093205/

# Fork changes
- project ported to Platformio
- changed 3d parts according to small tube ~16mm
- added functionality of 3 step motors
- add current state broadcasting during blinds moving
- add wipe setting function
- 2-3 pins for steppers are not reversed now. Please check that initilization of steppers is ok for you (near 86 line: Stepper1(D1, D2, D3, D4))

# Latest changes:
## 1.4.2 (29 April 2021)
  - Added support for Up / Down mechanical buttons (long press to tune position). Default pins are 22/23 (initialized with INPUT_PULLUP and activated with GND).  You can disable that feature with "USE_BUTTONS" macro.

## 1.4.1 (14 March 2021)
  - improve MQTT connection reliability. While MQTT hub is unavailable, controller will try to reconnect once per 60 seconds. In this time web interface will be still available.
  - fix update notifications and links.

## 1.4.0 (15 September 2020)
 - Support of ESP32 (ESP8266 should work too). It makes possible to easily connect up to 3 steppers with 4-pins connectors.
 - Switched to "CheapStepper" library. It allows to configure rotation speed (grep for "setRpm(30)" lines - maybe you'll need to adapt it for your motors).
 - Code was adapted for asynchronous control of steppers. Your steppers can work in parallel without loosing speed now.
 - Switched to another Wi-Fi library. In general it works like previous "WiFi manager", but supports ESP32 also.
 - Added "STOP" command for OpenHab compatibility. See OpenHab config example below.
 - HomeAssistant support. There are separate MQTT topics for blinds position now (out1, out2, out3). See HomeAssistant config example below.
 
## Non-backward compatible changes:
 - mDNS is not supported now, because used library is not compatible with ESP32. 


# Features
 1. A tiny webserver is setup on the esp8266 that will serve one page to the client
 2. Upon powering on the first time WIFI credentials, a hostname and - optional - MQTT server details is to be configured. You can specify if you want **clockwise (CW) rotation** to close the blind and you can also specify **MQTT authentication** if required. Connect your computer to a new WIFI hotspot named **esp-xxxxx**.
 3. Connect to your normal WIFI with your client and go to the IP address of the device. If you don't know the IP-address of the device check your router for the leases (or check the serial console in the Arduino IDE or check the `/raw/esp8266/register` MQTT message if you are using an MQTT server)
 4. As the webpage is loaded it will connect through a websocket directly to the device to progress updates and to control the device. If any other client connects the updates will be in sync.
 5. Go to the Settings page to calibrate the motors with the start and end positions of the roller blind. Follow the instructions on the page

# MQTT
- When it connects to WIFI and MQTT it will send a "register" message to topic `/raw/esp8266/register` with a payload containing chip-id and IP-address
- A message to `/raw/esp8266/[chip-id]/in[1-3]` will steer the selected blind according to the "payload actions" below
- Updates from the device will be sent to topic `/raw/esp8266/[chip-id]/out` as JSON (there are "out1", "out2", "out3" topics to get separate position of specific blinds)

### If you don't want to use MQTT
Simply do not enter any string in the MQTT server form field upon WIFI configuration of the device (step 3 above)

## OpenHab config example
You can define thing in this way (update "2955439908" with your chip id):
```
Thing mqtt:topic:livingroom:rollerblinds "Rollerblinds" (mqtt:broker:mosquitto) @ "Livingroom"
{
    Channels:
        Type rollershutter : rollerblinds1      "Rollerblinds 1"        [ stateTopic="/raw/esp8266/2955439908/out", transformationPattern="JSONPATH:$.position1", commandTopic="/raw/esp8266/2955439908/in1", formatBeforePublish="%d"]
        Type rollershutter : rollerblinds2      "Rollerblinds 2"        [ stateTopic="/raw/esp8266/2955439908/out", transformationPattern="JSONPATH:$.position2", commandTopic="/raw/esp8266/2955439908/in2", formatBeforePublish="%d"]
        Type rollershutter : rollerblinds3      "Rollerblinds 3"        [ stateTopic="/raw/esp8266/2955439908/out", transformationPattern="JSONPATH:$.position3", commandTopic="/raw/esp8266/2955439908/in3", formatBeforePublish="%d"]
}
```

## HomeAssistant config example
Most probably I'll add auto-discovery later. Right now you can add blinds in this way (update "2955439908" with your chip id):
```
cover:
  - platform: mqtt
    name: "Rollerblind 1"
    command_topic: "/raw/esp8266/2955439908/in1"
    set_position_topic: "/raw/esp8266/2955439908/in1"
    position_topic: "/raw/esp8266/2955439908/out1"
    payload_open: "0"
    payload_close: "100"
    payload_stop: "STOP"
    position_open: 0
    position_closed: 100
    value_template: "{{ value | int }}"
    optimistic: false
```


## Payload options
- `update` - send broadcast message about current state via MQTT and websockets
- `0-100` - (auto mode) A number between 0-100 to set % of opened blind. Requires calibration before use. E.g. `50` will open it to 50%

# Screenshots

## Control
![Control](https://user-images.githubusercontent.com/25607714/65042413-7933be00-d961-11e9-915c-1af87957b788.jpg)

## Calibrate
![Settings](https://user-images.githubusercontent.com/25607714/65042412-7933be00-d961-11e9-898c-ac620290863a.jpg)

## Communication settings
 ![WIFI Manager](https://user-images.githubusercontent.com/2181965/37288794-75244c84-2608-11e8-8c27-a17e1e854761.jpg)

## Sidebar
 ![Sidebar](https://user-images.githubusercontent.com/25607714/65042415-79cc5480-d961-11e9-9ee1-4ca9edb15909.jpg)
