// By: github.com/dimasragaa — IG: @dmsragaa
#include "Display.h"
#include "Config.h"

#if USE_OLED
#include <Ps3Controller.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include "State.h"
#include "Menu.h"
#include "Controls.h"   // ps3Linked()

// ---- Pilih library sesuai chip driver di Config.h ----
#if OLED_DRIVER == 2
// SH1106 (layar 1.3") — butuh library "Adafruit SH110X"
#include <Adafruit_SH110X.h>
#define C_WHITE SH110X_WHITE
#define C_BLACK SH110X_BLACK
#else
// SSD1306 (layar 0.96") — library "Adafruit SSD1306"
#include <Adafruit_SSD1306.h>
#define C_WHITE SSD1306_WHITE
#define C_BLACK SSD1306_BLACK
#endif

// ============================================================
//  GRID LAYOUT 128x64  (font textSize(1) = 6x8 px per karakter)
//
//  LAYAR RUN
//   y  0-10  : header bar (putih, teks hitam)
//   y 12-22  : motor L  — label(0,12) | bar(8,12,w112,h10) | val(121,12)
//   y 25-35  : motor R  — sama
//   y 37     : divider
//   y 39-46  : status NORMAL/INVERT + KICK
//   y 49     : divider
//   y 51-57  : status gerakan
//   y 59-63  : MAC controller PS3
//
//  LAYAR MENU
//   y  0-10  : header bar
//   y 12-23  : mode 0
//   y 24-35  : mode 1
//   y 50-63  : hint navigasi
// ============================================================

#if OLED_DRIVER == 2
static Adafruit_SH1106G display(OLED_W, OLED_H, &Wire, -1);
#else
static Adafruit_SSD1306 display(OLED_W, OLED_H, &Wire, -1);
#endif
static bool oledOK = false;

// ============================================================
//  PRIMITIVE HELPERS
// ============================================================
static void hLine(int y)
{
  display.drawFastHLine(0, y, 128, C_WHITE);
}

// Header bar: putih penuh, teks hitam kiri & kanan
static void header(const char *left, const char *right)
{
  display.fillRect(0, 0, 128, 11, C_WHITE);
  display.setTextColor(C_BLACK);
  display.setTextSize(1);
  display.setCursor(2, 2);
  display.print(left);
  if (right && right[0])
  {
    int rw = (int)strlen(right) * 6;
    display.setCursor(126 - rw, 2);
    display.print(right);
  }
  display.setTextColor(C_WHITE);
}

// Bar motor: -255..255, tengah=netral, kanan=maju, kiri=mundur
// Layout baru: label(0..5) | KOTAK bar(8..100) | gap | ANGKA(104..127)
//   kotak sengaja dikecilkan supaya angka +255/-255 muat tanpa tumpuk.
static void motorBar(const char *label, int val, int by)
{
  const int bx = 8;     // kotak mulai
  const int bw = 92;    // lebar kotak -> berakhir di x=100
  const int bh = 10;

  // label kiri
  display.setTextSize(1);
  display.setCursor(0, by + 1);
  display.print(label);

  // kotak bar
  display.drawRect(bx, by, bw, bh, C_WHITE);

  // isi bar dari tengah
  int mid = bx + bw / 2;
  if (val != 0)
  {
    int fill = (int)((long)abs(val) * (bw / 2 - 2) / 255);
    if (fill < 1) fill = 1;
    if (val > 0) display.fillRect(mid,        by + 2, fill, bh - 4, C_WHITE);
    else         display.fillRect(mid - fill, by + 2, fill, bh - 4, C_WHITE);
  }
  // garis tengah (referensi netral)
  display.drawFastVLine(mid, by + 1, bh - 2, C_WHITE);

  // angka di kanan (x=104), format +255 / -180 / 0
  char buf[6];
  snprintf(buf, sizeof(buf), "%c%d",
           val > 0 ? '+' : (val < 0 ? '-' : ' '), abs(val));
  display.setCursor(104, by + 1);
  display.print(buf);
}

// Bar progres 0..maxVal, kiri ke kanan
static void hBar(int val, int maxVal, int bx, int by, int bw, int bh)
{
  display.drawRect(bx, by, bw, bh, C_WHITE);
  if (val <= 0 || maxVal <= 0)
    return;
  int w = (int)((long)val * (bw - 2) / maxVal);
  if (w > bw - 2)
    w = bw - 2;
  if (w > 0)
    display.fillRect(bx + 1, by + 1, w, bh - 2, C_WHITE);
}

// Baris item menu (h=12): highlight box saat aktif
static void menuRow(uint8_t idx, uint8_t sel,
                    const char *label, const char *valStr = nullptr)
{
  int y = 12 + (int)idx * 12; // 12, 24, 36, 48
  bool act = (idx == sel);
  if (act)
  {
    display.fillRect(0, y, 128, 12, C_WHITE);
    display.setTextColor(C_BLACK);
  }
  display.setTextSize(1);
  display.setCursor(3, y + 2);
  display.print(act ? ">" : " ");
  display.print(" ");
  display.print(label);
  if (valStr && valStr[0])
  {
    int rw = (int)strlen(valStr) * 6;
    display.setCursor(125 - rw, y + 2);
    display.print(valStr);
  }
  display.setTextColor(C_WHITE);
}

// ============================================================
//  LAYAR: PS3 TERPUTUS
// ============================================================
static void drawDisconnected()
{
  // Ikon: lingkaran + X, besar dan terpusat
  const int cx = 64, cy = 24, r = 15;
  display.drawCircle(cx, cy, r, C_WHITE);
  display.drawLine(cx - 9, cy - 9, cx + 9, cy + 9, C_WHITE);
  display.drawLine(cx + 9, cy - 9, cx - 9, cy + 9, C_WHITE);
  // Teks di bawah — y=44 aman (44..51), y=54 aman (54..61)
  hLine(43);
  display.setTextSize(1);
  display.setCursor(4, 45);
  display.print("PS3 DISCONNECTED");
  display.setCursor(1, 55);
  display.print(PS3_MAC);   // MAC pairing (17 char pas di 128px)
}

// ============================================================
//  LAYAR: MODE RUN
//   0-10  header (mode + trigger)
//  14-23  bar motor L + angka
//  26-35  bar motor R + angka
//  38     divider
//  41-48  status gerak
//  51     divider
//  54-61  MAC controller
// ============================================================
static void drawRun()
{
  const char *modeName = menuItems[activeMenu][activeItem];

  // Trigger aktif
  const char *trigStr = "--";
  if      (Ps3.data.button.r2) trigStr = "KENCANG BANGET";
  else if (Ps3.data.button.r1) trigStr = "KENCANG";
  else if (Ps3.data.button.l2) trigStr = "PELAN BANGET";
  else if (Ps3.data.button.l1) trigStr = "PELAN";

  // --- Header ---
  header(modeName, trigStr);

  // --- Bar motor (kotak kecil, angka di kanan) ---
  motorBar("L", curLeft, 14);
  motorBar("R", curRight, 26);

  // --- Divider + status gerak + range RPM ---
  hLine(38);
  display.setTextSize(1);

  // status gerak (kiri)
  display.setCursor(0, 41);
  if (seqState == SEQ_FASE1 || seqState == SEQ_FASE2)
  {
    const char *label =
        (seqState == SEQ_FASE1)       ? "MUNDUR"      :
        (seqType == SQ_MUNDUR_MAJU)   ? "MAJU"        :
        (seqType == SQ_MUNDUR_KANAN)  ? "PUTAR KANAN" :
                                        "PUTAR KIRI";
    display.print(label);
  }
  else
  {
    // Tampilkan arah gerak sekarang (gabungan maju/mundur + kanan/kiri),
    // berlaku terus (termasuk saat L2/creep) — tidak ada label lain.
    int avg  = curLeft + curRight;   // >0 maju, <0 mundur
    int diff = curLeft - curRight;   // >0 belok kanan, <0 belok kiri
    const int TH = 15;               // ambang, supaya getaran kecil = DIAM

    bool goingFwd  = avg  >  TH;
    bool goingBack = avg  < -TH;
    bool turnRight = diff >  TH;
    bool turnLeft  = diff < -TH;

    char buf[16] = "";
    if      (goingFwd)  strcat(buf, "MAJU");
    else if (goingBack) strcat(buf, "MUNDUR");

    if (turnRight)      { if (buf[0]) strcat(buf, " "); strcat(buf, "KANAN"); }
    else if (turnLeft)  { if (buf[0]) strcat(buf, " "); strcat(buf, "KIRI");  }

    display.print(buf[0] ? buf : "DIAM");
  }

  if (g_invert)
  {
    display.setCursor(56, 41);
    display.print("INV");
  }

  // --- Divider + MAC ---
  hLine(51);
  display.setCursor(0, 54);
  display.print("PS3 ");
  display.print(PS3_MAC);
}

// ============================================================
//  LAYAR: MENU
// ============================================================
static void drawMenu()
{
  char hdr[22];
  snprintf(hdr, sizeof(hdr), "< %s >", menuTitle[menuPage]);
  header(hdr, "");

  if (menuPage == MENU_PAGE_PENGATURAN)
  {
    // Baris pengaturan: label di kiri, nilai SEKARANG (bukan teks tetap)
    // di kanan — ambil langsung dari *val supaya selalu sesuai kondisi
    // real-time, walau diubah dari halaman ini sendiri lewat KIRI/KANAN.
    for (uint8_t i = 0; i < menuLen[menuPage] && i < 4; i++)
    {
      char val[10];
      snprintf(val, sizeof(val), "%d%s", *settingsList[i].val, settingsList[i].unit);
      menuRow(i, menuItem, settingsList[i].label, val);
    }
  }
  else
  {
    // Halaman MODE MAIN: dua pilihan, Soccer dan Sumo
    for (uint8_t i = 0; i < menuLen[menuPage] && i < 4; i++)
      menuRow(i, menuItem, menuItems[menuPage][i]);
  }

  // Footer navigasi — beda teks per halaman.
  // Catatan lebar: layar 128px = maks 21 karakter di textSize(1) (6px/karakter).
  // Kalau nambah baris footer baru, selalu hitung dulu panjangnya, karena
  // versi awal footer ini sempat kepanjangan dan jadi ke-wrap sendiri oleh
  // Adafruit_GFX sampai tabrakan sama baris di bawahnya.
  display.setTextSize(1);
  if (menuPage == MENU_PAGE_PENGATURAN)
  {
    display.setCursor(0, 50);
    display.print("UP/DN:pilih  L/R:ubah");   // 21 char
    display.setCursor(0, 57);
    display.print("L1/R1:mode  O:keluar");    // 20 char
  }
  else
  {
    display.setCursor(0, 50);
    display.print("UP/DOWN:pilih mode");      // 18 char
    display.setCursor(0, 57);
    display.print("START:OK  O:batal");       // 17 char
  }
}

// ============================================================
//  LAYAR: IDLE
// ============================================================
static void drawIdle()
{
  // Teks besar "READY" di atas (textSize 2 = 12x16)
  display.setTextSize(2);
  display.setCursor(22, 4); // 5 karakter x 12px = 60px, center=(128-60)/2=34
  display.print("READY");
  // Garis pembatas
  hLine(24);
  display.setTextSize(1);
  display.setCursor(0, 28);
  display.print("Pilih mode via SELECT");
  display.setCursor(0, 38);
  display.print("PS3 terhubung  :)");
  hLine(50);
  display.setCursor(0, 54);
  display.print("Firmware: robot v2.0");
}

// ============================================================
//  ENTRY POINT
// ============================================================
static void drawOLED()
{
  bool disconnected = !ps3Linked();   // JANGAN Ps3.isConnected(), lihat Controls.cpp

  // Layar putih + tulisan hitam saat PS3 terputus (kebalikan dari biasanya).
  // Cukup 1 perintah hardware, bukan gambar ulang tiap elemen dengan warna
  // kebalik satu-satu -> ringan, dan cuma dikirim saat statusnya BERUBAH
  // (bukan tiap frame) supaya tidak nambah trafik I2C percuma.
  static bool lastInverted = false;
  if (disconnected != lastInverted)
  {
    lastInverted = disconnected;
    display.invertDisplay(disconnected);
  }

  display.clearDisplay();
  display.setTextColor(C_WHITE);

  if (disconnected)
    drawDisconnected();
  else if (sysState == ST_MENU)
    drawMenu();
  else if (sysState == ST_RUN)
    drawRun();
  else
    drawIdle();

  display.display();
}

// ============================================================
//  TASK OLED — berdiri sendiri, TIDAK numpang di loop()
//
//  Alasan: sekali gambar, isi layar (1 KB) dikirim lewat I2C dan itu
//  makan belasan milidetik. Kalau dikerjakan di dalam loop(), kontrol
//  stik + ramp motor ikut tertahan selama itu -> gerakan terasa nyendat
//  tiap layar refresh (paling kerasa saat sequence otomatis jalan).
//
//  Task ini dipasang di CORE 1 (bareng loop()) dengan prioritas sama,
//  jadi keduanya gantian tiap tick — loop() tetap dapat giliran ~1 ms
//  walau OLED sedang mengirim data.
//  CORE 0 sengaja tidak dipakai: itu jatah stack Bluetooth PS3, biar
//  koneksi stik tidak terganggu sama sekali.
//
//  Sebagian besar waktu task ini tidur (vTaskDelay), jadi bebannya
//  ke CPU nyaris nol saat tidak menggambar.
// ============================================================
static void oledTask(void *)
{
  for (;;)
  {
    // Refresh lebih cepat saat sequence aktif (animasi progress bar)
    uint32_t period = (sysState == ST_RUN && seqState != SEQ_IDLE)
                          ? 60
                          : OLED_MS;
    unsigned long now = millis();
    if (oledDirty || (now - lastOled >= period))
    {
      lastOled  = now;
      oledDirty = false;
      drawOLED();
    }
    vTaskDelay(10 / portTICK_PERIOD_MS); // cek tiap 10 ms, sisanya tidur
  }
}

void displaySetup()
{
  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(400000);
#if OLED_DRIVER == 2
  oledOK = display.begin(OLED_ADDR, true); // SH1106: begin(addr, reset)
#else
  oledOK = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR); // SSD1306
#endif
  if (oledOK)
  {
    display.clearDisplay();
    display.display();
    // OLED digambar di task sendiri (lihat catatan di oledTask)
    xTaskCreatePinnedToCore(oledTask, "oled", 4096, nullptr, 1, nullptr, 1);
  }
  else
    Serial.println("OLED tidak terdeteksi, lanjut tanpa OLED");
}

#else
void displaySetup() {}
#endif