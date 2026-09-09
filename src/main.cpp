// ============================================================
//  main.cpp  —  TITIK MASUK PROGRAM (setup & loop)
//  Untuk ubah nilai      -> Config.h
//  Untuk ubah tombol     -> Controls.cpp
//  Untuk ubah isi menu   -> Menu.cpp
//  Untuk ubah sequence   -> Sequence.cpp
//  Untuk ubah tampilan   -> Display.cpp
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

// ============================================================
//  SETUP
// ============================================================
void setup()
{
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

  applyModePreset();   // siapkan parameter mode default sebelum stik konek

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
    lastControl = now;
    controlsUpdate();   // semua logika tombol/stik/sequence
  }

  displayTick(now);     // update OLED (throttle di dalam)
  debugTick(now);       // cetak serial (throttle di dalam)

  delay(1); // yield ke scheduler FreeRTOS -> stack Bluetooth lebih stabil
}
