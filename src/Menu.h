#pragma once
#include <Arduino.h>

// ============================================================
//  Menu.h  —  ISI MENU, PRESET MODE, & PENGATURAN
//  Kalau mau ubah teks menu atau nilai preset tiap mode,
//  di file Menu.cpp inilah tempatnya.
// ============================================================

// Tabel menu (didefinisikan di Menu.cpp, dipakai juga oleh Display & Debug)
#define MENU_COUNT 3
extern const char*   menuTitle[MENU_COUNT];
extern const uint8_t menuLen[MENU_COUNT];
extern const char*   menuItems[MENU_COUNT][4];

// Terapkan preset parameter sesuai mode aktif (activeMenu/activeItem)
void applyModePreset();

// Ubah nilai di menu Pengaturan (dir = -1 / +1)
void adjustSetting(int dir);

// Langsung masuk mode jalan default (dipanggil saat stik baru konek)
void startDefaultMode();
