// By: github.com/dimasragaa — IG: @dmsragaa
#include "Menu.h"
#include "Config.h"
#include "State.h"
#include "Motor.h"
#include <Preferences.h>

// ============================================================
//  ISI MENU
// ============================================================
const char* menuTitle[MENU_COUNT] = { "MODE MAIN", "PENGATURAN" };
const uint8_t menuLen[MENU_COUNT] = { 2, SETTINGS_COUNT };

const char* menuItems[MENU_COUNT][4] = {
  { "Soccer", "Sumo", "", "" },
  { "", "", "", "" }   // halaman PENGATURAN pakai settingsList[] di bawah, bukan ini
};

// ============================================================
//  DAFTAR PENGATURAN LIVE — INTI dari "tanpa upload ulang"
//
//  `val` di bawah menunjuk LANGSUNG ke alamat variabel g_maxSpeed yang
//  sudah ada sejak awal di State.cpp. Variabel itu memang dibaca ULANG
//  tiap tick oleh driveManual() (Controls.cpp) untuk menghitung PWM motor.
//  Jadi begitu KIRI/KANAN mengubah isi *val di sini, tick BERIKUTNYA
//  (2 ms kemudian) driveManual() otomatis memakai nilai baru itu — tidak
//  ada "upload ulang" yang perlu terjadi sama sekali, karena tidak ada
//  kode yang berubah, cuma ISI SATU VARIABEL DI RAM yang berubah.
//
//  Bandingkan dengan angka #define di Config.h (mis. DEF_MAX_SPEED):
//  itu konstanta yang ditulis ke flash program SAAT COMPILE, jadi kalau
//  mau ganti angkanya ya wajib upload ulang. g_maxSpeed berbeda — dia
//  cuma nilai awalnya SAJA yang diambil dari Config.h; setelah itu dia
//  jadi variabel biasa yang bebas diubah kapan pun ESP32 menyala.
// ============================================================
SettingItem settingsList[SETTINGS_COUNT] = {
  { "Max Speed", &g_maxSpeed, SPEED_SET_MIN, SPEED_SET_MAX, SPEED_SET_STEP, "" },
};

// ============================================================
//  PENYIMPANAN NILAI KE FLASH (NVS)
//  Supaya setelan Max Speed tidak balik ke default tiap ESP32 restart —
//  termasuk restart OTOMATIS yang sekarang dipicu saat stik putus lama
//  (lihat linkWatchdog() di main.cpp). Tanpa ini, tiap restart begitu
//  akan diam-diam menghapus setelan yang baru saja diatur di lapangan.
// ============================================================
static Preferences prefs;

void settingsLoad()
{
  prefs.begin("robot", true);   // true = buka mode baca-saja
  g_maxSpeed = prefs.getInt("maxSpeed", DEF_MAX_SPEED);
  prefs.end();
  g_maxSpeed = constrain(g_maxSpeed, SPEED_SET_MIN, SPEED_SET_MAX);
}

void settingsSave()
{
  prefs.begin("robot", false);  // false = buka mode baca-tulis
  prefs.putInt("maxSpeed", g_maxSpeed);
  prefs.end();
}

// ============================================================
//  PRESET MODE
//  Catatan: g_maxSpeed SENGAJA TIDAK diatur di sini lagi. Dulu kedua
//  mode sama-sama memaksanya ke 255 tiap kali mode dipilih — kalau itu
//  dibiarkan, Max Speed hasil aturan manual di halaman PENGATURAN akan
//  ketimpa balik ke 255 setiap kali ganti/pilih ulang mode. Sekarang
//  Max Speed murni milik pengguna; rampStep & steerGain tetap beda per
//  mode seperti semula.
// ============================================================
void applyModePreset()
{
  kickArmed = false;

  switch (activeItem) {
    case 0:  // Soccer
      g_rampStep = 6;
      g_steerGain = 100;
      kickArmed = true;
      break;

    case 1:  // Sumo
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
