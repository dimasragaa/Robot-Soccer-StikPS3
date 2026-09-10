// By: github.com/dimasragaa — IG: @dmsragaa
#include "State.h"
#include "Config.h"

// Nilai awal diambil dari Config.h — jangan ubah di sini, ubah di Config.h.

int  g_maxSpeed  = DEF_MAX_SPEED;
int  g_steerGain = DEF_STEER_GAIN;
int  g_rampStep  = DEF_RAMP_STEP;
bool g_invert    = DEF_INVERT;

SysState sysState      = ST_RUN;   // langsung siap jalan begitu konek
bool     hasActiveMode = true;
bool     wasConnected  = false;

uint8_t menuPage = 0, menuItem = 0;
uint8_t activeMenu = DEFAULT_MENU, activeItem = DEFAULT_ITEM;

int curLeft = 0, curRight = 0;

bool kickArmed = false, kickActive = false;
unsigned long kickStart = 0;

SeqType  seqType  = SQ_NONE;
SeqPhase seqState = SEQ_IDLE;
unsigned long seqStart = 0;

bool oledDirty = true;
unsigned long lastControl = 0, lastOled = 0, lastPrint = 0;
