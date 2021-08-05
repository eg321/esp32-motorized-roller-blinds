*Russian version available [here](README_ru.md)*.

Hi there!

The project is dedictated to control of motorized blinds’ operation and its integration into home automation systems (f.ex. [Home Assistant](https://www.home-assistant.io/) and [OpenHab](https://www.openhab.org/)).

The project developing consists of 2 major areas:

- Software (firmware for boards based at ESP8266 / ESP32 modules)
- Hardware/Mechanical (3d printing models to upgrade your blinds into motorized).

A bit details below...

# ESP8266 / ESP32 firmware
I'm trying to keep compatibility with both platforms.
Main differences between them:
- There is fewer pins available at ESP8266. You can connect 2 steppers to single controller only (using 8 pins).  
- ESP32 can connect up to 4 steppers (or even more?) to single controller (using 16 pins).

If you would to connect 1 or 2 steppers only, use cheaper ESP8266 based controller (like Wemos D1 mini, NodeMCU, etc).

## Features
- Support for cheap 28BYJ-48 steppers (better to use 12 volt versions)
- Control of an unlimited number of steppers (limitation in 4 motors is the default for a more convenient UI). In fact, it's limited by hardware only (by the number of pins).
- Ability to set the rotation speed (12 volt motors can provide a higher speed)
- Ability to set all main settings through  Captive WiFi Portal (steppers used, pins, rotation speed, MQTT settings, etc.)
- Web interface for setting endpoints and controlling blinds (adapted for mobile devices also)
- Possibility of connecting an external mechanical switch (with the ability to stop the curtains in the desired position).
- MQTT support (both for controlling curtains and for setting end positions - can be complete replacement for Web UI)
- Easy integration with popular home automation systems like [Home Assistant](https://www.home-assistant.io/) or [OpenHab](https://www.openhab.org/) (via MQTT)
- Over the air (OTA) updates. No need to disconnect or disassemble the controller to update the firmware. The web interface notifies about the availability of new updates.
- Saving the position of each blind in the ROM (you do not need to re-calibrate or set the position after turning off the power)
- Control of all connected motors in parallel (asynchronous operation with steppers)
- Watchdog (automatic restart of the controller in case of freezing)
- Automatic MQTT re-connection in case of network problems
- Low power consumption when idle (stepper windings are switched-off when idle)
- DHCP over WiFi support

## Web-interface
Control blinds:

<img src="https://user-images.githubusercontent.com/10514429/127761354-48f777a2-bae6-4e8f-9864-e677d7ff6fbc.png" width=60%>

Set end-points:

<img src="https://user-images.githubusercontent.com/10514429/127761394-7f7a63c1-fb19-48bf-a2b4-b94497d89f7c.png" width=60%>

## Captive Portal
You'll be redirected to Captive portal after connecting to "esp-xxxx" WiFi network:

<img src="https://user-images.githubusercontent.com/10514429/127766193-c0b373a9-cd20-4383-8369-19cf8c513293.png" width=30%>

Fill pins for needed Steppers divided by comma and check pins for your mechanical buttons Up/Down (or disable it).

## MQTT
*Just do not setup MQTT details at Captive Portal, if you don't want to use MQTT.*

MQTT details:
- All controllers send a message to the common topic after successful connection: `ESP_Blinds/register` (in JSON with 2 fields: `chipId` and `ip`)
- Controller listening for commands at topic `ESP_Blinds/<chip_id>/in` (JSON)
- Controller send updates with state and positions to `ESP_Blinds/<chip_id>/out` (JSON)
- Separate topics for steppers (`/outN`) are disabled for MQTT messaging optimization, but you can enable it if needed for your automation flow.

## HomeAssistant integration
*Auto discovery is disabled now due testing issues. Will be enabled in future releases.*

<img src="https://user-images.githubusercontent.com/10514429/127761434-6b5441d2-5204-49c0-a82d-9eed0a084a33.png" width=60%>


Config example for 2 roller blinds (replace "_chip_Id_" with Chip id of your controller, watch it at registration topic - `/ESP_Blinds/register`):
```yaml
cover:
  - platform: mqtt
    name: "Blind 1"
    device_class: "blind"
    command_topic: "ESP_Blinds/_chip_Id_/in"
    set_position_topic: "ESP_Blinds/_chip_Id_/in"
    set_position_template: '{"num": 1, "action": "auto", "value": {{ 100 - position }} }'
    position_topic: "ESP_Blinds/_chip_Id_/out"
    position_template: '{{ value_json.position1 }}'
    payload_open: '{"num": 1, "action": "auto", "value": 0}'
    payload_close: '{"num": 1, "action": "auto", "value": 100}'
    payload_stop: '{"num": 1, "action": "stop", "value": 0}'
    position_open: 0
    position_closed: 100
    optimistic: false

  - platform: mqtt
    name: "Blind 2"
    device_class: "blind"
    command_topic: "ESP_Blinds/_chip_Id_/in"
    set_position_topic: "ESP_Blinds/_chip_Id_/in"
    set_position_template: '{"num": 2, "action": "auto", "value": {{ 100 - position }} }'
    position_topic: "ESP_Blinds/_chip_Id_/out"
    position_template: '{{ value_json.position2 }}'
    payload_open: '{"num": 2, "action": "auto", "value": 0}'
    payload_close: '{"num": 2, "action": "auto", "value": 100}'
    payload_stop: '{"num": 2, "action": "stop", "value": 0}'
    position_open: 0
    position_closed: 100
    optimistic: false
```
Use same approach for other your blinds.

# Mechanical part
The most popular option uses cheap 28BYJ-48 steppers, but in fact you can use any 4-pin stepper motors (dual winding steppers).
28BYJ-48 is most often used with the ULN2003 driver (better to use 12 volt version, it gives more torque).

I have developed 2 models for converting Leroy Merlin' roller blinds into motorized ones:
1. for the old models. It looks like they are almost not sold anymore.
2. for a new (will publish soon). The fixing holes match the original parts, so the curtains can be easily converted to motorized and vice versa.

3 printing models are available at `3d_parts` directory or at [Thingiverse](https://www.thingiverse.com/thing:4093205/) directly with some instructions.

# Future plans
* Complete implementation for  Home Assistant "Auto Discovery"
* Add support for other steppers and drivers (please propose at "issues")
* Add more configuration options at WebUI to allow re-configuration without "wipe settings"
* Prepare universal PCB
* Any ideas? Fill in the Issues.

# Project history
This project appeared as fork of @nidayand [repository](https://github.com/nidayand/motor-on-roller-blind-ws) originally.

Unfortunately, the project supported ESP8266 and 1 stepper only, plus it had not been updated for a long time. I added support for Platformio (for easier development), multiple steppers, and ESP32 later (so more than 2 steppers can be connected to single controller). It was published as v1.4.x.

Soon I wanted a more convenient motor control, a simple controller configuration without firmware re-build, but the original code was of little messy to maintain, although it worked.
As a result, a large refactoring of the firmware took place - new classes and areas of responsibility were allocated.

In fact, it became clear that almost nothing remained of the old project (only the Web UI almost unchanged). This project was detached and now developing on its own tree. 
