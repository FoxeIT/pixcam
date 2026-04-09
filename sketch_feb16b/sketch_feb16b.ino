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
#include "webserver.h"  // AP + HTTP server logic


// ─── Palette list (needed before lists[]) ────────────────────────
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

// ─── Menu lists (must be before loop()) ──────────────────────────
// NOTE: "Web Gallery" added as item 2; original items shift +1

// taking a picture - I wanna temporarily add smth that
//                    increases the resolution but it no workie :(
//            (16:28:39.502 -> fb: reported 320x240 len=38400 using 320x60)

const char* menuItemsa[6] = {
  "Return to cam",   //done
  "Gallery",         //DONE
  "Web Gallery",     //the website has a bug
  "Change palette",  //done
  "Game",            //there's a bug with resetting
  "Settings"         //done
};
const char* menuItemsb[8] = {
  "Back",              //DONE
  "Toggle flash",      //DONE
  "Flash brightness",  //DONE
  "Toggle dither",     //DONE
  "JPEG quality",      //DONE
  "Credits",           //DONE
  "Reset",             //DONE
  "Format SD"          //TODO
};

struct MenuList {
  const char** items;
  uint8_t length;
};

MenuList lists[] = {
  { menuItemsa, sizeof(menuItemsa) / sizeof(menuItemsa[0]) },
  { menuItemsb, sizeof(menuItemsb) / sizeof(menuItemsb[0]) },
  { paletteListPtrs, 0 }
};
const uint8_t LIST_COUNT = sizeof(lists) / sizeof(lists[0]);


// ─── Color / palette ─────────────────────────────────────────────
struct Color {
  uint8_t r, g, b;
};

const Color DEFAULT_PALETTE[] = {
  { 0, 0, 0 },
  { 255, 255, 255 },
  { 255, 0, 0 },
  { 0, 0, 255 },
};
const int DEFAULT_PALETTE_SIZE = 4;
const int MAX_PALETTE_SIZE = 256;

Color activePalette[MAX_PALETTE_SIZE];
int activePaletteSize = 0;


// ─── Developer note ──────────────────────────────────────────────
/*
  W loopie masz switch()

  Daj tam swój kod w takim formacie:

  case 3:          ← mode 2 jest zajęty przez webserver!
   *twój kod*
   return;

  Gdy dodajesz tryb, zmień wartość zmiennej maxModes poniżej.
*/
const int maxModes = 2;  // 0=cam, 1=menu, 2=webserver
/*
 Miłego kodowania szanowny (nie)kolego <3

      ┌┬┐┬┌┐┌┌┬┐┌─┐┬─┐ ┬┌─┐┬  ┌─┐
 ───  │││││││ │ ├─┘│┌┴┬┘├┤ │  └─┐
      ┴ ┴┴┘└┘ ┴ ┴  ┴┴ └─└─┘┴─┘└─┘
*/


// ─── Pin definitions ─────────────────────────────────────────────
#define SHUTTER_BUTTON 14
#define WAKEUP_GPIO GPIO_NUM_14

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define CLK_PIN 2
#define DT_PIN 3
#define I2C_SDA 41
#define I2C_SCL 42

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_NeoPixel diode(1, 47, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel diode2(1, 48, NEO_GRB + NEO_KHZ800);

// Camera pins
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 15
#define SIOD_GPIO_NUM 4
#define SIOC_GPIO_NUM 5
#define Y2_GPIO_NUM 11
#define Y3_GPIO_NUM 9
#define Y4_GPIO_NUM 8
#define Y5_GPIO_NUM 10
#define Y6_GPIO_NUM 12
#define Y7_GPIO_NUM 18
#define Y8_GPIO_NUM 17
#define Y9_GPIO_NUM 16
#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM 7
#define PCLK_GPIO_NUM 13

#define THRESHOLD 250


// ─── Bitmaps ─────────────────────────────────────────────────────
static const unsigned char PROGMEM image_Voltage_bits[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0xc0, 0x01, 0x80, 0x03, 0x80, 0x07, 0x00, 0x0f, 0xe0,
  0x01, 0xc0, 0x03, 0x80, 0x03, 0x00, 0x06, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
static const unsigned char PROGMEM image_paleta_bits[] = { 0x3e, 0x77, 0x5d, 0xff, 0xfe, 0xf8, 0x7c, 0x3e };
static const unsigned char PROGMEM image_SDcardMounted_bits[] = { 0xff, 0xe0, 0xff, 0x20, 0xff, 0xe0, 0xff, 0x20, 0xff, 0xe0, 0xff, 0x20, 0xff, 0xe0, 0xe6, 0x00 };
static const unsigned char PROGMEM image_SmallArrowDown_bits[] = { 0xf8, 0x70, 0x20 };
static const unsigned char PROGMEM image_Pin_pointer_bits[] = { 0x20, 0x70, 0xf8 };
static const unsigned char PROGMEM image_ButtonLeftSmall_bits[] = { 0x20, 0x60, 0xe0, 0x60, 0x20 };
static const unsigned char PROGMEM image_ButtonRightSmall_bits[] = { 0x80, 0xc0, 0xe0, 0xc0, 0x80 };
static const unsigned char PROGMEM image_paint_2_bits[] = {0xff,0xff,0x80,0xff,0x80,0xff,0x80,0xff,0x80,0xff,0x80,0xff,0x80,0xff,0x80,0xff,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0x01,0xff,0xff};


// ─── State ───────────────────────────────────────────────────────
Preferences preferences;
int mode = 0;
bool isPsram = false;
int pictureNumber = 0;
unsigned long pressStart = 0;
volatile int encoderDelta = 0;  // accumulated steps from ISR

void IRAM_ATTR encoderISR() {
  static uint8_t lastState = 0;
  uint8_t clk = digitalRead(CLK_PIN);
  uint8_t dt = digitalRead(DT_PIN);
  uint8_t state = (clk << 1) | dt;
  // Gray code transition table — invalid transitions produce 0
  static const int8_t table[16] = {
    0, -1, 1, 0,
    1, 0, 0, -1,
    -1, 0, 0, 1,
    0, 1, -1, 0
  };
  encoderDelta += table[(lastState << 2) | state];
  lastState = state;
}

char activePaletteName[64] = "default";

int totalMB, usedMB, freeMB;

int ledBrightness = 100;
bool useFlash = false;
bool useDither = true;  // toggle in settings to enable/disable dithering

// ─── Gallery state ───────────────────────────────────────────────
#define MAX_GALLERY_IMAGES 128
char galleryList[MAX_GALLERY_IMAGES][32];
int galleryCount = 0;
int galleryIndex = 0;          // current image in fullscreen


// ─── Nearest palette colour ──────────────────────────────────────
int nearestColor(int r, int g, int b) {
  int best = 0, bestDist = INT_MAX;
  for (int i = 0; i < activePaletteSize; i++) {
    int dr = r - activePalette[i].r;
    int dg = g - activePalette[i].g;
    int db = b - activePalette[i].b;
    int dist = dr * dr + dg * dg + db * db;
    if (dist < bestDist) {
      bestDist = dist;
      best = i;
    }
  }
  return best;
}


// ─── Palette loading ─────────────────────────────────────────────
bool loadPalette(const char* path) {
  if (path == nullptr) goto use_default;
  {
    File f = SD_MMC.open(path, FILE_READ);
    if (!f) {
      Serial.printf("Palette file not found: %s, using default\n", path);
      goto use_default;
    }
    activePaletteSize = 0;
    while (f.available() && activePaletteSize < MAX_PALETTE_SIZE) {
      String line = f.readStringUntil('\n');
      line.trim();
      if (line.length() == 0) continue;
      int r, g, b;
      // line.replace(',', ' ');
      if (sscanf(line.c_str(), "%d %d %d", &r, &g, &b) == 3) {
        activePalette[activePaletteSize++] = {
          (uint8_t)constrain(r, 0, 255),
          (uint8_t)constrain(g, 0, 255),
          (uint8_t)constrain(b, 0, 255)
        };
      }
    }
    f.close();
    if (activePaletteSize == 0) {
      Serial.println("Palette file empty or malformed, using default");
      goto use_default;
    }
    const char* filename = strrchr(path, '/');
    filename = filename ? filename + 1 : path;
    strncpy(activePaletteName, filename, sizeof(activePaletteName) - 1);
    activePaletteName[sizeof(activePaletteName) - 1] = '\0';
    Serial.printf("Loaded palette: %s (%d colors)\n", activePaletteName, activePaletteSize);
    return true;
  }
use_default:
  activePaletteSize = DEFAULT_PALETTE_SIZE;
  memcpy(activePalette, DEFAULT_PALETTE, sizeof(Color) * DEFAULT_PALETTE_SIZE);
  strncpy(activePaletteName, "default", sizeof(activePaletteName));
  activePaletteName[sizeof(activePaletteName) - 1] = '\0';
  Serial.printf("Using default palette (%d colors)\n", activePaletteSize);
  return false;
}


// ─── Floyd-Steinberg dithering ───────────────────────────────────
// void ditherRGB(uint8_t* buf, int w, int h) {
//   int sz = w * 3;
//   float* errCur = (float*)calloc(sz, sizeof(float));
//   float* errNxt = (float*)calloc(sz, sizeof(float));
//   if (!errCur || !errNxt) {
//     Serial.println("Dither alloc failed");
//     free(errCur);
//     free(errNxt);
//     return;
//   }
//   for (int y = 0; y < h; y++) {
//     memset(errNxt, 0, sz * sizeof(float));
//     for (int x = 0; x < w; x++) {
//       int idx = (y * w + x) * 3;
//       float r = constrain(buf[idx + 0] + errCur[x * 3 + 0], 0.0f, 255.0f);
//       float g = constrain(buf[idx + 1] + errCur[x * 3 + 1], 0.0f, 255.0f);
//       float b = constrain(buf[idx + 2] + errCur[x * 3 + 2], 0.0f, 255.0f);
//       int ci = nearestColor((int)r, (int)g, (int)b);
//       Color c = activePalette[ci];
//       buf[idx + 0] = c.r;
//       buf[idx + 1] = c.g;
//       buf[idx + 2] = c.b;
//       float er = r - c.r, eg = g - c.g, eb = b - c.b;
//       if (x + 1 < w) {
//         errCur[(x + 1) * 3 + 0] += er * 7 / 16;
//         errCur[(x + 1) * 3 + 1] += eg * 7 / 16;
//         errCur[(x + 1) * 3 + 2] += eb * 7 / 16;
//       }
//       if (x - 1 >= 0) {
//         errNxt[(x - 1) * 3 + 0] += er * 3 / 16;
//         errNxt[(x - 1) * 3 + 1] += eg * 3 / 16;
//         errNxt[(x - 1) * 3 + 2] += eb * 3 / 16;
//       }
//       errNxt[x * 3 + 0] += er * 5 / 16;
//       errNxt[x * 3 + 1] += eg * 5 / 16;
//       errNxt[x * 3 + 2] += eb * 5 / 16;
//       if (x + 1 < w) {
//         errNxt[(x + 1) * 3 + 0] += er * 1 / 16;
//         errNxt[(x + 1) * 3 + 1] += eg * 1 / 16;
//         errNxt[(x + 1) * 3 + 2] += eb * 1 / 16;
//       }
//     }
//     float* tmp = errCur;
//     errCur = errNxt;
//     errNxt = tmp;
//   }
//   free(errCur);
//   free(errNxt);
// }
void ditherRGB(uint8_t* buf, int w, int h) {
  int sz = w * 3;
  float* errCur = (float*)calloc(sz, sizeof(float));
  float* errNxt = (float*)calloc(sz, sizeof(float));
  
  if (!errCur || !errNxt) {
    Serial.println("Dither alloc failed");
    free(errCur);
    free(errNxt);
    return;
  }

  for (int y = 0; y < h; y++) {
    memset(errNxt, 0, sz * sizeof(float));
    for (int x = 0; x < w; x++) {
      int idx = (y * w + x) * 3;

      // 1. Read existing buffer values
      // Note: If buf is naturally BGR, then buf[idx+0] is Blue and buf[idx+2] is Red
      float b_val = constrain(buf[idx + 0] + errCur[x * 3 + 0], 0.0f, 255.0f);
      float g_val = constrain(buf[idx + 1] + errCur[x * 3 + 1], 0.0f, 255.0f);
      float r_val = constrain(buf[idx + 2] + errCur[x * 3 + 2], 0.0f, 255.0f);

      // 2. Find nearest color using the intended order (assuming nearestColor wants R, G, B)
      int ci = nearestColor((int)r_val, (int)g_val, (int)b_val);
      Color c = activePalette[ci];

      // 3. Write back to buffer in the swapped order (Blue, Green, Red)
      buf[idx + 0] = c.b; 
      buf[idx + 1] = c.g;
      buf[idx + 2] = c.r;

      // 4. Calculate error based on the swap
      float eb = b_val - c.b;
      float eg = g_val - c.g;
      float er = r_val - c.r;

      // 5. Propagate errors (Floyd-Steinberg)
      if (x + 1 < w) {
        errCur[(x + 1) * 3 + 0] += eb * 7 / 16.0f;
        errCur[(x + 1) * 3 + 1] += eg * 7 / 16.0f;
        errCur[(x + 1) * 3 + 2] += er * 7 / 16.0f;
      }
      if (x - 1 >= 0) {
        errNxt[(x - 1) * 3 + 0] += eb * 3 / 16.0f;
        errNxt[(x - 1) * 3 + 1] += eg * 3 / 16.0f;
        errNxt[(x - 1) * 3 + 2] += er * 3 / 16.0f;
      }
      errNxt[x * 3 + 0] += eb * 5 / 16.0f;
      errNxt[x * 3 + 1] += eg * 5 / 16.0f;
      errNxt[x * 3 + 2] += er * 5 / 16.0f;
      if (x + 1 < w) {
        errNxt[(x + 1) * 3 + 0] += eb * 1 / 16.0f;
        errNxt[(x + 1) * 3 + 1] += eg * 1 / 16.0f;
        errNxt[(x + 1) * 3 + 2] += er * 1 / 16.0f;
      }
    }
    // Swap error buffers for the next row
    float* tmp = errCur;
    errCur = errNxt;
    errNxt = tmp;
  }
  free(errCur);
  free(errNxt);
}

// ─── SD helpers ──────────────────────────────────────────────────
void updateUsed() {
  uint64_t totalBytes = SD_MMC.totalBytes();
  uint64_t usedBytes = SD_MMC.usedBytes();
  uint64_t freeBytes = totalBytes - usedBytes;
  totalMB = totalBytes / (1024.0 * 1024.0);
  usedMB = usedBytes / (1024.0 * 1024.0);
  freeMB = freeBytes / (1024.0 * 1024.0);
}

void resetPrefs() {
  preferences.clear();
  EEPROM.write(0, 0);
  EEPROM.commit();
  delay(50);
  display.clearDisplay();
  display.setFont(&FreeSans9pt7b);
  display.setCursor(0, 0);
  display.print("Restarting...");
  delay(250);
  ESP.restart();
}
int jpegQuality = 90;

// ─── setup() ─────────────────────────────────────────────────────
void setup() {
  preferences.begin("camera", false);
  ledBrightness = preferences.getInt("brightness", 100);
  jpegQuality = preferences.getInt("quality", 90);
  useFlash = preferences.getBool("flash", false);
  useDither = preferences.getBool("dither", true);
  pinMode(SHUTTER_BUTTON, INPUT_PULLUP);
  pinMode(CLK_PIN, INPUT_PULLUP);
  pinMode(DT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CLK_PIN), encoderISR, CHANGE);
  attachInterrupt(digitalPinToInterrupt(DT_PIN), encoderISR, CHANGE);

  Serial.begin(115200);
  Wire.begin(I2C_SDA, I2C_SCL);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 nie odnaleziony"));
    for (;;)
      ;
  }
  display.clearDisplay();
  display.display();
  loadingScreen(0, "CZY TO DZIALA");
  loadingScreen(30, "Inicjalizacja karty SD...");

  Serial.println("Starting SD Card");
  SD_MMC.setPins(39, 38, 40);
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("SD Card Mount Failed");
    return;
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    Serial.println("No SD Card attached");
    return;
  }
  if (cardType == CARD_MMC) Serial.println("SD Card Type: MMC");
  else if (cardType == CARD_SD) Serial.println("SD Card Type: SDSC");
  else if (cardType == CARD_SDHC) Serial.println("SD Card Type: SDHC");
  else Serial.println("SD Card Type: UNKNOWN");

  if (!SD_MMC.exists("/images")) SD_MMC.mkdir("/images");
  if (!SD_MMC.exists("/palettes")) SD_MMC.mkdir("/palettes");

  scanPalettes();
  loadingScreen(40, "Inicjalizacja flasha...");
  diode.begin();
  diode.clear();
  diode.setBrightness(ledBrightness);
  diode.show();

  loadPalette("/palettes/gameboy.pa");
  updateUsed();
  loadingScreen(60, "Inicjalizacja kamery");
  Serial.printf("PSRAM size: %d\n", ESP.getPsramSize());

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_QQVGA;
  config.jpeg_quality = 10;  // lower = higher quality (1-63)
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.fb_count = 1;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed: 0x%x\n", err);
    loadingScreen(0, "BLAD KAMERY");
    delay(10000);
    return;
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
  Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
  Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());
}


// ─── loop() ──────────────────────────────────────────────────────
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
    case 0:
      viewFinder();
      break;
    case 1:
      if (lastmode != mode) switchList(0);
      drawMenu();
      break;
    case 2:
      drawWebServerScreen();
      handleWebServer();  // pump HTTP requests
      break;
    case 3:  //change value
      drawValueChanger(changedValue);
      break;
    case 4:  // gallery list
      drawGalleryList();
      break;
    case 5:  // gallery fullscreen
      drawGalleryFullscreen();
      break;
    default:
      mode = 0;
      break;
  }

  if (encoderDelta != 0) {
    static int remainder = 0;
    int delta = encoderDelta;
    encoderDelta = 0;

    if (mode == 5) {
      // fullscreen: cycle through images
      galleryIndex -= delta;
      if (galleryIndex < 0) galleryIndex = galleryCount - 1;
      if (galleryIndex >= galleryCount) galleryIndex = 0;
    } else {
      remainder += delta;
      int steps = remainder / 2;
      remainder %= 2;
      if (steps > 0) {
        if (mode == 3) {
          cursorPos++;
        } else if (mode == 4) {
          if (cursorPos < galleryLen - 1) cursorPos++;
        } else {
          if (cursorPos < (int)lists[activeList].length - 1) cursorPos++;
        }
      } else if (steps < 0) {
        if (cursorPos > 0) cursorPos--;
      }
    }
  }

  if ((lastButState == buttonState) && (cursorPos == lastPosition) && (mode == lastmode)) {
    if (millis() - sleepTimer > sleepTime) {
      diode.setPixelColor(0,0,0,0);
      diode.show();
      display.clearDisplay();
      display.display();
      if (mode == 2) {
        stopWebServer();
      }
      esp_sleep_enable_ext0_wakeup(WAKEUP_GPIO, 0);
      rtc_gpio_pullup_dis(WAKEUP_GPIO);
      rtc_gpio_pulldown_en(WAKEUP_GPIO);
      esp_deep_sleep_start();
    }
  } else {
    sleepTimer = millis();
  }

  lastmode = mode;
  lastButState = buttonState;
  lastPosition = cursorPos;
}

void startChangingValue(int value) {
  switch (value) {
    case 0:  //LED
      mode = 3;
      cursorPos = ledBrightness;
      break;
    case 1:  // quality
      mode = 3;
      cursorPos = jpegQuality;
      break;
    default:
      break;
  }
  while (digitalRead(SHUTTER_BUTTON) == LOW) {}
}

void saveValue(int value) {
  switch (value) {
    case 0:
      ledBrightness = cursorPos;
      preferences.putInt("brightness", ledBrightness);
      diode.setBrightness(ledBrightness);
      mode = 1;
      lastmode = 1;
      switchList(1);
      break;
    case 1:
      jpegQuality = cursorPos;
      preferences.putInt("quality", jpegQuality);
      mode = 1;
      lastmode = 1;
      switchList(1);
      break;
    default:
      break;
  }
}
float VCWidth;
void drawValueChanger(int value) {
  display.clearDisplay();
  switch (value) {
    case 0:
      if (cursorPos < 0) cursorPos = 0;
      if (cursorPos > 255) cursorPos = 255;
      VCWidth = cursorPos * 120 / 255;
      display.clearDisplay();

      display.drawRoundRect(4, 45, 120, 12, 3, 1);

      display.fillRoundRect(4, 45, VCWidth, 12, 3, 1);

      display.setTextColor(1);
      display.setFont();
      display.setTextWrap(false);
      display.setCursor(4, 18);
      display.print("LED Brightness:");

      display.setFont(&FreeSans9pt7b);
      display.setCursor(4, 41);
      display.print(cursorPos);
      display.print("/255");
      drawTopbar();
      display.display();
      break;
    case 1:
      if (cursorPos < 0) cursorPos = 0;
      if (cursorPos > 100) cursorPos = 100;
      VCWidth = cursorPos * 120 / 100;
      display.clearDisplay();

      display.drawRoundRect(4, 45, 120, 12, 3, 1);

      display.fillRoundRect(4, 45, VCWidth, 12, 3, 1);

      display.setTextColor(1);
      display.setFont();
      display.setTextWrap(false);
      display.setCursor(4, 18);
      display.print("JPEG quality:");

      display.setFont(&FreeSans9pt7b);
      display.setCursor(4, 41);
      display.print(cursorPos);
      display.print("/100");
      drawTopbar();
      display.display();
      break;
    default: break;
  }
}

// ─── Button handler ──────────────────────────────────────────────
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
      // Short press while in webserver mode → stop server, return to cam
      if (time <= 700) {
        stopWebServer();
        scanPalettes();   // rescan in case palettes were uploaded via web
        mode = 0;
        updateUsed();
      }
      break;
    case 3:
      saveValue(changedValue);
      break;
    case 4:  // gallery list
      if (time <= 700) {
        if (cursorPos == 0) {
          // "Back" item
          mode = 1;
          switchList(0);
        } else {
          galleryIndex = cursorPos - 1;  // -1 because item 0 is "Back"
          mode = 5;
        }
      }
      break;
    case 5:  // gallery fullscreen
      mode = 4;
      cursorPos = galleryIndex + 1;
      break;
    default:
      break;
  }
}


// ─── Webserver OLED screen ───────────────────────────────────────
void drawWebServerScreen() {
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw < 1000) return;  // redraw once per second
  lastDraw = millis();

  display.clearDisplay();
  drawTopbar();

  display.setFont();
  display.setTextColor(1);
  display.setTextWrap(false);

  display.setCursor(0, 14);
  display.println("Web Gallery ON");

  display.setCursor(0, 24);
  display.print("SSID: ");
  display.println(AP_SSID);

  display.setCursor(0, 34);
  display.print("IP:   ");
  display.println(WiFi.softAPIP().toString());

  display.setCursor(0, 44);
  display.print("Clients: ");
  display.println(WiFi.softAPgetStationNum());

  display.setFont(&Picopixel);
  display.setCursor(0, 63);
  display.print("Short press to stop");

  display.display();
}


// ─── viewFinder() ────────────────────────────────────────────────
void viewFinder() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) return;

  display.clearDisplay();

  int startX = (160 - 128) / 2;
  int startY = (120 - 64) / 2;
  uint16_t* src = (uint16_t*)fb->buf;

  for (int y = 0; y < 64; y++) {
    for (int x = 0; x < 128; x++) {
      uint16_t px = src[(y + startY) * 160 + (x + startX)];
      px = (px << 8) | (px >> 8);
      uint8_t r = (px >> 11) << 3;
      uint8_t g = ((px >> 5) & 0x3F) << 2;
      uint8_t b = (px & 0x1F) << 3;
      uint8_t luma = (r * 77 + g * 150 + b * 29) >> 8;
      if (luma > 175) display.drawPixel(x, y, SSD1306_WHITE);
      else if (luma > 75 && x % 2) display.drawPixel(x + (y % 2), y, SSD1306_WHITE);
      else if (luma > 25 && x % 3 == 1 && y % 3 == 1) display.drawPixel(x, y, SSD1306_WHITE);
    }
  }
  esp_camera_fb_return(fb);
  drawTopbar();

  if (useFlash) {
    display.fillRect(114, 12, 13, 16, 0);
    display.drawRoundRect(113, 11, 16, 17, 1, 1);
    display.drawBitmap(114, 12, image_Voltage_bits, 16, 16, 1);
  }

  if (millis() - pressStart > 700 && digitalRead(SHUTTER_BUTTON) == LOW && pressStart != 0) {
    display.fillRoundRect(20, 15, 90, 16, 3, 0);
    display.drawRoundRect(20, 15, 89, 16, 3, 1);
    display.setTextColor(1);
    display.setTextWrap(false);
    display.setFont();
    display.setCursor(23, 19);
    display.print("Release - Menu");
  }
  display.display();
}


// ─── Menu ────────────────────────────────────────────────────────
void switchList(uint8_t listIndex) {
  if (listIndex >= LIST_COUNT) return;
  activeList = listIndex;
  cursorPos = 0;
}

void menuAction(uint8_t listIndex, uint8_t itemIndex) {
  switch (listIndex) {
    case 0:  // main menu
      switch (itemIndex) {
        case 0: mode = 0; break;  // Return to cam
        case 1:                   // Gallery
          scanGallery();
          cursorPos = 0;
          mode = 4;
          break;
        case 2:  // Web Gallery
          startWebServer();
          mode = 2;
          break;
        case 3: switchList(2); break;  // Change palette
        case 4: gameInit(); break;     // Game
        case 5: switchList(1); break;  // Settings
      }
      break;

    case 1:  // settings
      switch (itemIndex) {
        case 0: switchList(0); break;
        case 1:
          useFlash = !useFlash;
          display.fillRoundRect(20, 15, 88, 16, 3, 0);
          display.drawRoundRect(20, 15, 88, 16, 3, 1);
          display.setFont();
          display.setCursor(useFlash ? 44 : 41, 19);
          display.print(useFlash ? "Enabled" : "Disabled");
          display.display();
          delay(1000);
          break;
        case 2:
          changedValue = 0;
          startChangingValue(changedValue);
          break;
        case 3:
          useDither = !useDither;
          display.fillRoundRect(20, 15, 88, 16, 3, 0);
          display.drawRoundRect(20, 15, 88, 16, 3, 1);
          display.setFont();
          display.setCursor(useDither ? 44 : 41, 19);
          display.print(useDither ? "Enabled" : "Disabled");
          display.display();
          delay(1000);
          break;
        case 4:
          changedValue = 1;
          startChangingValue(changedValue);
          break;
        case 5: 
          display.clearDisplay();

          display.setTextColor(1);
          display.setTextWrap(false);
          display.setFont(&FreeSerifBold12pt7b);
          display.setCursor(33, 19);
          display.print("PixCam");

          display.drawBitmap(14, 4, image_paint_2_bits, 16, 16, 1);

          display.setFont();
          display.setCursor(11, 25);
          display.print("Made by:");

          display.setCursor(11, 35);
          display.print("Perfect Nonsense");

          display.drawLine(8, 38, 5, 38, 1);

          display.drawLine(4, 38, 4, 55, 1);

          display.setCursor(18, 52);
          display.print("Foxe");

          display.setCursor(18, 44);
          display.print("Mint");

          display.drawLine(15, 47, 4, 47, 1);

          display.drawLine(15, 56, 4, 56, 1);

          display.setFont(&Picopixel);
          display.setCursor(43, 50);
          display.print("- UI, firmware, case");

          display.setCursor(43, 58);
          display.print("- Website, firmware");

          display.display();
          while (digitalRead(SHUTTER_BUTTON) == LOW) {}
          while (digitalRead(SHUTTER_BUTTON) == HIGH) {}
          break;
        case 6:
          display.clearDisplay();

          display.setTextColor(1);
          display.setTextWrap(false);
          display.setFont(&FreeSans9pt7b);
          display.setCursor(2, 17);
          display.print("Reset settings?");

          display.setFont();
          display.setCursor(5, 24);
          display.print("Hold to reset EEPROM");

          display.setCursor(11, 33);
          display.print("and your settings.");

          display.drawRoundRect(4, 43, 121, 18, 5, 1);

          display.setCursor(17, 48);
          display.print("//// Button \\\\");

          display.display();
          while (digitalRead(SHUTTER_BUTTON) == HIGH) {}
          pressStart = millis();
          display.clearDisplay();

          display.setTextColor(1);
          display.setTextWrap(false);
          display.setFont(&FreeSans9pt7b);
          display.setCursor(8, 17);
          display.print("Are you sure?");

          display.setFont();
          display.setCursor(17, 24);
          display.print("Hold to confirm.");

          display.setCursor(17, 33);
          display.print("Press to cancel.");

          display.drawRoundRect(4, 43, 121, 18, 5, 1);

          display.fillRoundRect(6, 45, 117, 14, 3, 1);

          display.setTextColor(0);
          display.setCursor(11, 48);
          display.print("Release to cancel.");

          display.display();

          while (digitalRead(SHUTTER_BUTTON) == LOW) {
            if (millis() - pressStart > 3000) {
              display.clearDisplay();

              display.setTextColor(1);
              display.setTextWrap(false);
              display.setFont(&FreeSans9pt7b);
              display.setCursor(8, 17);
              display.print("Are you sure?");

              display.setFont();
              display.setCursor(17, 24);
              display.print("Hold to confirm.");

              display.setCursor(17, 33);
              display.print("Press to cancel.");

              display.drawRoundRect(4, 43, 121, 18, 5, 1);

              display.drawRoundRect(6, 45, 117, 14, 3, 1);

              display.setCursor(14, 48);
              display.print("RELEASE TO RESET.");

              display.display();

            }
          }
          if (millis()-pressStart > 3000) {
            resetPrefs();
            display.clearDisplay();

            display.setTextColor(1);
            display.setTextWrap(false);
            display.setFont(&FreeSans9pt7b);
            display.setCursor(-1, 13);
            display.print("Reset complete.");

            display.setFont(&Org_01);
            display.setCursor(0, 26);
            display.print("Restarting camera to");

            display.drawLine(0, 18, 127, 18, 1);

            display.setCursor(0, 32);
            display.print("apply default values...");

            display.setFont(&Picopixel);
            display.setCursor(0, 63);
            display.print("//=\\  //-\\  //-\\  //-\\  //-\\   ...");

            display.display();
            delay(500);
            ESP.restart();
          }
          break;
        case 7: 
          display.clearDisplay();
          display.setTextColor(1);
          display.setTextWrap(false);
          display.setFont(&FreeSans9pt7b);
          display.setCursor(2, 17);
          display.print("Format SD card?");
          display.setFont();
          display.setCursor(5, 24);
          display.print("Hold to format. ALL");
          display.setCursor(5, 33);
          display.print("data will be lost.");
          display.drawRoundRect(4, 43, 121, 18, 5, 1);
          display.setCursor(17, 48);
          display.print("//// Button \\\\");
          display.display();
          while (digitalRead(SHUTTER_BUTTON) == HIGH) {}
          pressStart = millis();
          display.clearDisplay();
          display.setTextColor(1);
          display.setTextWrap(false);
          display.setFont(&FreeSans9pt7b);
          display.setCursor(8, 17);
          display.print("Are you sure?");
          display.setFont();
          display.setCursor(17, 24);
          display.print("Hold to confirm.");
          display.setCursor(17, 33);
          display.print("Press to cancel.");
          display.drawRoundRect(4, 43, 121, 18, 5, 1);
          display.fillRoundRect(6, 45, 117, 14, 3, 1);
          display.setTextColor(0);
          display.setCursor(11, 48);
          display.print("Release to cancel.");
          display.display();
          while (digitalRead(SHUTTER_BUTTON) == LOW) {
            if (millis() - pressStart > 3000) {
              display.clearDisplay();
              display.setTextColor(1);
              display.setTextWrap(false);
              display.setFont(&FreeSans9pt7b);
              display.setCursor(8, 17);
              display.print("Are you sure?");
              display.setFont();
              display.setCursor(17, 24);
              display.print("Hold to confirm.");
              display.setCursor(17, 33);
              display.print("Press to cancel.");
              display.drawRoundRect(4, 43, 121, 18, 5, 1);
              display.drawRoundRect(6, 45, 117, 14, 3, 1);
              display.setCursor(14, 48);
              display.print("RELEASE TO FORMAT.");
              display.display();
            }
          }
          if (millis() - pressStart > 3000) {
            display.clearDisplay();
            display.setTextColor(1);
            display.setFont(&FreeSans9pt7b);
            display.setCursor(4, 17);
            display.print("Formatting...");
            display.setFont(&Org_01);
            display.setCursor(0, 32);
            display.print("This may take a while.");
            display.display();
            SD_MMC.end();
            // Re-mount and format
            SD_MMC.setPins(39, 38, 40);
            if (SD_MMC.begin("/sdcard", true)) {
              // Delete all files in /images and /palettes
              File dir = SD_MMC.open("/images");
              if (dir) {
                File f;
                while ((f = dir.openNextFile())) {
                  String p = "/images/" + String(f.name());
                  f.close();
                  SD_MMC.remove(p);
                }
                dir.close();
              }
              dir = SD_MMC.open("/palettes");
              if (dir) {
                File f;
                while ((f = dir.openNextFile())) {
                  String p = "/palettes/" + String(f.name());
                  f.close();
                  SD_MMC.remove(p);
                }
                dir.close();
              }
              EEPROM.write(0, 0);
              EEPROM.commit();
              pictureNumber = 1;
              paletteCount = 0;
              lists[2].length = 0;
              galleryCount = 0;
              updateUsed();
              display.clearDisplay();
              display.setTextColor(1);
              display.setFont(&FreeSans9pt7b);
              display.setCursor(-1, 13);
              display.print("Format complete.");
              display.setFont(&Org_01);
              display.setCursor(0, 26);
              display.print("Images and palettes");
              display.drawLine(0, 18, 127, 18, 1);
              display.setCursor(0, 32);
              display.print("have been deleted.");
              display.setFont(&Picopixel);
              display.setCursor(0, 63);
              display.print("//=\\  //-\\  //-\\  //-\\  //-\\   ...");
              display.display();
              delay(2000);
              ESP.restart();
            } else {
              display.clearDisplay();
              display.setFont(&FreeSans9pt7b);
              display.setTextColor(1);
              display.setCursor(4, 17);
              display.print("Format failed.");
              display.setFont();
              display.setCursor(4, 28);
              display.print("SD card not found.");
              display.display();
              delay(2000);
            }
          }
          switchList(1);
          break;
      }
      break;

    case 2:  // palette picker
      selectedPalette = itemIndex;
      loadPalette(getSelectedPalettePath());
      switchList(0);
      break;
  }
}

void drawMenu() {
  const MenuList& list = lists[activeList];
  uint8_t scrollTop = 0;
  if (cursorPos >= 3) scrollTop = cursorPos - 3 + 1;

  display.clearDisplay();
  if (list.length > 3) {
    display.drawBitmap(120, 15, image_Pin_pointer_bits, 5, 3, 1);
    display.drawBitmap(120, 56, image_SmallArrowDown_bits, 5, 3, 1);
    uint8_t barH = max(4, (34 * 3) / list.length);
    uint8_t barY = ((34 - barH) * cursorPos) / (list.length - 1) + 20;
    display.drawRoundRect(120, 20, 4, 34, SSD1306_WHITE, 1);
    display.fillRect(121, barY, 2, barH, SSD1306_WHITE);
  }

  const int rowY[3] = { 14, 30, 46 };
  const int textY[3] = { 24, 33, 49 };  // Adjusted for Picopixel baseline
  // Use default (non-Picopixel) font – keep original style
  for (uint8_t row = 0; row < 3; row++) {
    uint8_t itemIndex = scrollTop + row;
    if (itemIndex >= list.length) break;
    display.setTextWrap(false);
    if (itemIndex == cursorPos) {
      display.fillRoundRect(3, rowY[row], 114, 14, 3, 1);
      display.setTextColor(0);
    } else {
      display.drawRoundRect(3, rowY[row], 114, 14, 3, 1);
      display.setTextColor(1);
    }
    display.setFont();
    display.setTextSize(1);
    display.setCursor(6, rowY[row] + 4);
    display.print(list.items[itemIndex]);
  }

  drawTopbar();
  display.display();
}


// ─── Loading screen ──────────────────────────────────────────────
void loadingScreen(int progress, String text) {
  static const unsigned char PROGMEM image_Sprite_0001_bits[] = {
    0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0xc0,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x30, 0x00, 0x00, 0x00,
    0x00, 0x03, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x03, 0x80, 0x0e, 0x00, 0x1f, 0xc0, 0x00, 0x04, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x01, 0x80, 0x0c, 0x60, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x30, 0x00, 0x00, 0x60, 0x0c, 0x30, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00,
    0x00, 0x18, 0x0c, 0x30, 0x00, 0x0c, 0x00, 0x00, 0x10, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x18, 0x0c,
    0x33, 0xc6, 0xde, 0x1e, 0x1e, 0x78, 0x00, 0x00, 0xb0, 0x00, 0x00, 0x68, 0x0c, 0x64, 0x67, 0x0c,
    0x23, 0x33, 0x30, 0x00, 0x00, 0x8c, 0x07, 0x01, 0x88, 0x0f, 0xcf, 0xe6, 0x0c, 0x7f, 0x60, 0x30,
    0x00, 0x00, 0x83, 0x18, 0xc6, 0x08, 0x0c, 0x0c, 0x06, 0x0c, 0x60, 0x60, 0x30, 0x00, 0x00, 0x80,
    0xe0, 0x38, 0x08, 0x0c, 0x0c, 0x06, 0x0c, 0x60, 0x60, 0x30, 0x00, 0x00, 0x80, 0x40, 0x10, 0x08,
    0x0c, 0x0e, 0x16, 0x0c, 0x70, 0xe0, 0x30, 0x00, 0x00, 0x80, 0x40, 0x10, 0x08, 0x0c, 0x06, 0x26,
    0x0c, 0x31, 0x31, 0x30, 0x00, 0x00, 0x80, 0x40, 0x10, 0x08, 0x1e, 0x03, 0xcf, 0x1e, 0x1e, 0x1e,
    0x1c, 0x00, 0x00, 0x80, 0x40, 0x10, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x40, 0x10, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x40, 0x10,
    0x08, 0x1c, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x60, 0x30, 0x08, 0x0e, 0x08,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x18, 0xc0, 0x08, 0x0e, 0x08, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x80, 0x07, 0x00, 0x08, 0x0b, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x80, 0x02, 0x00, 0x08, 0x0b, 0x88, 0x7c, 0xee, 0x1d, 0x3c, 0xee, 0x1d, 0x3c, 0x80, 0x02,
    0x00, 0x08, 0x09, 0xc8, 0xc6, 0x73, 0x23, 0x46, 0x73, 0x23, 0x46, 0x80, 0x02, 0x00, 0x08, 0x08,
    0xc9, 0x83, 0x63, 0x21, 0xfe, 0x63, 0x21, 0xfe, 0x80, 0x02, 0x00, 0x08, 0x08, 0x69, 0x83, 0x63,
    0x18, 0xc0, 0x63, 0x18, 0xc0, 0xc0, 0x02, 0x00, 0x18, 0x08, 0x39, 0x83, 0x63, 0x0f, 0xc0, 0x63,
    0x0f, 0xc0, 0x30, 0x02, 0x00, 0x60, 0x08, 0x19, 0x83, 0x63, 0x23, 0xe1, 0x63, 0x23, 0xe1, 0x0c,
    0x02, 0x01, 0x80, 0x08, 0x18, 0xc6, 0x63, 0x23, 0x62, 0x63, 0x23, 0x62, 0x03, 0x82, 0x0e, 0x00,
    0x1c, 0x08, 0x7c, 0xf7, 0xbe, 0x3c, 0xf7, 0xbe, 0x3c, 0x00, 0x62, 0x30, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1a, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
  };
  display.clearDisplay();
  display.drawRoundRect(5, 48, 118, 12, 3, 1);
  display.fillRoundRect(7, 50, 114 * progress / 100, 8, 2, 1);
  display.drawBitmap(5, 6, image_Sprite_0001_bits, 104, 32, 1);
  display.setTextColor(1);
  display.setTextWrap(false);
  display.setFont(&Picopixel);
  display.setCursor(6, 45);
  display.print(text);
  display.display();
}


// ─── takePicture() ───────────────────────────────────────────────
// Capture flow:
//   1. Switch sensor to QVGA JPEG for the snapshot
//   2. Grab frame (JPEG bytes straight from camera)
//   3. Decode JPEG → RGB888 via esp_jpg_decode (correct channel order guaranteed)
//   4. Optionally dither in-place
//   5. Re-encode to JPEG and save
//   6. Switch sensor back to QQVGA RGB565 for the viewfinder

// Callback used by esp_jpg_decode to write decoded pixels into our buffer
struct DecodeCtx {
  uint8_t* buf;
  int w;
  int h;
};

static bool jpgDecodeCallback(void* arg, size_t x, size_t y,
                              size_t w, size_t h, uint8_t* data) {
  DecodeCtx* ctx = (DecodeCtx*)arg;
  for (size_t row = 0; row < h; row++) {
    uint8_t* dst = ctx->buf + ((y + row) * ctx->w + x) * 3;
    memcpy(dst, data + row * w * 3, w * 3);
  }
  return true;
}

void takePicture() {
  display.clearDisplay();
  display.setFont();
  display.setCursor(0, 0);
  display.println("Taking picture...");
  display.display();

  // Do NOT change framesize or pixformat here.
  // The PSRAM framebuffer was allocated once at init for QQVGA RGB565.
  // Calling set_framesize() at runtime makes fb->width/height report the
  // new size while the buffer itself stays the original allocation — so
  // you get the right dimensions reported but only the first N rows filled,
  // with the rest being zero (dark) or garbage. This is what caused the
  // black bottom 75% of the image.
  //
  // Instead: capture at QQVGA (same as viewfinder), derive W and H
  // directly from fb->len so we never trust the metadata fields.

  // Discard one frame so the sensor has settled exposure for the save.
  camera_fb_t* dummy = esp_camera_fb_get();
  if (dummy) esp_camera_fb_return(dummy);
  delay(50);

  if (useFlash) {
    diode.setPixelColor(0, diode.Color(0, 255, 0));
    diode.show();
    for (int i = 0; i<3;i++) {
      dummy = esp_camera_fb_get();
      if (dummy) esp_camera_fb_return(dummy);
      delay(200);
    }
    diode.setPixelColor(0, diode.Color(0, 0, 0));
    diode.show();
    delay(200);
    diode.setPixelColor(0, diode.Color(0, 255, 0));
    diode.show();
  }

  camera_fb_t* fb = esp_camera_fb_get();
  if (useFlash) {
    diode.setPixelColor(0, diode.Color(0, 0, 0));
    diode.show();
  }
  if (!fb) {
    Serial.println("Capture failed");
    return;
  }

  // Derive true pixel count from buffer length (2 bytes per RGB565 pixel).
  // Use fb->width as the stride (it matches the allocated row width),
  // but recompute H from len so we never process uninitialized bytes.
  int W = fb->width;
  int totalPx = fb->len / 2;
  int H = (W > 0) ? (totalPx / W) : 0;

  Serial.printf("fb: reported %dx%d  len=%d  using %dx%d\n",
                fb->width, fb->height, fb->len, W, H);

  if (W == 0 || H == 0) {
    Serial.println("Bad frame dimensions");
    esp_camera_fb_return(fb);
    return;
  }

  uint8_t* rgbBuf = (uint8_t*)ps_malloc(W * H * 3);
  if (!rgbBuf) {
    Serial.println("ps_malloc failed");
    esp_camera_fb_return(fb);
    return;
  }

  // Manual RGB565 → RGB888, only the pixels we actually have.
  // Byte-swap matches the viewfinder conversion.
  uint16_t* src = (uint16_t*)fb->buf;
  uint8_t* dst = rgbBuf;
  for (int i = 0; i < W * H; i++) {
    uint16_t px = src[i];
    px = (px << 8) | (px >> 8);        // fix endianness
    *dst++ = (px & 0x1F) << 3;         // B
    *dst++ = ((px >> 5) & 0x3F) << 2;  // G
    *dst++ = (px >> 11) << 3;          // R
  }
  esp_camera_fb_return(fb);

  // Dithering
  display.println("Processing...");
  display.display();
  if (useDither) ditherRGB(rgbBuf, W, H);

  // Re-encode to JPEG for saving
  uint8_t* jpgBuf = nullptr;
  size_t jpgLen = 0;
  bool encOk = fmt2jpg(rgbBuf, W * H * 3, W, H,
                       PIXFORMAT_RGB888, jpegQuality, &jpgBuf, &jpgLen);
  free(rgbBuf);

  if (!encOk || jpgLen == 0) {
    Serial.println("Encode failed");
    if (jpgBuf) free(jpgBuf);
    return;
  }

  char path[32];
  snprintf(path, sizeof(path), "/images/img_%03d.jpg", pictureNumber);
  display.println("Saving...");
  display.display();

  File f = SD_MMC.open(path, FILE_WRITE);
  if (f) {
    f.write(jpgBuf, jpgLen);
    f.close();
    EEPROM.write(0, pictureNumber);
    EEPROM.commit();
    pictureNumber++;
    Serial.printf("Saved: %s (%dx%d)\n", path, W, H);
  }
  free(jpgBuf);
  updateUsed();
}


// ─── drawTopbar() ────────────────────────────────────────────────
void drawTopbar() {
  display.drawLine(0, 11, 127, 11, 1);
  display.fillRect(0, 0, 128, 11, 0);
  display.setTextColor(1);
  display.setTextWrap(false);
  display.setFont(&Picopixel);
  display.setCursor(16, 7);
  display.print(freeMB);
  display.print(" MB");
  display.drawBitmap(2, 1, image_SDcardMounted_bits, 11, 8, 1);
  display.setCursor(63, 7);
  display.print(activePaletteName);
  display.drawBitmap(52, 1, image_paleta_bits, 8, 8, 1);
}


// ─── drawMemOverlay() ────────────────────────────────────────────
void drawMemOverlay(bool push) {
  display.setCursor(0, 0);
  display.setFont();
  display.println("Mem stats (f/t):");
  display.print("Heap: ");
  display.print(ESP.getFreeHeap());
  display.print("/");
  display.println(ESP.getHeapSize());
  display.print(ESP.getFreePsram());
  display.print("/");
  display.print(ESP.getPsramSize());
  if (push) display.display();
}



// ─── scanPalettes() ──────────────────────────────────────────────
void scanPalettes() {
  paletteCount = 0;
  File dir = SD_MMC.open("/palettes");
  if (!dir || !dir.isDirectory()) {
    Serial.println("No /palettes directory found");
    return;
  }
  File entry;
  while ((entry = dir.openNextFile()) && paletteCount < MAX_PALETTES) {
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
  Serial.printf("Found %d palettes\n", paletteCount);
}


// ─── Game ────────────────────────────────────────────────────────
const int c = 261, d = 294, e = 329, f = 349, g = 391, gS = 415, a = 440, aS = 455, b = 466;
const int cH = 523, cSH = 554, dH = 587, dSH = 622, eH = 659, fH = 698, fSH = 740, gH = 784, gSH = 830, aH = 880;

const unsigned char PROGMEM dioda16[] = {
  0x00, 0x00, 0x00, 0x00, 0x1C, 0x00, 0x3F, 0xF0, 0x3C, 0x00, 0x3C, 0x00, 0xFF, 0x00, 0x7F, 0xFF,
  0x7F, 0xFF, 0xFF, 0x00, 0x3C, 0x00, 0x3C, 0x00, 0x1F, 0xF0, 0x1C, 0x00, 0x00, 0x00, 0x00, 0x00
};
const unsigned char PROGMEM storm[] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x7F, 0xFE, 0x00, 0x00, 0x00, 0x07, 0x80, 0x01, 0xE0, 0x00, 0x00, 0x0C,
  0x00, 0x00, 0x20, 0x00, 0x00, 0x18, 0x00, 0x00, 0x18, 0x00, 0x00, 0x30, 0x00, 0x00, 0x04, 0x00,
  0x00, 0x20, 0x00, 0x00, 0x04, 0x00, 0x00, 0x20, 0x00, 0x00, 0x04, 0x00, 0x00, 0x60, 0x00, 0x00,
  0x02, 0x00, 0x00, 0x40, 0x00, 0x00, 0x02, 0x00, 0x00, 0x40, 0x00, 0x00, 0x01, 0x00, 0x00, 0x40,
  0x00, 0x00, 0x01, 0x00, 0x00, 0x40, 0x00, 0x00, 0x01, 0x00, 0x00, 0x7F, 0xE0, 0x00, 0x01, 0x00,
  0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x7F, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0xD7, 0xFF, 0xFF,
  0xE1, 0x00, 0x01, 0xBF, 0xFC, 0x1F, 0xFA, 0x80, 0x01, 0xBF, 0xF1, 0xCF, 0xFA, 0x80, 0x01, 0x3F,
  0xC2, 0x37, 0xF7, 0x80, 0x01, 0xEF, 0x9C, 0x01, 0xE7, 0xC0, 0x01, 0xE0, 0x70, 0x06, 0x06, 0x80,
  0x01, 0xE0, 0xC0, 0x03, 0x06, 0x80, 0x01, 0xFF, 0x80, 0x01, 0xFF, 0x80, 0x01, 0xF8, 0x00, 0x00,
  0x1D, 0xC0, 0x03, 0x70, 0x00, 0x80, 0x0C, 0x60, 0x05, 0xB0, 0x07, 0xF0, 0x08, 0x90, 0x09, 0x10,
  0x1F, 0xF8, 0x09, 0xD0, 0x0B, 0x90, 0x1F, 0x7C, 0x03, 0xF0, 0x0F, 0xC0, 0xFC, 0x0F, 0x07, 0x90,
  0x0D, 0x43, 0xC0, 0x03, 0x07, 0x90, 0x05, 0x64, 0x00, 0x00, 0xCF, 0x10, 0x07, 0xFC, 0x00, 0x00,
  0x26, 0x10, 0x01, 0x80, 0x00, 0x00, 0x10, 0x20, 0x01, 0x00, 0x00, 0x00, 0x0E, 0x40, 0x01, 0x80,
  0x07, 0xF0, 0x01, 0x80, 0x00, 0x80, 0x07, 0xC8, 0x00, 0x80, 0x00, 0x80, 0x0B, 0xE8, 0x00, 0x80,
  0x00, 0x87, 0x97, 0xE9, 0xE0, 0x80, 0x00, 0x87, 0xDF, 0xEF, 0xA0, 0x80, 0x00, 0x4B, 0xFF, 0xFF,
  0xA0, 0x80, 0x00, 0x6B, 0xDF, 0xFB, 0xA3, 0x00, 0x00, 0x24, 0x97, 0xE8, 0x24, 0x00, 0x00, 0x1E,
  0x1F, 0xC0, 0x2C, 0x00, 0x00, 0x07, 0xF8, 0x1F, 0xF0, 0x00, 0x00, 0x00, 0x0F, 0xF8, 0x00, 0x00
};

int strzalX = 0, strzalY = 0, czyStrzal = 0, pozycjaWroga = 8, kierunek = 0, koniec = 0;
int kula1X = 95, kula1Y = 0, kula2X = 95, kula2Y = 0, kula3X = 95, kula3Y = 0, kula4X = 95, kula4Y = 0;
int punkty = 0, predkosc = 3, predkoscWroga = 1, minCzas = 600, maxCzas = 1200, promien = 10;
int zycia = 5, startLicznika = 0, liczbaKul = 0, poziom = 1, srodek = 95;
unsigned long czasStart = 0, czasLosowy = 0, czasAktualny = 0, czasPoziomu = 0;
int pozycjaGracza = 30;
int encoderGamePos = 0;

void gameInit() {
  display.clearDisplay();
  display.drawBitmap(6, 11, storm, 48, 48, 1);
  display.setFont(&FreeSans9pt7b);
  display.setTextColor(WHITE);
  display.setCursor(65, 14);
  display.println("xWing");
  display.setFont();
  display.setCursor(65, 17);
  display.setTextSize(0);
  display.println("vs");
  display.setCursor(65, 30);
  display.println("Death");
  display.setCursor(65, 43);
  display.println("star");
  display.setTextSize(0);
  display.setCursor(65, 55);
  display.println("by FoxenIT");
  display.display();
  resetGry();
  delay(500);
  while (!koniec) rozgrywka();
  ekranKoncowy();
  delay(250);
}

void rozgrywka() {
  display.clearDisplay();
  if (startLicznika == 0) {
    czasStart = millis();
    czasLosowy = random(400, 1200);
    startLicznika = 1;
  }
  czasAktualny = millis();
  if ((czasAktualny - czasPoziomu) > 50000) {
    czasPoziomu = czasAktualny;
    poziom++;
    predkosc++;
    if (poziom % 2 == 0) {
      predkoscWroga++;
      promien--;
    }
    minCzas -= 50;
    maxCzas -= 50;
  }
  if ((czasLosowy + czasStart) < czasAktualny) {
    startLicznika = 0;
    liczbaKul++;
    switch (liczbaKul) {
      case 1:
        kula1X = 95;
        kula1Y = pozycjaWroga;
        break;
      case 2:
        kula2X = 95;
        kula2Y = pozycjaWroga;
        break;
      case 3:
        kula3X = 95;
        kula3Y = pozycjaWroga;
        break;
      case 4:
        kula4X = 95;
        kula4Y = pozycjaWroga;
        break;
    }
  }
  if (liczbaKul > 0) {
    display.drawCircle(kula1X, kula1Y, 2, 1);
    kula1X -= predkosc;
  }
  if (liczbaKul > 1) {
    display.drawCircle(kula2X, kula2Y, 1, 1);
    kula2X -= predkosc;
  }
  if (liczbaKul > 2) {
    display.drawCircle(kula3X, kula3Y, 4, 1);
    kula3X -= predkosc;
  }
  if (liczbaKul > 3) {
    display.drawCircle(kula4X, kula4Y, 2, 1);
    kula4X -= predkosc;
  }

  if (encoderDelta != 0) {
    encoderGamePos -= encoderDelta;  // negate if direction is backwards
    encoderDelta = 0;
    encoderGamePos = constrain(encoderGamePos, 2, 46);
    pozycjaGracza = encoderGamePos;
  }

  if (digitalRead(SHUTTER_BUTTON) == LOW && czyStrzal == 0) {
    czyStrzal = 1;
    strzalX = 6;
    strzalY = pozycjaGracza + 8;
  }
  if (czyStrzal == 1) {
    strzalX += 8;
    display.drawLine(strzalX, strzalY, strzalX + 4, strzalY, 1);
  }

  display.drawBitmap(4, pozycjaGracza, dioda16, 16, 16, 1);
  display.fillCircle(srodek, pozycjaWroga, promien, 1);
  display.fillCircle(srodek + 2, pozycjaWroga + 3, promien / 3, 0);

  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(33, 57);
  display.println("score:");
  display.setCursor(68, 57);
  display.println(punkty);
  display.setCursor(33, 0);
  display.println("lives:");
  display.setCursor(68, 0);
  display.println(zycia);
  display.setCursor(110, 0);
  display.println("L:");
  display.setCursor(122, 0);
  display.println(poziom);
  display.setCursor(108, 57);
  display.println(czasAktualny / 1000);

  if (strzalX > 128) czyStrzal = 0;
  if (kierunek == 0) pozycjaWroga += predkoscWroga;
  else pozycjaWroga -= predkoscWroga;
  if (pozycjaWroga >= (64 - promien)) kierunek = 1;
  if (pozycjaWroga <= promien) kierunek = 0;

  if (strzalY >= pozycjaWroga - promien && strzalY <= pozycjaWroga + promien && strzalX > (srodek - promien) && strzalX < (srodek + promien)) {
    strzalX = -20;
    punkty++;
    czyStrzal = 0;
  }

  int srodekGracza = pozycjaGracza + 8;
  if ((kula1Y >= srodekGracza - 8 && kula1Y <= srodekGracza + 8 && kula1X < 12 && kula1X > 4) || (kula2Y >= srodekGracza - 8 && kula2Y <= srodekGracza + 8 && kula2X < 12 && kula2X > 4) || (kula3Y >= srodekGracza - 8 && kula3Y <= srodekGracza + 8 && kula3X < 12 && kula3X > 4) || (kula4Y >= srodekGracza - 8 && kula4Y <= srodekGracza + 8 && kula4X < 12 && kula4X > 4)) {
    zycia--;
    kula1X = kula2X = kula3X = kula4X = 95;
    kula1Y = kula2Y = kula3Y = kula4Y = -50;
    liczbaKul = 0;
  }
  if (kula4X < 1) {
    liczbaKul = 0;
    kula4X = 200;
  }
  if (zycia == 0) koniec = 1;
  display.display();
}

void ekranKoncowy() {
  display.clearDisplay();
  display.setFont();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(7, 10);
  display.println("GAME OVER!");
  display.setTextSize(1);
  display.setCursor(7, 30);
  display.println("score:");
  display.setCursor(44, 30);
  display.println(punkty);
  display.setCursor(7, 40);
  display.println("level:");
  display.setCursor(44, 40);
  display.println(poziom);
  display.setCursor(7, 50);
  display.println("time(s):");
  display.setCursor(60, 50);
  display.println(czasAktualny / 1000);
  display.display();
}

void resetGry() {
  strzalX = 0;
  strzalY = 0;
  czyStrzal = 0;
  pozycjaWroga = 8;
  kierunek = 0;
  koniec = 0;
  kula1X = 95;
  kula1Y = 0;
  kula2X = 95;
  kula2Y = 0;
  kula3X = 95;
  kula3Y = 0;
  kula4X = 95;
  kula4Y = 0;
  punkty = 0;
  predkosc = 3;
  predkoscWroga = 1;
  minCzas = 600;
  maxCzas = 1200;
  promien = 12;
  zycia = 5;
  startLicznika = 0;
  liczbaKul = 0;
  poziom = 1;
  czasStart = 0;
  czasLosowy = 0;
  czasAktualny = 0;
  czasPoziomu = millis();  // anchor to now so level-up doesn't fire immediately
  encoderDelta = 0;
  encoderGamePos = 30;
  pozycjaGracza = 30;
}


// ─── Gallery ─────────────────────────────────────────────────────

// Scan /images, sort newest-first (by filename descending, since
// names are img_NNN.jpg and higher number = newer).
void scanGallery() {
  galleryCount = 0;
  File dir = SD_MMC.open("/images");
  if (!dir || !dir.isDirectory()) {
    Serial.println("No /images directory");
    return;
  }
  File entry;
  while ((entry = dir.openNextFile()) && galleryCount < MAX_GALLERY_IMAGES) {
    String name = entry.name();
    if (name.endsWith(".jpg") || name.endsWith(".JPG")) {
      strncpy(galleryList[galleryCount], entry.name(), 31);
      galleryList[galleryCount][31] = '\0';
      galleryCount++;
    }
    entry.close();
  }
  dir.close();

  // Bubble sort descending by filename (newest = highest number = last alpha)
  for (int i = 0; i < galleryCount - 1; i++) {
    for (int j = 0; j < galleryCount - i - 1; j++) {
      if (strcmp(galleryList[j], galleryList[j + 1]) < 0) {
        char tmp[32];
        strcpy(tmp, galleryList[j]);
        strcpy(galleryList[j], galleryList[j + 1]);
        strcpy(galleryList[j + 1], tmp);
      }
    }
  }
  Serial.printf("Gallery: %d images\n", galleryCount);
}

// Gallery list view — item 0 is always "Back", then image names
void drawGalleryList() {
  // Total items = 1 (Back) + galleryCount
  int totalItems = 1 + galleryCount;
  galleryLen = totalItems;
  cursorPos = constrain(cursorPos, 0, totalItems - 1);

  int scrollTop = 0;
  if (cursorPos >= 3) scrollTop = cursorPos - 2;

  display.clearDisplay();

  if (totalItems > 3) {
    display.drawBitmap(120, 15, image_Pin_pointer_bits, 5, 3, 1);
    display.drawBitmap(120, 56, image_SmallArrowDown_bits, 5, 3, 1);
    uint8_t barH = max(4, (34 * 3) / totalItems);
    uint8_t barY = (totalItems > 1)
                     ? ((34 - barH) * cursorPos) / (totalItems - 1) + 20
                     : 20;
    display.drawRoundRect(120, 20, 4, 34, SSD1306_WHITE, 1);
    display.fillRect(121, barY, 2, barH, SSD1306_WHITE);
  }

  const int rowY[3] = { 14, 30, 46 };
  for (uint8_t row = 0; row < 3; row++) {
    int itemIndex = scrollTop + row;
    if (itemIndex >= totalItems) break;

    bool selected = (itemIndex == cursorPos);
    display.setTextWrap(false);
    if (selected) {
      display.fillRoundRect(3, rowY[row], 114, 14, 3, 1);
      display.setTextColor(0);
    } else {
      display.drawRoundRect(3, rowY[row], 114, 14, 3, 1);
      display.setTextColor(1);
    }
    display.setFont();
    display.setTextSize(1);
    display.setCursor(6, rowY[row] + 4);
    if (itemIndex == 0) {
      display.print("Back");
    } else {
      display.print(galleryList[itemIndex - 1]);
    }
  }

  drawTopbar();
  display.display();
}

// Decode and scale a JPEG from SD to 128x53 (leaving 11px for topbar
// and 11px for the bottom UI strip), then draw pixel-by-pixel.
// Uses fmt2rgb888 to decode, then nearest-neighbour downscale.
void drawGalleryImage(int idx) {
  if (idx < 0 || idx >= galleryCount) return;

  char path[48];
  snprintf(path, sizeof(path), "/images/%s", galleryList[idx]);

  File f = SD_MMC.open(path, FILE_READ);
  if (!f) {
    Serial.printf("Gallery: can't open %s\n", path);
    return;
  }

  size_t jpgLen = f.size();
  uint8_t* jpgBuf = (uint8_t*)ps_malloc(jpgLen);
  if (!jpgBuf) {
    Serial.println("Gallery: malloc failed");
    f.close();
    return;
  }
  f.read(jpgBuf, jpgLen);
  f.close();

  // Decode to RGB888 — fmt2rgb888 needs a pre-allocated output buffer
  const int srcW = 160, srcH = 120;
  const int dstW = 128, dstH = 52;

  uint8_t* rgbBuf = (uint8_t*)ps_malloc(srcW * srcH * 3);
  if (!rgbBuf) {
    Serial.println("Gallery: rgb malloc failed");
    free(jpgBuf);
    return;
  }

  bool ok = fmt2rgb888(jpgBuf, jpgLen, PIXFORMAT_JPEG, rgbBuf);
  free(jpgBuf);
  if (!ok) {
    Serial.println("Gallery: decode failed");
    free(rgbBuf);
    return;
  }

  display.clearDisplay();

  for (int dy = 0; dy < dstH; dy++) {
    int sy = (dy * srcH) / dstH;
    for (int dx = 0; dx < dstW; dx++) {
      int sx = (dx * srcW) / dstW;
      int idx3 = (sy * srcW + sx) * 3;
      uint8_t r = rgbBuf[idx3 + 0];
      uint8_t g = rgbBuf[idx3 + 1];
      uint8_t b = rgbBuf[idx3 + 2];
      uint8_t luma = (r * 77 + g * 150 + b * 29) >> 8;
      if (luma > 175) display.drawPixel(dx, dy, SSD1306_WHITE);
      else if (luma > 75 && dx % 2) display.drawPixel(dx + (dy % 2), dy, SSD1306_WHITE);
      else if (luma > 25 && dx % 3 == 1 && dy % 3 == 1) display.drawPixel(dx, dy, SSD1306_WHITE);
    }
  }
  free(rgbBuf);
}

// Gallery fullscreen: image + bottom UI strip + optional action menu
void drawGalleryFullscreen() {
  static int lastDrawnIndex = -1;
  // Only redraw the image when the index changes (expensive)
  if (galleryIndex != lastDrawnIndex) {
    drawGalleryImage(galleryIndex);
    lastDrawnIndex = galleryIndex;
  }

  // ── Bottom UI strip ──────────────────────────────────────────────
  display.fillRect(0, 52, 128, 12, 0);
  display.drawLine(0, 52, 127, 52, 1);

  display.setTextColor(1);
  display.setTextWrap(false);
  display.setFont(&Picopixel);
  display.setCursor(4, 60);
  display.print(galleryList[galleryIndex]);

  display.drawBitmap(56, 56, image_ButtonLeftSmall_bits, 3, 5, 1);
  display.drawBitmap(121, 56, image_ButtonRightSmall_bits, 3, 5, 1);

  // Progress bar: fill proportional to position
  display.drawRoundRect(61, 56, 56, 5, 1, 1);
  if (galleryCount > 1) {
    int fillW = max(1, (galleryIndex * 54) / (galleryCount - 1));
    display.fillRect(62, 57, fillW, 3, 1);
  }



  display.display();

}
