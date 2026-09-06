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
//  PARAMETER LIVE (di-set oleh preset saat mulai main)
// ============================================================
int  g_maxSpeed  = 255;
int  g_steerGain = 100;
int  g_rampStep  = 6;
bool g_invert    = false;

const int DEADBAND = 12;
const unsigned long CONTROL_MS = 5;
const unsigned long OLED_MS    = 120;
const unsigned long PRINT_MS   = 200;

// ============================================================
//  STATE MACHINE
// ============================================================
enum SysState { ST_IDLE, ST_MENU, ST_RUN };
SysState sysState = ST_IDLE;
bool hasActiveMode = false;

// --- Menu: 2 halaman ---
const uint8_t MENU_COUNT = 2;
const char* menuTitle[MENU_COUNT] = { "JENIS GAME", "HADAP KE" };
const uint8_t menuLen[MENU_COUNT]  = { 2, 2 };
const char* menuItems[MENU_COUNT][2] = {
  { "Sumo", "Soccer" },
  { "Hadap Gawang", "Hadap Bola" }
};

uint8_t menuPage = 0;          // halaman menu aktif
uint8_t sel[MENU_COUNT] = {0, 0}; // pilihan tersimpan per halaman
uint8_t activeGame = 0;        // 0=Sumo, 1=Soccer
uint8_t activeOrient = 0;      // 0=Gawang, 1=Bola

// ============================================================
//  MOTOR & KICK STATE
// ============================================================
int curLeft = 0, curRight = 0;
bool kickArmed = false;

bool kickActive = false;
unsigned long kickStart = 0;
const unsigned long KICK_MS = 120;

unsigned long lastControl = 0, lastOled = 0, lastPrint = 0;
bool oledDirty = true;

// ============================================================
//  EDGE DETECTION TOMBOL
// ============================================================
bool pSelect=0, pStart=0, pUp=0, pDown=0, pL1=0, pR1=0, pCircle=0;
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
void drawOLED();
void printSerial();

// ============================================================
//  CALLBACK KONEKSI
// ============================================================
void onConnect(){ digitalWrite(LED, HIGH); Serial.println(">> PS3 CONNECTED"); oledDirty = true; }

// ============================================================
//  SETUP
// ============================================================
void setup()
{
  Serial.begin(115200);

  // Opsional: matikan brownout detector kalau ESP32 reset saat motor nyentak.
  // #include "soc/rtc_cntl_reg.h"
  // WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  setCpuFrequencyMhz(240);

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
  Wire.setClock(400000);
  oledOK = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (oledOK){ display.clearDisplay(); display.display(); }
  else Serial.println("OLED tidak terdeteksi, lanjut tanpa OLED");
#endif

  Ps3.attachOnConnect(onConnect);
  Ps3.begin("f8:2f:a8:89:f9:83");
  Serial.println("SETUP DONE");
}

// ============================================================
//  LOOP
// ============================================================
void loop()
{
  handleKick(false);

  unsigned long now = millis();
  if (now - lastControl >= CONTROL_MS)
  {
    lastControl = now;

    if (!Ps3.isConnected())
    {
      stopMotorsSmooth();
    }
    else
    {
      bool eSelect = edge(Ps3.data.button.select, pSelect);
      bool eStart  = edge(Ps3.data.button.start,  pStart);
      bool eUp     = edge(Ps3.data.button.up,     pUp);
      bool eDown   = edge(Ps3.data.button.down,   pDown);
      bool eL1     = edge(Ps3.data.button.l1,     pL1);
      bool eR1     = edge(Ps3.data.button.r1,     pR1);
      bool eCircle = edge(Ps3.data.button.circle, pCircle);

      // SELECT: buka / tutup menu
      if (eSelect){
        if (sysState == ST_MENU) sysState = hasActiveMode ? ST_RUN : ST_IDLE;
        else { sysState = ST_MENU; menuPage = 0; }
        oledDirty = true;
      }

      if (sysState == ST_MENU)
      {
        stopMotorsSmooth(); // motor mati selama di menu

        if (eL1 || eR1){ menuPage = (menuPage + 1) % MENU_COUNT; oledDirty = true; } // pindah halaman
        if (eUp)  { sel[menuPage] = (sel[menuPage] + menuLen[menuPage] - 1) % menuLen[menuPage]; oledDirty = true; }
        if (eDown){ sel[menuPage] = (sel[menuPage] + 1) % menuLen[menuPage];                     oledDirty = true; }

        if (eCircle){ sysState = hasActiveMode ? ST_RUN : ST_IDLE; oledDirty = true; } // batal

        if (eStart) // START = konfirmasi -> MAIN
        {
          hasActiveMode = true;
          applyModePreset();          // set game + orientasi + parameter
          sysState = ST_RUN;
          oledDirty = true;
        }
      }
      else if (sysState == ST_RUN)
      {
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
      else // ST_IDLE
      {
        stopMotorsSmooth();
      }
    }
  }

#if USE_OLED
  if (oledOK && (oledDirty || (now - lastOled >= OLED_MS))){ lastOled = now; oledDirty = false; drawOLED(); }
#endif

  if (now - lastPrint >= PRINT_MS){ lastPrint = now; printSerial(); }

  delay(1); // yield ke scheduler -> Bluetooth lebih stabil
}

// ============================================================
//  PRESET SAAT MULAI MAIN
// ============================================================
void applyModePreset()
{
  activeGame   = sel[0]; // 0=Sumo, 1=Soccer
  activeOrient = sel[1]; // 0=Gawang, 1=Bola

  if (activeGame == 0) {            // SUMO
    g_maxSpeed = 255; g_rampStep = 8; g_steerGain = 100; kickArmed = false;
  } else {                          // SOCCER
    g_maxSpeed = 255; g_rampStep = 6; g_steerGain = 100; kickArmed = true;
  }
  // Orientasi (Gawang/Bola) saat ini sebatas label.
  // Untuk auto-hadap butuh sensor arah (IMU/kompas seperti MPU6050).
}

// ============================================================
//  RENDER OLED
// ============================================================
#if USE_OLED
void drawOLED()
{
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (!Ps3.isConnected()){
    display.setTextSize(1); display.setCursor(0,0);  display.println("PS3 TERPUTUS");
    display.setCursor(0,20); display.println("Menunggu koneksi...");
    display.display(); return;
  }

  if (sysState == ST_MENU){
    display.setTextSize(1);
    display.setCursor(0,0);
    display.printf("MENU %d/%d: %s", menuPage+1, MENU_COUNT, menuTitle[menuPage]);
    display.drawLine(0,10,127,10,SSD1306_WHITE);
    for (uint8_t i=0;i<menuLen[menuPage];i++){
      display.setCursor(6, 16 + i*12);
      display.print(i==sel[menuPage] ? "> " : "  ");
      display.print(menuItems[menuPage][i]);
    }
    display.setCursor(0,44);
    display.printf("Pilih: %s / %s", menuItems[0][sel[0]], menuItems[1][sel[1]]);
    display.setCursor(0,56); display.print("L1/R1:hal  START:MAIN");
  }
  else if (sysState == ST_RUN){
    display.setTextSize(1); display.setCursor(0,0);
    display.printf("MAIN: %s", menuItems[0][activeGame]);
    display.drawLine(0,10,127,10,SSD1306_WHITE);
    display.setCursor(0,16); display.printf("Hadap: %s", menuItems[1][activeOrient]);
    display.setCursor(0,30); display.printf("L:%4d  R:%4d", curLeft, curRight);
    if (kickArmed){ display.setCursor(0,42); display.print("KICK ARMED (kotak)"); }
    display.setCursor(0,56); display.print("SELECT: buka menu");
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
//  CETAK KE SERIAL MONITOR
// ============================================================
void printSerial()
{
  if (!Ps3.isConnected()){ Serial.println("PS3: DISCONNECTED"); return; }

  if (sysState == ST_MENU){
    Serial.printf("[MENU] hal=%d/%d %s | pilih: game=%s hadap=%s\n",
      menuPage+1, MENU_COUNT, menuTitle[menuPage],
      menuItems[0][sel[0]], menuItems[1][sel[1]]);
  }
  else if (sysState == ST_RUN){
    int kec = g_maxSpeed * 50 / 100;
    if      (Ps3.data.button.r2) kec = g_maxSpeed;
    else if (Ps3.data.button.r1) kec = g_maxSpeed * 75 / 100;
    else if (Ps3.data.button.l2) kec = g_maxSpeed * 12 / 100;
    else if (Ps3.data.button.l1) kec = g_maxSpeed * 25 / 100;

    Serial.printf("[RUN] game=%s hadap=%s | rx=%4d ly=%4d | kec=%3d | L=%4d R=%4d | kick=%s\n",
      menuItems[0][activeGame], menuItems[1][activeOrient],
      Ps3.data.analog.stick.rx, Ps3.data.analog.stick.ly,
      kec, curLeft, curRight, kickArmed ? "ARM" : "off");
  }
  else {
    Serial.println("[IDLE] tekan SELECT untuk buka menu (stik nonaktif)");
  }
}