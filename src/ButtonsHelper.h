#ifndef ESP32_MOTORIZED_ROLLER_BLINDS_BUTTONSHELPER_H
#define ESP32_MOTORIZED_ROLLER_BLINDS_BUTTONSHELPER_H

#include "Button2.h"

class ButtonsHelper {
public:
    Button2 buttonUp, buttonDown;
    bool useButtons;
    uint8_t pinButtonUp;
    uint8_t pinButtonDown;

    void setupButtons();

    void processButtons();
};


#endif //ESP32_MOTORIZED_ROLLER_BLINDS_BUTTONSHELPER_H
