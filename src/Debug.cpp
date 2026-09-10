// By: github.com/dimasragaa — IG: @dmsragaa
#include "Debug.h"
#include <Ps3Controller.h>
#include "Config.h"
#include "State.h"
#include "Menu.h"
#include "Controls.h"   // triggerSpeed()

static void printSerial()
{
  if (!ps3Linked()) { Serial.println("PS3: DISCONNECTED"); return; }

  const char* stName = (sysState==ST_IDLE) ? "IDLE" :
                       (sysState==ST_MENU) ? "MENU" : "RUN";

  if (sysState == ST_MENU) {
    // PENTING: menuItems[][] cuma berisi 4 slot per halaman, dan itu
    // hanya dipakai halaman MODE MAIN (Soccer/Sumo). Halaman PENGATURAN
    // punya baris SENDIRI di settingsList[] (sekarang 8 baris) — kalau
    // menuItems[menuPage][menuItem] tetap dipanggil apa adanya begitu
    // menuItem >= 4, itu membaca alamat di luar array dan crash saat
    // dicetak (persis backtrace strlen() yang barusan terjadi).
    if (menuPage == MENU_PAGE_PENGATURAN) {
      SettingItem &s = settingsList[menuItem];
      Serial.printf("[%s] pengaturan %s > %s\n",
        stName, menuItems[MENU_PAGE_MODE][activeItem], s.label);
    } else {
      Serial.printf("[%s] menu=%s > %s | set: max=%d steer=%d%% ramp=%d inv=%s\n",
        stName, menuTitle[menuPage], menuItems[menuPage][menuItem],
        g_maxSpeed, g_steerGain, g_rampStep, g_invert ? "ON" : "OFF");
    }
  }
  else if (sysState == ST_RUN) {
    int kec = triggerSpeed();
    Serial.printf("[RUN] mode=%s | rx=%4d ly=%4d | kec=%3d | L=%4d R=%4d | max=%d steer=%d%% ramp=%d inv=%s kick=%s\n",
      menuItems[activeMenu][activeItem],
      Ps3.data.analog.stick.rx, Ps3.data.analog.stick.ly,
      kec, curLeft, curRight,
      g_maxSpeed, g_steerGain, g_rampStep,
      g_invert ? "ON" : "off", kickArmed ? "ARM" : "off");
  }
  else { // IDLE
    Serial.println("[IDLE] tekan SELECT untuk buka menu (stik nonaktif)");
  }
}

void debugTick(unsigned long now)
{
  if (now - lastPrint >= PRINT_MS) { lastPrint = now; printSerial(); }
}
