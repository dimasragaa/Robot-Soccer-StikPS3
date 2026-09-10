#pragma once
#include <Arduino.h>
#include "State.h"   // butuh enum SeqType

// ============================================================
//  Sequence.h  —  GERAKAN OTOMATIS 2 FASE (mundur -> maju/putar)
//  Durasi & kecepatan-nya diatur di Config.h (SEQ_* / SEQX_*).
//
//  By: github.com/dimasragaa — IG: @dmsragaa
// ============================================================

// Mulai / batalkan sequence. Tekan tombol yang sama saat jalan = batal.
void mulaiSequence(SeqType t, const char* nama);

// Jalankan sequence yang sedang aktif (panggil tiap tick saat seqState != SEQ_IDLE)
void runSequence();
