#pragma once
#include <Arduino.h>

// ============================================================
//  Menu.h  —  ISI MENU & PRESET MODE
//
//  By: github.com/dimasragaa — IG: @dmsragaa
// ============================================================

#define MENU_COUNT 2
#define MENU_PAGE_MODE        0   // "MODE MAIN"  : pilih Soccer/Sumo
#define MENU_PAGE_PENGATURAN  1   // "PENGATURAN" : ubah nilai live pakai stik

extern const char*   menuTitle[MENU_COUNT];
extern const uint8_t menuLen[MENU_COUNT];
extern const char*   menuItems[MENU_COUNT][4];   // dipakai halaman MODE MAIN saja

void applyModePreset();
void startDefaultMode();

// ------------------------------------------------------------
//  PENGATURAN LIVE — satu baris = satu nilai yang bisa diubah dari stik
//  di layar PENGATURAN (tombol KIRI/KANAN mengubah item yang disorot).
//  Layar ini muncul OTOMATIS setelah pilih Soccer/Sumo di MODE MAIN,
//  sebelum benar-benar masuk RUN — lihat handleMenu() di Controls.cpp.
// ------------------------------------------------------------
struct SettingItem
{
  const char *label;
  const char *key;   // nama kunci di flash. Nanti ditambah nomor mode
                      // (mis. "spdR2" -> "spdR20"/"spdR21"). Maks 13 huruf,
                      // karena kunci NVS dibatasi 15 karakter.
  int        *val;   // pointer LANGSUNG ke variabel g_* asli — lihat
                      // catatan panjang soal ini di Menu.cpp
  int lo, hi, step;
  const char *unit;    // contoh: "%" ; kosongkan "" kalau tidak perlu satuan
  bool persist;        // true = simpan ke flash, bertahan lintas restart
};

#define SETTINGS_COUNT 6
extern SettingItem settingsList[SETTINGS_COUNT];

void settingsLoad();   // baca nilai tersimpan dari flash (NVS) — panggil di setup()
void settingsSave();   // simpan nilai sekarang ke flash — panggil tiap ada perubahan
