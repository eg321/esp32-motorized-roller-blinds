#ifndef ESP32_MOTORIZED_ROLLER_BLINDS_STEPPERHELPER_H
#define ESP32_MOTORIZED_ROLLER_BLINDS_STEPPERHELPER_H

#include <CheapStepper.h>

class StepperHelper {
private:
    CheapStepper* stepper = nullptr; // will be initialized dynamically (if connected only)

public:
    String pinsStr;
    String action; //manual / auto / ""
    int set;
    int pos;
    int route = 0; //Direction of blind (1 = down, 0 = stop, -1 = up)
    int currentPosition = 0;
    int targetPosition = 0; //0-100% (set by client)
    int maxPosition = 100000;

    CheapStepper* getStepper();

    bool isConnected();

    int *parsePins();

    void disablePowerToCoils();
};


#endif //ESP32_MOTORIZED_ROLLER_BLINDS_STEPPERHELPER_H
