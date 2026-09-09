#include "Menu.h"
#include "Config.h"
#include "State.h"
#include "Motor.h"

// ============================================================
//  ISI MENU  (ubah teks di sini)
// ============================================================
const char* menuTitle[MENU_COUNT] = { "POSISI START", "MODE MAIN", "PENGATURAN" };
const uint8_t menuLen[MENU_COUNT]  = { 2, 4, 4 };
const char* menuItems[MENU_COUNT][4] = {
  { "Hadap Gawang", "Hadap Bola", "", "" },
  { "Normal", "Dribble", "Attack", "Defense" },
  { "Kecepatan Max", "Sensitiv Belok", "Kehalusan", "Balik Arah" }
};

// ============================================================
//  PRESET MODE  (ubah nilai tiap mode di sini)
// ============================================================
void applyModePreset()
{
  kickArmed = false;
  if (activeMenu == 0) {            // POSISI START
    g_maxSpeed = 200; g_rampStep = 6; g_steerGain = 100;
    // Hadap Gawang / Hadap Bola: parameter sama, kick siap saat "Hadap Bola"
    if (activeItem == 1) kickArmed = true;
  } else {                          // MODE MAIN
    switch (activeItem) {
      case 0: g_maxSpeed = 255; g_rampStep = 6; g_steerGain = 100; break;                  // Normal
      case 1: g_maxSpeed = 120; g_rampStep = 4; g_steerGain = 70;  break;                  // Dribble
      case 2: g_maxSpeed = 255; g_rampStep = 8; g_steerGain = 100; kickArmed = true; break;// Attack
      case 3: g_maxSpeed = 180; g_rampStep = 7; g_steerGain = 90;  break;                  // Defense
    }
  }
}

// ============================================================
//  ADJUST PENGATURAN (menu 3) — batas nilai diatur di sini
// ============================================================
void adjustSetting(int dir)
{
  switch (menuItem) {
    case 0: g_maxSpeed  = constrain(g_maxSpeed  + dir*15, 100, 255); break;
    case 1: g_steerGain = constrain(g_steerGain + dir*10, 40, 120);  break;
    case 2: g_rampStep  = constrain(g_rampStep  + dir*1,  2,  12);   break;
    case 3: g_invert    = !g_invert;                                 break;
  }
}

// ============================================================
//  MASUK MODE JALAN OTOMATIS (saat stik baru konek)
// ============================================================
void startDefaultMode()
{
  activeMenu = DEFAULT_MENU;
  activeItem = DEFAULT_ITEM;
  hasActiveMode = true;
  applyModePreset();

  // mulai dari kondisi bersih
  seqState = SEQ_IDLE;
  seqType  = SQ_NONE;
  curLeft = curRight = 0;
  setMotorL(0);
  setMotorR(0);

  sysState = ST_RUN;
  oledDirty = true;
  Serial.printf(">> Stik konek -> langsung RUN (mode: %s)\n",
                menuItems[activeMenu][activeItem]);
}
