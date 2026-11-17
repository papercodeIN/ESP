/**
 * @file pio_entry.cpp
 * @brief Arduino entry point; all logic lives in app.cpp.
 */
#include <Arduino.h>

#include "app.h"

void setup()
{
    Serial.begin(115200);
    delay(300);
    Serial.println();
    app::start();
}

void loop()
{
    app::tick();
}
