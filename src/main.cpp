#include <Arduino.h>
#include <Ps3Controller.h>

// ============================================================
//  KONFIGURASI PWM
// ============================================================
#define MOTOR_FREQ 20000 // 20 kHz PWM (di atas ambang dengar, halus)
#define MOTOR_RES  8      // resolusi 8-bit -> nilai 0..255

// --- Left Motor (BTS7960) ---
#define RPWM  32
#define RLPWM 33
// --- Right Motor (BTS7960) ---
#define LPWM  25
#define LLPWM 26

// --- Channel LEDC ---
#define LPWM_CH  0
#define LLPWM_CH 1
#define RPWM_CH  2
#define RLPWM_CH 3

#define LED      2
#define KICK_PIN 18 // opsional: solenoid/penendang (aktif HIGH, ubah bila perlu)

// ============================================================
//  MODE KECEPATAN  (sebagai % dari kecepatan maksimal)
// ============================================================
const int MAX_SPEED = 255; // kecepatan maksimal (ceiling PWM, jangan > 255)

// Tangga kecepatan -> ubah angka persen sesuai selera
const int SPD_L2     = MAX_SPEED * 12  / 100; // ~31  : paling lambat
const int SPD_L1     = MAX_SPEED * 25  / 100; // ~64  : pelan (1/2 normal)
const int SPD_NORMAL = MAX_SPEED * 50  / 100; // ~128 : default
const int SPD_R1     = MAX_SPEED * 75  / 100; // ~191 : boost 50% (1/2 menuju max)
const int SPD_R2     = MAX_SPEED * 100 / 100; // 255  : boost 100% (penuh)

// ============================================================
//  PARAMETER KEHALUSAN  (tuning di sini)
// ============================================================
const int DEADBAND  = 12; // zona mati stik (redam noise/drift di tengah)
const int RAMP_STEP = 6;  // perubahan PWM max per update.
                          //  -> kecil = makin mulus tapi respon lebih lembut
                          //  -> besar = makin responsif tapi lebih menyentak
const unsigned long CONTROL_MS = 5;   // periode loop kontrol (ms)
const unsigned long PRINT_MS   = 250; // periode cetak serial (jangan spam!)

// ============================================================
//  STATE
// ============================================================
int curLeft  = 0; // output aktual motor kiri (setelah di-ramp)
int curRight = 0; // output aktual motor kanan (setelah di-ramp)

unsigned long lastControl = 0;
unsigned long lastPrint   = 0;

// kick non-blocking
bool kickActive = false;
unsigned long kickStart = 0;
const unsigned long KICK_MS = 120; // durasi pulsa tendang

// ============================================================
//  DEKLARASI FUNGSI
// ============================================================
int  applyDeadband(int value, int threshold);
int  ramp(int current, int target, int step);
void setMotor(uint8_t fwd_ch, uint8_t rev_ch, int speed);
void stopMotors();
void handleKick(bool trigger);

// ============================================================
//  SETUP
// ============================================================
void setup()
{
  Serial.begin(115200);

  pinMode(LED, OUTPUT);
  pinMode(KICK_PIN, OUTPUT);
  digitalWrite(KICK_PIN, LOW);

  // Setup 4 channel PWM
  ledcSetup(LPWM_CH,  MOTOR_FREQ, MOTOR_RES);
  ledcAttachPin(LPWM,  LPWM_CH);
  ledcSetup(LLPWM_CH, MOTOR_FREQ, MOTOR_RES);
  ledcAttachPin(LLPWM, LLPWM_CH);
  ledcSetup(RPWM_CH,  MOTOR_FREQ, MOTOR_RES);
  ledcAttachPin(RPWM,  RPWM_CH);
  ledcSetup(RLPWM_CH, MOTOR_FREQ, MOTOR_RES);
  ledcAttachPin(RLPWM, RLPWM_CH);

  stopMotors();

  Ps3.begin("f8:2f:a8:89:f9:83"); // MAC address ESP32 (sesuaikan)
  digitalWrite(LED, HIGH);
  Serial.println("SETUP DONE");
}

// ============================================================
//  LOOP  -- ringan, tanpa delay, berbasis millis
// ============================================================
void loop()
{
  unsigned long now = millis();

  // jaga pulsa tendang tetap non-blocking
  handleKick(false);

  // jalankan kontrol motor tiap CONTROL_MS
  if (now - lastControl < CONTROL_MS) return;
  lastControl = now;

  // --- Jika controller putus: berhenti mulus, LED mati ---
  if (!Ps3.isConnected())
  {
    curLeft  = ramp(curLeft,  0, RAMP_STEP);
    curRight = ramp(curRight, 0, RAMP_STEP);
    setMotor(LPWM_CH, LLPWM_CH, curLeft);
    setMotor(RPWM_CH, RLPWM_CH, curRight);
    digitalWrite(LED, LOW);
    return;
  }
  digitalWrite(LED, HIGH);

  // --- Pilih mode kecepatan langsung dari tombol (prioritas: R2>R1>L2>L1) ---
  int kec = SPD_NORMAL;
  if (Ps3.data.button.r2)      kec = SPD_R2; // boost penuh
  else if (Ps3.data.button.r1) kec = SPD_R1; // boost 50%
  else if (Ps3.data.button.l2) kec = SPD_L2; // paling lambat
  else if (Ps3.data.button.l1) kec = SPD_L1; // pelan

  // --- Tombol kotak = tendang ---
  handleKick(Ps3.data.button.square);

  // --- Baca stik (mentah -128..127) ---
  int rawSteer = Ps3.data.analog.stick.rx;
  int rawThr   = Ps3.data.analog.stick.ly;

  // deadband pada stik MENTAH -> titik tengah bersih
  rawSteer = applyDeadband(rawSteer, DEADBAND);
  rawThr   = applyDeadband(rawThr,   DEADBAND);

  // skala ke -kec..kec (arah dipertahankan sama seperti kode asli)
  int steering = map(rawSteer, -128, 128, kec, -kec);
  int throttle = map(rawThr,   -128, 128, kec, -kec);

  // --- Arcade mixing (sama seperti kode kamu) ---
  int targetLeft  = throttle - steering;
  int targetRight = throttle + steering;
  targetLeft  = constrain(targetLeft,  -255, 255);
  targetRight = constrain(targetRight, -255, 255);

  // --- RAMPING: inti kehalusan. Geser bertahap ke target ---
  curLeft  = ramp(curLeft,  targetLeft,  RAMP_STEP);
  curRight = ramp(curRight, targetRight, RAMP_STEP);

  setMotor(LPWM_CH, LLPWM_CH, curLeft);
  setMotor(RPWM_CH, RLPWM_CH, curRight);

  // --- Serial di-throttle (tidak tiap loop) ---
  if (now - lastPrint >= PRINT_MS)
  {
    lastPrint = now;
    Serial.printf("thr=%4d str=%4d | L=%4d R=%4d\n",
                  throttle, steering, curLeft, curRight);
  }
}

// ============================================================
//  FUNGSI PENDUKUNG
// ============================================================
int applyDeadband(int value, int threshold)
{
  return (abs(value) < threshold) ? 0 : value;
}

// geser 'current' menuju 'target' maksimal 'step' per panggilan
int ramp(int current, int target, int step)
{
  if (current < target) return min(current + step, target);
  if (current > target) return max(current - step, target);
  return current;
}

void setMotor(uint8_t fwd_ch, uint8_t rev_ch, int speed)
{
  if (speed > 0)
  {
    ledcWrite(fwd_ch, speed);
    ledcWrite(rev_ch, 0);
  }
  else if (speed < 0)
  {
    ledcWrite(fwd_ch, 0);
    ledcWrite(rev_ch, -speed);
  }
  else
  {
    ledcWrite(fwd_ch, 0);
    ledcWrite(rev_ch, 0);
  }
}

void stopMotors()
{
  curLeft = curRight = 0;
  setMotor(LPWM_CH, LLPWM_CH, 0);
  setMotor(RPWM_CH, RLPWM_CH, 0);
}

// pulsa tendang non-blocking
void handleKick(bool trigger)
{
  if (trigger && !kickActive)
  {
    kickActive = true;
    kickStart  = millis();
    digitalWrite(KICK_PIN, HIGH);
  }
  if (kickActive && (millis() - kickStart >= KICK_MS))
  {
    kickActive = false;
    digitalWrite(KICK_PIN, LOW);
  }
}