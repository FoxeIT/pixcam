#include "esp_camera.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Org_01.h"
#include "Picopixel.h"

/*
  _   ___        ___    ____    _      ____ ___ _  _____  ____  _____ __  __ 
 | | | \ \      / / \  / ___|  / \    / ___|_ _| |/ / _ \|  _ \| ____|  \/  |
 | | | |\ \ /\ / / _ \| |  _  / _ \   \___ \| || ' / | | | | | |  _| | |\/| |
 | |_| | \ V  V / ___ \ |_| |/ ___ \   ___) | || . \ |_| | |_| | |___| |  | |
  \___/   \_/\_/_/   \_\____/_/   \_\ |____/___|_|\_\___/|____/|_____|_|  |_|
                                                                             

 W loopie masz switch()

 Daj tam swój kod w takim formacie:

 case 2:
  *twój kod*
  return;

 Polecam zrobić osobną funkcję do rysowania menu dla zachowania czytelności kodu.

 Aby zmieniać tryby, użyj przycisku:

  ---- Przycisk ----
  |                |
 IO12           5V/3.3V

 Jak zrobię poprawnie działające menu, to zmienię też jak to działa.

 Gdy dodajesz tryb, zmień wartość zmiennej maxModes poniżej.
*/
const int maxModes = 1;
/*
 Miłego kodowania szanowny (nie)kolego <3

      ┌┬┐┬┌┐┌┌┬┐┌─┐┬─┐ ┬┌─┐┬  ┌─┐
 ───  │││││││ │ ├─┘│┌┴┬┘├┤ │  └─┐
      ┴ ┴┴┘└┘ ┴ ┴  ┴┴ └─└─┘┴─┘└─┘
*/


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C  // Najczęstszy adres I2C dla OLED

// Definicja pinów I2C dla ESP32-CAM
#define I2C_SDA 15
#define I2C_SCL 14

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Konfiguracja kamery (AI-Thinker)
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22

static const unsigned char PROGMEM image_SDcardFail_bits[] = {0xff,0xe0,0xed,0xe0,0xff,0xe0,0xe1,0xe0,0xde,0xe0,0xff,0xe0,0xff,0xe0,0xe6,0x00};

static const unsigned char PROGMEM image_SDcardMounted_bits[] = {0xff,0xe0,0xff,0x20,0xff,0xe0,0xff,0x20,0xff,0xe0,0xff,0x20,0xff,0xe0,0xe6,0x00};

static const unsigned char PROGMEM image_SmallArrowDown_bits[] = {0xf8,0x70,0x20};

static const unsigned char PROGMEM image_Pin_pointer_bits[] = {0x20,0x70,0xf8};

int mode = 0;

void setup() {
  pinMode(INPUT_PULLUP, 12);
  Serial.begin(115200);

  // 1. Inicjalizacja I2C na wybranych pinach
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 nie odnaleziony"));
    for (;;)
      ;
  }
  display.clearDisplay();
  display.display();

  // 2. Konfiguracja kamery
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
  config.pixel_format = PIXFORMAT_GRAYSCALE;  // Obraz czarno-biały (skala szarości)
  config.frame_size = FRAMESIZE_QQVGA;        // 160x120 (najmniejsza dostępna)
  config.fb_count = 1;

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Błąd inicjalizacji kamery");
    return;
  }
}

void loop() {
  switch (mode) {
    case 0:
      viewFinder();
      return;
    case 1:
      drawMenu(1, 1);
      return;

    default:
      mode = 0;
      return;
  }
  delay(50);
  if (digitalRead(12) == HIGH) {while(digitalRead(12) == HIGH){}mode = mode + 1;if (mode>maxModes){mode = 0;}}
}

void viewFinder() {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Błąd pobrania klatki");
    return;
  }

  display.clearDisplay();

  // Przetwarzanie i wyświetlanie obrazu
  // Pobieramy środkowy fragment 128x64 z klatki 160x120
  int startX = (160 - 128) / 2;
  int startY = (120 - 64) / 2;

  for (int y = 0; y < 64; y++) {
    for (int x = 0; x < 128; x++) {
      // Pobranie jasnościb piksela (0-255)
      uint8_t pixel = fb->buf[(y + startY) * 160 + (x + startX)];

      // Progowanie (Dithering/Threshold): powyżej 127 = biały, poniżej = czarny
      if (pixel > 170) {
        display.drawPixel(x, y, SSD1306_WHITE);
      } else {
        if (pixel > 80 && x % 2) {
          display.drawPixel(x + (y % 2), y, SSD1306_WHITE);
        }
      }
    }
  }
  esp_camera_fb_return(fb); 
  display.drawLine(0, 11, 127, 11, 1);

  display.fillRect(0, 0, 128, 11, 0);

  display.setTextColor(1);
  display.setTextWrap(false);
  display.setFont(&Org_01);
  display.setCursor(107, 7);
  display.print("100");

  display.drawRoundRect(102, 1, 23, 9, 3, 1);

  display.setFont(&Picopixel);
  display.setCursor(16, 7);
  display.print("2137mb");

  display.drawBitmap(2, 1, image_SDcardMounted_bits, 11, 8, 1);

  display.drawBitmap(2, 1, image_SDcardFail_bits, 11, 8, 1);

  display.fillCircle(64, 74, 23, 0);

  display.drawCircle(64, 74, 23, 1);

  display.setFont();
  display.setCursor(53, 56);
  display.print("0.5x");

  display.display();
        // Wysłanie bufora na ekran
   // Zwolnienie pamięci
}

void drawMenu(int lenght, int position) {
  display.clearDisplay();
    
  display.drawLine(0, 11, 127, 11, 1);

  display.setTextColor(1);
  display.setTextWrap(false);
  display.setFont(&Org_01);
  display.setCursor(107, 7);
  display.print("100");

  display.drawRoundRect(102, 1, 23, 9, 3, 1);

  display.setFont(&Picopixel);
  display.setCursor(16, 7);
  display.print("2137mb");

  display.drawBitmap(2, 1, image_SDcardMounted_bits, 11, 8, 1);

  display.drawBitmap(2, 1, image_SDcardFail_bits, 11, 8, 1);

  display.drawBitmap(120, 15, image_Pin_pointer_bits, 5, 3, 1);

  display.drawBitmap(120, 56, image_SmallArrowDown_bits, 5, 3, 1);

  display.fillRect(121, 20, 3, 34, 1);

  display.drawRoundRect(120, 20, 5, 34, 2, 1);

  display.drawRoundRect(3, 14, 114, 14, 3, 1);

  display.setFont();
  display.setCursor(6, 17);
  display.print("Back to camera");

  display.fillRoundRect(3, 30, 114, 14, 3, 1);

  display.setTextColor(0);
  display.setCursor(6, 33);
  display.print("Palettes");

  display.drawRoundRect(3, 46, 114, 14, 3, 1);

  display.setTextColor(1);
  display.setCursor(6, 49);
  display.print("Settings");

  display.display();

}