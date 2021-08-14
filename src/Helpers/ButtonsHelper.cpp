#include "ButtonsHelper.h"

void ButtonsHelper::setupButtons() {
    if (!useButtons) {
        return;
    }

    buttonUp = Button2(pinButtonUp);
    buttonDown = Button2(pinButtonDown);

    buttonUp.setDoubleClickTime(500);
    buttonDown.setDoubleClickTime(500);
}

void ButtonsHelper::processButtons() {
    buttonUp.loop();
    buttonDown.loop();
}
