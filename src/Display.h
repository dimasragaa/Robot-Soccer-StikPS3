#pragma once
#include <Arduino.h>

// ============================================================
//  Display.h  —  TAMPILAN OLED
//  Kalau USE_OLED 0 di Config.h, fungsi-fungsi ini jadi kosong
//  otomatis (tidak error walau OLED tidak dipasang).
//
//  By: github.com/dimasragaa — IG: @dmsragaa
// ============================================================

// Init OLED + jalankan task penggambar. Panggil sekali di setup().
// Setelah ini OLED mengurus dirinya sendiri di task terpisah —
// loop() tidak perlu (dan tidak boleh) ikut menggambar lagi.
void displaySetup();
