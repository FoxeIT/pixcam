#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/FreeSans9pt7b.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ESP32-CAM piny
#define BUTTON_UP 15
#define BUTTON_DOWN 16
#define BUTTON_SHOOT 12
#define BUZZER_PIN 2
#define SDA_PIN 13
#define SCL_PIN 14

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

void setup() {
  Wire.begin(SDA_PIN, SCL_PIN);
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_SHOOT, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
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

void loop() {
  if(go==0){
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

  // czekamy na przycisk, dopiero potem restart
  if(digitalRead(BUTTON_SHOOT)==0){
    tone(BUZZER_PIN,280,300); delay(300);
    tone(BUZZER_PIN,250,200); delay(200);
    tone(BUZZER_PIN,370,300); delay(300);
    ponovo();
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
