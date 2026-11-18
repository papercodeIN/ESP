/**
 * @file app.h
 * @brief Application entry points used by the PlatformIO firmware.
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

void appSetup();
void appLoop();
void appPrepareRestart();
const char *appResetReason();
uint32_t appBootCount();
uint32_t appBacnetRxPackets();
uint32_t appBacnetDroppedPackets();
uint32_t appBacnetTxErrors();
size_t appActiveCovSubscriptions();
