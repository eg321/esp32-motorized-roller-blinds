#include "StepperHelper.h"

CheapStepper* StepperHelper::getStepper() {
    if (stepper == nullptr) {
        int* pins;
        pins = parsePins();
        Serial.printf("Initializing Stepper using pins: %i %i %i %i ...\r\n", pins[0], pins[1], pins[2], pins[3]);
        stepper = new CheapStepper(pins[0], pins[1], pins[2], pins[3]);
    }

    return stepper;
}

bool StepperHelper::isConnected() {
    return pinsStr.length() > 0;
}

int* StepperHelper::parsePins() {
    static int pins[4];
    sscanf(pinsStr.c_str(), "%d,%d,%d,%d", &pins[0], &pins[1], &pins[2], &pins[3]);
    Serial.printf("Parsed pins for Stepper: %i %i %i %i\r\n", pins[0], pins[1], pins[2], pins[3]);

    return pins;
};

void StepperHelper::disablePowerToCoils() {
    auto stepper = getStepper();

    digitalWrite(stepper->getPin(0), LOW);
    digitalWrite(stepper->getPin(1), LOW);
    digitalWrite(stepper->getPin(2), LOW);
    digitalWrite(stepper->getPin(3), LOW);
}