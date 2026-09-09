#include "Display.h"
#include "Config.h"

#if USE_OLED
#include <Ps3Controller.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "State.h"
#include "Menu.h"

static Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, -1);
static bool oledOK = false;

// ============================================================
//  HELPER GAMBAR
// ============================================================
static void hLine(int y){
  display.drawFastHLine(0, y, 128, SSD1306_WHITE);
}

// Header bar putih dengan teks hitam di kiri dan kanan
static void header(const char* left, const char* right){
  display.fillRect(0, 0, 128, 11, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(1);
  display.setCursor(2, 2);   display.print(left);
  // teks kanan: hitung posisi manual (6px per karakter)
  int rw = strlen(right) * 6;
  display.setCursor(126 - rw, 2); display.print(right);
  display.setTextColor(SSD1306_WHITE);
}

// Bar progress horizontal (val -255..255, maju ke kanan, mundur ke kiri)
static void motorBar(int val, int bx, int by, int bw, int bh){
  display.drawRect(bx, by, bw, bh, SSD1306_WHITE);
  if (val == 0) return;
  int mid    = bx + bw / 2;
  int barW   = (int)((long)abs(val) * (bw / 2 - 2) / 255);
  if (barW < 1) barW = 1;
  if (val > 0) display.fillRect(mid,        by + 1, barW, bh - 2, SSD1306_WHITE);
  else         display.fillRect(mid - barW, by + 1, barW, bh - 2, SSD1306_WHITE);
}

// Bar horizontal 0..maxVal (untuk pengaturan dan creep)
static void hBar(int val, int maxVal, int bx, int by, int bw, int bh){
  display.drawRect(bx, by, bw, bh, SSD1306_WHITE);
  if (val <= 0) return;
  int w = (int)((long)val * (bw - 2) / maxVal);
  if (w > bw - 2) w = bw - 2;
  if (w > 0) display.fillRect(bx + 1, by + 1, w, bh - 2, SSD1306_WHITE);
}

// Baris menu dengan highlight box saat aktif
static void menuRow(uint8_t idx, uint8_t sel, const char* label,
                    const char* valStr = nullptr){
  int y = 13 + idx * 13;
  bool active = (idx == sel);
  if (active){
    display.fillRect(0, y, 128, 12, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  }
  display.setTextSize(1);
  display.setCursor(4, y + 2);
  display.print(active ? ">" : " ");
  display.print(" ");
  display.print(label);
  if (valStr){
    int rw = strlen(valStr) * 6;
    display.setCursor(126 - rw, y + 2);
    display.print(valStr);
  }
  display.setTextColor(SSD1306_WHITE);
}

// ============================================================
//  LAYAR: PS3 TERPUTUS
// ============================================================
static void drawDisconnected(){
  display.setTextSize(1);

  // Ikon lingkaran dengan X di tengah
  int cx = 64, cy = 26, r = 13;
  display.drawCircle(cx, cy, r, SSD1306_WHITE);
  display.drawLine(cx - 8, cy - 8, cx + 8, cy + 8, SSD1306_WHITE);
  display.drawLine(cx + 8, cy - 8, cx - 8, cy + 8, SSD1306_WHITE);

  hLine(44);
  display.setCursor(5, 48);  display.print("PS3  DISCONNECTED");
  display.setCursor(5, 57);  display.print("Waiting for controller...");
}

// ============================================================
//  LAYAR: MODE RUN (kontrol normal)
// ============================================================
static void drawRun(){
  // --- Header ---
  const char* modeName = menuItems[activeMenu][activeItem];
  // Trigger indicator (kanan header)
  const char* trigStr = "---";
  if      (Ps3.data.button.r2) trigStr = "R2 ";
  else if (Ps3.data.button.r1) trigStr = "R1 ";
  else if (Ps3.data.button.l2) trigStr = "L2!";
  else if (Ps3.data.button.l1) trigStr = "L1 ";
  header(modeName, trigStr);

  // --- Bar motor kiri ---
  display.setTextSize(1);
  display.setCursor(1, 14); display.print("L");
  motorBar(curLeft,  10, 13, 108, 8);
  // nilai motor pojok kanan (3 digit)
  display.setCursor(120, 14);
  if (abs(curLeft) < 100) display.print(" ");
  if (abs(curLeft) < 10)  display.print(" ");
  display.print(abs(curLeft));

  // --- Bar motor kanan ---
  display.setCursor(1, 24); display.print("R");
  motorBar(curRight, 10, 23, 108, 8);
  display.setCursor(120, 24);
  if (abs(curRight) < 100) display.print(" ");
  if (abs(curRight) < 10)  display.print(" ");
  display.print(abs(curRight));

  // --- Divider ---
  hLine(33);

  // --- Status tengah ---
  display.setCursor(0, 36);
  display.print("MAX:"); display.print(g_maxSpeed);
  display.print(" STR:");display.print(g_steerGain);display.print("%");

  // --- Baris bawah: sequence / creep / kick / normal ---
  hLine(46);
  display.setCursor(0, 49);

  if (seqState == SEQ_FASE1){
    // blink ">> MUNDUR <<"
    if ((millis() / 400) % 2 == 0) display.print(">> MUNDUR... <<");
    else                            display.print("               ");
  }
  else if (seqState == SEQ_FASE2){
    const char* faseLabel =
      (seqType == SQ_MUNDUR_MAJU)  ? ">> MAJU...   <<" :
      (seqType == SQ_MUNDUR_KANAN) ? ">> PUTAR KANAN " :
                                     ">> PUTAR KIRI  ";
    if ((millis() / 400) % 2 == 0) display.print(faseLabel);
    else                            display.print("               ");
  }
  else if (Ps3.data.button.l2){
    // CREEP: tampilkan bar kecepatan kecil
    display.print("CREEP  ");
    int cPct = CREEP_SPEED_PCT;
    hBar(cPct, 100, 42, 49, 50, 7);
    display.setCursor(95, 49); display.print(cPct); display.print("%");
  }
  else if (kickArmed){
    display.print("KICK ARMED");
    display.setCursor(0, 57); display.print("[segitiga] untuk tendang");
  }
  else {
    display.print("X:maju []:kanan O:kiri");
  }

  // Footer hint (hanya kalau tidak ada baris kick panjang)
  if (!kickArmed || seqState != SEQ_IDLE){
    display.setCursor(0, 57);
    display.print("SELECT:menu");
  }
}

// ============================================================
//  LAYAR: MENU NAVIGASI
// ============================================================
static void drawMenu(){
  // Header dengan nama halaman
  char hdr[20];
  snprintf(hdr, sizeof(hdr), "< %s >", menuTitle[menuPage]);
  header(hdr, "");

  // Tampilkan item (maks 4)
  for (uint8_t i = 0; i < menuLen[menuPage] && i < 4; i++){
    if (menuPage == 2){
      // Menu Pengaturan: nilai di kanan + mini bar
      char valBuf[8] = "";
      switch(i){
        case 0: snprintf(valBuf, sizeof(valBuf), "%d",   g_maxSpeed);  break;
        case 1: snprintf(valBuf, sizeof(valBuf), "%d%%", g_steerGain); break;
        case 2: snprintf(valBuf, sizeof(valBuf), "%d",   g_rampStep);  break;
        case 3: snprintf(valBuf, sizeof(valBuf), "%s",   g_invert ? "ON" : "OFF"); break;
      }
      menuRow(i, menuItem, menuItems[menuPage][i], valBuf);

      // Mini bar di sebelah kanan untuk item numerik
      if (i != 3 && menuItem != i){
        int bx = 80, by = 14 + i * 13 + 3, bw = 30, bh = 5;
        int val = 0, mx = 1;
        if (i == 0){ val = g_maxSpeed;  mx = 255; }
        if (i == 1){ val = g_steerGain; mx = 120; }
        if (i == 2){ val = g_rampStep;  mx = 12;  }
        hBar(val, mx, bx, by, bw, bh);
      }
    }
    else {
      menuRow(i, menuItem, menuItems[menuPage][i]);
    }
  }

  // Footer
  hLine(57);
  display.setTextSize(1);
  display.setCursor(0, 58);
  display.print("L1/R1:hal  STR:ok  O:batal");
}

// ============================================================
//  LAYAR: IDLE (belum ada mode dipilih)
// ============================================================
static void drawIdle(){
  display.setTextSize(2);
  display.setCursor(20, 6);
  display.print("READY");
  display.setTextSize(1);
  hLine(26);
  display.setCursor(0, 32); display.print("Tekan SELECT untuk");
  display.setCursor(0, 42); display.print("membuka menu & pilih mode.");
  hLine(54);
  display.setCursor(0, 57); display.print("PS3 terhubung");
}

// ============================================================
//  ENTRY POINT
// ============================================================
static void drawOLED(){
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (!Ps3.isConnected()) drawDisconnected();
  else if (sysState == ST_MENU) drawMenu();
  else if (sysState == ST_RUN)  drawRun();
  else                          drawIdle();

  display.display();
}

void displaySetup(){
  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(400000);
  oledOK = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (oledOK){ display.clearDisplay(); display.display(); }
  else Serial.println("OLED tidak terdeteksi, lanjut tanpa OLED");
}

void displayTick(unsigned long now){
  // RUN: refresh sedikit lebih sering supaya bar motor terasa live
  uint32_t period = (sysState == ST_RUN && seqState != SEQ_IDLE) ? 80 : OLED_MS;
  if (oledOK && (oledDirty || (now - lastOled >= period))){
    lastOled = now; oledDirty = false; drawOLED();
  }
}

#else
void displaySetup(){}
void displayTick(unsigned long){}
#endif