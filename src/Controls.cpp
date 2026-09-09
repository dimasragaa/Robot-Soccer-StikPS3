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
//    L1/R1 : ganti halaman   UP/DOWN : pilih item
//    LEFT/RIGHT : ubah nilai (hanya menu Pengaturan)
//    START : konfirmasi       BULAT : batal
// ============================================================

// --- State sebelumnya untuk deteksi "1 tekan = 1 aksi" ---
static bool pSelect=0, pStart=0, pUp=0, pDown=0, pLeft=0, pRight=0, pL1=0, pR1=0;
static bool pCircle=0, pCross=0, pSquare=0;

static bool edge(bool now, bool &prev){ bool e = now && !prev; prev = now; return e; }

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
//  Kontrol manual: stik -> motor
// ------------------------------------------------------------
static void driveManual()
{
  bool creepMode = Ps3.data.button.l2;

  // Saat creep: kecepatan ceiling rendah + ramp sangat halus
  int kec      = creepMode ? (g_maxSpeed * CREEP_SPEED_PCT / 100) : triggerSpeed();
  int rampStep = creepMode ? CREEP_RAMP_STEP : g_rampStep;

  handleKick(kickArmed && Ps3.data.button.triangle);  // kick = SEGITIGA

  int rawSteer = applyDeadband(Ps3.data.analog.stick.rx, DEADBAND);
  int rawThr   = applyDeadband(Ps3.data.analog.stick.ly, DEADBAND);

  int steering = map(rawSteer, -128, 128, kec, -kec);
  int throttle = map(rawThr,   -128, 128, kec, -kec);
  steering = steering * g_steerGain / 100;
  if (g_invert) steering = -steering;

  int targetL = constrain(throttle - steering, -255, 255);
  int targetR = constrain(throttle + steering, -255, 255);

  curLeft  = ramp(curLeft,  targetL, rampStep);
  curRight = ramp(curRight, targetR, rampStep);
  setMotorL(curLeft);
  setMotorR(curRight);
}

// ------------------------------------------------------------
//  Navigasi menu
// ------------------------------------------------------------
static void handleMenu(bool eL1, bool eR1, bool eUp, bool eDown,
                       bool eLeft, bool eRight, bool eCircle, bool eStart)
{
  stopMotorsSmooth(); // motor mati selama di menu

  if (eL1)   { menuPage = (menuPage + MENU_COUNT - 1) % MENU_COUNT; menuItem = 0; oledDirty = true; }
  if (eR1)   { menuPage = (menuPage + 1) % MENU_COUNT;              menuItem = 0; oledDirty = true; }
  if (eUp)   { menuItem = (menuItem + menuLen[menuPage] - 1) % menuLen[menuPage]; oledDirty = true; }
  if (eDown) { menuItem = (menuItem + 1) % menuLen[menuPage];                     oledDirty = true; }

  // Left/Right hanya untuk menu Pengaturan (adjust nilai)
  if (menuPage == 2) {
    if (eLeft)  { adjustSetting(-1); oledDirty = true; }
    if (eRight) { adjustSetting(+1); oledDirty = true; }
  }

  if (eCircle) { sysState = hasActiveMode ? ST_RUN : ST_IDLE; oledDirty = true; } // batal

  if (eStart)  // konfirmasi
  {
    if (menuPage <= 1) {           // pilih MODE -> masuk RUN
      activeMenu = menuPage; activeItem = menuItem;
      hasActiveMode = true;
      applyModePreset();
      sysState = ST_RUN;
    } else {                        // menu Pengaturan -> simpan & keluar
      sysState = hasActiveMode ? ST_RUN : ST_IDLE;
    }
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
  bool nowConnected = Ps3.isConnected();
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