#pragma once
#include <Arduino.h>

// ============================================================
//  Motor.h  —  KENDALI HARDWARE MOTOR (driver BTN)
//
//  By: github.com/dimasragaa — IG: @dmsragaa
// ============================================================

// Siapkan pin arah + PWM. Panggil sekali di setup().
void motorSetup();

// Helper matematika
int  ramp(int cur, int tgt, int step);     // ubah nilai bertahap (halus)

// Kendali motor (spd: -255..255 ; >0 maju, <0 mundur, 0 rem)
void setMotor(uint8_t d1, uint8_t d2, uint8_t ch, int spd);
void setMotorL(int spd);   // Motor 1 (kiri)
void setMotorR(int spd);   // Motor 2 (kanan)

// Berhenti mulus (dipakai saat menu / stik putus)
void stopMotorsSmooth();
