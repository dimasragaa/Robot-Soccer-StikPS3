#pragma once
#include <Arduino.h>

// ============================================================
//  Kicker.h  —  MEKANISME TENDANG (solenoid, pulsa non-blocking)
// ============================================================

void kickerSetup();            // siapkan pin, panggil di setup()
void handleKick(bool trigger); // trigger=true -> mulai pulsa tendang
