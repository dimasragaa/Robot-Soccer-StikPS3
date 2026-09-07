#include <Arduino.h>
#include <Ps3Controller.h>

// ============================================================
//  OPSI OLED  (set 0 kalau OLED belum terpasang / library belum ada)
// ============================================================
#define USE_OLED 1
#if USE_OLED
  #include <Wire.h>
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>
  #define OLED_W    128
  #define OLED_H    64
  #define OLED_ADDR 0x3C     // ganti 0x3D bila perlu
  #define OLED_SDA  21
  #define OLED_SCL  22
  Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, -1);
  bool oledOK = false;
#endif

// ============================================================
//  KONFIGURASI PWM
// ============================================================
#define MOTOR_FREQ 20000
#define MOTOR_RES  8

// --- Left Motor (BTS7960) ---
#define RPWM  32
#define RLPWM 33
// --- Right Motor (BTS7960) ---
#define LPWM  25
#define LLPWM 26

#define LPWM_CH  0
#define LLPWM_CH 1
#define RPWM_CH  2
#define RLPWM_CH 3

#define LED      2
#define KICK_PIN 18

// ============================================================
//  PARAMETER LIVE (dipakai saat RUN, bisa diubah lewat menu Pengaturan)
// ============================================================
int  g_maxSpeed  = 255;   // ceiling kecepatan (0..255)
int  g_steerGain = 100;   // sensitivitas belok (%)
int  g_rampStep  = 6;     // kehalusan (perubahan PWM per update)
bool g_invert    = false; // balik arah belok

const int DEADBAND = 12;
const unsigned long CONTROL_MS = 5;
const unsigned long OLED_MS    = 120;
const unsigned long PRINT_MS   = 200; // periode cetak serial

// ============================================================
//  STATE MACHINE
// ============================================================
enum SysState { ST_IDLE, ST_MENU, ST_RUN };
SysState sysState = ST_IDLE;
bool hasActiveMode = false;

// --- Struktur menu ---
const uint8_t MENU_COUNT = 3;
const char* menuTitle[MENU_COUNT] = { "POSISI START", "MODE MAIN", "PENGATURAN" };
const uint8_t menuLen[MENU_COUNT]  = { 2, 4, 4 };
const char* menuItems[MENU_COUNT][4] = {
  { "Hadap Gawang", "Hadap Bola", "", "" },
  { "Normal", "Dribble", "Attack", "Defense" },
  { "Kecepatan Max", "Sensitiv Belok", "Kehalusan", "Balik Arah" }
};

uint8_t menuPage = 0;   // menu 1/2/3
uint8_t menuItem = 0;   // item terpilih
uint8_t activeMenu = 0, activeItem = 0;

// ============================================================
//  MOTOR & KICK STATE
// ============================================================
int curLeft = 0, curRight = 0;
bool kickArmed = false;

bool kickActive = false;
unsigned long kickStart = 0;
const unsigned long KICK_MS = 120;

// ============================================================
//  AUTO-SEQUENCE (tombol X: mundur -> puter kiri)
// ============================================================
enum SeqState { SEQ_IDLE, SEQ_MUNDUR, SEQ_PUTAR };
SeqState seqState  = SEQ_IDLE;
unsigned long seqStart = 0;

// === KONSTANTA WAKTU — ubah di sini untuk kalibrasi ===
const unsigned long SEQ_MUNDUR_MS = 300; // mundur 1 detik
const unsigned long SEQ_PUTAR_MS  = 500; // putar kiri 2 detik
// Kecepatan saat sequence (0..255) — turunkan kalau terlalu jauh/banyak
const int SEQ_SPEED_MUNDUR = 180;
const int SEQ_SPEED_PUTAR  = 150;

unsigned long lastControl = 0, lastOled = 0, lastPrint = 0;
bool oledDirty = true;

// ============================================================
//  EDGE DETECTION TOMBOL  (1 tekan = 1 aksi)
// ============================================================
bool pSelect=0, pStart=0, pUp=0, pDown=0, pLeft=0, pRight=0, pL1=0, pR1=0, pCircle=0, pCross=0;
bool edge(bool now, bool &prev){ bool e = now && !prev; prev = now; return e; }

// ============================================================
//  DEKLARASI
// ============================================================
int  applyDeadband(int v, int t);
int  ramp(int cur, int tgt, int step);
void setMotor(uint8_t f, uint8_t r, int spd);
void stopMotorsSmooth();
void handleKick(bool trigger);
void applyModePreset();
void adjustSetting(int dir);
void runSequence();
void drawOLED();
void printSerial();

// ============================================================
//  CALLBACK KONEKSI PS3
// ============================================================
void onConnect()    { digitalWrite(LED, HIGH); Serial.println(">> PS3 CONNECTED");    oledDirty = true; }
void onDisconnect() { digitalWrite(LED, LOW);  Serial.println(">> PS3 DISCONNECTED"); oledDirty = true; }

// ============================================================
//  SETUP
// ============================================================
void setup()
{
  Serial.begin(115200);

  // Opsional: matikan brownout detector kalau reset terus saat motor nyentak.
  // Ini menyembunyikan gejala, TETAP perbaiki power-nya. Buka komentar bila perlu:
  // #include "soc/rtc_cntl_reg.h"
  // WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  setCpuFrequencyMhz(240); // pastikan CPU full speed untuk stack BT

  pinMode(LED, OUTPUT);
  pinMode(KICK_PIN, OUTPUT);
  digitalWrite(KICK_PIN, LOW);

  ledcSetup(LPWM_CH,  MOTOR_FREQ, MOTOR_RES); ledcAttachPin(LPWM,  LPWM_CH);
  ledcSetup(LLPWM_CH, MOTOR_FREQ, MOTOR_RES); ledcAttachPin(LLPWM, LLPWM_CH);
  ledcSetup(RPWM_CH,  MOTOR_FREQ, MOTOR_RES); ledcAttachPin(RPWM,  RPWM_CH);
  ledcSetup(RLPWM_CH, MOTOR_FREQ, MOTOR_RES); ledcAttachPin(RLPWM, RLPWM_CH);
  stopMotorsSmooth();

#if USE_OLED
  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(400000); // I2C cepat -> update OLED ringan
  oledOK = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (oledOK) { display.clearDisplay(); display.display(); }
  else Serial.println("OLED tidak terdeteksi, lanjut tanpa OLED");
#endif

  Ps3.attachOnConnect(onConnect);
  Ps3.begin("f8:2f:a8:89:f9:83"); // MAC address (controller harus dipair ke MAC ini)
  Serial.println("SETUP DONE");
}

// ============================================================
//  LOOP
// ============================================================
void loop()
{
  handleKick(false); // jaga pulsa tendang non-blocking

  unsigned long now = millis();
  if (now - lastControl >= CONTROL_MS)
  {
    lastControl = now;

    if (!Ps3.isConnected())
    {
      stopMotorsSmooth();          // fail-safe: berhenti mulus saat putus
    }
    else
    {
      // ---- Baca edge tombol navigasi ----
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

      // ---- SELECT: buka/tutup menu dari mana saja ----
      if (eSelect)
      {
        if (sysState == ST_MENU) sysState = hasActiveMode ? ST_RUN : ST_IDLE;
        else { sysState = ST_MENU; menuPage = 0; menuItem = 0; }
        oledDirty = true;
      }

      if (sysState == ST_MENU)
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
          } else {                        // menu pengaturan -> simpan & keluar
            sysState = hasActiveMode ? ST_RUN : ST_IDLE;
          }
          oledDirty = true;
        }
      }
      else if (sysState == ST_RUN)
      {
        // --- Tombol X: mulai sequence (bisa di-cancel tombol X lagi) ---
        if (eCross) {
          if (seqState == SEQ_IDLE) {
            seqState = SEQ_MUNDUR;   // mulai sequence
            seqStart = millis();
            Serial.println("[SEQ] Mulai: mundur 3 detik...");
            oledDirty = true;
          } else {
            seqState = SEQ_IDLE;     // cancel sequence
            Serial.println("[SEQ] Dibatalkan");
            oledDirty = true;
          }
        }

        // --- Jalankan sequence non-blocking ---
        if (seqState != SEQ_IDLE) {
          runSequence();
        } else {
          // --- Kontrol manual normal (hanya kalau tidak sedang sequence) ---
          int kec = g_maxSpeed * 50 / 100;
          if      (Ps3.data.button.r2) kec = g_maxSpeed;
          else if (Ps3.data.button.r1) kec = g_maxSpeed * 75 / 100;
          else if (Ps3.data.button.l2) kec = g_maxSpeed * 12 / 100;
          else if (Ps3.data.button.l1) kec = g_maxSpeed * 25 / 100;

          handleKick(kickArmed && Ps3.data.button.square);

          int rawSteer = applyDeadband(Ps3.data.analog.stick.rx, DEADBAND);
          int rawThr   = applyDeadband(Ps3.data.analog.stick.ly, DEADBAND);

          int steering = map(rawSteer, -128, 128, kec, -kec);
          int throttle = map(rawThr,   -128, 128, kec, -kec);
          steering = steering * g_steerGain / 100;
          if (g_invert) steering = -steering;

          int targetL = constrain(throttle - steering, -255, 255);
          int targetR = constrain(throttle + steering, -255, 255);

          curLeft  = ramp(curLeft,  targetL, g_rampStep);
          curRight = ramp(curRight, targetR, g_rampStep);
          setMotor(LPWM_CH, LLPWM_CH, curLeft);
          setMotor(RPWM_CH, RLPWM_CH, curRight);
        }
      }
      else // ST_IDLE
      {
        stopMotorsSmooth(); // stik tidak berfungsi sebelum masuk mode
      }
    }
  }

  // ---- Update OLED (throttled) ----
#if USE_OLED
  if (oledOK && (oledDirty || (now - lastOled >= OLED_MS))) {
    lastOled = now; oledDirty = false; drawOLED();
  }
#endif

  // ---- Cetak semua data ke Serial Monitor (throttled) ----
  if (now - lastPrint >= PRINT_MS) { lastPrint = now; printSerial(); }

  delay(1); // yield ke scheduler FreeRTOS -> stack Bluetooth lebih stabil
}

// ============================================================
//  PRESET MODE
// ============================================================
void applyModePreset()
{
  kickArmed = false;
  if (activeMenu == 0) {            // POSISI START
    g_maxSpeed = 200; g_rampStep = 6; g_steerGain = 100;
    // Hadap Gawang / Hadap Bola: parameter sama, beda label + kick siap saat hadap bola
    if (activeItem == 1) kickArmed = true;
  } else {                          // MODE MAIN
    switch (activeItem) {
      case 0: g_maxSpeed = 255; g_rampStep = 6; g_steerGain = 100; break;                 // Normal
      case 1: g_maxSpeed = 120; g_rampStep = 4; g_steerGain = 70;  break;                 // Dribble
      case 2: g_maxSpeed = 255; g_rampStep = 8; g_steerGain = 100; kickArmed = true; break;// Attack
      case 3: g_maxSpeed = 180; g_rampStep = 7; g_steerGain = 90;  break;                 // Defense
    }
  }
}

// ============================================================
//  ADJUST PENGATURAN (menu 3)
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
//  AUTO-SEQUENCE: mundur 3 detik -> putar kiri 2 detik
//  Dipanggil tiap tick selama seqState != SEQ_IDLE
//  Tidak pakai delay — murni state machine berbasis millis
// ============================================================
void runSequence()
{
  unsigned long elapsed = millis() - seqStart;

  if (seqState == SEQ_MUNDUR)
  {
    // Kedua motor mundur (nilai negatif = mundur)
    int target = -SEQ_SPEED_MUNDUR;
    curLeft  = ramp(curLeft,  target, g_rampStep);
    curRight = ramp(curRight, target, g_rampStep);
    setMotor(LPWM_CH, LLPWM_CH, curLeft);
    setMotor(RPWM_CH, RLPWM_CH, curRight);

    if (elapsed >= SEQ_MUNDUR_MS) {
      seqState = SEQ_PUTAR;
      seqStart = millis(); // reset timer untuk fase berikutnya
      Serial.println("[SEQ] Selesai mundur -> putar kiri 2 detik...");
      oledDirty = true;
    }
  }
  else if (seqState == SEQ_PUTAR)
  {
    // Putar kiri di tempat: motor kiri mundur, motor kanan maju
    curLeft  = ramp(curLeft,  -SEQ_SPEED_PUTAR, g_rampStep);
    curRight = ramp(curRight,  SEQ_SPEED_PUTAR, g_rampStep);
    setMotor(LPWM_CH, LLPWM_CH, curLeft);
    setMotor(RPWM_CH, RLPWM_CH, curRight);

    if (elapsed >= SEQ_PUTAR_MS) {
      seqState = SEQ_IDLE;  // sequence selesai
      curLeft = curRight = 0;
      setMotor(LPWM_CH, LLPWM_CH, 0);
      setMotor(RPWM_CH, RLPWM_CH, 0);
      Serial.println("[SEQ] Selesai! Kembali ke kontrol manual.");
      oledDirty = true;
    }
  }
}

// ============================================================
//  RENDER OLED
// ============================================================
#if USE_OLED
void drawOLED()
{
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (!Ps3.isConnected()) {
    display.setTextSize(1); display.setCursor(0,0);  display.println("PS3 TERPUTUS");
    display.setCursor(0,20); display.println("Menunggu koneksi...");
    display.display(); return;
  }

  if (sysState == ST_MENU) {
    display.setTextSize(1);
    display.setCursor(0,0);
    display.print("< "); display.print(menuTitle[menuPage]); display.println(" >");
    display.drawLine(0,10,127,10,SSD1306_WHITE);
    for (uint8_t i=0;i<menuLen[menuPage];i++){
      display.setCursor(6, 14 + i*12);
      display.print(i==menuItem ? "> " : "  ");
      display.print(menuItems[menuPage][i]);
      if (menuPage==2){ // tampilkan nilai setting
        display.setCursor(96, 14 + i*12);
        if (i==0) display.print(g_maxSpeed);
        if (i==1) { display.print(g_steerGain); display.print("%"); }
        if (i==2) display.print(g_rampStep);
        if (i==3) display.print(g_invert?"ON":"OFF");
      }
    }
    display.setCursor(0,56); display.print("L1/R1:menu START:OK");
  }
  else if (sysState == ST_RUN) {
    display.setTextSize(1); display.setCursor(0,0);
    display.print("MODE: ");
    display.println(menuItems[activeMenu][activeItem]);
    display.drawLine(0,10,127,10,SSD1306_WHITE);
    display.setCursor(0,16); display.print("Max:");   display.print(g_maxSpeed);
    display.print(" Str:"); display.print(g_steerGain); display.println("%");
    display.setCursor(0,28); display.print("L:"); display.print(curLeft);
    display.print("  R:"); display.print(curRight);
    if (seqState == SEQ_MUNDUR) {
      unsigned long sisa = (SEQ_MUNDUR_MS - (millis() - seqStart)) / 1000 + 1;
      display.setCursor(0,40); display.printf(">> MUNDUR... %lus", sisa);
    } else if (seqState == SEQ_PUTAR) {
      unsigned long sisa = (SEQ_PUTAR_MS - (millis() - seqStart)) / 1000 + 1;
      display.setCursor(0,40); display.printf(">> PUTAR KIRI... %lus", sisa);
    } else if (kickArmed) {
      display.setCursor(0,40); display.print("KICK ARMED (kotak)");
    }
    display.setCursor(0,56); display.print("SELECT:menu  X:mundur");
  }
  else { // IDLE
    display.setTextSize(2); display.setCursor(0,4);  display.println("READY");
    display.setTextSize(1); display.setCursor(0,30); display.println("Tekan SELECT");
    display.setCursor(0,42); display.println("untuk buka menu");
  }
  display.display();
}
#endif

// ============================================================
//  FUNGSI DASAR
// ============================================================
int applyDeadband(int v, int t){ return (abs(v) < t) ? 0 : v; }

int ramp(int cur, int tgt, int step){
  if (cur < tgt) return min(cur + step, tgt);
  if (cur > tgt) return max(cur - step, tgt);
  return cur;
}

void setMotor(uint8_t f, uint8_t r, int spd){
  if (spd > 0){ ledcWrite(f, spd);  ledcWrite(r, 0); }
  else if (spd < 0){ ledcWrite(f, 0); ledcWrite(r, -spd); }
  else { ledcWrite(f, 0); ledcWrite(r, 0); }
}

void stopMotorsSmooth(){
  curLeft  = ramp(curLeft,  0, g_rampStep);
  curRight = ramp(curRight, 0, g_rampStep);
  setMotor(LPWM_CH, LLPWM_CH, curLeft);
  setMotor(RPWM_CH, RLPWM_CH, curRight);
}

void handleKick(bool trigger){
  if (trigger && !kickActive){ kickActive = true; kickStart = millis(); digitalWrite(KICK_PIN, HIGH); }
  if (kickActive && (millis() - kickStart >= KICK_MS)){ kickActive = false; digitalWrite(KICK_PIN, LOW); }
}

// ============================================================
//  CETAK DATA KE SERIAL MONITOR
// ============================================================
void printSerial()
{
  if (!Ps3.isConnected()) { Serial.println("PS3: DISCONNECTED"); return; }

  const char* stName = (sysState==ST_IDLE) ? "IDLE" :
                       (sysState==ST_MENU) ? "MENU" : "RUN";

  if (sysState == ST_MENU) {
    Serial.printf("[%s] menu=%s > %s | set: max=%d steer=%d%% ramp=%d inv=%s\n",
      stName, menuTitle[menuPage], menuItems[menuPage][menuItem],
      g_maxSpeed, g_steerGain, g_rampStep, g_invert ? "ON" : "OFF");
  }
  else if (sysState == ST_RUN) {
    // Kecepatan aktif dari trigger
    int kec = g_maxSpeed * 50 / 100;
    if      (Ps3.data.button.r2) kec = g_maxSpeed;
    else if (Ps3.data.button.r1) kec = g_maxSpeed * 75 / 100;
    else if (Ps3.data.button.l2) kec = g_maxSpeed * 12 / 100;
    else if (Ps3.data.button.l1) kec = g_maxSpeed * 25 / 100;

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