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
//  MODE KECEPATAN
// ============================================================
const int SPD_NORMAL = 120; // default
const int SPD_SLOW   = 60;  // R1
const int SPD_TURBO  = 190; // R2

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

  // --- Pilih mode kecepatan langsung dari tombol (tanpa tabel kombo) ---
  int kec = SPD_NORMAL;
  if (Ps3.data.button.r2)      kec = SPD_TURBO;
  else if (Ps3.data.button.r1) kec = SPD_SLOW;

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
//////coba aja dulu yang bawah ini fix bisa tapi sering putus
// #include <Arduino.h>
// #include <Ps3Controller.h>

// #define MOTOR_TIMER 8

// #define MOTOR_FREQ 20000 // 20khz pwm freq

// #define LED 2
// #define KICK_PIN 18
// #define ON 1
// #define OFF 0

// #define ON 0
// #define OFF 1
// int data_stick = 0;
// int bulat = 1024, atas = 8, kanan = 4, kiri = 1, bawah = 2, x = 512, segitiga = 2048, kotak = 256;
// const int L1 = 8192;
// const int L2 = 32768;
// const int R2 = 16384;
// const int R1 = 4096;

// unsigned long timelast = 0;
// #define LPWM 25  // Right Motor RPWM//26 aio lama
// #define LLPWM 26 // Right Motor LPWM//27 aio lama

// // BTS7960 Left Motor Pins
// #define RPWM 32  // Left Motor RPWM//33 aio lama
// #define RLPWM 33 // Left Motor LPWM//25 aio lama

// // PWM Channels
// #define LPWM_CH 0
// #define LLPWM_CH 1
// #define RPWM_CH 2
// #define RLPWM_CH 3
// #define LED 2
// int slw = 120; // normal
// int slow = 60; // slow
// int fst = 190; // turbo

// int throttle = 0;
// int steering = 0;

// int kec = 0;

// // put function declarations here:
// int applyDeadband(int value, int threshold = 30);
// void setMotor(uint8_t fwd_ch, uint8_t rev_ch, int speed);
// void stopMotors();
// void stick_ON_PS3();
// void jalan();
// void kick(int statement);
// void stick(int timesampling);

// int applyDeadband(int value, int threshold)
// {
//   return (abs(value) < threshold) ? 0 : value;
// }

// void setup()
// {
//   // put your setup code here, to run once:
//   Serial.begin(115200);
//   Ps3.begin("f8:2f:a8:89:f9:83"); // mac address ps3
//   pinMode(LED, OUTPUT);

//   // Setup PWM channels
//   ledcSetup(LPWM_CH, MOTOR_FREQ, MOTOR_TIMER);
//   ledcAttachPin(LPWM, LPWM_CH);

//   ledcSetup(LLPWM_CH, MOTOR_FREQ, MOTOR_TIMER);
//   ledcAttachPin(LLPWM, LLPWM_CH);

//   ledcSetup(RPWM_CH, MOTOR_FREQ, MOTOR_TIMER);
//   ledcAttachPin(RPWM, RPWM_CH);

//   ledcSetup(RLPWM_CH, MOTOR_FREQ, MOTOR_TIMER);
//   ledcAttachPin(RLPWM, RLPWM_CH);
//   digitalWrite(LED, HIGH);
//   Serial.println("SETUP DONE");
//   stopMotors();
// }

// void loop()
// {
//   // put your main code here, to run repeatedly:
//   stick_ON_PS3();
//   jalan();
// }

// void stick(int timesampling)
// {
//   unsigned long timenow = millis();
//   if (timenow - timelast >= timesampling)
//   {
//     timelast = timenow;
//     stick_ON_PS3();
//   }
//   Serial.println(data_stick);
// }

// void kick(int statement)
// {
//   if (statement == 0)
//   {
//   }
// }

// void jalan()
// {
//   int leftSpeed, rightSpeed;

//   if (data_stick == 0)
//   {
//     kec = slw;
//   }
//   if (data_stick == R2)
//   {
//     kec = fst;
//   }
//   if (data_stick == R1)
//   {
//     kec = slow;
//   }
//   if (data_stick == R1 + R2)
//   {
//     kec = slow;
//   }
//   if (data_stick == kotak)
//   {
//     kick(ON);
//   }

//   steering = map(Ps3.data.analog.stick.rx, -128, 128, kec, kec * -1);
//   throttle = map(Ps3.data.analog.stick.ly, -128, 128, kec, kec * -1);
//   steering = applyDeadband(steering);
//   throttle = applyDeadband(throttle);
//   leftSpeed = throttle - steering;
//   rightSpeed = throttle + steering;
//   leftSpeed = constrain(leftSpeed, -255, 255);
//   rightSpeed = constrain(rightSpeed, -255, 255);
//   Serial.print("steering =");
//   Serial.print(steering);
//   Serial.print("   ");
//   Serial.print("throttle =");
//   Serial.print(throttle);
//   Serial.print("   ");
//   Serial.print("MOTOR =");
//   Serial.print(leftSpeed);
//   Serial.print("   ");
//   Serial.print(rightSpeed);
//   Serial.print("   ");
//   Serial.print("data_stick = ");
//   Serial.print(data_stick);
//   Serial.println(" ");
//   setMotor(LPWM_CH, LLPWM_CH, leftSpeed);
//   setMotor(RPWM_CH, RLPWM_CH, rightSpeed);
//   data_stick = 0;
//   delay(5);
// }

// void setMotor(uint8_t fwd_ch, uint8_t rev_ch, int speed)
// {
//   if (speed > 0)
//   {
//     ledcWrite(fwd_ch, speed);
//     ledcWrite(rev_ch, 0);
//   }
//   else if (speed < 0)
//   {
//     ledcWrite(fwd_ch, 0);
//     ledcWrite(rev_ch, -speed);
//   }
//   else
//   {
//     ledcWrite(fwd_ch, 0);
//     ledcWrite(rev_ch, 0);
//   }
// }

// void stopMotors()
// {
//   setMotor(LPWM_CH, LLPWM_CH, 0);
//   setMotor(RPWM_CH, RLPWM_CH, 0);
// }

// void stick_ON_PS3()
// {
//   if (Ps3.isConnected())
//   {

//     if (Ps3.data.button.right)
//     {
//       data_stick = kanan;
//     }
//     if (Ps3.data.button.left)
//     {
//       data_stick = kiri;
//     }
//     if (Ps3.data.button.up)
//     {
//       data_stick = atas;
//     }
//     if (Ps3.data.button.down)
//     {
//       data_stick = bawah;
//     }
//     if (Ps3.data.button.cross)
//     {
//       data_stick = x;
//     }
//     if (Ps3.data.button.circle)
//     {
//       data_stick = bulat;
//     }
//     if (Ps3.data.button.l1)
//     {
//       data_stick = L1;
//     }
//     if (Ps3.data.button.l2)
//     {
//       data_stick = L2;
//     }
//     if (Ps3.data.button.r1)
//     {
//       data_stick = R1;
//     }
//     if (Ps3.data.button.r2)
//     {
//       data_stick = R2;
//     }
//     if (Ps3.data.button.triangle)
//     {
//       data_stick = segitiga;
//     }

//     if (Ps3.data.button.r1 && Ps3.data.button.r2)
//     {
//       data_stick = R1 + R2;
//     }

//     if (Ps3.data.button.right && Ps3.data.button.cross)
//     {
//       data_stick = kanan + x;
//     }
//     if (Ps3.data.button.left && Ps3.data.button.cross)
//     {
//       data_stick = kiri + x;
//     }
//     // if( Ps3.data.button.up && Ps3.data.button.cross)        {data_stick = atas+x;}
//     // if( Ps3.data.button.cross && Ps3.data.button.up)        {data_stick = x+atas;}
//     if (Ps3.data.button.down && Ps3.data.button.cross)
//     {
//       data_stick = bawah + x;
//     }

//     if (Ps3.data.button.right && Ps3.data.button.l2)
//     {
//       data_stick = kanan + L2;
//     }
//     if (Ps3.data.button.left && Ps3.data.button.l2)
//     {
//       data_stick = kiri + L2;
//     }
//     if (Ps3.data.button.cross && Ps3.data.button.l2)
//     {
//       data_stick = x + L2;
//     }
//     if (Ps3.data.button.down && Ps3.data.button.l2)
//     {
//       data_stick = bawah + L2;
//     }

//     if (Ps3.data.button.right && Ps3.data.button.l1)
//     {
//       data_stick = kanan + L1;
//     }
//     if (Ps3.data.button.left && Ps3.data.button.l1)
//     {
//       data_stick = kiri + L1;
//     }
//     if (Ps3.data.button.cross && Ps3.data.button.l1)
//     {
//       data_stick = x + L1;
//     }
//     if (Ps3.data.button.down && Ps3.data.button.l1)
//     {
//       data_stick = bawah + L1;
//     }

//     if (Ps3.data.button.right && Ps3.data.button.l2 && Ps3.data.button.l1)
//     {
//       data_stick = kanan + L2 + L1;
//     }
//     if (Ps3.data.button.left && Ps3.data.button.l2 && Ps3.data.button.l1)
//     {
//       data_stick = kiri + L2 + L1;
//     }
//     if (Ps3.data.button.cross && Ps3.data.button.l2 && Ps3.data.button.l1)
//     {
//       data_stick = x + L2 + L1;
//     }
//     if (Ps3.data.button.down && Ps3.data.button.l2 && Ps3.data.button.l1)
//     {
//       data_stick = bawah + L2 + L1;
//     }

//     if (Ps3.data.button.right && Ps3.data.button.cross && Ps3.data.button.l1)
//     {
//       data_stick = kanan + x + L1;
//     }
//     if (Ps3.data.button.left && Ps3.data.button.cross && Ps3.data.button.l1)
//     {
//       data_stick = kiri + x + L1;
//     }
//     if (Ps3.data.button.down && Ps3.data.button.cross && Ps3.data.button.l1)
//     {
//       data_stick = bawah + x + L1;
//     }

//     if (Ps3.data.button.right && Ps3.data.button.cross && Ps3.data.button.l2)
//     {
//       data_stick = kanan + x + L2;
//     }
//     if (Ps3.data.button.left && Ps3.data.button.cross && Ps3.data.button.l2)
//     {
//       data_stick = kiri + x + L2;
//     }
//     if (Ps3.data.button.down && Ps3.data.button.cross && Ps3.data.button.l2)
//     {
//       data_stick = bawah + x + L2;
//     }

//     if (Ps3.data.button.right && Ps3.data.button.cross && Ps3.data.button.l1 && Ps3.data.button.l2)
//     {
//       data_stick = kanan + x + L1 + L2;
//     }
//     if (Ps3.data.button.left && Ps3.data.button.cross && Ps3.data.button.l1 && Ps3.data.button.l2)
//     {
//       data_stick = kiri + x + L1 + L2;
//     }
//     if (Ps3.data.button.down && Ps3.data.button.cross && Ps3.data.button.l1 && Ps3.data.button.l2)
//     {
//       data_stick = bawah + x + L1 + L2;
//     }

//     if (Ps3.data.button.right && Ps3.data.button.l1 && Ps3.data.button.down && Ps3.data.button.cross)
//     {
//       data_stick = kanan + L1 + bawah + x;
//     }
//     if (Ps3.data.button.left && Ps3.data.button.l1 && Ps3.data.button.down && Ps3.data.button.cross)
//     {
//       data_stick = kiri + L1 + bawah + x;
//     }
//     if (Ps3.data.button.right && Ps3.data.button.l2 && Ps3.data.button.down && Ps3.data.button.cross)
//     {
//       data_stick = kanan + L2 + bawah + x;
//     }
//     if (Ps3.data.button.left && Ps3.data.button.l2 && Ps3.data.button.down && Ps3.data.button.cross)
//     {
//       data_stick = kiri + L2 + bawah + x;
//     }

//     if (Ps3.data.button.right && Ps3.data.button.cross && Ps3.data.button.l1 && Ps3.data.button.l2 && Ps3.data.button.down)
//     {
//       data_stick = kanan + x + L1 + L2 + bawah;
//     }
//     if (Ps3.data.button.left && Ps3.data.button.cross && Ps3.data.button.l1 && Ps3.data.button.l2 && Ps3.data.button.down)
//     {
//       data_stick = kiri + x + L1 + L2 + bawah;
//     }
//     if (Ps3.data.button.cross && Ps3.data.button.l1 && Ps3.data.button.l2 && Ps3.data.button.down)
//     {
//       data_stick = bawah + x + L1 + L2 + bawah;
//     }
//     /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//     if (Ps3.data.button.right && Ps3.data.button.up)
//     {
//       data_stick = kanan + atas;
//     }
//     if (Ps3.data.button.left && Ps3.data.button.up)
//     {
//       data_stick = kiri + atas;
//     }
//     if (Ps3.data.button.down && Ps3.data.button.up)
//     {
//       data_stick = bawah + atas;
//     }
//     if (Ps3.data.button.cross && Ps3.data.button.up)
//     {
//       data_stick = x + atas;
//     }
//     if (Ps3.data.button.circle && Ps3.data.button.up)
//     {
//       data_stick = bulat + atas;
//     }
//     if (Ps3.data.button.l1 && Ps3.data.button.up)
//     {
//       data_stick = L1 + atas;
//     }
//     if (Ps3.data.button.l2 && Ps3.data.button.up)
//     {
//       data_stick = L2 + atas;
//     }
//     if (Ps3.data.button.r1 && Ps3.data.button.up)
//     {
//       data_stick = R1 + atas;
//     }
//     if (Ps3.data.button.r2 && Ps3.data.button.up)
//     {
//       data_stick = R2 + atas;
//     }
//     if (Ps3.data.button.triangle && Ps3.data.button.up)
//     {
//       data_stick = segitiga + atas;
//     }

//     if (Ps3.data.button.right && Ps3.data.button.cross && Ps3.data.button.up)
//     {
//       data_stick = kanan + x + atas;
//     }
//     if (Ps3.data.button.left && Ps3.data.button.cross && Ps3.data.button.up)
//     {
//       data_stick = kiri + x + atas;
//     }
//     // if( Ps3.data.button.up && Ps3.data.button.cross && Ps3.data.button.up)        {data_stick = atas+x+atas;}
//     if (Ps3.data.button.down && Ps3.data.button.cross && Ps3.data.button.up)
//     {
//       data_stick = bawah + x + atas;
//     }

//     if (Ps3.data.button.right && Ps3.data.button.l2 && Ps3.data.button.up)
//     {
//       data_stick = kanan + L2 + atas;
//     }
//     if (Ps3.data.button.left && Ps3.data.button.l2 && Ps3.data.button.up)
//     {
//       data_stick = kiri + L2 + atas;
//     }
//     if (Ps3.data.button.cross && Ps3.data.button.l2 && Ps3.data.button.up)
//     {
//       data_stick = x + L2 + atas;
//     }
//     if (Ps3.data.button.down && Ps3.data.button.l2 && Ps3.data.button.up)
//     {
//       data_stick = bawah + L2 + atas;
//     }

//     if (Ps3.data.button.right && Ps3.data.button.l1 && Ps3.data.button.up)
//     {
//       data_stick = kanan + L1 + atas;
//     }
//     if (Ps3.data.button.left && Ps3.data.button.l1 && Ps3.data.button.up)
//     {
//       data_stick = kiri + L1 + atas;
//     }
//     if (Ps3.data.button.cross && Ps3.data.button.l1 && Ps3.data.button.up)
//     {
//       data_stick = x + L1 + atas;
//     }
//     if (Ps3.data.button.down && Ps3.data.button.l1 && Ps3.data.button.up)
//     {
//       data_stick = bawah + L1 + atas;
//     }

//     if (Ps3.data.button.right && Ps3.data.button.l2 && Ps3.data.button.l1 && Ps3.data.button.up)
//     {
//       data_stick = kanan + L2 + L1 + atas;
//     }
//     if (Ps3.data.button.left && Ps3.data.button.l2 && Ps3.data.button.l1 && Ps3.data.button.up)
//     {
//       data_stick = kiri + L2 + L1 + atas;
//     }
//     if (Ps3.data.button.cross && Ps3.data.button.l2 && Ps3.data.button.l1 && Ps3.data.button.up)
//     {
//       data_stick = x + L2 + L1 + atas;
//     }
//     if (Ps3.data.button.down && Ps3.data.button.l2 && Ps3.data.button.l1 && Ps3.data.button.up)
//     {
//       data_stick = bawah + L2 + L1 + atas;
//     }

//     if (Ps3.data.button.right && Ps3.data.button.cross && Ps3.data.button.l1 && Ps3.data.button.up)
//     {
//       data_stick = kanan + x + L1 + atas;
//     }
//     if (Ps3.data.button.left && Ps3.data.button.cross && Ps3.data.button.l1 && Ps3.data.button.up)
//     {
//       data_stick = kiri + x + L1 + atas;
//     }
//     if (Ps3.data.button.down && Ps3.data.button.cross && Ps3.data.button.l1 && Ps3.data.button.up)
//     {
//       data_stick = bawah + x + L1 + atas;
//     }

//     if (Ps3.data.button.right && Ps3.data.button.cross && Ps3.data.button.l2 && Ps3.data.button.up)
//     {
//       data_stick = kanan + x + L2 + atas;
//     }
//     if (Ps3.data.button.left && Ps3.data.button.cross && Ps3.data.button.l2 && Ps3.data.button.up)
//     {
//       data_stick = kiri + x + L2 + atas;
//     }
//     if (Ps3.data.button.down && Ps3.data.button.cross && Ps3.data.button.l2 && Ps3.data.button.up)
//     {
//       data_stick = bawah + x + L2 + atas;
//     }

//     if (Ps3.data.button.right && Ps3.data.button.cross && Ps3.data.button.l1 && Ps3.data.button.l2 && Ps3.data.button.up)
//     {
//       data_stick = kanan + x + L1 + L2 + atas;
//     }
//     if (Ps3.data.button.left && Ps3.data.button.cross && Ps3.data.button.l1 && Ps3.data.button.l2 && Ps3.data.button.up)
//     {
//       data_stick = kiri + x + L1 + L2 + atas;
//     }
//     if (Ps3.data.button.down && Ps3.data.button.cross && Ps3.data.button.l1 && Ps3.data.button.l2 && Ps3.data.button.up)
//     {
//       data_stick = bawah + x + L1 + L2 + atas;
//     }

//     if (Ps3.data.button.right && Ps3.data.button.l1 && Ps3.data.button.down && Ps3.data.button.cross && Ps3.data.button.up)
//     {
//       data_stick = kanan + L1 + bawah + x + atas;
//     }
//     if (Ps3.data.button.left && Ps3.data.button.l1 && Ps3.data.button.down && Ps3.data.button.cross && Ps3.data.button.up)
//     {
//       data_stick = kiri + L1 + bawah + x + atas;
//     }
//     if (Ps3.data.button.right && Ps3.data.button.l2 && Ps3.data.button.down && Ps3.data.button.cross && Ps3.data.button.up)
//     {
//       data_stick = kanan + L2 + bawah + x + atas;
//     }
//     if (Ps3.data.button.left && Ps3.data.button.l2 && Ps3.data.button.down && Ps3.data.button.cross && Ps3.data.button.up)
//     {
//       data_stick = kiri + L2 + bawah + x + atas;
//     }

//     if (Ps3.data.button.right && Ps3.data.button.cross && Ps3.data.button.l1 && Ps3.data.button.l2 && Ps3.data.button.down && Ps3.data.button.up)
//     {
//       data_stick = kanan + x + L1 + L2 + bawah + atas;
//     }
//     if (Ps3.data.button.left && Ps3.data.button.cross && Ps3.data.button.l1 && Ps3.data.button.l2 && Ps3.data.button.down && Ps3.data.button.up)
//     {
//       data_stick = kiri + x + L1 + L2 + bawah + atas;
//     }
//     if (Ps3.data.button.cross && Ps3.data.button.l1 && Ps3.data.button.l2 && Ps3.data.button.down && Ps3.data.button.up)
//     {
//       data_stick = bawah + x + L1 + L2 + bawah + atas;
//     }
//   }
// }