#pragma once

#include "AppConfig.h"

void initI2CBuses();
void scanI2CBuses();
void beginSensors();
void readSensors();
void evaluateAlerts();
float readAmbientLux();
