#pragma once

#include <Arduino.h>

void initOTA(const String& hostname, const String& otaPassword);
void processOTALoop();