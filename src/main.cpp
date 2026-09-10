// ============================================================
//  main.cpp  —  TITIK MASUK PROGRAM (setup & loop)
//  Isinya sengaja pendek: cuma "merangkai" modul-modul lain.
//  Untuk ubah nilai      -> Config.h
//  Untuk ubah tombol     -> Controls.cpp
//  Untuk ubah isi menu   -> Menu.cpp
//  Untuk ubah sequence   -> Sequence.cpp
//  Untuk ubah tampilan   -> Display.cpp
//
//  By: github.com/dimasragaa — IG: @dmsragaa
// ============================================================
#include <Arduino.h>
#include <Ps3Controller.h>

#include "Config.h"
#include "State.h"
#include "Motor.h"
#include "Kicker.h"
#include "Controls.h"
#include "Menu.h"
#include "Display.h"
#include "Debug.h"

// ============================================================
//  CALLBACK KONEKSI PS3
// ============================================================
void onConnect()    { digitalWrite(LED_PIN, HIGH); Serial.println(">> PS3 CONNECTED");    oledDirty = true; }
void onDisconnect() { digitalWrite(LED_PIN, LOW);  Serial.println(">> PS3 DISCONNECTED");
                      wasConnected = false; oledDirty = true; }

// Dipanggil tiap paket masuk dari stik (~100x/detik) di task Bluetooth.
// Sengaja cuma catat waktu — inilah dasar deteksi putus di ps3Linked(),
// karena callback onDisconnect di atas TIDAK PERNAH dijalankan library.
void onPs3Data() { lastPs3Packet = millis(); }

// ============================================================
//  PENGAWAS SAMBUNGAN STIK  —  restart otomatis biar reconnect cepat
//
//  Kenapa perlu: waktu stik dimatikan, jalur Bluetooth yang lama TIDAK
//  pernah ditutup. Library tidak melapor putus, dan radio baru menyerah
//  sendiri setelah puluhan detik. Selama sisa jalur lama itu masih
//  nyangkut di tumpukan Bluetooth, permintaan sambungan baru dari stik
//  ikut tertahan — inilah yang bikin nyambung lagi terasa lama.
//  Restart membersihkan seluruh tumpukan Bluetooth sekaligus, jadi
//  begitu tombol PS ditekan, ESP32 sudah siap menerima dari kondisi
//  bersih. Pairing TIDAK hilang, jadi tidak perlu pairing ulang.
//
//  Tiga pengaman:
//   1. Hanya jalan kalau stik PERNAH tersambung sejak ESP32 menyala.
//      Tanpa ini, ESP32 yang dinyalakan tanpa stik akan restart terus.
//   2. Kalau sambungan pulih sebelum jeda habis, restart dibatalkan.
//   3. Motor & solenoid dimatikan dulu sebelum restart.
// ============================================================
static void linkWatchdog(bool linked, unsigned long now)
{
#if PS3_RESTART_MS > 0
  static bool          hadLink = false;  // pernah konek sejak boot?
  static unsigned long lostAt  = 0;      // kapan putus mulai dihitung

  if (linked) { hadLink = true; lostAt = 0; return; }
  if (!hadLink) return;                  // [1] belum pernah konek
  if (lostAt == 0) { lostAt = now; return; }
  if (now - lostAt < PS3_RESTART_MS) return;   // [2] masih ditunggu

  // [3] pastikan robot benar-benar diam sebelum reset
  setMotorL(0);
  setMotorR(0);
  digitalWrite(KICK_PIN, LOW);

  Serial.println(">> Stik putus -> ESP32 restart biar sambungan berikutnya cepat");
  Serial.flush();
  ESP.restart();
#endif
}

// ============================================================
//  SETUP
// ============================================================
void setup()
{
  // Buffer kirim diperbesar supaya baris debug yang panjang tidak menahan
  // loop() saat menunggu UART. Harus dipanggil SEBELUM begin().
  Serial.setTxBufferSize(256);
  Serial.begin(115200);

  // Opsional: matikan brownout detector kalau reset terus saat motor nyentak.
  // Ini menyembunyikan gejala, TETAP perbaiki power-nya. Buka komentar bila perlu:
  // #include "soc/rtc_cntl_reg.h"
  // WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  setCpuFrequencyMhz(240); // pastikan CPU full speed untuk stack BT

  pinMode(LED_PIN, OUTPUT);
  kickerSetup();
  motorSetup();
  displaySetup();

  // URUTAN WAJIB: tentukan mode dulu, pasang nilai pabriknya, baru
  // settingsLoad() menimpa dengan setelan simpanan untuk mode itu.
  // Kalau dibalik, preset akan menghapus setelan yang baru saja dibaca.
  activeItem = modeLoad();   // lanjutkan mode terakhir, bukan selalu Soccer
  applyModePreset();
  settingsLoad();

  Ps3.attach(onPs3Data);          // pencatat waktu paket (deteksi putus)
  Ps3.attachOnConnect(onConnect);
  Ps3.attachOnDisconnect(onDisconnect);
  Ps3.begin(PS3_MAC);  // controller harus dipair ke MAC ini
  Serial.println("SETUP DONE");
}

// ============================================================
//  LOOP
// ============================================================
void loop()
{
  handleKick(false); // jaga pulsa tendang non-blocking

  unsigned long now = millis();
  if (now - lastControl >= CONTROL_MS) {
    // Maju SATU periode, bukan "lastControl = now". Bedanya: kalau tick
    // sempat telat (task lain menyela), cara ini otomatis menyusul di
    // putaran berikutnya, jadi rata-rata kontrol tetap 1x per CONTROL_MS.
    // Kalau dipatok ke now, tiap keterlambatan hilang begitu saja dan
    // ramp motor jadi ikut melambat — akselerasi terasa tidak konsisten.
    lastControl += CONTROL_MS;
    if (now - lastControl > CONTROL_MS * 4) lastControl = now;  // ketinggalan jauh: selaraskan
    controlsUpdate();   // semua logika tombol/stik/sequence
  }

  // --- Satu tempat untuk semua reaksi atas perubahan status sambungan ---
  // LED indikator + catatan waktu di serial. Tidak bisa mengandalkan
  // onConnect/onDisconnect: callback putusnya tidak pernah dijalankan
  // library, jadi dulu LED tetap menyala walau stik sudah dimatikan.
  // Isi blok ini hanya jalan saat status BERUBAH, bukan tiap loop.
  static bool          linkedPrev = false;
  static unsigned long lastPacketBeforeLost = 0;

  bool linked = ps3Linked();
  if (linked != linkedPrev)
  {
    linkedPrev = linked;
    digitalWrite(LED_PIN, linked ? HIGH : LOW);

    if (!linked) {
      lastPacketBeforeLost = lastPs3Packet;   // saat data benar-benar berhenti
      Serial.println(">> PS3 PUTUS");
    }
    else if (lastPacketBeforeLost) {
      // Nyambung lagi TANPA lewat restart. Jeda dihitung dari paket
      // terakhir sampai paket pertama yang baru, jadi angkanya waktu
      // kosong sebenarnya (tidak termasuk ambang deteksi PS3_TIMEOUT_MS).
      Serial.printf(">> PS3 NYAMBUNG LAGI - kosong %lu ms\n",
                    lastPs3Packet - lastPacketBeforeLost);
      lastPacketBeforeLost = 0;
    }
    else {
      // Sambungan pertama sejak ESP32 menyala. Angka ini yang penting
      // untuk mengukur cepat-lambatnya reconnect sesudah restart.
      Serial.printf(">> PS3 TERSAMBUNG - %lu ms sejak ESP32 menyala\n", now);
    }
  }

  linkWatchdog(linked, now);   // restart otomatis kalau putus berkepanjangan

  // OLED tidak digambar di sini: sudah punya task sendiri (Display.cpp),
  // supaya kiriman data ke layar tidak menahan kontrol stik/motor.
  debugTick(now);       // cetak serial (throttle di dalam)

  // vTaskDelay yield bersih ke FreeRTOS scheduler (lebih baik dari delay(1))
  // -> BT stack mendapat giliran CPU tepat waktu -> koneksi lebih stabil
  vTaskDelay(1 / portTICK_PERIOD_MS);
}