#include "esp_camera.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Org_01.h"
#include "Picopixel.h"
#include "FreeSerifBold12pt7b.h"
#include "FS.h"
#include "SD_MMC.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "driver/rtc_io.h"
#include <EEPROM.h>
#include "img_converters.h"
#include "FreeSans9pt7b.h"
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>
#include "webserverr.h"

bool memOverlay = false;

// ─── Palette list ────────────────────────────────────────────────
#define MAX_PALETTES 256
char paletteList[MAX_PALETTES][512];
int paletteCount = 0;
int selectedPalette = 0;
const char* paletteListPtrs[MAX_PALETTES];

const char* getSelectedPalettePath() {
  if (paletteCount == 0) return nullptr;
  static char fullPath[80];
  snprintf(fullPath, sizeof(fullPath), "/palettes/%s", paletteList[selectedPalette]);
  return fullPath;
}

// ─── Menu lists ──────────────────────────────────────────────────
const char* menuItemsMain[7] = {
  "Return to cam",
  "Gallery",
  "Web Portal",
  "Change palette",
  "Games",
  "Settings",
  "Sleep mode"
};

const char* menuItemsGames[3] = {
  "Back",
  "Star Wars",
  "Tetris"
};

const char* menuItemsSettings[9] = {
  "Back",
  "Toggle flash",
  "Flash brightness",
  "Toggle dither",
  "JPEG quality",
  "Credits",
  "Reset",
  "Format SD",
  "Toggle mem overlay"
};

struct MenuList {
  const char** items;
  uint8_t length;
};

MenuList lists[] = {
  { menuItemsMain,     7 },
  { menuItemsSettings, 9 },
  { paletteListPtrs,   0 },
  { menuItemsGames,    3 }
};
const uint8_t LIST_COUNT = sizeof(lists) / sizeof(lists[0]);

// ─── Color / palette ─────────────────────────────────────────────
struct Color { uint8_t r, g, b; };

const Color DEFAULT_PALETTE[] = {
  { 0, 0, 0 }, { 255, 255, 255 }, { 255, 0, 0 }, { 0, 0, 255 },
};
const int DEFAULT_PALETTE_SIZE = 4;
const int MAX_PALETTE_SIZE = 256;

Color activePalette[MAX_PALETTE_SIZE];
int activePaletteSize = 0;

const int maxModes = 9;

// ─── Pin definitions ─────────────────────────────────────────────
#define SHUTTER_BUTTON 14
#define WAKEUP_GPIO    GPIO_NUM_14

#define SCREEN_WIDTH   128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define SCREEN_ADDRESS  0x3C

#define CLK_PIN  2
#define DT_PIN   3
#define I2C_SDA 41
#define I2C_SCL 42

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_NeoPixel diode(1, 47, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel diode2(1, 48, NEO_GRB + NEO_KHZ800);

#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  15
#define SIOD_GPIO_NUM   4
#define SIOC_GPIO_NUM   5
#define Y2_GPIO_NUM    11
#define Y3_GPIO_NUM     9
#define Y4_GPIO_NUM     8
#define Y5_GPIO_NUM    10
#define Y6_GPIO_NUM    12
#define Y7_GPIO_NUM    18
#define Y8_GPIO_NUM    17
#define Y9_GPIO_NUM    16
#define VSYNC_GPIO_NUM  6
#define HREF_GPIO_NUM   7
#define PCLK_GPIO_NUM  13

#define THRESHOLD 250

// ─── Bitmaps ─────────────────────────────────────────────────────
static const unsigned char PROGMEM image_Voltage_bits[] = {
  0x00,0x00,0x00,0x00,0x00,0x40,0x00,0xc0,0x01,0x80,0x03,0x80,0x07,0x00,0x0f,0xe0,
  0x01,0xc0,0x03,0x80,0x03,0x00,0x06,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
static const unsigned char PROGMEM image_paleta_bits[] = {
  0x3e,0x77,0x5d,0xff,0xfe,0xf8,0x7c,0x3e
};
static const unsigned char PROGMEM image_SDcardMounted_bits[] = {
  0xff,0xe0,0xff,0x20,0xff,0xe0,0xff,0x20,0xff,0xe0,0xff,0x20,0xff,0xe0,0xe6,0x00
};
static const unsigned char PROGMEM image_SmallArrowDown_bits[] = { 0xf8,0x70,0x20 };
static const unsigned char PROGMEM image_Pin_pointer_bits[]    = { 0x20,0x70,0xf8 };
static const unsigned char PROGMEM image_ButtonLeftSmall_bits[]  = { 0x20,0x60,0xe0,0x60,0x20 };
static const unsigned char PROGMEM image_ButtonRightSmall_bits[] = { 0x80,0xc0,0xe0,0xc0,0x80 };
static const unsigned char PROGMEM image_paint_17_bits[] = {
  0x00,0x20,0x00,0x00,0x50,0x00,0x0f,0xff,0x80,0x11,0x04,0x40,0x22,0x02,0x20,
  0x24,0xf9,0x20,0x29,0x04,0xa0,0x32,0x72,0x60,0x24,0x89,0x20,0x65,0x05,0x30,
  0xa5,0x05,0x28,0x65,0x05,0x30,0x24,0x89,0x20,0x32,0x72,0x60,0x29,0x04,0xa0,
  0x24,0xf9,0x20,0x22,0x02,0x20,0x11,0x04,0x40,0x0f,0xff,0x80,0x00,0x50,0x00,
  0x00,0x20,0x00
};

// ─── State ───────────────────────────────────────────────────────
Preferences preferences;
int mode = 0;
bool isPsram = false;
int pictureNumber = 0;
unsigned long pressStart = 0;
volatile int encoderDelta = 0;

void IRAM_ATTR encoderISR() {
  static uint8_t lastState = 0;
  uint8_t clk = digitalRead(CLK_PIN);
  uint8_t dt  = digitalRead(DT_PIN);
  uint8_t state = (clk << 1) | dt;
  static const int8_t table[16] = {
    0,-1,1,0, 1,0,0,-1, -1,0,0,1, 0,1,-1,0
  };
  encoderDelta += table[(lastState << 2) | state];
  lastState = state;
}

char activePaletteName[64] = "default";
int totalMB, usedMB, freeMB;
int ledBrightness = 100;
bool useFlash  = false;
bool useDither = true;

// ─── Gallery state ───────────────────────────────────────────────
#define MAX_GALLERY_IMAGES 128
char galleryList[MAX_GALLERY_IMAGES][32];
int galleryCount = 0;
int galleryIndex = 0;

// ─── Function forward declarations ───────────────────────────────
void drawTopbar();
void drawMemOverlay(bool push);
void drawMenu();
void switchList(uint8_t listIndex);
void menuAction(uint8_t listIndex, uint8_t itemIndex);
void loadingScreen(int progress, String text);
void viewFinder();
void takePicture();
void drawWebServerScreen();
void drawGalleryList();
void drawGalleryFullscreen();
void drawGalleryImage(int idx);
void drawValueChanger(int value);
void startChangingValue(int value);
void saveValue(int value);
void buttonActionHandler(unsigned long time);
void scanPalettes();
void scanGallery();
void updateUsed();
void resetPrefs();
bool loadPalette(const char* path);
int  nearestColor(int r, int g, int b);
void ditherRGB(uint8_t* buf, int w, int h);

// Game forward declarations
void gameInit();
void rozgrywka();
void ekranKoncowy();
void resetGry();
void tetrisInit();
void tetrisLoop();

// ─── Nearest palette colour ──────────────────────────────────────
int nearestColor(int r, int g, int b) {
  int best = 0, bestDist = INT_MAX;
  for (int i = 0; i < activePaletteSize; i++) {
    int dr = r - activePalette[i].r;
    int dg = g - activePalette[i].g;
    int db = b - activePalette[i].b;
    int dist = dr*dr + dg*dg + db*db;
    if (dist < bestDist) { bestDist = dist; best = i; }
  }
  return best;
}

// ─── Palette loading ─────────────────────────────────────────────
bool loadPalette(const char* path) {
  if (path == nullptr) goto use_default;
  {
    File f = SD_MMC.open(path, FILE_READ);
    if (!f) { Serial.printf("Palette not found: %s\n", path); goto use_default; }
    activePaletteSize = 0;
    while (f.available() && activePaletteSize < MAX_PALETTE_SIZE) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) continue;
      int r, g, b;
      if (sscanf(line.c_str(), "%d %d %d", &r, &g, &b) == 3) {
        activePalette[activePaletteSize++] = {
          (uint8_t)constrain(r,0,255),
          (uint8_t)constrain(g,0,255),
          (uint8_t)constrain(b,0,255)
        };
      }
    }
    f.close();
    if (activePaletteSize == 0) goto use_default;
    const char* filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;
    strncpy(activePaletteName, filename, sizeof(activePaletteName)-1);
    activePaletteName[sizeof(activePaletteName)-1] = '\0';
    return true;
  }
use_default:
  activePaletteSize = DEFAULT_PALETTE_SIZE;
  memcpy(activePalette, DEFAULT_PALETTE, sizeof(Color)*DEFAULT_PALETTE_SIZE);
  strncpy(activePaletteName, "default", sizeof(activePaletteName));
  return false;
}

void ditherRGB(uint8_t* buf, int w, int h) {
  int sz = w * 3;
  float* errCur = (float*)calloc(sz, sizeof(float));
  float* errNxt = (float*)calloc(sz, sizeof(float));
  if (!errCur || !errNxt) { free(errCur); free(errNxt); return; }
  for (int y = 0; y < h; y++) {
    memset(errNxt, 0, sz * sizeof(float));
    for (int x = 0; x < w; x++) {
      int idx = (y * w + x) * 3;
      float b_val = constrain(buf[idx+0] + errCur[x*3+0], 0.f, 255.f);
      float g_val = constrain(buf[idx+1] + errCur[x*3+1], 0.f, 255.f);
      float r_val = constrain(buf[idx+2] + errCur[x*3+2], 0.f, 255.f);
      int ci = nearestColor((int)r_val, (int)g_val, (int)b_val);
      Color c = activePalette[ci];
      buf[idx+0] = c.b; buf[idx+1] = c.g; buf[idx+2] = c.r;
      float eb = b_val - c.b, eg = g_val - c.g, er = r_val - c.r;
      if (x+1 < w) { errCur[(x+1)*3+0]+=eb*7/16.f; errCur[(x+1)*3+1]+=eg*7/16.f; errCur[(x+1)*3+2]+=er*7/16.f; }
      if (x-1 >= 0) { errNxt[(x-1)*3+0]+=eb*3/16.f; errNxt[(x-1)*3+1]+=eg*3/16.f; errNxt[(x-1)*3+2]+=er*3/16.f; }
      errNxt[x*3+0]+=eb*5/16.f; errNxt[x*3+1]+=eg*5/16.f; errNxt[x*3+2]+=er*5/16.f;
      if (x+1 < w) { errNxt[(x+1)*3+0]+=eb/16.f; errNxt[(x+1)*3+1]+=eg/16.f; errNxt[(x+1)*3+2]+=er/16.f; }
    }
    float* tmp = errCur; errCur = errNxt; errNxt = tmp;
  }
  free(errCur); free(errNxt);
}

// ─── SD helpers ──────────────────────────────────────────────────
void updateUsed() {
  uint64_t tot  = SD_MMC.totalBytes();
  uint64_t used = SD_MMC.usedBytes();
  totalMB = tot  / (1024*1024);
  usedMB  = used / (1024*1024);
  freeMB  = totalMB - usedMB;
}

void resetPrefs() {
  preferences.clear();
  EEPROM.write(0, 0); EEPROM.commit();
  delay(50);
  display.clearDisplay();
  display.setFont(&FreeSans9pt7b);
  display.setCursor(0,0); display.print("Restarting...");
  delay(250);
  ESP.restart();
}

int jpegQuality = 90;

// ═══════════════════════════════════════════════════════════════════
//  TETRIS  —  completely rewritten
// ═══════════════════════════════════════════════════════════════════

// Board: 10 wide × 20 tall, cell size 3 px → 30×60 px board
// Board drawn at x=1, y=2 → ends at x=31, y=62
// Right panel: x=34..127 for score / next / level

#define TW       10        // board columns
#define TH       20        // board rows
#define TCS       3        // cell size pixels
#define TBX       1        // board x offset
#define TBY       2        // board y offset
#define TPX      34        // right panel x

uint8_t tBoard[TH][TW];   // 0=empty, 1-7=colour

// Piece bitmasks [type][rotation][row], 4-wide strip, MSB=left
// Encoded as 4 nibbles per rotation (4 rows of 4 bits)
static const uint8_t TP[7][4][4] = {
  // I
  {{0,0xF,0,0},{0x4,0x4,0x4,0x4},{0,0xF,0,0},{0x4,0x4,0x4,0x4}},
  // O
  {{0x6,0x6,0,0},{0x6,0x6,0,0},{0x6,0x6,0,0},{0x6,0x6,0,0}},
  // T
  {{0x4,0xE,0,0},{0x4,0x6,0x4,0},{0,0xE,0x4,0},{0x4,0xC,0x4,0}},
  // S
  {{0x6,0xC,0,0},{0x4,0x6,0x2,0},{0x6,0xC,0,0},{0x4,0x6,0x2,0}},
  // Z
  {{0xC,0x6,0,0},{0x2,0x6,0x4,0},{0xC,0x6,0,0},{0x2,0x6,0x4,0}},
  // L
  {{0x2,0xE,0,0},{0x4,0x4,0x6,0},{0,0xE,0x8,0},{0xC,0x4,0x4,0}},
  // J
  {{0x8,0xE,0,0},{0x6,0x4,0x4,0},{0,0xE,0x2,0},{0x4,0x4,0xC,0}}
};

// SRS wall-kick table (for non-I pieces) indexed by [oldRot][test]
static const int8_t TKick[4][4][2] = {
  {{ 0,0},{-1,0},{-1,1},{0,-2}},
  {{ 0,0},{ 1,0},{ 1,-1},{0,2}},
  {{ 0,0},{ 1,0},{ 1,1},{0,-2}},
  {{ 0,0},{-1,0},{-1,-1},{0,2}}
};
static const int8_t TKickI[4][4][2] = {
  {{ 0,0},{-2,0},{ 1,0},{-2,-1}},
  {{ 0,0},{-1,0},{ 2,0},{-1,2}},
  {{ 0,0},{ 2,0},{-1,0},{ 2,1}},
  {{ 0,0},{ 1,0},{-2,0},{ 1,-2}}
};

// Active piece state
int   tPX, tPY, tPT, tPR;   // position, type, rotation
int   tNextT;                // next piece type
long  tScore;
int   tLevel, tLines;
bool  tGameOver;

unsigned long tLastFall;
unsigned long tLockTimer;
bool  tLockActive;

// Drop interval in ms based on level
int tDropMs() { return max(80, 500 - tLevel * 42); }

// Check if bit (row,col) is set in piece [type][rot]
inline bool tBit(int t, int r, int row, int col) {
  if (row < 0 || row > 3 || col < 0 || col > 3) return false;
  return (TP[t][r][row] >> (3 - col)) & 1;
}

bool tCollides(int t, int r, int px, int py) {
  for (int row = 0; row < 4; row++)
    for (int col = 0; col < 4; col++) {
      if (!tBit(t, r, row, col)) continue;
      int bx = px + col, by = py + row;
      if (bx < 0 || bx >= TW || by >= TH) return true;
      if (by >= 0 && tBoard[by][bx]) return true;
    }
  return false;
}

// SRS rotation
bool tRotate(int newR) {
  bool isI = (tPT == 0);
  for (int k = 0; k < 4; k++) {
    int dx = isI ? TKickI[tPR][k][0] : TKick[tPR][k][0];
    int dy = isI ? TKickI[tPR][k][1] : TKick[tPR][k][1];
    if (!tCollides(tPT, newR, tPX + dx, tPY + dy)) {
      tPX += dx; tPY += dy; tPR = newR;
      return true;
    }
  }
  return false;
}

void tLock() {
  for (int row = 0; row < 4; row++)
    for (int col = 0; col < 4; col++)
      if (tBit(tPT, tPR, row, col)) {
        int bx = tPX + col, by = tPY + row;
        if (by >= 0 && by < TH && bx >= 0 && bx < TW)
          tBoard[by][bx] = tPT + 1;
      }
}

int tClearLines() {
  int cleared = 0;
  for (int y = TH - 1; y >= 0; y--) {
    bool full = true;
    for (int x = 0; x < TW; x++) if (!tBoard[y][x]) { full = false; break; }
    if (full) {
      cleared++;
      for (int r = y; r > 0; r--) memcpy(tBoard[r], tBoard[r-1], TW);
      memset(tBoard[0], 0, TW);
      y++;
    }
  }
  return cleared;
}

void tSpawn() {
  tPT = tNextT;
  tNextT = random(7);
  tPR = 0;
  tPX = TW / 2 - 2;
  tPY = -1;
  tLockActive = false;
  if (tCollides(tPT, tPR, tPX, 0)) {
    tPY = 0;
    tGameOver = true;
  }
}

void tetrisInit() {
  memset(tBoard, 0, sizeof(tBoard));
  tScore = 0; tLevel = 1; tLines = 0;
  tGameOver = false; tLockActive = false;
  tNextT = random(7);
  tSpawn();
  tLastFall = millis();
}

// ── Draw helpers ─────────────────────────────────────────────────
// Draw one board cell (filled or outline)
void tDrawCell(int bx, int by, bool filled) {
  int sx = TBX + bx * TCS;
  int sy = TBY + by * TCS;
  if (filled) display.fillRect(sx, sy, TCS-1, TCS-1, SSD1306_WHITE);
  else         display.drawRect(sx, sy, TCS-1, TCS-1, SSD1306_WHITE);
}

// Draw next piece in right panel at pixel (px, py)
void tDrawNext(int px, int py) {
  display.setFont(); display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(px, py); display.print("NXT");
  for (int row = 0; row < 4; row++)
    for (int col = 0; col < 4; col++)
      if (tBit(tNextT, 0, row, col))
        display.fillRect(px + col * 4, py + 8 + row * 4, 3, 3, SSD1306_WHITE);
}

void tetrisDraw() {
  display.clearDisplay();

  // Board border
  display.drawRect(TBX - 1, TBY - 1, TW * TCS + 2, TH * TCS + 2, SSD1306_WHITE);

  // Locked cells
  for (int y = 0; y < TH; y++)
    for (int x = 0; x < TW; x++)
      if (tBoard[y][x]) tDrawCell(x, y, true);

  // Active piece (ghost first)
  if (!tGameOver) {
    // Ghost piece (drop shadow)
    int ghostY = tPY;
    while (!tCollides(tPT, tPR, tPX, ghostY + 1)) ghostY++;
    if (ghostY != tPY) {
      for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++)
          if (tBit(tPT, tPR, row, col)) {
            int by = ghostY + row, bx = tPX + col;
            if (by >= 0 && by < TH) tDrawCell(bx, by, false);
          }
    }
    // Active piece
    for (int row = 0; row < 4; row++)
      for (int col = 0; col < 4; col++)
        if (tBit(tPT, tPR, row, col)) {
          int by = tPY + row, bx = tPX + col;
          if (by >= 0 && by < TH) tDrawCell(bx, by, true);
        }
  }

  // ── Right panel ────────────────────────────────────────────────
  display.drawLine(TPX - 2, 0, TPX - 2, 63, SSD1306_WHITE);

  // Next piece
  tDrawNext(TPX, 0);

  // Score / level / lines
  display.setFont(&Picopixel);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(TPX, 30); display.print("SCR");
  display.setCursor(TPX, 37);
  // Score can be long; abbreviate if needed
  if (tScore >= 100000) {
    display.print(tScore / 1000); display.print("K");
  } else {
    display.print(tScore);
  }

  display.setCursor(TPX, 46); display.print("LVL");
  display.setCursor(TPX, 53); display.print(tLevel);

  display.setCursor(TPX + 18, 46); display.print("LNS");
  display.setCursor(TPX + 18, 53); display.print(tLines);

  // Game-over overlay
  if (tGameOver) {
    display.fillRoundRect(0, 18, 32, 28, 2, SSD1306_BLACK);
    display.drawRoundRect(0, 18, 32, 28, 2, SSD1306_WHITE);
    display.setFont(&Picopixel);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(2, 25); display.print("GAME");
    display.setCursor(2, 33); display.print("OVER");
    display.setCursor(2, 42); display.print("PRESS");
  }

  if (memOverlay) drawMemOverlay(false);
  display.display();
}

// ── Main Tetris loop (called from main loop()) ────────────────────
void tetrisLoop() {
  static bool btnWasDown = false;
  static unsigned long btnDownAt = 0;
  static int  lastEncoderDelta = 0;

  unsigned long now = millis();
  bool btnDown = (digitalRead(SHUTTER_BUTTON) == LOW);

  // ── Button press tracking ────────────────────────────────────
  if (btnDown && !btnWasDown) {
    btnWasDown = true;
    btnDownAt  = now;
  }

  if (!btnDown && btnWasDown) {
    btnWasDown = false;
    unsigned long held = now - btnDownAt;

    if (tGameOver) {
      if (held < 700) {
        // Short press on game over → restart
        tetrisInit();
        tetrisDraw();
        return;
      } else {
        // Long press → back to games menu
        mode = 8;
        switchList(3);
        return;
      }
    } else {
      if (held < 700) {
        // Rotate
        tRotate((tPR + 1) % 4);
        // Restart lock timer on rotation
        if (tLockActive) tLockTimer = now;
        tetrisDraw();
      } else {
        // Hard drop (long press)
        while (!tCollides(tPT, tPR, tPX, tPY + 1)) tPY++;
        tLock();
        int cl = tClearLines();
        tLines += cl;
        if (cl > 0) {
          const long pts[5] = {0,100,300,500,800};
          tScore += pts[min(cl,4)] * tLevel;
          tLevel = 1 + tLines / 10;
        }
        tSpawn();
        tLastFall = now;
        tetrisDraw();
        if (tGameOver) return;
      }
    }
  }

  if (tGameOver) {
    tetrisDraw();
    delay(20);
    return;
  }

  // ── Encoder = move left / right ─────────────────────────────
  if (encoderDelta != 0) {
    int move = (encoderDelta > 0) ? 1 : -1;
    encoderDelta = 0;
    if (!tCollides(tPT, tPR, tPX + move, tPY)) {
      tPX += move;
      if (tLockActive) tLockTimer = now;  // reset lock on slide
    }
    tetrisDraw();
  }

  // ── Gravity ─────────────────────────────────────────────────
  if (now - tLastFall > (unsigned long)tDropMs()) {
    tLastFall = now;
    if (!tCollides(tPT, tPR, tPX, tPY + 1)) {
      tPY++;
      tLockActive = false;
    } else {
      // Can't fall → start lock delay
      if (!tLockActive) { tLockActive = true; tLockTimer = now; }
    }
    tetrisDraw();
  }

  // ── Lock delay ──────────────────────────────────────────────
  if (tLockActive && (now - tLockTimer > 300)) {
    tLock();
    int cl = tClearLines();
    tLines += cl;
    if (cl > 0) {
      const long pts[5] = {0,100,300,500,800};
      tScore += pts[min(cl,4)] * tLevel;
      tLevel = 1 + tLines / 10;
    }
    tSpawn();
    tLastFall = now;
    if (tGameOver) tetrisDraw();
  }

  delay(10);
}


// ═══════════════════════════════════════════════════════════════════
//  STAR WARS (xWing vs Death Star)  —  fixed menu return
// ═══════════════════════════════════════════════════════════════════

const unsigned char PROGMEM dioda16[] = {
  0x00,0x00,0x00,0x00,0x1C,0x00,0x3F,0xF0,0x3C,0x00,0x3C,0x00,0xFF,0x00,0x7F,0xFF,
  0x7F,0xFF,0xFF,0x00,0x3C,0x00,0x3C,0x00,0x1F,0xF0,0x1C,0x00,0x00,0x00,0x00,0x00
};
const unsigned char PROGMEM storm[] = {
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x7F,0xFE,0x00,0x00,0x00,0x07,0x80,0x01,0xE0,0x00,0x00,0x0C,
  0x00,0x00,0x20,0x00,0x00,0x18,0x00,0x00,0x18,0x00,0x00,0x30,0x00,0x00,0x04,0x00,
  0x00,0x20,0x00,0x00,0x04,0x00,0x00,0x20,0x00,0x00,0x04,0x00,0x00,0x60,0x00,0x00,
  0x02,0x00,0x00,0x40,0x00,0x00,0x02,0x00,0x00,0x40,0x00,0x00,0x01,0x00,0x00,0x40,
  0x00,0x00,0x01,0x00,0x00,0x40,0x00,0x00,0x01,0x00,0x00,0x7F,0xE0,0x00,0x01,0x00,
  0x00,0x7F,0xFF,0xFF,0xFF,0x00,0x00,0x7F,0xFF,0xFF,0xFF,0x00,0x00,0xD7,0xFF,0xFF,
  0xE1,0x00,0x01,0xBF,0xFC,0x1F,0xFA,0x80,0x01,0xBF,0xF1,0xCF,0xFA,0x80,0x01,0x3F,
  0xC2,0x37,0xF7,0x80,0x01,0xEF,0x9C,0x01,0xE7,0xC0,0x01,0xE0,0x70,0x06,0x06,0x80,
  0x01,0xE0,0xC0,0x03,0x06,0x80,0x01,0xFF,0x80,0x01,0xFF,0x80,0x01,0xF8,0x00,0x00,
  0x1D,0xC0,0x03,0x70,0x00,0x80,0x0C,0x60,0x05,0xB0,0x07,0xF0,0x08,0x90,0x09,0x10,
  0x1F,0xF8,0x09,0xD0,0x0B,0x90,0x1F,0x7C,0x03,0xF0,0x0F,0xC0,0xFC,0x0F,0x07,0x90,
  0x0D,0x43,0xC0,0x03,0x07,0x90,0x05,0x64,0x00,0x00,0xCF,0x10,0x07,0xFC,0x00,0x00,
  0x26,0x10,0x01,0x80,0x00,0x00,0x10,0x20,0x01,0x00,0x00,0x00,0x0E,0x40,0x01,0x80,
  0x07,0xF0,0x01,0x80,0x00,0x80,0x07,0xC8,0x00,0x80,0x00,0x80,0x0B,0xE8,0x00,0x80,
  0x00,0x87,0x97,0xE9,0xE0,0x80,0x00,0x87,0xDF,0xEF,0xA0,0x80,0x00,0x4B,0xFF,0xFF,
  0xA0,0x80,0x00,0x6B,0xDF,0xFB,0xA3,0x00,0x00,0x24,0x97,0xE8,0x24,0x00,0x00,0x1E,
  0x1F,0xC0,0x2C,0x00,0x00,0x07,0xF8,0x1F,0xF0,0x00,0x00,0x00,0x0F,0xF8,0x00,0x00
};

int strzalX=0, strzalY=0, czyStrzal=0, pozycjaWroga=8, kierunek=0, koniec=0;
int kula1X=95,kula1Y=0,kula2X=95,kula2Y=0,kula3X=95,kula3Y=0,kula4X=95,kula4Y=0;
int punkty=0, predkosc=3, predkoscWroga=1, minCzas=600, maxCzas=1200, promien=10;
int zycia=5, startLicznika=0, liczbaKul=0, poziom=1, srodek=95;
unsigned long czasStart=0, czasLosowy=0, czasAktualny=0, czasPoziomu=0;
int pozycjaGracza=30;
int encoderGamePos=0;

// gameInit: show splash, run game loop, show game over, return to menu
void gameInit() {
  // ── Splash screen ─────────────────────────────────────────────
  display.clearDisplay();
  display.drawBitmap(6, 11, storm, 48, 48, 1);
  display.setFont(&FreeSans9pt7b);
  display.setTextColor(WHITE);
  display.setCursor(65, 14); display.println("xWing");
  display.setFont();
  display.setCursor(65, 17); display.println("vs");
  display.setCursor(65, 30); display.println("Death");
  display.setCursor(65, 43); display.println("star");
  display.setCursor(65, 55); display.println("by FoxenIT");
  if (memOverlay) drawMemOverlay(false);
  display.display();

  resetGry();
  delay(800);

  // ── Game loop ─────────────────────────────────────────────────
  // Run until koniec flag is set inside rozgrywka()
  while (!koniec) {
    rozgrywka();
  }

  // ── Game-over screen ──────────────────────────────────────────
  ekranKoncowy();

  // Wait for button press to acknowledge, then return to main menu
  delay(200);
  while (digitalRead(SHUTTER_BUTTON) == HIGH) { delay(10); }  // wait press
  while (digitalRead(SHUTTER_BUTTON) == LOW)  { delay(10); }  // wait release

  // Return to main menu
  mode = 1;
  switchList(0);
}

void rozgrywka() {
  display.clearDisplay();

  if (startLicznika == 0) {
    czasStart  = millis();
    czasLosowy = random(400, 1200);
    startLicznika = 1;
  }
  czasAktualny = millis();

  // Level progression
  if ((czasAktualny - czasPoziomu) > 50000) {
    czasPoziomu = czasAktualny;
    poziom++;
    predkosc++;
    if (poziom % 2 == 0) { predkoscWroga++; promien--; }
    minCzas -= 50; maxCzas -= 50;
  }

  // Spawn bullet
  if ((czasLosowy + czasStart) < czasAktualny) {
    startLicznika = 0;
    liczbaKul++;
    switch (liczbaKul) {
      case 1: kula1X=95; kula1Y=pozycjaWroga; break;
      case 2: kula2X=95; kula2Y=pozycjaWroga; break;
      case 3: kula3X=95; kula3Y=pozycjaWroga; break;
      case 4: kula4X=95; kula4Y=pozycjaWroga; break;
    }
  }

  // Draw and move bullets
  if (liczbaKul > 0) { display.drawCircle(kula1X, kula1Y, 2, 1); kula1X -= predkosc; }
  if (liczbaKul > 1) { display.drawCircle(kula2X, kula2Y, 1, 1); kula2X -= predkosc; }
  if (liczbaKul > 2) { display.drawCircle(kula3X, kula3Y, 4, 1); kula3X -= predkosc; }
  if (liczbaKul > 3) { display.drawCircle(kula4X, kula4Y, 2, 1); kula4X -= predkosc; }

  // Encoder controls player
  if (encoderDelta != 0) {
    encoderGamePos -= encoderDelta;
    encoderDelta = 0;
    encoderGamePos = constrain(encoderGamePos, 2, 46);
    pozycjaGracza = encoderGamePos;
  }

  // Fire
  if (digitalRead(SHUTTER_BUTTON) == LOW && czyStrzal == 0) {
    czyStrzal = 1;
    strzalX = 6;
    strzalY = pozycjaGracza + 8;
  }
  if (czyStrzal == 1) {
    strzalX += 8;
    display.drawLine(strzalX, strzalY, strzalX + 4, strzalY, 1);
  }

  // Draw sprites
  display.drawBitmap(4, pozycjaGracza, dioda16, 16, 16, 1);
  display.fillCircle(srodek, pozycjaWroga, promien, 1);
  display.fillCircle(srodek + 2, pozycjaWroga + 3, promien / 3, 0);

  // HUD
  display.setTextSize(1); display.setTextColor(WHITE);
  display.setCursor(33, 57); display.println("score:");
  display.setCursor(68, 57); display.println(punkty);
  display.setCursor(33, 0);  display.println("lives:");
  display.setCursor(68, 0);  display.println(zycia);
  display.setCursor(110, 0); display.println("L:");
  display.setCursor(122, 0); display.println(poziom);
  display.setCursor(108, 57); display.println(czasAktualny / 1000);

  // Bullet out of screen reset
  if (strzalX > 128) czyStrzal = 0;

  // Move enemy
  if (kierunek == 0) pozycjaWroga += predkoscWroga;
  else               pozycjaWroga -= predkoscWroga;
  if (pozycjaWroga >= (64 - promien)) kierunek = 1;
  if (pozycjaWroga <= promien)        kierunek = 0;

  // Hit detection (player shot → enemy)
  if (strzalY >= pozycjaWroga - promien && strzalY <= pozycjaWroga + promien
      && strzalX > (srodek - promien) && strzalX < (srodek + promien)) {
    strzalX = -20; punkty++; czyStrzal = 0;
  }

  // Hit detection (enemy bullets → player)
  int sg = pozycjaGracza + 8;
  auto hitPlayer = [&](int kx, int ky) {
    return (ky >= sg - 8 && ky <= sg + 8 && kx < 12 && kx > 4);
  };
  if ((liczbaKul > 0 && hitPlayer(kula1X, kula1Y)) ||
      (liczbaKul > 1 && hitPlayer(kula2X, kula2Y)) ||
      (liczbaKul > 2 && hitPlayer(kula3X, kula3Y)) ||
      (liczbaKul > 3 && hitPlayer(kula4X, kula4Y))) {
    zycia--;
    kula1X = kula2X = kula3X = kula4X = 95;
    kula1Y = kula2Y = kula3Y = kula4Y = -50;
    liczbaKul = 0;
  }

  // Reset bullet wave
  if (liczbaKul > 3 && kula4X < 1) {
    liczbaKul = 0;
    kula4X = 200;
  }

  if (zycia <= 0) koniec = 1;

  if (memOverlay) drawMemOverlay(false);
  display.display();
  delay(16);  // ~60 fps cap
}

void ekranKoncowy() {
  display.clearDisplay();
  display.setFont();
  display.setTextSize(2); display.setTextColor(WHITE);
  display.setCursor(7, 10); display.println("GAME OVER!");
  display.setTextSize(1);
  display.setCursor(7, 30); display.println("score:");
  display.setCursor(44,30); display.println(punkty);
  display.setCursor(7, 40); display.println("level:");
  display.setCursor(44,40); display.println(poziom);
  display.setCursor(7, 50); display.println("time(s):");
  display.setCursor(60,50); display.println(czasAktualny / 1000);
  display.setFont(&Picopixel);
  display.setCursor(7, 63); display.println("Press button to continue");
  if (memOverlay) drawMemOverlay(false);
  display.display();
}

void resetGry() {
  strzalX=0; strzalY=0; czyStrzal=0;
  pozycjaWroga=8; kierunek=0; koniec=0;
  kula1X=95; kula1Y=0; kula2X=95; kula2Y=0;
  kula3X=95; kula3Y=0; kula4X=95; kula4Y=0;
  punkty=0; predkosc=3; predkoscWroga=1;
  minCzas=600; maxCzas=1200; promien=12;
  zycia=5; startLicznika=0; liczbaKul=0; poziom=1;
  czasStart=0; czasLosowy=0; czasAktualny=0;
  czasPoziomu=millis();
  encoderDelta=0; encoderGamePos=30; pozycjaGracza=30;
}


// ═══════════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  delay(200);
  randomSeed(analogRead(0));

  preferences.begin("camera", false);
  ledBrightness = preferences.getInt("brightness", 100);
  jpegQuality   = preferences.getInt("quality", 90);
  useFlash      = preferences.getBool("flash", false);
  useDither     = preferences.getBool("dither", true);

  pinMode(SHUTTER_BUTTON, INPUT_PULLUP);
  pinMode(CLK_PIN, INPUT_PULLUP);
  pinMode(DT_PIN,  INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CLK_PIN), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(DT_PIN),  encoderISR, CHANGE);

  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 not found"));
    for (;;);
  }
  display.clearDisplay(); display.display();
  loadingScreen(0,  "CZY TO DZIALA");
  loadingScreen(30, "Inicjalizacja karty SD...");

  SD_MMC.setPins(39, 38, 40);
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD Card Mount Failed");
    return;
  }
  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) { Serial.println("No SD Card"); return; }

  if (!SD_MMC.exists("/images"))   SD_MMC.mkdir("/images");
  if (!SD_MMC.exists("/palettes")) SD_MMC.mkdir("/palettes");

  scanPalettes();
  loadingScreen(40, "Inicjalizacja flasha...");

  diode.begin(); diode.clear();
  diode.setBrightness(ledBrightness); diode.show();

  loadPalette("/palettes/gameboy.pa");
  updateUsed();
  loadingScreen(60, "Inicjalizacja kamery");

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk  = XCLK_GPIO_NUM;
  config.pin_pclk  = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href  = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn  = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz  = 20000000;
  config.pixel_format  = PIXFORMAT_RGB565;
  config.frame_size    = FRAMESIZE_QQVGA;
  config.jpeg_quality  = 10;
  config.fb_location   = CAMERA_FB_IN_PSRAM;
  config.fb_count      = 1;
  config.grab_mode     = CAMERA_GRAB_WHEN_EMPTY;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    loadingScreen(0, "BLAD KAMERY");
    delay(10000); return;
  }
  sensor_t* s = esp_camera_sensor_get();
  s->set_hmirror(s, 1);

  loadingScreen(80, "Inicjalizacja EEPROM...");
  EEPROM.begin(1);
  pictureNumber = EEPROM.read(0) + 1;
  if (pictureNumber > 255) pictureNumber = 1;
  isPsram = psramFound();

  loadingScreen(100, "Gotowe!");
  delay(500);
}


// ═══════════════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════════════

uint8_t activeList = 0;
int cursorPos = 0;
int lastmode = 67;
int lastButState = 0;
int lastPosition = 0;
unsigned long sleepTimer = millis();
int sleepTime = 60000;
int changedValue = 0;
int galleryLen = 1;

void loop() {
  if (digitalRead(SHUTTER_BUTTON) == LOW) {
    if (pressStart == 0) pressStart = millis();
  } else {
    if (pressStart != 0) {
      buttonActionHandler(millis() - pressStart);
      pressStart = 0;
    }
  }

  bool buttonState = digitalRead(SHUTTER_BUTTON);

  switch (mode) {
    case 0: viewFinder();                                 break;
    case 1:
      if (lastmode != mode) switchList(0);
      drawMenu();
      break;
    case 2:
      drawWebServerScreen();
      handleWebServer();
      break;
    case 3: drawValueChanger(changedValue);               break;
    case 4: drawGalleryList();                            break;
    case 5: drawGalleryFullscreen();                      break;
    case 6: rozgrywka();                                  break;  // called per-frame; gameInit() is entry point
    case 7: tetrisLoop();                                 break;
    case 8:
      if (lastmode != mode) switchList(3);
      drawMenu();
      break;
    default: mode = 0;                                    break;
  }

  // Encoder handling (games handle their own encoder inside their loops)
  if (encoderDelta != 0) {
    static int remainder = 0;
    int delta = encoderDelta;
    encoderDelta = 0;

    if (mode == 5) {
      galleryIndex -= delta;
      if (galleryIndex < 0) galleryIndex = galleryCount - 1;
      if (galleryIndex >= galleryCount) galleryIndex = 0;
    } else if (mode == 6 || mode == 7) {
      // games consume encoderDelta themselves
    } else {
      remainder += delta;
      int steps = remainder / 2;
      remainder %= 2;
      if (steps > 0) {
        if (mode == 3) { cursorPos++; }
        else if (mode == 4) { if (cursorPos < galleryLen-1) cursorPos++; }
        else { if (cursorPos < (int)lists[activeList].length-1) cursorPos++; }
      } else if (steps < 0) {
        if (cursorPos > 0) cursorPos--;
      }
    }
  }

  // Sleep logic
  if ((lastButState == buttonState) && (cursorPos == lastPosition) && (mode == lastmode)) {
    bool doSleep = (millis() - sleepTimer > (unsigned long)sleepTime);
    if (!doSleep && mode == 2)
      doSleep = (millis() - sleepTimer > (unsigned long)(sleepTime * 5));

    if (doSleep) {
      if (mode == 2) stopWebServer();
      diode.setPixelColor(0, 0, 0, 0); diode.show();
      diode2.setPixelColor(0, 0, 0, 0); diode2.show();
      display.ssd1306_command(SSD1306_DISPLAYOFF);
      display.clearDisplay(); display.display();
      SD_MMC.end(); esp_camera_deinit(); Wire.end();
      WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
      rtc_gpio_isolate(GPIO_NUM_14);
      delay(100);
      esp_sleep_enable_ext0_wakeup(WAKEUP_GPIO, 0);
      delay(50);
      esp_deep_sleep_start();
    }
  } else {
    sleepTimer = millis();
  }

  lastmode    = mode;
  lastButState = buttonState;
  lastPosition = cursorPos;
}


// ═══════════════════════════════════════════════════════════════════
//  VALUE CHANGER
// ═══════════════════════════════════════════════════════════════════

void startChangingValue(int value) {
  switch (value) {
    case 0: mode = 3; cursorPos = ledBrightness; break;
    case 1: mode = 3; cursorPos = jpegQuality;   break;
    default: break;
  }
  while (digitalRead(SHUTTER_BUTTON) == LOW) {}
}

void saveValue(int value) {
  switch (value) {
    case 0:
      ledBrightness = cursorPos;
      preferences.putInt("brightness", ledBrightness);
      diode.setBrightness(ledBrightness);
      mode = 1; lastmode = 1; switchList(1);
      break;
    case 1:
      jpegQuality = cursorPos;
      preferences.putInt("quality", jpegQuality);
      mode = 1; lastmode = 1; switchList(1);
      break;
    default: break;
  }
}

float VCWidth;
void drawValueChanger(int value) {
  display.clearDisplay();
  switch (value) {
    case 0:
      cursorPos = constrain(cursorPos, 0, 255);
      VCWidth = cursorPos * 120.0f / 255.0f;
      display.drawRoundRect(4, 45, 120, 12, 3, 1);
      display.fillRoundRect(4, 45, (int)VCWidth, 12, 3, 1);
      display.setTextColor(1); display.setFont(); display.setTextWrap(false);
      display.setCursor(4, 18); display.print("LED Brightness:");
      display.setFont(&FreeSans9pt7b);
      display.setCursor(4, 41); display.print(cursorPos); display.print("/255");
      drawTopbar();
      if (memOverlay) drawMemOverlay(false);
      display.display();
      break;
    case 1:
      cursorPos = constrain(cursorPos, 0, 100);
      VCWidth = cursorPos * 120.0f / 100.0f;
      display.drawRoundRect(4, 45, 120, 12, 3, 1);
      display.fillRoundRect(4, 45, (int)VCWidth, 12, 3, 1);
      display.setTextColor(1); display.setFont(); display.setTextWrap(false);
      display.setCursor(4, 18); display.print("JPEG quality:");
      display.setFont(&FreeSans9pt7b);
      display.setCursor(4, 41); display.print(cursorPos); display.print("/100");
      drawTopbar();
      if (memOverlay) drawMemOverlay(false);
      display.display();
      break;
    default: break;
  }
}


// ═══════════════════════════════════════════════════════════════════
//  BUTTON HANDLER
// ═══════════════════════════════════════════════════════════════════

void buttonActionHandler(unsigned long time) {
  switch (mode) {
    case 0:
      if (time > 700) mode = 1;
      else takePicture();
      break;
    case 1:
      menuAction(activeList, cursorPos);
      break;
    case 2:
      if (time <= 700) {
        stopWebServer();
        scanPalettes();
        mode = 0;
        updateUsed();
      }
      break;
    case 3:
      saveValue(changedValue);
      break;
    case 4:
      if (time <= 700) {
        if (cursorPos == 0) { mode = 1; switchList(0); }
        else { galleryIndex = cursorPos - 1; mode = 5; }
      }
      break;
    case 5:
      mode = 4; cursorPos = galleryIndex + 1;
      break;
    case 8:
      if (time <= 700) menuAction(3, cursorPos);
      break;
    default: break;
  }
}


// ═══════════════════════════════════════════════════════════════════
//  WEB SERVER SCREEN
// ═══════════════════════════════════════════════════════════════════

void drawWebServerScreen() {
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw < 1000) return;
  lastDraw = millis();
  display.clearDisplay();
  drawTopbar();
  display.setFont(); display.setTextColor(1); display.setTextWrap(false);
  display.setCursor(0, 14); display.println("Web Gallery ON");
  display.setCursor(0, 24); display.print("SSID: "); display.println(AP_SSID);
  display.setCursor(0, 34); display.print("IP:   "); display.println(WiFi.softAPIP().toString());
  display.setCursor(0, 44); display.print("Clients: "); display.println(WiFi.softAPgetStationNum());
  display.setFont(&Picopixel);
  display.setCursor(0, 63); display.print("Short press to stop");
  if (memOverlay) drawMemOverlay(false);
  display.display();
}


// ═══════════════════════════════════════════════════════════════════
//  VIEWFINDER
// ═══════════════════════════════════════════════════════════════════

void viewFinder() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) return;
  display.clearDisplay();
  int startX = (160-128)/2, startY = (120-64)/2;
  uint16_t* src = (uint16_t*)fb->buf;
  for (int y = 0; y < 64; y++) {
    for (int x = 0; x < 128; x++) {
      uint16_t px = src[(y+startY)*160 + (x+startX)];
      px = (px<<8)|(px>>8);
      uint8_t r = (px>>11)<<3, g = ((px>>5)&0x3F)<<2, b = (px&0x1F)<<3;
      uint8_t luma = (r*77+g*150+b*29)>>8;
      if (luma > 175) display.drawPixel(x, y, SSD1306_WHITE);
      else if (luma > 120 && x%2) display.drawPixel(x+(y%2), y, SSD1306_WHITE);
      else if (luma > 30 && x%3==1 && y%3==1) display.drawPixel(x, y, SSD1306_WHITE);
    }
  }
  esp_camera_fb_return(fb);
  drawTopbar();
  if (useFlash) {
    display.fillRect(114, 12, 16, 16, 0);
    display.drawRoundRect(113, 11, 16, 17, 1, 1);
    display.drawBitmap(114, 12, image_Voltage_bits, 16, 16, 1);
  }
  if (millis()-pressStart > 700 && digitalRead(SHUTTER_BUTTON)==LOW && pressStart!=0) {
    display.fillRoundRect(20, 15, 90, 16, 3, 0);
    display.drawRoundRect(20, 15, 89, 16, 3, 1);
    display.setTextColor(1); display.setTextWrap(false); display.setFont();
    display.setCursor(23, 19); display.print("Release - Menu");
  }
  if (memOverlay) drawMemOverlay(false);
  display.display();
}


// ═══════════════════════════════════════════════════════════════════
//  MENU
// ═══════════════════════════════════════════════════════════════════

void switchList(uint8_t listIndex) {
  if (listIndex >= LIST_COUNT) return;
  activeList = listIndex;
  cursorPos  = 0;
}

void menuAction(uint8_t listIndex, uint8_t itemIndex) {
  switch (listIndex) {
    case 0:  // main menu
      switch (itemIndex) {
        case 0: mode = 0; break;
        case 1: scanGallery(); cursorPos=0; mode=4; break;
        case 2: startWebServer(); mode=2; break;
        case 3: switchList(2); break;
        case 4: switchList(3); mode=8; break;
        case 5: switchList(1); break;
        case 6:  // sleep
          diode.setPixelColor(0,0,0,0); diode.show();
          diode2.setPixelColor(0,0,0,0); diode2.show();
          display.ssd1306_command(SSD1306_DISPLAYOFF);
          display.clearDisplay(); display.display();
          SD_MMC.end(); esp_camera_deinit(); Wire.end();
          WiFi.disconnect(true); WiFi.mode(WIFI_OFF);
          rtc_gpio_isolate(GPIO_NUM_14); delay(100);
          esp_sleep_enable_ext0_wakeup(WAKEUP_GPIO, 0); delay(50);
          esp_deep_sleep_start();
          break;
      }
      break;

    case 1:  // settings
      switch (itemIndex) {
        case 0: switchList(0); break;
        case 1:
          useFlash = !useFlash;
          display.fillRoundRect(20,15,88,16,3,0);
          display.drawRoundRect(20,15,88,16,3,1);
          display.setFont(); display.setCursor(useFlash?44:41,19);
          display.print(useFlash?"Enabled":"Disabled");
          display.display(); delay(1000);
          break;
        case 2: changedValue=0; startChangingValue(changedValue); break;
        case 3:
          useDither = !useDither;
          display.fillRoundRect(20,15,88,16,3,0);
          display.drawRoundRect(20,15,88,16,3,1);
          display.setFont(); display.setCursor(useDither?44:41,19);
          display.print(useDither?"Enabled":"Disabled");
          display.display(); delay(1000);
          break;
        case 4: changedValue=1; startChangingValue(changedValue); break;
        case 5:  // credits
          display.clearDisplay();
          display.setTextColor(1); display.setTextWrap(false);
          display.setFont(&FreeSerifBold12pt7b);
          display.setCursor(33,19); display.print("PixCam");
          display.drawBitmap(12,2,image_paint_17_bits,21,21,1);
          display.setFont(); display.setCursor(11,25); display.print("Made by:");
          display.setCursor(11,35); display.print("Perfect Nonsense");
          display.drawLine(8,38,5,38,1); display.drawLine(4,38,4,55,1);
          display.setCursor(18,52); display.print("Foxe");
          display.setCursor(18,44); display.print("Mint");
          display.drawLine(15,47,4,47,1); display.drawLine(15,56,4,56,1);
          display.setFont(&Picopixel);
          display.setCursor(43,50); display.print("- UI, firmware, case");
          display.setCursor(43,58); display.print("- Website, firmware");
          display.display();
          while (digitalRead(SHUTTER_BUTTON)==LOW)  {}
          while (digitalRead(SHUTTER_BUTTON)==HIGH) {}
          while (digitalRead(SHUTTER_BUTTON)==LOW)  {}
          break;
        case 6:  // reset
          display.clearDisplay();
          display.setTextColor(1); display.setTextWrap(false);
          display.setFont(&FreeSans9pt7b); display.setCursor(2,17);
          display.print("Reset settings?");
          display.setFont(); display.setCursor(5,24);
          display.print("Hold to reset EEPROM");
          display.setCursor(11,33); display.print("and your settings.");
          display.drawRoundRect(4,43,121,18,5,1);
          display.setCursor(17,48); display.print("//// Button \\\\");
          display.display();
          while (digitalRead(SHUTTER_BUTTON)==HIGH) {}
          pressStart = millis();
          display.clearDisplay();
          display.setTextColor(1); display.setTextWrap(false);
          display.setFont(&FreeSans9pt7b); display.setCursor(8,17);
          display.print("Are you sure?");
          display.setFont(); display.setCursor(17,24); display.print("Hold to confirm.");
          display.setCursor(17,33); display.print("Press to cancel.");
          display.drawRoundRect(4,43,121,18,5,1);
          display.fillRoundRect(6,45,117,14,3,1);
          display.setTextColor(0); display.setCursor(11,48);
          display.print("Release to cancel.");
          display.display();
          while (digitalRead(SHUTTER_BUTTON)==LOW) {
            if (millis()-pressStart > 3000) {
              display.clearDisplay();
              display.setTextColor(1); display.setTextWrap(false);
              display.setFont(&FreeSans9pt7b); display.setCursor(8,17);
              display.print("Are you sure?");
              display.setFont(); display.setCursor(17,24); display.print("Hold to confirm.");
              display.setCursor(17,33); display.print("Press to cancel.");
              display.drawRoundRect(4,43,121,18,5,1);
              display.drawRoundRect(6,45,117,14,3,1);
              display.setCursor(14,48); display.print("RELEASE TO RESET.");
              display.display();
            }
          }
          if (millis()-pressStart > 3000) {
            resetPrefs();
          }
          break;
        case 7:  // format SD
          display.clearDisplay();
          display.setTextColor(1); display.setTextWrap(false);
          display.setFont(&FreeSans9pt7b); display.setCursor(2,17);
          display.print("Format SD card?");
          display.setFont(); display.setCursor(5,24); display.print("Hold to format. ALL");
          display.setCursor(5,33); display.print("data will be lost.");
          display.drawRoundRect(4,43,121,18,5,1);
          display.setCursor(17,48); display.print("//// Button \\\\");
          display.display();
          while (digitalRead(SHUTTER_BUTTON)==HIGH) {}
          pressStart = millis();
          display.clearDisplay();
          display.setTextColor(1); display.setTextWrap(false);
          display.setFont(&FreeSans9pt7b); display.setCursor(8,17);
          display.print("Are you sure?");
          display.setFont(); display.setCursor(17,24); display.print("Hold to confirm.");
          display.setCursor(17,33); display.print("Press to cancel.");
          display.drawRoundRect(4,43,121,18,5,1);
          display.fillRoundRect(6,45,117,14,3,1);
          display.setTextColor(0); display.setCursor(11,48);
          display.print("Release to cancel.");
          display.display();
          while (digitalRead(SHUTTER_BUTTON)==LOW) {
            if (millis()-pressStart > 3000) {
              display.clearDisplay();
              display.setTextColor(1); display.setTextWrap(false);
              display.setFont(&FreeSans9pt7b); display.setCursor(8,17);
              display.print("Are you sure?");
              display.setFont(); display.setCursor(17,24); display.print("Hold to confirm.");
              display.setCursor(17,33); display.print("Press to cancel.");
              display.drawRoundRect(4,43,121,18,5,1);
              display.drawRoundRect(6,45,117,14,3,1);
              display.setCursor(14,48); display.print("RELEASE TO FORMAT.");
              display.display();
            }
          }
          if (millis()-pressStart > 3000) {
            display.clearDisplay();
            display.setTextColor(1); display.setFont(&FreeSans9pt7b);
            display.setCursor(4,17); display.print("Formatting...");
            display.setFont(&Org_01); display.setCursor(0,32);
            display.print("This may take a while.");
            display.display();
            SD_MMC.end();
            SD_MMC.setPins(39,38,40);
            if (SD_MMC.begin("/sdcard",true)) {
              File dir = SD_MMC.open("/images");
              if (dir) { File f; while((f=dir.openNextFile())) { String p="/images/"+String(f.name()); f.close(); SD_MMC.remove(p); } dir.close(); }
              dir = SD_MMC.open("/palettes");
              if (dir) { File f; while((f=dir.openNextFile())) { String p="/palettes/"+String(f.name()); f.close(); SD_MMC.remove(p); } dir.close(); }
              EEPROM.write(0,0); EEPROM.commit();
              pictureNumber=1; paletteCount=0; lists[2].length=0; galleryCount=0;
              updateUsed();
              display.clearDisplay(); display.setTextColor(1);
              display.setFont(&FreeSans9pt7b); display.setCursor(-1,13);
              display.print("Format complete.");
              display.setFont(&Org_01); display.setCursor(0,26);
              display.print("Images and palettes");
              display.drawLine(0,18,127,18,1);
              display.setCursor(0,32); display.print("have been deleted.");
              display.setFont(&Picopixel); display.setCursor(0,63);
              display.print("//=\\  //-\\  //-\\  ...");
              display.display(); delay(2000); ESP.restart();
            } else {
              display.clearDisplay(); display.setFont(&FreeSans9pt7b);
              display.setTextColor(1); display.setCursor(4,17);
              display.print("Format failed."); display.setFont();
              display.setCursor(4,28); display.print("SD card not found.");
              display.display(); delay(2000);
            }
          }
          switchList(1);
          break;
        case 8:  // mem overlay toggle
          memOverlay = !memOverlay;
          display.fillRoundRect(20,15,88,16,3,0);
          display.drawRoundRect(20,15,88,16,3,1);
          display.setFont(); display.setCursor(memOverlay?44:41,19);
          display.print(memOverlay?"Enabled":"Disabled");
          display.display(); delay(1000);
          break;
      }
      break;

    case 2:  // palette picker
      selectedPalette = itemIndex;
      loadPalette(getSelectedPalettePath());
      switchList(0);
      break;

    case 3:  // games menu
      switch (itemIndex) {
        case 0: switchList(0); mode=1; break;
        case 1: gameInit(); break;   // Star Wars — returns to menu internally
        case 2:
          tetrisInit();
          mode = 7;
          break;
      }
      break;
  }
}

void drawMenu() {
  const MenuList& list = lists[activeList];
  uint8_t scrollTop = 0;
  if (cursorPos >= 3) scrollTop = cursorPos - 3 + 1;

  display.clearDisplay();
  if (list.length > 3) {
    display.drawBitmap(120,15,image_Pin_pointer_bits,5,3,1);
    display.drawBitmap(120,56,image_SmallArrowDown_bits,5,3,1);
    uint8_t barH = max(4,(34*3)/list.length);
    uint8_t barY = ((34-barH)*cursorPos)/(list.length-1)+20;
    display.drawRoundRect(120,20,4,34,SSD1306_WHITE,1);
    display.fillRect(121,barY,2,barH,SSD1306_WHITE);
  }
  const int rowY[3]  = {14,30,46};
  for (uint8_t row = 0; row < 3; row++) {
    uint8_t idx = scrollTop + row;
    if (idx >= list.length) break;
    display.setTextWrap(false);
    if (idx == cursorPos) {
      display.fillRoundRect(3,rowY[row],114,14,3,1);
      display.setTextColor(0);
    } else {
      display.drawRoundRect(3,rowY[row],114,14,3,1);
      display.setTextColor(1);
    }
    display.setFont(); display.setTextSize(1);
    display.setCursor(6, rowY[row]+4);
    display.print(list.items[idx]);
  }
  drawTopbar();
  if (memOverlay) drawMemOverlay(false);
  display.display();
}


// ═══════════════════════════════════════════════════════════════════
//  LOADING SCREEN
// ═══════════════════════════════════════════════════════════════════

void loadingScreen(int progress, String text) {
  static const unsigned char PROGMEM image_Sprite_0001_bits[] = {
    0x00,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x18,0xc0,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x60,0x30,0x00,0x00,0x00,
    0x00,0x03,0xc0,0x00,0x00,0x00,0x00,0x03,0x80,0x0e,0x00,0x1f,0xc0,0x00,0x04,0x00,
    0x00,0x00,0x00,0x00,0x0c,0x00,0x01,0x80,0x0c,0x60,0x00,0x0c,0x00,0x00,0x00,0x00,
    0x00,0x30,0x00,0x00,0x60,0x0c,0x30,0x00,0x0c,0x00,0x00,0x00,0x00,0x00,0xc0,0x00,
    0x00,0x18,0x0c,0x30,0x00,0x0c,0x00,0x00,0x10,0x00,0x00,0xc0,0x00,0x00,0x18,0x0c,
    0x33,0xc6,0xde,0x1e,0x1e,0x78,0x00,0x00,0xb0,0x00,0x00,0x68,0x0c,0x64,0x67,0x0c,
    0x23,0x33,0x30,0x00,0x00,0x8c,0x07,0x01,0x88,0x0f,0xcf,0xe6,0x0c,0x7f,0x60,0x30,
    0x00,0x00,0x83,0x18,0xc6,0x08,0x0c,0x0c,0x06,0x0c,0x60,0x60,0x30,0x00,0x00,0x80,
    0xe0,0x38,0x08,0x0c,0x0c,0x06,0x0c,0x60,0x60,0x30,0x00,0x00,0x80,0x40,0x10,0x08,
    0x0c,0x0e,0x16,0x0c,0x70,0xe0,0x30,0x00,0x00,0x80,0x40,0x10,0x08,0x0c,0x06,0x26,
    0x0c,0x31,0x31,0x30,0x00,0x00,0x80,0x40,0x10,0x08,0x1e,0x03,0xcf,0x1e,0x1e,0x1e,
    0x1c,0x00,0x00,0x80,0x40,0x10,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x80,0x40,0x10,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x40,0x10,
    0x08,0x1c,0x1c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x60,0x30,0x08,0x0e,0x08,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x18,0xc0,0x08,0x0e,0x08,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x80,0x07,0x00,0x08,0x0b,0x08,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x80,0x02,0x00,0x08,0x0b,0x88,0x7c,0xee,0x1d,0x3c,0xee,0x1d,0x3c,0x80,0x02,
    0x00,0x08,0x09,0xc8,0xc6,0x73,0x23,0x46,0x73,0x23,0x46,0x80,0x02,0x00,0x08,0x08,
    0xc9,0x83,0x63,0x21,0xfe,0x63,0x21,0xfe,0x80,0x02,0x00,0x08,0x08,0x69,0x83,0x63,
    0x18,0xc0,0x63,0x18,0xc0,0xc0,0x02,0x00,0x18,0x08,0x39,0x83,0x63,0x0f,0xc0,0x63,
    0x0f,0xc0,0x30,0x02,0x00,0x60,0x08,0x19,0x83,0x63,0x23,0xe1,0x63,0x23,0xe1,0x0c,
    0x02,0x01,0x80,0x08,0x18,0xc6,0x63,0x23,0x62,0x63,0x23,0x62,0x03,0x82,0x0e,0x00,
    0x1c,0x08,0x7c,0xf7,0xbe,0x3c,0xf7,0xbe,0x3c,0x00,0x62,0x30,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1a,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
  };
  display.clearDisplay();
  display.drawRoundRect(5,48,118,12,3,1);
  display.fillRoundRect(7,50,114*progress/100,8,2,1);
  display.drawBitmap(5,6,image_Sprite_0001_bits,104,32,1);
  display.setTextColor(1); display.setTextWrap(false);
  display.setFont(&Picopixel); display.setCursor(6,45); display.print(text);
  if (memOverlay) drawMemOverlay(false);
  display.display();
}


// ═══════════════════════════════════════════════════════════════════
//  TAKE PICTURE
// ═══════════════════════════════════════════════════════════════════

void takePicture() {
  display.clearDisplay(); display.setFont();
  display.setCursor(0,0); display.println("Taking picture...");
  if (memOverlay) drawMemOverlay(false);
  display.display();

  camera_fb_t* dummy = esp_camera_fb_get();
  if (dummy) esp_camera_fb_return(dummy);
  delay(50);

  if (useFlash) {
    diode.setPixelColor(0, diode.Color(255,255,255)); diode.show();
    for (int i=0;i<3;i++) { dummy=esp_camera_fb_get(); if(dummy) esp_camera_fb_return(dummy); delay(200); }
    diode.setPixelColor(0, diode.Color(0,0,0)); diode.show();
    delay(300);
    diode.setPixelColor(0, diode.Color(255,255,255)); delay(150); diode.show();
  }

  camera_fb_t* fb = esp_camera_fb_get();
  if (useFlash) { diode.setPixelColor(0,diode.Color(0,0,0)); diode.show(); }
  if (!fb) { Serial.println("Capture failed"); return; }

  int W = fb->width;
  int totalPx = fb->len / 2;
  int H = (W > 0) ? (totalPx / W) : 0;
  if (W==0 || H==0) { esp_camera_fb_return(fb); return; }

  uint8_t* rgbBuf = (uint8_t*)ps_malloc(W*H*3);
  if (!rgbBuf) { esp_camera_fb_return(fb); return; }

  uint16_t* src = (uint16_t*)fb->buf;
  uint8_t*  dst = rgbBuf;
  for (int i=0; i<W*H; i++) {
    uint16_t px = src[i]; px = (px<<8)|(px>>8);
    *dst++ = (px&0x1F)<<3;
    *dst++ = ((px>>5)&0x3F)<<2;
    *dst++ = (px>>11)<<3;
  }
  esp_camera_fb_return(fb);

  display.println("Processing...");
  if (memOverlay) drawMemOverlay(false);
  display.display();
  if (useDither) ditherRGB(rgbBuf, W, H);

  uint8_t* jpgBuf = nullptr; size_t jpgLen = 0;
  bool encOk = fmt2jpg(rgbBuf, W*H*3, W, H, PIXFORMAT_RGB888, jpegQuality, &jpgBuf, &jpgLen);
  free(rgbBuf);
  if (!encOk || jpgLen==0) { if (jpgBuf) free(jpgBuf); return; }

  char path[32];
  snprintf(path, sizeof(path), "/images/img_%03d.jpg", pictureNumber);
  display.println("Saving...");
  if (memOverlay) drawMemOverlay(false);
  display.display();

  File f = SD_MMC.open(path, FILE_WRITE);
  if (f) { f.write(jpgBuf, jpgLen); f.close(); EEPROM.write(0, pictureNumber); EEPROM.commit(); pictureNumber++; }
  free(jpgBuf);
  updateUsed();
}


// ═══════════════════════════════════════════════════════════════════
//  TOPBAR & MEM OVERLAY
// ═══════════════════════════════════════════════════════════════════

void drawTopbar() {
  display.drawLine(0,11,127,11,1);
  display.fillRect(0,0,128,11,0);
  display.setTextColor(1); display.setTextWrap(false);
  display.setFont(&Picopixel);
  display.setCursor(16,7); display.print(freeMB); display.print(" MB");
  display.drawBitmap(2,1,image_SDcardMounted_bits,11,8,1);
  display.setCursor(63,7); display.print(activePaletteName);
  display.drawBitmap(52,1,image_paleta_bits,8,8,1);
}

void drawMemOverlay(bool push) {
  display.setCursor(0,0); display.setFont();
  display.println("Mem stats (f/t):");
  display.print("Heap: "); display.print(ESP.getFreeHeap());
  display.print("/"); display.println(ESP.getHeapSize());
  display.print(ESP.getFreePsram()); display.print("/");
  display.print(ESP.getPsramSize());
  if (push) display.display();
}


// ═══════════════════════════════════════════════════════════════════
//  SCAN PALETTES
// ═══════════════════════════════════════════════════════════════════

void scanPalettes() {
  paletteCount = 0;
  File dir = SD_MMC.open("/palettes");
  if (!dir || !dir.isDirectory()) return;
  File entry;
  while ((entry=dir.openNextFile()) && paletteCount < MAX_PALETTES) {
    String name = entry.name();
    if (name.endsWith(".pa")) {
      strncpy(paletteList[paletteCount], entry.name(), 63);
      paletteList[paletteCount][63] = '\0';
      paletteListPtrs[paletteCount] = paletteList[paletteCount];
      paletteCount++;
    }
    entry.close();
  }
  dir.close();
  lists[2].length = paletteCount;
}


// ═══════════════════════════════════════════════════════════════════
//  GALLERY
// ═══════════════════════════════════════════════════════════════════

void scanGallery() {
  galleryCount = 0;
  File dir = SD_MMC.open("/images");
  if (!dir || !dir.isDirectory()) return;
  File entry;
  while ((entry=dir.openNextFile()) && galleryCount < MAX_GALLERY_IMAGES) {
    String name = entry.name();
    if (name.endsWith(".jpg") || name.endsWith(".JPG")) {
      strncpy(galleryList[galleryCount], entry.name(), 31);
      galleryList[galleryCount][31] = '\0';
      galleryCount++;
    }
    entry.close();
  }
  dir.close();
  // Sort descending (newest first)
  for (int i=0; i<galleryCount-1; i++)
    for (int j=0; j<galleryCount-i-1; j++)
      if (strcmp(galleryList[j], galleryList[j+1]) < 0) {
        char tmp[32];
        strcpy(tmp, galleryList[j]);
        strcpy(galleryList[j], galleryList[j+1]);
        strcpy(galleryList[j+1], tmp);
      }
}

void drawGalleryList() {
  int totalItems = 1 + galleryCount;
  galleryLen = totalItems;
  cursorPos  = constrain(cursorPos, 0, totalItems-1);
  int scrollTop = 0;
  if (cursorPos >= 3) scrollTop = cursorPos - 2;

  display.clearDisplay();
  if (totalItems > 3) {
    display.drawBitmap(120,15,image_Pin_pointer_bits,5,3,1);
    display.drawBitmap(120,56,image_SmallArrowDown_bits,5,3,1);
    uint8_t barH = max(4,(34*3)/totalItems);
    uint8_t barY = (totalItems>1) ? ((34-barH)*cursorPos)/(totalItems-1)+20 : 20;
    display.drawRoundRect(120,20,4,34,SSD1306_WHITE,1);
    display.fillRect(121,barY,2,barH,SSD1306_WHITE);
  }
  const int rowY[3] = {14,30,46};
  for (uint8_t row=0; row<3; row++) {
    int itemIndex = scrollTop + row;
    if (itemIndex >= totalItems) break;
    bool selected = (itemIndex == cursorPos);
    display.setTextWrap(false);
    if (selected) { display.fillRoundRect(3,rowY[row],114,14,3,1); display.setTextColor(0); }
    else          { display.drawRoundRect(3,rowY[row],114,14,3,1); display.setTextColor(1); }
    display.setFont(); display.setTextSize(1);
    display.setCursor(6, rowY[row]+4);
    if (itemIndex==0) display.print("Back");
    else              display.print(galleryList[itemIndex-1]);
  }
  drawTopbar();
  if (memOverlay) drawMemOverlay(false);
  display.display();
}

void drawGalleryImage(int idx) {
  if (idx < 0 || idx >= galleryCount) return;
  char path[48];
  snprintf(path, sizeof(path), "/images/%s", galleryList[idx]);
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) return;
  size_t jpgLen = f.size();
  uint8_t* jpgBuf = (uint8_t*)ps_malloc(jpgLen);
  if (!jpgBuf) { f.close(); return; }
  f.read(jpgBuf, jpgLen); f.close();

  const int srcW=160, srcH=120, dstW=128, dstH=52;
  uint8_t* rgbBuf = (uint8_t*)ps_malloc(srcW*srcH*3);
  if (!rgbBuf) { free(jpgBuf); return; }

  bool ok = fmt2rgb888(jpgBuf, jpgLen, PIXFORMAT_JPEG, rgbBuf);
  free(jpgBuf);
  if (!ok) { free(rgbBuf); return; }

  display.clearDisplay();
  for (int dy=0; dy<dstH; dy++) {
    int sy = (dy*srcH)/dstH;
    for (int dx=0; dx<dstW; dx++) {
      int sx = (dx*srcW)/dstW;
      int i3 = (sy*srcW+sx)*3;
      uint8_t r=rgbBuf[i3+0], g=rgbBuf[i3+1], b=rgbBuf[i3+2];
      uint8_t luma = (r*77+g*150+b*29)>>8;
      if (luma>175) display.drawPixel(dx,dy,SSD1306_WHITE);
      else if (luma>75 && dx%2) display.drawPixel(dx+(dy%2),dy,SSD1306_WHITE);
      else if (luma>25 && dx%3==1 && dy%3==1) display.drawPixel(dx,dy,SSD1306_WHITE);
    }
  }
  free(rgbBuf);
}

void drawGalleryFullscreen() {
  static int lastDrawnIndex = -1;
  if (galleryIndex != lastDrawnIndex) {
    drawGalleryImage(galleryIndex);
    lastDrawnIndex = galleryIndex;
  }
  display.fillRect(0,52,128,12,0);
  display.drawLine(0,52,127,52,1);
  display.setTextColor(1); display.setTextWrap(false);
  display.setFont(&Picopixel); display.setCursor(4,60);
  display.print(galleryList[galleryIndex]);
  display.drawBitmap(56,56,image_ButtonLeftSmall_bits,3,5,1);
  display.drawBitmap(121,56,image_ButtonRightSmall_bits,3,5,1);
  display.drawRoundRect(61,56,56,5,1,1);
  if (galleryCount>1) {
    int fillW = max(1,(galleryIndex*54)/(galleryCount-1));
    display.fillRect(62,57,fillW,3,1);
  }
  if (memOverlay) drawMemOverlay(false);
  display.display();
}
