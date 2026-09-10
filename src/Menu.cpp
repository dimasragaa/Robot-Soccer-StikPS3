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
  { "Max Speed", "maxSpeed",  &g_maxSpeed,  SPEED_SET_MIN, SPEED_SET_MAX, SPEED_SET_STEP, "",  true },
  { "Steering",  "steerGain", &g_steerGain, STEER_SET_MIN, STEER_SET_MAX, STEER_SET_STEP, "%", true },
  { "R2 Boost",  "spdR2",     &g_spdR2,     TRIG_SET_MIN,  TRIG_SET_MAX,  TRIG_SET_STEP,  "%", true },
  { "R1 Cepat",  "spdR1",     &g_spdR1,     TRIG_SET_MIN,  TRIG_SET_MAX,  TRIG_SET_STEP,  "%", true },
  { "L1 Pelan",  "spdL1",     &g_spdL1,     TRIG_SET_MIN,  TRIG_SET_MAX,  TRIG_SET_STEP,  "%", true },
  { "L2 Creep",  "spdL2",     &g_spdL2,     TRIG_SET_MIN,  TRIG_SET_MAX,  TRIG_SET_STEP,  "%", true },
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

// PENTING soal urutan panggil: settingsLoad() HARUS dipanggil SESUDAH
// applyModePreset(). applyModePreset() memasang nilai pabrik mode ini,
// lalu settingsLoad() menimpanya dengan nilai simpanan pengguna kalau ada.
// Jadi mode yang belum pernah diatur manual tetap berperilaku seperti dulu.
void settingsLoad()
{
  prefs.begin("robot", true);   // true = buka mode baca-saja
  for (uint8_t i = 0; i < SETTINGS_COUNT; i++)
  {
    SettingItem &s = settingsList[i];
    if (!s.persist) continue;

    char k[16];
    snprintf(k, sizeof(k), "%s%u", s.key, activeItem);
    *s.val = prefs.getInt(k, *s.val);         // default = nilai pabrik mode ini
    *s.val = constrain(*s.val, s.lo, s.hi);   // jaga-jaga kalau batas berubah
  }
  prefs.end();
}

void settingsSave()
{
  prefs.begin("robot", false);  // false = buka mode baca-tulis
  for (uint8_t i = 0; i < SETTINGS_COUNT; i++)
  {
    SettingItem &s = settingsList[i];
    if (!s.persist) continue;

    char k[16];
    snprintf(k, sizeof(k), "%s%u", s.key, activeItem);
    prefs.putInt(k, *s.val);
  }
  prefs.end();
}

// ============================================================
//  PRESET MODE — NILAI PABRIK
//  Ini cuma titik awal. Segera setelah ini, settingsLoad() akan menimpa
//  nilai-nilai yang pernah diatur manual untuk mode bersangkutan. Jadi:
//    - mode yang belum pernah disentuh menu  -> pakai angka di bawah ini
//    - mode yang sudah pernah diatur manual  -> pakai simpanan pengguna
//  Keduanya tersimpan terpisah per mode, jadi atur di Sumo tidak
//  mengubah Soccer sama sekali.
// ============================================================
void applyModePreset()
{
  // --- Sama untuk semua mode ---
  kickArmed  = false;
  g_maxSpeed = DEF_MAX_SPEED;
  g_spdR2    = SPD_R2;
  g_spdR1    = SPD_R1;
  g_spdL1    = SPD_L1;
  g_spdL2    = CREEP_SPEED_PCT;

  // --- Yang memang beda per mode ---
  switch (activeItem) {
    case 0:  // Soccer
      g_rampStep  = 6;
      g_steerGain = 100;
      kickArmed   = true;
      break;

    case 1:  // Sumo
      g_rampStep  = 10;
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
