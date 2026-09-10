#pragma once
#include <Arduino.h>

// ============================================================
//  State.h  —  ENUM & VARIABEL YANG DIPAKAI BERSAMA ANTAR FILE
//  Ini "papan status" robot saat berjalan (bukan setelan tetap).
//  Nilai awalnya di-set di State.cpp (diambil dari Config.h).
//
//  By: github.com/dimasragaa — IG: @dmsragaa
// ============================================================

// --- Status sistem ---
enum SysState { ST_IDLE, ST_MENU, ST_RUN };

// --- Jenis auto-sequence ---
enum SeqType  { SQ_NONE, SQ_MUNDUR_MAJU, SQ_MUNDUR_KANAN, SQ_MUNDUR_KIRI };

// --- Fase sequence: FASE1 = mundur, FASE2 = maju/putar, REM = turun halus ---
//  SEQ_REM ditambahkan supaya sequence tidak berakhir dengan memotong
//  tenaga mendadak dari nilai penuh ke nol (bikin sentakan mekanis).
enum SeqPhase { SEQ_IDLE, SEQ_FASE1, SEQ_FASE2, SEQ_REM };

// --- Parameter jalan (live, bisa berubah lewat menu Pengaturan) ---
extern int  g_maxSpeed;
extern int  g_steerGain;
extern int  g_rampStep;
extern bool g_invert;

// --- Kecepatan tiap trigger, persen dari g_maxSpeed (live juga) ---
// Dulu ini konstanta SPD_* / CREEP_SPEED_PCT di Config.h yang butuh upload
// ulang untuk diubah. Sekarang variabel biasa supaya bisa diatur dari menu.
// Nilai awalnya tetap diambil dari Config.h lewat applyModePreset().
extern int  g_spdR2;   // R2 ditahan  -> "KENCANG BANGET"
extern int  g_spdR1;   // R1 ditahan  -> "KENCANG"
extern int  g_spdL1;   // L1 ditahan  -> "PELAN"
extern int  g_spdL2;   // L2 ditahan  -> "PELAN BANGET" (creep)
extern int  g_spdNorm; // tidak ada trigger ditahan -> "--"

// --- Status sistem & koneksi ---
extern SysState sysState;
extern bool hasActiveMode;
extern bool wasConnected;

// Waktu (millis) paket terakhir yang masuk dari stik PS3.
// Ditulis dari callback paket (task Bluetooth), dibaca dari task lain,
// makanya volatile. Dipakai oleh ps3Linked() di Controls.cpp.
extern volatile unsigned long lastPs3Packet;

// --- Indeks menu ---
extern uint8_t menuPage, menuItem;      // yang sedang di-navigasi
extern uint8_t activeMenu, activeItem;  // mode yang sedang aktif

// --- Kondisi motor sekarang (nilai PWM ter-ramp) ---
extern int curLeft, curRight;

// --- Status tendang ---
extern bool kickArmed, kickActive;
extern unsigned long kickStart;

// --- Status sequence ---
extern SeqType  seqType;
extern SeqPhase seqState;
extern unsigned long seqStart;

// --- Flag & timer tampilan ---
// volatile: dinyalakan dari task lain (loop/callback PS3), dibaca & dimatikan
// oleh task OLED. Tanpa ini compiler boleh menyimpannya di register.
extern volatile bool oledDirty;
extern unsigned long lastControl, lastOled, lastPrint;
