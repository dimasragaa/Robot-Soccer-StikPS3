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

int applyDeadband(int v, int t){ return (abs(v) < t) ? 0 : v; }

int ramp(int cur, int tgt, int step){
  if (cur < tgt) return min(cur + step, tgt);
  if (cur > tgt) return max(cur - step, tgt);
  return cur;
}

// Driver BTN: 2 pin arah (D1/D2) + 1 pin PWM kecepatan.
void setMotor(uint8_t d1, uint8_t d2, uint8_t ch, int spd){
  spd = constrain(spd, -255, 255);
  if (spd > 0){
    digitalWrite(d1, HIGH); digitalWrite(d2, LOW);
    ledcWrite(ch, spd);
  } else if (spd < 0){
    digitalWrite(d1, LOW);  digitalWrite(d2, HIGH);
    ledcWrite(ch, -spd);
  } else {
    digitalWrite(d1, LOW);  digitalWrite(d2, LOW);
    ledcWrite(ch, 0);
  }
}

void setMotorL(int spd){ setMotor(M1_D1, M1_D2, M1_CH, spd); }  // Motor 1
void setMotorR(int spd){ setMotor(M2_D3, M2_D4, M2_CH, spd); }  // Motor 2

void stopMotorsSmooth(){
  curLeft  = ramp(curLeft,  0, g_rampStep);
  curRight = ramp(curRight, 0, g_rampStep);
  setMotorL(curLeft);
  setMotorR(curRight);
}
