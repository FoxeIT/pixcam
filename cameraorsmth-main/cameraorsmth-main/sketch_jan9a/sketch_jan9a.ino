#include "esp_camera.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Org_01.h"
#include "Picopixel.h"
#include "FS.h"                // SD Card ESP32
#include "SD_MMC.h"            // SD Card ESP32
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <EEPROM.h>            // read and write from flash memory





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
#define SHUTTER_BUTTON 13

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
  pinMode(INPUT_PULLUP, SHUTTER_BUTTON);
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
  loadingScreen(20, "Inicjalizacja kamery");

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
    loadingScreen(0, "BLAD KAMERY");
    delay(10000);
    return;
  }

  loadingScreen(40, "Inicjalizacja karty SD...");

  if(!SD_MMC.begin()){
    Serial.println("SD Card Mount Failed");
    return;
  }

  loadingScreen(50, "Inicjalizacja karty SD...")
  
  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){
    loadingScreen(0, "Brak karty SD");
    delay(1000);
    return;
  }

  loadingScreen(100, "Gotowe!");
}
int lastButtonState = LOW;
void loop() {
  //input tutaj
  bool buttonState = digitalRead()
  switch (mode) {
    case 0:
      viewFinder();
      return;
    case 1:
      // główne menu
      drawMenu(0, 0);
      return;
    case 2
  
    default:
      mode = 0;
      return;
  }
  delay(50);
  //do usunięcia
  if (digitalRead(SHUTTER_BUTTON) == HIGH){while(digitalRead(SHUTTER_BUTTON) == HIGH){}mode = mode + 1;}
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
      if (pixel > 200) {
        display.drawPixel(x, y, SSD1306_WHITE);
      } else {
        if (pixel > 100 && x % 2) {
          display.drawPixel(x + (y % 2), y, SSD1306_WHITE);
        } else {
          if (pixel > 50 && x % 3 == 1 && y % 3 == 1) {
            display.drawPixel(x, y, SSD1306_WHITE);
          }
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

void menuActionHandler(int position, int type) {
  switch (type) {
    case 0:
      switch(position) {
        case 0:
          mode = 0;
          return;
        case 1:
          //gejleria
          return;
        case 2:
          //gjerki
          return;
        default:
          return;
      }
    
    default:
      return;
    }
  }
}

void drawMenu(int position, int type) {
  int arrayLenght;
  switch (type) {
    case 0:
      //główne menu
      String[3] menuItems = {"Return to menu", "Gallery", "Games"};
      arrayLenght = 3;
  }
  int pages = arrayLenght-1/3
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

  display.fillRect(121, 20, 3, 34/(pages+1), 1);

  display.drawRoundRect(120, 20, 5, 34, 2, 1);
  display.setFont();
  if (position%3==0){display.fillRoundRect(3, 14, 114, 14, 3, 1);display.setTextColor(0);} else {display.drawRoundRect(3, 14, 114, 14, 3, 1);display.setTextColor(1);}

  display.setCursor(6, 17);
  display.print("Back to camera");

  if (position%3==1){display.fillRoundRect(3, 30, 114, 14, 3, 1);display.setTextColor(0);} else {display.drawRoundRect(3, 30, 114, 14, 3, 1);display.setTextColor(1);}

  display.setCursor(6, 33);
  display.print("Palettes");

  if (position%3==2){display.fillRoundRect(3, 46, 114, 14, 3, 1);display.setTextColor(0);} else {display.drawRoundRect(3, 46, 114, 14, 3, 1);display.setTextColor(1);}

  display.setCursor(6, 49);
  display.print("Settings");

  display.display();
}

void loadingScreen(int progress, String text) {
  static const unsigned char PROGMEM image_Sprite_0001_bits[] = {0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x60,0x60,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,0x18,0x40,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x00,0x01,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x80,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xf0,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1f,0xf8,0x1c,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1f,0xf8,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x9f,0xf8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0x1f,0xf8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x1f,0xf8,0xec,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xcf,0xf8,0x38,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x0f,0xf0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x42,0x21,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x42,0x20,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x06,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0xfc,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0xff,0xab,0xe0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0x55,0x55,0x5e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xfa,0xaa,0x00,0x01,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf5,0x54,0x00,0x00,0x00,0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0f,0xaa,0xa0,0x00,0x00,0x01,0xc0,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0xf5,0x50,0x00,0x00,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3e,0xaa,0x00,0x00,0x00,0x00,0x07,0xff,0xfe,0x07,0x80,0x00,0x00,0x00,0x07,0xd5,0x50,0x00,0x0f,0xff,0xff,0xff,0xff,0xff,0xff,0x80,0x00,0x00,0x01,0xfa,0xab,0xff,0xff,0xff,0xe0,0x00,0x00,0xff,0xf1,0xc0,0x00,0x00,0x03,0xfe,0x57,0xfc,0x7e,0x00,0x00,0x00,0x1f,0xff,0x00,0x00,0x38,0x00,0x00,0xfc,0x3f,0xf8,0x07,0x80,0x03,0xff,0xff,0xe0,0x00,0x00,0x00,0x06,0x00,0x1f,0x0f,0xc0,0x00,0x38,0x03,0xfc,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0xc0,0x60,0x70,0x00,0x01,0xdf,0xfc,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

  display.clearDisplay();

  display.drawRoundRect(5, 48, 118, 12, 3, 1);

  display.fillRoundRect(7, 50, 114*progress/100, 8, 2, 1);

  display.drawBitmap(5, 6, image_Sprite_0001_bits, 118, 32, 1);

  display.setTextColor(1);
  display.setTextWrap(false);
  display.setFont(&Picopixel);
  display.setCursor(6, 45);
  display.print(text);

  display.display();

}