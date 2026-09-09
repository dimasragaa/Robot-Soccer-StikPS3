#pragma once
#include <Arduino.h>

// ============================================================
//  Config.h  —  SATU-SATUNYA TEMPAT UBAH NILAI / KALIBRASI
//  File lain TIDAK perlu disentuh untuk sekadar ganti pin/kecepatan.
// ============================================================

// ------------------------------------------------------------
//  OLED  (set USE_OLED 0 kalau OLED belum terpasang)
// ------------------------------------------------------------
#define USE_OLED   1
#define OLED_W     128
#define OLED_H     64
#define OLED_ADDR  0x3C     // ganti 0x3D bila perlu
#define OLED_SDA   21
#define OLED_SCL   22

// ------------------------------------------------------------
//  PIN MOTOR  (driver BTN: 2 pin arah + 1 pin PWM per motor)
// ------------------------------------------------------------
// --- Motor 1 (KIRI) ---
#define M1_D1   25   // arah 1
#define M1_D2   26   // arah 2
#define M1_PWM  27   // kecepatan (PWM 1)
// --- Motor 2 (KANAN) ---
#define M2_D3   32   // arah 1
#define M2_D4   33   // arah 2
#define M2_PWM  14   // kecepatan (PWM 2)

// Kanal LEDC untuk pin PWM (1 kanal per motor)
#define M1_CH   0
#define M2_CH   1

// ------------------------------------------------------------
//  PWM MOTOR
// ------------------------------------------------------------
#define MOTOR_FREQ 20000   // 20 kHz 
#define MOTOR_RES  8       // 8-bit -> nilai 0..255

// ------------------------------------------------------------
//  PIN LAIN
// ------------------------------------------------------------
#define LED_PIN   2
#define KICK_PIN  18

// ------------------------------------------------------------
//  KONEKSI PS3  (MAC address yang di-pair ke controller)
// ------------------------------------------------------------
#define PS3_MAC "5c:6d:21:55:e7:37"

// ------------------------------------------------------------
//  PARAMETER JALAN — NILAI AWAL (bisa diubah live lewat menu Pengaturan)
// ------------------------------------------------------------
#define DEF_MAX_SPEED   255   // ceiling kecepatan (0..255)
#define DEF_STEER_GAIN  100   // sensitivitas belok (%)
#define DEF_RAMP_STEP   6     // kehalusan (perubahan PWM per update)
#define DEF_INVERT      false // balik arah belok

// ------------------------------------------------------------
//  TIMING (milidetik)
// ------------------------------------------------------------
#define DEADBAND     12   // ambang stik dianggap netral
#define CONTROL_MS   5    // periode baca kontrol
#define OLED_MS      120  // periode refresh OLED
#define PRINT_MS     200  // periode cetak serial

// ------------------------------------------------------------
//  TENDANG (KICK)
// ------------------------------------------------------------
#define KICK_MS      120  // lama pulsa solenoid tendang

// ------------------------------------------------------------
//  MODE DEFAULT saat stik baru konek (tanpa tekan SELECT/START)
//  Halaman: 0=POSISI START, 1=MODE MAIN, 2=PENGATURAN
//  Item MODE MAIN: 0=Normal 1=Dribble 2=Attack 3=Defense
// ------------------------------------------------------------
#define DEFAULT_MENU  1
#define DEFAULT_ITEM  0

// ------------------------------------------------------------
//  KECEPATAN TRIGGER  (persen dari g_maxSpeed)
//  Ubah di sini kalau mau L2 jadi lebih pelan/cepat, dll.
// ------------------------------------------------------------
#define SPD_R2       100  // R2 ditekan  -> full
#define SPD_R1       75   // R1 ditekan
#define SPD_L2       50   // L2 ditekan
#define SPD_L1       25   // L1 ditekan
#define SPD_DEFAULT  50   // tidak ada trigger ditekan

// ------------------------------------------------------------
//  AUTO-SEQUENCE
//    X     : mundur -> maju lagi
//    KOTAK : mundur -> putar KANAN
//    BULAT : mundur -> putar KIRI
// ------------------------------------------------------------
// Durasi fase (ms)
#define SEQX_MUNDUR_MS  1000  // X: lama mundur
#define SEQX_MAJU_MS    1000  // X: lama maju
#define SEQ_MUNDUR_MS   300   // kotak/bulat: lama mundur
#define SEQ_PUTAR_MS    500   // kotak/bulat: lama putar

// Kecepatan saat sequence (0..255)
#define SEQ_SPEED_MUNDUR 180
#define SEQ_SPEED_MAJU   180
#define SEQ_SPEED_PUTAR  150