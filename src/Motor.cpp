// By: github.com/dimasragaa — IG: @dmsragaa
#include "Motor.h"
#include "Config.h"
#include "State.h"

void motorSetup()
{
  // Pin arah = output digital biasa
  pinMode(M1_D1, OUTPUT); pinMode(M1_D2, OUTPUT);
  pinMode(M2_D3, OUTPUT); pinMode(M2_D4, OUTPUT);
  digitalWrite(M1_D1, LOW); digitalWrite(M1_D2, LOW);
  digitalWrite(M2_D3, LOW); digitalWrite(M2_D4, LOW);

  // Pin kecepatan = PWM (LEDC)
  ledcSetup(M1_CH, MOTOR_FREQ, MOTOR_RES); ledcAttachPin(M1_PWM, M1_CH);
  ledcSetup(M2_CH, MOTOR_FREQ, MOTOR_RES); ledcAttachPin(M2_PWM, M2_CH);
  ledcWrite(M1_CH, 0); ledcWrite(M2_CH, 0);

  stopMotorsSmooth();
}

int ramp(int cur, int tgt, int step){
  if (cur < tgt) return min(cur + step, tgt);
  if (cur > tgt) return max(cur - step, tgt);
  return cur;
}

// Driver BTN: 2 pin arah (D1/D2) + 1 pin PWM kecepatan.
// Cache arah terakhir per kanal -> skip digitalWrite kalau arah tidak berubah
// (digitalWrite GPIO cukup mahal, dipanggil tiap 2ms jadi penghematan nyata)
static int8_t lastDir[2] = {0, 0};  // indeks = kanal (M1_CH / M2_CH)

void setMotor(uint8_t d1, uint8_t d2, uint8_t ch, int spd){
  spd = constrain(spd, -255, 255);
  int8_t dir = (spd > 0) ? 1 : (spd < 0) ? -1 : 0;

  if (dir != lastDir[ch]) {   // arah berubah -> update pin arah
    lastDir[ch] = dir;
    if (dir > 0){
      digitalWrite(d1, HIGH); digitalWrite(d2, LOW);
    } else if (dir < 0){
      digitalWrite(d1, LOW);  digitalWrite(d2, HIGH);
    } else {
      digitalWrite(d1, LOW);  digitalWrite(d2, LOW);
    }
  }
  ledcWrite(ch, (dir == 0) ? 0 : abs(spd));
}

void setMotorL(int spd){ setMotor(M1_D1, M1_D2, M1_CH, spd); }  // Motor 1
void setMotorR(int spd){ setMotor(M2_D3, M2_D4, M2_CH, spd); }  // Motor 2

void stopMotorsSmooth(){
  // Sudah benar-benar diam -> tidak perlu apa-apa. Penjaga ini di dalam
  // sini, bukan di tiap pemanggil, supaya semua jalur (stik putus, menu,
  // mode diam) ikut terlindungi dan tidak ada yang kelewat. Tanpa ini
  // motor terus diperintah "berhenti" tiap 2 ms padahal sudah berhenti.
  if (curLeft == 0 && curRight == 0) return;

  curLeft  = ramp(curLeft,  0, g_rampStep);
  curRight = ramp(curRight, 0, g_rampStep);
  setMotorL(curLeft);
  setMotorR(curRight);
}