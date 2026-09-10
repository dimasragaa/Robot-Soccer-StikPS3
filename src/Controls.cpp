// By: github.com/dimasragaa — IG: @dmsragaa
#include "Controls.h"
#include <Ps3Controller.h>
#include "Config.h"
#include "State.h"
#include "Motor.h"
#include "Kicker.h"
#include "Menu.h"
#include "Sequence.h"

// ============================================================
//  PETA TOMBOL (mode RUN)
//    Stik kanan (rx) : belok        Stik kiri (ly) : maju/mundur
//    R2/R1/L2/L1     : gas bertahap  SEGITIGA       : tendang
//    X     : mundur -> maju          KOTAK : mundur -> putar KANAN
//    BULAT : mundur -> putar KIRI    SELECT: buka/tutup menu
//  Navigasi menu (2 langkah setelah SELECT):
//    1) MODE MAIN  : UP/DOWN pilih Soccer/Sumo, START lanjut ke langkah 2
//    2) PENGATURAN : UP/DOWN pilih baris, LEFT/RIGHT ubah nilainya,
//                    START konfirmasi & mulai RUN, BULAT batal (balik ke 1)
//    BULAT di langkah 1 : keluar menu sepenuhnya
// ============================================================

// --- State sebelumnya untuk deteksi "1 tekan = 1 aksi" ---
// (UP/DOWN/KIRI/KANAN tidak ada di sini — keempatnya pakai Repeater di bawah.
//  L1/R1 juga tidak: di mode RUN keduanya dibaca langsung sebagai tombol
//  ditahan, dan di menu sudah tidak dipakai sejak alurnya jadi 2 langkah.)
static bool pSelect=0, pStart=0;
static bool pCircle=0, pCross=0, pSquare=0;

static bool edge(bool now, bool &prev){ bool e = now && !prev; prev = now; return e; }

// --- Tombol dengan AUTO-REPEAT (dipakai UP/DOWN/KIRI/KANAN di menu) ---
//  Sekali ketuk -> 1 langkah (sama seperti edge biasa).
//  Ditahan      -> setelah REPEAT_DELAY_MS mulai jalan sendiri, awalnya
//                   pelan (REPEAT_RATE_MS) lalu dipercepat sendiri
//                   (REPEAT_FAST_MS) kalau ditahan lebih dari
//                   REPEAT_ACCEL_MS.
//  Jeda awal penting supaya ketukan pendek tetap = 1 langkah persis.
//  Percepatan penting karena langkahnya cuma 1: tanpa itu, menggeser
//  Max Speed dari 60 ke 255 harus ditahan belasan detik.
struct Repeater { bool held; unsigned long tDown, tLast; };

static bool repeatFire(bool now, Repeater &r)
{
  if (!now) { r.held = false; return false; }        // dilepas -> reset

  unsigned long t = millis();
  if (!r.held) {                                      // baru ditekan
    r.held = true; r.tDown = t; r.tLast = t;
    return true;                                      // langkah pertama
  }

  unsigned long held = t - r.tDown;
  if (held < REPEAT_DELAY_MS) return false;           // masih dalam jeda awal

  unsigned long rate = (held > REPEAT_ACCEL_MS) ? REPEAT_FAST_MS : REPEAT_RATE_MS;
  if (t - r.tLast < rate) return false;               // belum waktunya ulang
  r.tLast = t;
  return true;
}

static Repeater rLeft, rRight, rUp, rDown;

// --- Setelan yang sudah diubah tapi belum ditulis ke flash ---
//  Sengaja di lingkup file, bukan di dalam handleMenu(), supaya jalur
//  keluar yang TIDAK lewat handleMenu() juga bisa menyimpannya:
//  tombol SELECT dan stik putus keduanya menutup menu dari luar. Kalau
//  flag ini terkurung di dalam handleMenu(), menekan SELECT sambil masih
//  menahan KIRI/KANAN akan membuat perubahan terakhir hilang saat restart.
static bool settingsDirty = false;

static void settingsFlush()
{
  if (!settingsDirty) return;
  settingsSave();
  settingsDirty = false;
}

// ------------------------------------------------------------
//  STATUS KONEKSI STIK YANG BISA DIPERCAYA
//
//  JANGAN pakai Ps3.isConnected(): flag di dalam library hanya
//  dinyalakan saat paket pertama masuk dan TIDAK PERNAH dimatikan —
//  callback putus di ps3_l2cap.c isinya cuma cetak log. Akibatnya
//  sekali stik konek, library selamanya bilang "masih terhubung"
//  walau stik sudah dimatikan.
//
//  Jadi kita ukur sendiri: stik mengirim paket ~100x per detik walau
//  didiamkan. Kalau paket berhenti lebih dari PS3_TIMEOUT_MS,
//  berarti sudah benar-benar putus.
// ------------------------------------------------------------
bool ps3Linked()
{
  unsigned long t = lastPs3Packet;   // baca sekali (32-bit = atomik di ESP32)
  return (t != 0) && (millis() - t < PS3_TIMEOUT_MS);
}

// ------------------------------------------------------------
//  Kecepatan aktif dari trigger (persen di Config.h)
//  Catatan: L2 (creep) tidak masuk sini karena juga butuh ramp khusus.
//  Fungsi ini dipakai untuk Serial Monitor & tampilan saja.
// ------------------------------------------------------------
int triggerSpeed(){
  // Semua g_spd* di bawah bisa diatur dari menu PENGATURAN, termasuk
  // g_spdNorm (saat tidak ada trigger ditahan). Dulu yang terakhir itu
  // konstanta 50% — akibatnya kalau L1 disetel di atas 50, tombol
  // "pelan" justru lebih cepat daripada tidak menekan apa-apa.
  int kec = g_maxSpeed * g_spdNorm / 100;
  if      (Ps3.data.button.r2) kec = g_maxSpeed * g_spdR2 / 100;
  else if (Ps3.data.button.r1) kec = g_maxSpeed * g_spdR1 / 100;
  else if (Ps3.data.button.l2) kec = g_maxSpeed * g_spdL2 / 100;
  else if (Ps3.data.button.l1) kec = g_maxSpeed * g_spdL1 / 100;
  return kec;
}

// ------------------------------------------------------------
//  Bentuk input stik: deadband re-normalisasi + kurva expo
//  raw: -128..127  ->  hasil: -1000..1000 (fixed point, 1000 = penuh)
// ------------------------------------------------------------
static int shapeInput(int raw)
{
  int a = abs(raw);

  // Stik memberi -128..+127, jadi sisi negatif punya 1 langkah lebih
  // banyak. Kalau dibiarkan, satu arah dapat 100% sementara arah
  // sebaliknya cuma ~99% — maju & mundur tidak sama kuat. Dipangkas
  // ke 127 supaya kedua arah benar-benar simetris.
  if (a > 127) a = 127;
  if (a < DEADBAND) return 0;

  // [B] Re-normalisasi: mulai dari 0 tepat di tepi deadband (tanpa loncat)
  int mag = (a - DEADBAND) * 1000 / (127 - DEADBAND);   // 0..1000
  if (mag > 1000) mag = 1000;

  // [C] Expo: campur linear & kubik -> tengah landai, ujung tetap penuh
  long cube    = (long)mag * mag / 1000 * mag / 1000;    // (mag^3), 0..1000
  int  shaped  = ((100 - EXPO_PCT) * mag + EXPO_PCT * (int)cube) / 100;

  return (raw < 0) ? -shaped : shaped;
}

// ------------------------------------------------------------
//  [E] Ramp asimetris: akselerasi halus, pengereman lebih gesit
// ------------------------------------------------------------
static int rampAxis(int cur, int tgt, int stepUp)
{
  int stepDn = stepUp * RAMP_BRAKE_X;
  // "melambat" bila menuju 0 atau berganti arah
  bool braking = (abs(tgt) < abs(cur)) || ((long)cur * tgt < 0);
  int step = braking ? stepDn : stepUp;
  if (cur < tgt) return min(cur + step, tgt);
  if (cur > tgt) return max(cur - step, tgt);
  return cur;
}

// ------------------------------------------------------------
//  Kontrol manual: stik -> motor
// ------------------------------------------------------------
static void driveManual()
{
  bool creepMode = Ps3.data.button.l2;

  // Saat creep: kecepatan ceiling rendah + ramp sangat halus
  int kec      = creepMode ? (g_maxSpeed * g_spdL2 / 100) : triggerSpeed();
  int rampStep = creepMode ? CREEP_RAMP_STEP : g_rampStep;

  handleKick(kickArmed && Ps3.data.button.triangle);  // kick = SEGITIGA

  // [B][C] Bentuk input dulu (deadband mulus + expo), skala -1000..1000
  int thr = shapeInput(Ps3.data.analog.stick.ly);
  int str = shapeInput(Ps3.data.analog.stick.rx);

  // Skala ke kecepatan aktif (stik atas = ly negatif = maju -> dibalik)
  int throttle = -thr * kec / 1000;
  int steering = -str * kec / 1000;
  steering = steering * g_steerGain / 100;
  if (g_invert) steering = -steering;

  int targetL = throttle - steering;
  int targetR = throttle + steering;

  // [A] Mixing proporsional: jika meluap >255, kecilkan KEDUANYA seimbang
  //     supaya rasio belok (selisih L-R) tetap terjaga di semua kecepatan
  int m = max(abs(targetL), abs(targetR));
  if (m > 255) {
    targetL = targetL * 255 / m;
    targetR = targetR * 255 / m;
  }

  // [D] Trim koreksi ketimpangan motor kiri/kanan
  targetL = targetL * MOTOR_TRIM_L / 100;
  targetR = targetR * MOTOR_TRIM_R / 100;

  // [E] Ramp asimetris (halus saat gas, gesit saat rem/ganti arah)
  curLeft  = rampAxis(curLeft,  targetL, rampStep);
  curRight = rampAxis(curRight, targetR, rampStep);
  setMotorL(curLeft);
  setMotorR(curRight);
}

// ------------------------------------------------------------
//  Navigasi menu — alurnya 2 langkah:
//    1) MODE MAIN     : pilih Soccer/Sumo, START -> lanjut ke langkah 2
//    2) PENGATURAN    : atur Max Speed & Kehalusan UNTUK MODE YANG BARU
//                        DIPILIH, START -> baru benar-benar masuk RUN
//                        (O/BULAT di sini balik ke daftar mode, batal)
//  Preset mode (applyModePreset) dipasang begitu masuk langkah 2, lalu
//  settingsLoad() menimpanya dengan setelan simpanan milik mode itu.
// ------------------------------------------------------------
static void handleMenu(bool eUp, bool eDown,
                       bool eLeft, bool eRight, bool eCircle, bool eStart)
{
  stopMotorsSmooth();   // penjaga "sudah diam" ada di dalam fungsinya

  if (menuPage == MENU_PAGE_PENGATURAN)
  {
    // --- Langkah 2: layar pengaturan mode yang baru dipilih ---
    // Berhenti di ujung (bukan muter balik ke atas/bawah). Ini juga yang
    // menutup celah crash kemarin: menuItem TIDAK PERNAH bisa lebih dari
    // SETTINGS_COUNT-1, jadi settingsList[menuItem] selalu di dalam batas.
    if (eUp   && menuItem > 0)                  { menuItem--; oledDirty = true; }
    if (eDown && menuItem < SETTINGS_COUNT - 1) { menuItem++; oledDirty = true; }

    SettingItem &s = settingsList[menuItem];

    if (!s.val)
    {
      // Baris AKSI (Reset). Tidak punya nilai, jadi KIRI/KANAN = jalankan.
      // Sengaja tidak auto-repeat berulang: settingsReset() menulis nilai
      // yang sama terus, jadi menahan tombol pun hasilnya tetap sama.
      if (eLeft || eRight) { settingsReset(); oledDirty = true; }
    }
    else
    {
      // KIRI/KANAN ubah nilai baris yang sedang disorot. Berlaku detik itu
      // juga (baca catatan panjangnya di Menu.cpp). Tombol ini auto-repeat,
      // jadi ditahan = nilainya jalan terus.
      if (eLeft)  { *s.val = max(s.lo, *s.val - s.step); settingsDirty = true; oledDirty = true; }
      if (eRight) { *s.val = min(s.hi, *s.val + s.step); settingsDirty = true; oledDirty = true; }
    }

    // Menulis ke flash SEKALI setelah tombol dilepas, bukan tiap langkah.
    // Kalau ditulis tiap langkah, menahan tombol beberapa detik akan
    // menghasilkan puluhan penulisan flash — boros umur flash dan tiap
    // penulisan makan waktu, jadi nilainya terasa tersendat saat digeser.
    if (!Ps3.data.button.left && !Ps3.data.button.right) settingsFlush();

    if (eCircle) { settingsFlush(); menuPage = MENU_PAGE_MODE; menuItem = activeItem; oledDirty = true; }
    if (eStart)  { settingsFlush(); sysState = ST_RUN;                                oledDirty = true; }
  }
  else
  {
    // --- Langkah 1: daftar mode (Soccer/Sumo) --- juga berhenti di ujung
    if (eUp   && menuItem > 0)                               { menuItem--; oledDirty = true; }
    if (eDown && menuItem < menuLen[MENU_PAGE_MODE] - 1)      { menuItem++; oledDirty = true; }

    if (eCircle) { sysState = hasActiveMode ? ST_RUN : ST_IDLE; oledDirty = true; } // batal, keluar menu total

    if (eStart)
    {
      activeMenu = MENU_PAGE_MODE;
      activeItem = menuItem;
      hasActiveMode = true;
      applyModePreset();          // pasang nilai pabrik mode ini
      settingsLoad();             // lalu timpa dengan setelan MILIK MODE INI dari flash
      modeSave();                 // ingat mode ini untuk restart berikutnya
      menuPage = MENU_PAGE_PENGATURAN;
      menuItem = 0;
      oledDirty = true;
      // sysState TETAP ST_MENU -> lanjut ke layar pengaturan, belum RUN
    }
  }
}

// ------------------------------------------------------------
//  Kontrol saat mode RUN (sequence + kontrol manual)
// ------------------------------------------------------------
static void handleRun(bool eCross, bool eSquare, bool eCircle)
{
  // Pemicu sequence (tekan tombol sama = batal)
  if (eCross)  mulaiSequence(SQ_MUNDUR_MAJU,  "mundur -> maju");
  if (eSquare) mulaiSequence(SQ_MUNDUR_KANAN, "mundur -> putar KANAN");
  if (eCircle) mulaiSequence(SQ_MUNDUR_KIRI,  "mundur -> putar KIRI");

  if (seqState != SEQ_IDLE) runSequence();  // sedang sequence
  else                      driveManual();  // kontrol manual normal
}

// ============================================================
//  UPDATE UTAMA — dipanggil tiap tick dari loop()
// ============================================================
void controlsUpdate()
{
  // Begitu stik konek: langsung masuk mode jalan, tanpa SELECT/START
  bool nowConnected = ps3Linked();
  if (nowConnected && !wasConnected) startDefaultMode();
  wasConnected = nowConnected;

  // Fail-safe saat putus. settingsFlush() dulu: kalau stik mati sewaktu
  // kamu sedang mengatur, perubahannya tetap tersimpan — apalagi robot
  // akan restart sendiri beberapa saat lagi (linkWatchdog di main.cpp).
  if (!nowConnected) { settingsFlush(); stopMotorsSmooth(); return; }

  // Baca edge semua tombol navigasi
  bool eSelect = edge(Ps3.data.button.select, pSelect);
  bool eStart  = edge(Ps3.data.button.start,  pStart);
  // UP/DOWN pakai auto-repeat juga: ditahan = pindah baris terus,
  // tidak perlu ketuk berkali-kali.
  bool eUp     = repeatFire(Ps3.data.button.up,   rUp);
  bool eDown   = repeatFire(Ps3.data.button.down, rDown);
  // KIRI/KANAN pakai auto-repeat: ditahan = nilainya jalan terus
  bool eLeft   = repeatFire(Ps3.data.button.left,  rLeft);
  bool eRight  = repeatFire(Ps3.data.button.right, rRight);
  bool eCircle = edge(Ps3.data.button.circle, pCircle);
  bool eCross  = edge(Ps3.data.button.cross,  pCross);
  bool eSquare = edge(Ps3.data.button.square, pSquare);

  // SELECT: buka/tutup menu dari mana saja
  if (eSelect) {
    if (sysState == ST_MENU) {
      settingsFlush();   // simpan dulu; handleMenu() tidak jalan tick ini
      sysState = hasActiveMode ? ST_RUN : ST_IDLE;
    }
    else {
      // Masuk menu = batalkan gerakan otomatis yang sedang jalan.
      // Tanpa ini statusnya cuma BEKU: begitu keluar menu, sisa fasenya
      // langsung diloncati (waktunya sudah lewat) dan motor tersentak.
      seqState = SEQ_IDLE;
      seqType  = SQ_NONE;
      sysState = ST_MENU; menuPage = 0; menuItem = 0;
    }
    oledDirty = true;
  }

  if      (sysState == ST_MENU) handleMenu(eUp, eDown, eLeft, eRight, eCircle, eStart);
  else if (sysState == ST_RUN)  handleRun(eCross, eSquare, eCircle);
  else                          stopMotorsSmooth(); // ST_IDLE
}