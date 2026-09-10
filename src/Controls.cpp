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
//  Navigasi menu:
//    UP/DOWN : pilih mode
//    LEFT/RIGHT : tidak digunakan
//    START : konfirmasi       BULAT : batal
// ============================================================

// --- State sebelumnya untuk deteksi "1 tekan = 1 aksi" ---
static bool pSelect=0, pStart=0, pUp=0, pDown=0, pLeft=0, pRight=0, pL1=0, pR1=0;
static bool pCircle=0, pCross=0, pSquare=0;

static bool edge(bool now, bool &prev){ bool e = now && !prev; prev = now; return e; }

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
  int kec = g_maxSpeed * SPD_DEFAULT / 100;
  if      (Ps3.data.button.r2) kec = g_maxSpeed * SPD_R2 / 100;
  else if (Ps3.data.button.r1) kec = g_maxSpeed * SPD_R1 / 100;
  else if (Ps3.data.button.l2) kec = g_maxSpeed * CREEP_SPEED_PCT / 100;
  else if (Ps3.data.button.l1) kec = g_maxSpeed * SPD_L1 / 100;
  return kec;
}

// ------------------------------------------------------------
//  Bentuk input stik: deadband re-normalisasi + kurva expo
//  raw: -128..127  ->  hasil: -1000..1000 (fixed point, 1000 = penuh)
// ------------------------------------------------------------
static int shapeInput(int raw)
{
  int a = abs(raw);
  if (a < DEADBAND) return 0;

  // [B] Re-normalisasi: mulai dari 0 tepat di tepi deadband (tanpa loncat)
  int mag = (a - DEADBAND) * 1000 / (128 - DEADBAND);   // 0..1000
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
  int kec      = creepMode ? (g_maxSpeed * CREEP_SPEED_PCT / 100) : triggerSpeed();
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
//  Navigasi menu
// ------------------------------------------------------------
static void handleMenu(bool eL1, bool eR1, bool eUp, bool eDown,
                       bool eLeft, bool eRight, bool eCircle, bool eStart)
{
  // Ramp motor ke 0 hanya kalau belum berhenti (hemat CPU & GPIO setiap tick)
  if (curLeft != 0 || curRight != 0) stopMotorsSmooth();

  // Hanya ada satu halaman MODE MAIN, jadi L1/R1 tidak melakukan apa-apa.
  if (eUp)   { menuItem = (menuItem + menuLen[menuPage] - 1) % menuLen[menuPage]; oledDirty = true; }
  if (eDown) { menuItem = (menuItem + 1) % menuLen[menuPage];                     oledDirty = true; }

  if (eCircle) { sysState = hasActiveMode ? ST_RUN : ST_IDLE; oledDirty = true; } // batal

  if (eStart)  // konfirmasi
  {
    // Pilih MODE -> masuk RUN
    activeMenu = menuPage;
    activeItem = menuItem;
    hasActiveMode = true;
    applyModePreset();
    sysState = ST_RUN;
    oledDirty = true;
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

  if (!nowConnected) { stopMotorsSmooth(); return; } // fail-safe saat putus

  // Baca edge semua tombol navigasi
  bool eSelect = edge(Ps3.data.button.select, pSelect);
  bool eStart  = edge(Ps3.data.button.start,  pStart);
  bool eUp     = edge(Ps3.data.button.up,     pUp);
  bool eDown   = edge(Ps3.data.button.down,   pDown);
  bool eLeft   = edge(Ps3.data.button.left,   pLeft);
  bool eRight  = edge(Ps3.data.button.right,  pRight);
  bool eL1     = edge(Ps3.data.button.l1,     pL1);
  bool eR1     = edge(Ps3.data.button.r1,     pR1);
  bool eCircle = edge(Ps3.data.button.circle, pCircle);
  bool eCross  = edge(Ps3.data.button.cross,  pCross);
  bool eSquare = edge(Ps3.data.button.square, pSquare);

  // SELECT: buka/tutup menu dari mana saja
  if (eSelect) {
    if (sysState == ST_MENU) sysState = hasActiveMode ? ST_RUN : ST_IDLE;
    else { sysState = ST_MENU; menuPage = 0; menuItem = 0; }
    oledDirty = true;
  }

  if      (sysState == ST_MENU) handleMenu(eL1, eR1, eUp, eDown, eLeft, eRight, eCircle, eStart);
  else if (sysState == ST_RUN)  handleRun(eCross, eSquare, eCircle);
  else                          stopMotorsSmooth(); // ST_IDLE
}