#include "ButtonsHelper.h"

void ButtonsHelper::setupButtons() {
    if (!useButtons) {
        return;
    }

    buttonUp = Button2(pinButtonUp);
    buttonDown = Button2(pinButtonDown);
}

void ButtonsHelper::processButtons() {
    buttonUp.loop();
    buttonDown.loop();
}
