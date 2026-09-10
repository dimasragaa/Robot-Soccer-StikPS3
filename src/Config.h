#pragma once
#include <Arduino.h>

// ============================================================
//  Config.h  —  SATU-SATUNYA TEMPAT UBAH NILAI / KALIBRASI
//  Semua angka yang mungkin ingin kamu utak-atik ada di sini.
//  File lain TIDAK perlu disentuh untuk sekadar ganti pin/kecepatan.
//
//  By: github.com/dimasragaa — IG: @dmsragaa
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
#define MOTOR_FREQ 20000   // 20 kHz (senyap, aman untuk BTN)
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
//  DETEKSI STIK PUTUS
//  Library PS3 tidak pernah melapor "putus" — callback disconnect di
//  dalamnya cuma mencetak log, flag-nya tidak pernah dimatikan. Jadi
//  kita ukur sendiri: stik mengirim paket ~100x per detik walau diam,
//  kalau paket berhenti selama ini berarti sudah tidak terhubung.
//  Jangan terlalu kecil (BT sesekali telat); 1 detik sudah aman.
// ------------------------------------------------------------
#define PS3_TIMEOUT_MS  1000

// Restart otomatis setelah stik terdeteksi putus, supaya sambungan
// BERIKUTNYA cepat. Alasannya ada di linkWatchdog() di main.cpp.
// Ini jeda tunggu setelah putus terdeteksi: kalau stik sempat pulih
// sebelum waktu ini habis, restart dibatalkan.
// Jangan terlalu pendek supaya gangguan sesaat tidak bikin restart.
// Isi 0 kalau mau mematikan fitur restart otomatis ini.
#define PS3_RESTART_MS  1200

// ------------------------------------------------------------
//  PARAMETER JALAN — NILAI AWAL (bisa diubah live lewat menu Pengaturan)
// ------------------------------------------------------------
#define DEF_MAX_SPEED   255   // ceiling kecepatan (0..255)
#define DEF_STEER_GAIN  100   // sensitivitas belok (%)
#define DEF_RAMP_STEP   6     // kehalusan (perubahan PWM per update)
#define DEF_INVERT      false // balik arah belok

// ------------------------------------------------------------
//  PENGATURAN LIVE (SELECT > PENGATURAN)
//  Diubah lewat stik SAAT ESP32 JALAN -> langsung dipakai motor detik itu
//  juga, tanpa upload ulang. Ini beda dari angka lain di file ini: semua
//  #define di Config.h itu KONSTANTA yang dipatok saat compile (upload
//  ulang wajib kalau diubah), sedangkan Max Speed di sini disimpan di
//  variabel g_maxSpeed (RAM) yang memang dibaca ulang tiap tick oleh
//  driveManual() — itu sebabnya bisa "hidup" tanpa compile ulang.
//  Detail lengkapnya ada di komentar Menu.cpp.
//  Nilai yang diatur disimpan ke flash (NVS) juga, supaya tidak hilang
//  walau ESP32 restart/mati (termasuk restart otomatis saat stik putus).
// ------------------------------------------------------------
//  Semua *_SET_STEP di bawah = 1 supaya bisa dipaskan sehalus mungkin.
//  Untuk menggeser jauh, tahan tombolnya (auto-repeat, lihat di bawah).
#define SPEED_SET_MIN   60    // batas bawah Max Speed yang boleh diatur
#define SPEED_SET_MAX   255   // batas atas
#define SPEED_SET_STEP  1     // besar loncatan tiap tekan KIRI/KANAN

// Steering (sensitivitas belok, g_steerGain) — sama seperti Max Speed:
// DISIMPAN permanen ke flash begitu diubah, dan disimpan TERPISAH PER MODE
// (lihat settingsLoad/settingsSave di Menu.cpp), jadi atur di Sumo tidak
// mengubah Soccer. Mode yang belum pernah diatur manual tetap pakai nilai
// pabriknya masing-masing (Soccer 100%, Sumo 110%).
// Kehalusan gerak (g_rampStep) TIDAK diatur lewat menu, tetap beda per
// mode seperti semula (Soccer=6, Sumo=10), karena itu bukan parameter belok.
#define STEER_SET_MIN   50    // belok paling landai
#define STEER_SET_MAX   200   // belok paling tajam
#define STEER_SET_STEP  1

// Batas pengaturan trigger R2/R1/L1/L2 (persen dari Max Speed).
// Sama seperti Max Speed & Steering: disimpan per mode, jadi setelan
// trigger di Sumo tidak menyentuh Soccer.
#define TRIG_SET_MIN    0
#define TRIG_SET_MAX    100
#define TRIG_SET_STEP   1

// --- AUTO-REPEAT tombol KIRI/KANAN di layar PENGATURAN ---
// Sekali ketuk = 1 langkah. Ditahan = nilainya jalan terus sendiri.
// Karena langkahnya cuma 1, menggeser jauh (mis. Max Speed 60 -> 255)
// butuh banyak langkah. Makanya lajunya DIPERCEPAT sendiri kalau tombol
// ditahan lama: pelan dulu biar gampang berhenti di angka yang pas,
// lalu ngebut kalau memang mau menyeberang jauh.
#define REPEAT_DELAY_MS 400   // ditahan selama ini dulu, baru mulai jalan
#define REPEAT_RATE_MS  60    // laju awal saat mulai jalan sendiri
#define REPEAT_ACCEL_MS 1000  // ditahan lebih lama dari ini -> pindah ke laju cepat
#define REPEAT_FAST_MS  15    // laju cepat untuk menyeberang rentang panjang

// ------------------------------------------------------------
//  TIMING (milidetik)
// ------------------------------------------------------------
#define DEADBAND     12   // ambang stik dianggap netral
#define CONTROL_MS   2    // periode baca kontrol (2ms = tangkap tiap paket BT ~8ms)
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
#define DEFAULT_MENU  0
#define DEFAULT_ITEM  0

// ------------------------------------------------------------
//  KECEPATAN TRIGGER  (persen dari g_maxSpeed)
//  Ubah di sini kalau mau L2 jadi lebih pelan/cepat, dll.
// ------------------------------------------------------------
#define SPD_R2       100  // R2 ditekan  -> full
#define SPD_R1       75   // R1 ditekan
#define SPD_L1       25   // L1 ditekan
#define SPD_DEFAULT  50   // tidak ada trigger ditekan

// --- MODE CREEP (L2) : merayap perlahan untuk pendekatan bola ---
// Kecepatan maksimal saat creep (% dari g_maxSpeed). Kecilkan untuk lebih lambat.
#define CREEP_SPEED_PCT  18   // ~18% dari maxSpeed (misal 255 -> ~46 PWM)
// Ramp step khusus creep: makin kecil makin halus akselerasinya
#define CREEP_RAMP_STEP   1   // naik/turun 1 PWM per tick (sangat halus)

// ------------------------------------------------------------
//  PENGHALUSAN & KESERASIAN GERAK  (fitur baru)
// ------------------------------------------------------------
// [C] Kurva EXPO respons stik: 0 = linear, makin besar makin landai di tengah.
//     Bagus untuk kontrol presisi. 0..100. Coba 20-35.
#define EXPO_PCT        25

// [D] TRIM per motor untuk mengoreksi motor kiri/kanan yang beda kuat.
//     100 = normal. Kalau robot narik ke KANAN saat maju lurus,
//     berarti motor kiri lebih kuat -> KECILKAN MOTOR_TRIM_L (mis. 96).
//     Kalau narik ke KIRI -> kecilkan MOTOR_TRIM_R.
#define MOTOR_TRIM_L    100   // 90..100
#define MOTOR_TRIM_R    100   // 90..100

// [E] Pengereman lebih gesit dari akselerasi (kelipatan dari ramp step).
//     2 = saat melambat/ganti arah, step 2x lebih cepat. 1 = simetris.
#define RAMP_BRAKE_X    2

// ------------------------------------------------------------
//  AUTO-SEQUENCE
//    X     : mundur -> maju lagi
//    KOTAK : mundur -> putar KANAN
//    BULAT : mundur -> putar KIRI
// ------------------------------------------------------------
// Durasi fase (ms)
#define SEQX_MUNDUR_MS  200  // X: lama mundur
#define SEQX_MAJU_MS    550  // X: lama maju
#define SEQ_MUNDUR_MS   400   // kotak/bulat: lama mundur
#define SEQ_PUTAR_MS    500   // kotak/bulat: lama putar

// Kecepatan saat sequence (0..255)
#define SEQ_SPEED_MUNDUR 180
#define SEQ_SPEED_MAJU   255
#define SEQ_SPEED_PUTAR  150