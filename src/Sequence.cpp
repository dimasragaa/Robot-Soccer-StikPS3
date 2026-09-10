// By: github.com/dimasragaa — IG: @dmsragaa
#include "Sequence.h"
#include "Config.h"
#include "State.h"
#include "Motor.h"

// ============================================================
//  MULAI / BATALKAN SEQUENCE
//  - tombol sama ditekan saat sequence jalan  -> batal
//  - tombol lain ditekan saat sequence jalan   -> ganti sequence
// ============================================================
void mulaiSequence(SeqType t, const char* nama)
{
  if (seqState != SEQ_IDLE && seqType == t) {   // batal
    seqState = SEQ_IDLE;
    seqType  = SQ_NONE;
    Serial.println("[SEQ] Dibatalkan");
  } else {                                       // mulai / ganti
    seqType  = t;
    seqState = SEQ_FASE1;                        // fase 1 selalu mundur
    seqStart = millis();
    Serial.printf("[SEQ] Mulai: %s\n", nama);
  }
  oledDirty = true;
}

// ============================================================
//  AUTO-SEQUENCE 2 FASE (non-blocking, murni berbasis millis)
//    FASE1 = mundur
//    FASE2 = maju / putar kanan / putar kiri, tergantung seqType
// ============================================================
void runSequence()
{
  unsigned long elapsed = millis() - seqStart;

  // Durasi tiap fase tergantung jenis sequence
  unsigned long durMundur = (seqType == SQ_MUNDUR_MAJU) ? SEQX_MUNDUR_MS : SEQ_MUNDUR_MS;
  unsigned long durFase2  = (seqType == SQ_MUNDUR_MAJU) ? SEQX_MAJU_MS   : SEQ_PUTAR_MS;

  if (seqState == SEQ_FASE1)
  {
    // Kedua motor mundur
    curLeft  = ramp(curLeft,  -SEQ_SPEED_MUNDUR, g_rampStep);
    curRight = ramp(curRight, -SEQ_SPEED_MUNDUR, g_rampStep);
    setMotorL(curLeft);
    setMotorR(curRight);

    if (elapsed >= durMundur) {
      seqState = SEQ_FASE2;
      seqStart = millis();   // reset timer fase berikutnya
      Serial.println(seqType == SQ_MUNDUR_MAJU  ? "[SEQ] Mundur selesai -> maju lagi..."   :
                     seqType == SQ_MUNDUR_KANAN ? "[SEQ] Mundur selesai -> putar KANAN..." :
                                                  "[SEQ] Mundur selesai -> putar KIRI...");
      oledDirty = true;
    }
  }
  else if (seqState == SEQ_FASE2)
  {
    int tgtL, tgtR;
    if (seqType == SQ_MUNDUR_MAJU) {
      tgtL =  SEQ_SPEED_MAJU;  tgtR =  SEQ_SPEED_MAJU;   // maju
    } else if (seqType == SQ_MUNDUR_KANAN) {
      tgtL =  SEQ_SPEED_PUTAR; tgtR = -SEQ_SPEED_PUTAR;  // putar kanan
    } else {
      tgtL = -SEQ_SPEED_PUTAR; tgtR =  SEQ_SPEED_PUTAR;  // putar kiri
    }

    curLeft  = ramp(curLeft,  tgtL, g_rampStep);
    curRight = ramp(curRight, tgtR, g_rampStep);
    setMotorL(curLeft);
    setMotorR(curRight);

    if (elapsed >= durFase2) {
      // Dulu di sini nilai motor dipotong langsung dari penuh ke nol.
      // Itu sentakan mekanis yang tidak perlu, jadi sekarang masuk fase
      // REM: turun bertahap dulu sampai benar-benar diam, baru selesai.
      seqState = SEQ_REM;
      Serial.println("[SEQ] Fase 2 selesai -> mengerem halus...");
      oledDirty = true;
    }
  }
  else if (seqState == SEQ_REM)
  {
    stopMotorsSmooth();   // turunkan bertahap; berhenti sendiri saat 0

    if (curLeft == 0 && curRight == 0) {
      seqState = SEQ_IDLE;   // selesai
      seqType  = SQ_NONE;
      Serial.println("[SEQ] Selesai! Kembali ke kontrol manual.");
      oledDirty = true;
    }
  }
}
