#pragma once
#include <Arduino.h>

// ============================================================
//  Menu.h  —  ISI MENU & PRESET MODE
//
//  By: github.com/dimasragaa — IG: @dmsragaa
// ============================================================

#define MENU_COUNT 1

extern const char*   menuTitle[MENU_COUNT];
extern const uint8_t menuLen[MENU_COUNT];
extern const char*   menuItems[MENU_COUNT][4];

void applyModePreset();
void startDefaultMode();
