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
  { "Max Speed", &g_maxSpeed,  SPEED_SET_MIN, SPEED_SET_MAX, SPEED_SET_STEP, "",  true },
  { "Steering",  &g_steerGain, STEER_SET_MIN, STEER_SET_MAX, STEER_SET_STEP, "%", true },
};

// ============================================================
//  PENYIMPANAN NILAI KE FLASH (NVS) — TERPISAH PER MODE
//
//  Supaya setelan tidak balik ke default tiap ESP32 restart — termasuk
//  restart OTOMATIS yang sekarang dipicu saat stik putus lama (lihat
//  linkWatchdog() di main.cpp). Tanpa ini, tiap restart begitu akan diam-
//  diam menghapus setelan yang baru saja diatur di lapangan.
//
//  Kuncinya diberi akhiran nomor mode ("maxSpeed0", "maxSpeed1", dst,
//  sesuai activeItem — 0=Soccer, 1=Sumo). Jadi atur di Sumo TIDAK PERNAH
//  menimpa Soccer, karena keduanya baris/kunci berbeda di flash. Dulu
//  (sebelum ini) cuma ada satu kunci "maxSpeed" yang dipakai bersama
//  kedua mode, itu sebabnya atur di satu mode ikut mengubah mode lain.
// ============================================================
static Preferences prefs;

// Nilai bawaan tiap mode kalau BELUM PERNAH diatur manual (persis sama
// seperti preset lama sebelum ada halaman PENGATURAN ini): Soccer 100%,
// Sumo 110%. Max Speed bawaannya sama untuk semua mode (DEF_MAX_SPEED).
static int defaultSteerFor(uint8_t mode) { return (mode == 1) ? 110 : 100; }

void settingsLoad()
{
  char keyMax[14], keySteer[14];
  snprintf(keyMax,   sizeof(keyMax),   "maxSpeed%u",  activeItem);
  snprintf(keySteer, sizeof(keySteer), "steerGain%u", activeItem);

  prefs.begin("robot", true);   // true = buka mode baca-saja
  g_maxSpeed  = prefs.getInt(keyMax,   DEF_MAX_SPEED);
  g_steerGain = prefs.getInt(keySteer, defaultSteerFor(activeItem));
  prefs.end();

  g_maxSpeed  = constrain(g_maxSpeed,  SPEED_SET_MIN, SPEED_SET_MAX);
  g_steerGain = constrain(g_steerGain, STEER_SET_MIN, STEER_SET_MAX);
}

void settingsSave()
{
  char keyMax[14], keySteer[14];
  snprintf(keyMax,   sizeof(keyMax),   "maxSpeed%u",  activeItem);
  snprintf(keySteer, sizeof(keySteer), "steerGain%u", activeItem);

  prefs.begin("robot", false);  // false = buka mode baca-tulis
  prefs.putInt(keyMax,   g_maxSpeed);
  prefs.putInt(keySteer, g_steerGain);
  prefs.end();
}

// ============================================================
//  PRESET MODE
//  Catatan: g_maxSpeed & g_steerGain SENGAJA TIDAK diatur di sini lagi.
//  Keduanya sekarang murni milik pengguna lewat halaman PENGATURAN dan
//  disimpan permanen (settingsSave) — kalau dipatok ulang di sini tiap
//  pilih mode, setelan manual itu akan ketimpa balik. Cuma g_rampStep
//  (kehalusan gerak, BUKAN parameter belok) yang tetap beda per mode.
// ============================================================
void applyModePreset()
{
  kickArmed = false;

  switch (activeItem) {
    case 0:  // Soccer
      g_rampStep = 6;
      kickArmed = true;
      break;

    case 1:  // Sumo
      g_rampStep = 10;
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
  settingsLoad();   // Max Speed & Steering milik mode default ini (per-mode di flash)

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
