#include "Menu.h"
#include "Config.h"
#include "State.h"
#include "Motor.h"

// ============================================================
//  ISI MENU
//  Hanya MODE MAIN yang ditampilkan.
// ============================================================
const char* menuTitle[MENU_COUNT] = { "MODE MAIN" };
const uint8_t menuLen[MENU_COUNT] = { 2 };

const char* menuItems[MENU_COUNT][4] = {
  { "Soccer", "Sumo", "", "" }
};

// ============================================================
//  PRESET MODE
// ============================================================
void applyModePreset()
{
  kickArmed = false;

  switch (activeItem) {
    case 0:  // Soccer
      g_maxSpeed = 255;
      g_rampStep = 6;
      g_steerGain = 100;
      kickArmed = true;
      break;

    case 1:  // Sumo
      g_maxSpeed = 255;
      g_rampStep = 10;
      g_steerGain = 110;
      break;
  }
}

// ============================================================
//  MASUK MODE JALAN OTOMATIS
// ============================================================
void startDefaultMode()
{
  activeMenu = DEFAULT_MENU;
  activeItem = DEFAULT_ITEM;
  hasActiveMode = true;
  applyModePreset();

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
