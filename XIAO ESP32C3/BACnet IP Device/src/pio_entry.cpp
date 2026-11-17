/**
 * @file pio_entry.cpp
 * @brief PlatformIO firmware entry points.
 */
#include <Arduino.h>

#include "app.h"

void setup()
{
    appSetup();
}

void loop()
{
    appLoop();
}
