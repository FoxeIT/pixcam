#include "esp_camera.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "Org_01.h"
#include "Picopixel.h"
#include <Fonts/FreeSans9pt7b.h>

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
const int maxModes = 3;
/*
 Miłego kodowania szanowny (nie)kolego <3

      ┌┬┐┬┌┐┌┌┬┐┌─┐┬─┐ ┬┌─┐┬  ┌─┐
 ───  │││││││ │ ├─┘│┌┴┬┘├┤ │  └─┐
      ┴ ┴┴┘└┘ ┴ ┴  ┴┴ └─└─┘┴─┘└─┘
*/
bool gameInitialized = false;
// Nuty
const int c=261; const int d=294; const int e=329; const int f=349; const int g=391; const int gS=415;
const int a=440; const int aS=455; const int b=466; const int cH=523; const int cSH=554;
const int dH=587; const int dSH=622; const int eH=659; const int fH=698; const int fSH=740;
const int gH=784; const int gSH=830; const int aH=880;

// Bitmapy
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

// --- Zmienne gry ---
int metx=0, mety=0, postoji=0, nep=8, smjer=0, go=0;
int rx=95, ry=0, rx2=95, ry2=0, rx3=95, ry3=0, rx4=95, ry4=0;
int bodovi=0, brzina=3, bkugle=1, najmanja=600, najveca=1200, promjer=10;
int zivoti=5, poc=0, ispaljeno=0, nivo=1, centar=95;
unsigned long pocetno=0, odabrano=0, trenutno=0, nivovrije=0;
int poz=30;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C  // Najczęstszy adres I2C dla OLED

// Definicja pinów I2C dla ESP32-CAM
#define BUTTON_UP 15
#define BUTTON_DOWN 16
#define BUTTON_SHOOT 12
#define BUZZER_PIN 2
#define SDA_PIN 13
#define SCL_PIN 14

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

int mode = 2;

void setup() {

  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_SHOOT, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.begin(115200);

  // 1. Inicjalizacja I2C na wybranych pinach
  Wire.begin(SDA_PIN, SCL_PIN);
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

    case 2:
      gameLoop();
      return;

    default:
      mode = 0;
      return;
  }
  delay(50);
  if (digitalRead(12) == HIGH) {while(digitalRead(12) == HIGH){}mode = mode ++;if (mode>maxModes){mode = 0;}}
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












void gameSetup() {
  Wire.begin(SDA_PIN, SCL_PIN);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_SHOOT, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  display.clearDisplay();

  display.drawBitmap(6, 11, storm, 48,48, 1);
  display.setFont(&FreeSans9pt7b);
  display.setTextColor(WHITE);
  display.setCursor(65,14); display.println("xWing");
  display.setFont(); display.setCursor(65,17); display.setTextSize(0); display.println("vs");
  display.setCursor(65,30); display.println("Death");
  display.setCursor(65,43); display.println("star");

  display.setTextSize(0); display.setCursor(65,55); display.println("by FoxenIT");
  display.display();

  beep(a,500); beep(a,500); beep(a,500);
  beep(f,350); beep(cH,150); beep(a,500);
  beep(f,350); beep(cH,150); beep(a,650);

  delay(500);
}


void gameLoop() {
  if (!gameInitialized) {
    gameSetup();
    gameInitialized = true;
  }

  if (go == 0) {
    gameplay();
  } else {
    gameOverScreen();
  }


}


void gameplay() {
  display.clearDisplay();

  // przesunięcie meteorytów, losowanie
  if(poc==0){
    pocetno=millis();
    odabrano= random(400,1200);
    poc=1;
  }
  trenutno=millis();

  // poziomy
  if((trenutno-nivovrije)>50000){
    nivovrije=trenutno;
    nivo++;
    brzina++;
    if(nivo % 2 == 0) { bkugle++; promjer--; }
    najmanja -=50;
    najveca -=50;
  }

  if((odabrano+pocetno)<trenutno){
    poc=0;
    ispaljeno++;
    switch(ispaljeno){
      case 1: rx=95; ry=nep; break;
      case 2: rx2=95; ry2=nep; break;
      case 3: rx3=95; ry3=nep; break;
      case 4: rx4=95; ry4=nep; break;
    }
  }

  // ruch strzałów
  if(ispaljeno>0){ display.drawCircle(rx,ry,2,1); rx-=brzina; }
  if(ispaljeno>1){ display.drawCircle(rx2,ry2,1,1); rx2-=brzina; }
  if(ispaljeno>2){ display.drawCircle(rx3,ry3,4,1); rx3-=brzina; }
  if(ispaljeno>3){ display.drawCircle(rx4,ry4,2,1); rx4-=brzina; }

  // ruch gracza
  if(digitalRead(BUTTON_UP)==0 && poz>=2) poz-=2;
  if(digitalRead(BUTTON_DOWN)==0 && poz<=46) poz+=2;

  // wystrzał
  if(digitalRead(BUTTON_SHOOT)==0 && postoji==0){
    postoji=1; metx=6; mety=poz+8; tone(BUZZER_PIN,1200,20);
  }
  if(postoji==1){ metx+=8; display.drawLine(metx,mety,metx+4,mety,1); }
  
  display.drawBitmap(4, poz,dioda16, 16,16, 1);
  display.fillCircle(centar,nep,promjer,1);
  display.fillCircle(centar+2,nep+3,promjer/3,0);

  display.setTextSize(1); display.setTextColor(WHITE);
  display.setCursor(33,57); display.println("score:"); display.setCursor(68,57); display.println(bodovi);
  display.setCursor(33,0); display.println("lives:"); display.setCursor(68,0); display.println(zivoti);
  display.setCursor(110,0); display.println("L:"); display.setCursor(122,0); display.println(nivo);
  display.setCursor(108,57); display.println(trenutno/1000);

  if(metx>128) postoji=0;

  // ruch meteorytów
  if(smjer==0) nep+=bkugle; else nep-=bkugle;
  if(nep>=(64-promjer)) smjer=1;
  if(nep<=promjer) smjer=0;

  // kolizja strzał-meteoryt
  if(mety>=nep-promjer && mety<=nep+promjer && metx>(centar-promjer)&&metx<(centar+promjer)){
    metx=-20; tone(BUZZER_PIN,500,20); bodovi++;
    postoji=0;
  }

  int pozicija=poz+8;

  // kolizja meteoryt-gracz
  if((ry>=pozicija-8 && ry<=pozicija+8 && rx<12&&rx>4) ||
     (ry2>=pozicija-8 && ry2<=pozicija+8 && rx2<12&&rx2>4) ||
     (ry3>=pozicija-8 && ry3<=pozicija+8 && rx3<12&&rx3>4) ||
     (ry4>=pozicija-8 && ry4<=pozicija+8 && rx4<12&&rx4>4)){
    tone(BUZZER_PIN,100,100);
    zivoti--;
    rx=rx2=rx3=rx4=95; ry=ry2=ry3=ry4=-50;
    ispaljeno=0;
  }

  if(rx4<1){ ispaljeno=0; rx4=200; }

  if(zivoti==0) go=1;

  display.display();
}

// --- Ekran Game Over ---
void gameOverScreen(){
  display.clearDisplay();
  display.setFont();  
  display.setTextSize(2); display.setTextColor(WHITE);
  display.setCursor(7,10); display.println("GAME OVER!");
  display.setTextSize(1);
  display.setCursor(7,30); display.println("score:");
  display.setCursor(44,30); display.println(bodovi);
  display.setCursor(7,40); display.println("level:");
  display.setCursor(44,40); display.println(nivo);
  display.setCursor(7,50); display.println("time(s):");
  display.setCursor(60,50); display.println(trenutno/1000);
  display.display();

  if (digitalRead(BUTTON_SHOOT) == LOW) {
    tone(BUZZER_PIN,280,300); delay(300);
    tone(BUZZER_PIN,250,200); delay(200);
    tone(BUZZER_PIN,370,300); delay(300);

    ponovo();                 // reset zmiennych gry
    gameInitialized = false;  // pozwala ponownie zainicjalizować grę
    mode = 1;                 // POWRÓT DO MENU
  }

}

// --- Reset gry ---
void ponovo(){
  metx=0; mety=0; postoji=0; nep=8; smjer=0; go=0;
  rx=95; ry=0; rx2=95; ry2=0; rx3=95; ry3=0; rx4=95; ry4=0;
  bodovi=0; brzina=3; bkugle=1; najmanja=600; najveca=1200; promjer=12;
  zivoti=5; poc=0; ispaljeno=0; nivo=1; pocetno=0; odabrano=0; trenutno=0; nivovrije=0;
}

// --- Dźwięki ---
void beep(int note, int duration) {
  ledcWriteTone(0, note);
  delay(duration);
  ledcWriteTone(0,0);
  delay(50);
}
