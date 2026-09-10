#pragma once
#include <Arduino.h>

// ============================================================
//  Controls.h  —  SEMUA LOGIKA TOMBOL & STIK ADA DI SINI
//  Kalau mau ubah "tombol apa melakukan apa", buka Controls.cpp.
//
//  By: github.com/dimasragaa — IG: @dmsragaa
// ============================================================

// Dipanggil tiap tick kontrol dari loop().
// Menangani: deteksi koneksi, baca tombol, navigasi menu,
// kontrol manual stik, dan pemicu sequence.
void controlsUpdate();

// Hitung kecepatan aktif dari trigger (R2/R1/L2/L1).
// Dipakai bersama oleh kontrol manual & cetak serial.
int triggerSpeed();
