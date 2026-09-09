#pragma once
#include <Arduino.h>

// ============================================================
//  Display.h  —  TAMPILAN OLED
//  Kalau USE_OLED 0 di Config.h, fungsi-fungsi ini jadi kosong
//  otomatis (tidak error walau OLED tidak dipasang).
// ============================================================

void displaySetup();               // init OLED, panggil di setup()
void displayTick(unsigned long now); // render bila perlu (throttle internal)
