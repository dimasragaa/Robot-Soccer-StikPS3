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

void displaySetup()
{
  Wire.begin(OLED_SDA, OLED_SCL);
  Wire.setClock(400000); // I2C cepat -> update OLED ringan
  oledOK = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (oledOK) { display.clearDisplay(); display.display(); }
  else Serial.println("OLED tidak terdeteksi, lanjut tanpa OLED");
}

static void drawOLED()
{
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (!Ps3.isConnected()) {
    display.setTextSize(1); display.setCursor(0,0);  display.println("PS3 TERPUTUS");
    display.setCursor(0,20); display.println("Menunggu koneksi...");
    display.display(); return;
  }

  if (sysState == ST_MENU) {
    display.setTextSize(1);
    display.setCursor(0,0);
    display.print("< "); display.print(menuTitle[menuPage]); display.println(" >");
    display.drawLine(0,10,127,10,SSD1306_WHITE);
    for (uint8_t i=0;i<menuLen[menuPage];i++){
      display.setCursor(6, 14 + i*12);
      display.print(i==menuItem ? "> " : "  ");
      display.print(menuItems[menuPage][i]);
      if (menuPage==2){ // tampilkan nilai setting
        display.setCursor(96, 14 + i*12);
        if (i==0) display.print(g_maxSpeed);
        if (i==1) { display.print(g_steerGain); display.print("%"); }
        if (i==2) display.print(g_rampStep);
        if (i==3) display.print(g_invert?"ON":"OFF");
      }
    }
    display.setCursor(0,56); display.print("L1/R1:menu START:OK");
  }
  else if (sysState == ST_RUN) {
    display.setTextSize(1); display.setCursor(0,0);
    display.print("MODE: ");
    display.println(menuItems[activeMenu][activeItem]);
    display.drawLine(0,10,127,10,SSD1306_WHITE);
    display.setCursor(0,16); display.print("Max:");   display.print(g_maxSpeed);
    display.print(" Str:"); display.print(g_steerGain); display.println("%");
    display.setCursor(0,28); display.print("L:"); display.print(curLeft);
    display.print("  R:"); display.print(curRight);
    if (seqState == SEQ_FASE1) {
      display.setCursor(0,40); display.print(">> MUNDUR...");
    } else if (seqState == SEQ_FASE2) {
      display.setCursor(0,40);
      display.print(seqType == SQ_MUNDUR_MAJU  ? ">> MAJU..."        :
                    seqType == SQ_MUNDUR_KANAN ? ">> PUTAR KANAN..." :
                                                 ">> PUTAR KIRI...");
    } else if (kickArmed) {
      display.setCursor(0,40); display.print("KICK ARMED (segitiga)");
    }
    display.setCursor(0,56); display.print("X:mjr  []:kanan  O:kiri");
  }
  else { // IDLE
    display.setTextSize(2); display.setCursor(0,4);  display.println("READY");
    display.setTextSize(1); display.setCursor(0,30); display.println("Tekan SELECT");
    display.setCursor(0,42); display.println("untuk buka menu");
  }
  display.display();
}

void displayTick(unsigned long now)
{
  if (oledOK && (oledDirty || (now - lastOled >= OLED_MS))) {
    lastOled = now; oledDirty = false; drawOLED();
  }
}

#else  // ===== OLED dimatikan: fungsi kosong =====
void displaySetup(){}
void displayTick(unsigned long){}
#endif
