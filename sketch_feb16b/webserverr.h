#pragma once
// ─── webserver.h ─────────────────────────────────────────────────────────────
// PixCam web server — serves index.html from LittleFS (internal flash),
// images/palettes still live on SD card (SD_MMC).
//
// Endpoints:
//   GET  /                        → index.html from LittleFS
//   GET  /stream        (port 81) → MJPEG live stream
//   GET  /capture?save=1          → JPEG snapshot; if save=1 writes to SD
//   GET  /control?var=x&val=y     → sensor control
//   GET  /status                  → JSON: heap, psram, uptime, sd_free
//   GET  /gallery                 → JSON array of filenames from SD /images/
//   GET  /gallery/<n>             → serve JPEG from SD /images/
//   DELETE /gallery/<n>           → delete from SD
//   POST /upload-palette?name=x   → save palette body to SD /palettes/
//   POST /reboot                  → ESP.restart()
//
// Setup (do once, then every time you change index.html):
//   1. Install "ESP32 LittleFS Data Upload" plugin for Arduino IDE
//      https://github.com/lorol/arduino-esp32fs-plugin
//   2. Create a folder called "data" next to your .ino file.
//   3. Put index.html inside that "data" folder.
//   4. Tools -> ESP32 LittleFS Data Upload  (uploads to flash)
//   5. Partition scheme: Tools -> "16M Flash (3MB APP/9.9MB FATFS)" or use
//      the custom partitions.csv provided alongside this file.
// ─────────────────────────────────────────────────────────────────────────────

#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include "esp_camera.h"
#include "SD_MMC.h"
#include "img_converters.h"
#include <Adafruit_NeoPixel.h>

extern Adafruit_NeoPixel diode;
extern bool useFlash;
extern int pictureNumber;
extern int jpegQuality;

// EEPROM is included in the main sketch; we just need the object
#include <EEPROM.h>

// ── AP credentials ────────────────────────────────────────────────────────────
#ifndef AP_SSID
  #define AP_SSID "PixCam"
#endif
#ifndef AP_PASS
  #define AP_PASS ""          // empty = open network
#endif
// ─────────────────────────────────────────────────────────────────────────────

static WebServer httpServer(80);
static WebServer streamServer(81);
static bool webServerRunning = false;

static void handleRoot();
static void handleCapture();
static void handleControl();
static void handleStatus();
static void handleGalleryList();
static void handleGalleryFile();
static void handlePaletteUpload();
static void handlePaletteList();
static void handlePaletteFile();
static void handleReboot();
static void handleStream();
static void handleNotFound();

// ─────────────────────────────────────────────────────────────────────────────
//  Shared RGB565 framebuffer → JPEG helper
//  Returns true + fills outBuf/outLen.
//  If it allocated outBuf, *owned = true — caller must free() it.
//  If fb is already JPEG, outBuf points into fb->buf — do NOT free.
// ─────────────────────────────────────────────────────────────────────────────
static bool fb_to_jpg(camera_fb_t* fb,
                       uint8_t** outBuf, size_t* outLen,
                       bool* owned,
                       int quality = 12)
{
  *owned = false;
  if (fb->format == PIXFORMAT_JPEG) {
    *outBuf = fb->buf;
    *outLen = fb->len;
    return (*outLen > 0);
  }
  int W = fb->width;
  int H = fb->height;   // ← use the actual field, don't recompute
  if (H <= 0) return false;

  uint8_t* rgb = (uint8_t*)ps_malloc(W * H * 3);
  if (!rgb) return false;

  uint16_t* src = (uint16_t*)fb->buf;
  uint8_t* dst = rgb;
  for (int i = 0; i < W * H; i++) {
    uint16_t px = src[i];
    px = (px << 8) | (px >> 8);        // fix endianness
    *dst++ = (px & 0x1F) << 3;         // B
    *dst++ = ((px >> 5) & 0x3F) << 2;  // G
    *dst++ = (px >> 11) << 3;          // R
  }
  *outBuf = nullptr; *outLen = 0;
  bool ok = fmt2jpg(rgb, W * H * 3, W, H, PIXFORMAT_RGB888, quality, outBuf, outLen);
  free(rgb);
  if (ok) *owned = true;
  return ok && (*outLen > 0);
}

// ─────────────────────────────────────────────────────────────────────────────
//  GET /  →  serve index.html from LittleFS
// ─────────────────────────────────────────────────────────────────────────────
static void handleRoot() {
  File f = LittleFS.open("/index.html", "r");
  if (!f) {
    httpServer.send(200, "text/html",
      "<!DOCTYPE html><html><head><meta charset='UTF-8'/><title>PixCam</title></head>"
      "<body style='background:#0d0f13;color:#0fece1;font-family:monospace;padding:40px;line-height:2'>"
      "<h2>PixCam &mdash; LittleFS not ready</h2>"
      "<p>index.html has not been uploaded to flash yet.</p>"
      "<ol>"
      "<li>Please contact a Perfect Nonsense technician</li>"
      "</ol>"
      "<p>Camera OK. <a href='/capture' style='color:#40e880'>Test capture</a></p>"
      "</body></html>");
    return;
  }
  // Cache for 1 hour — it only changes when you re-flash
  httpServer.sendHeader("Cache-Control", "max-age=3600");
  httpServer.streamFile(f, "text/html");
  f.close();
}

// ─────────────────────────────────────────────────────────────────────────────
//  GET /capture?save=1
// ─────────────────────────────────────────────────────────────────────────────
static void handleCapture() {
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");

  if (useFlash) {
    diode.setPixelColor(0, diode.Color(255, 255, 255));
    diode.show();
  }

  camera_fb_t* fb = esp_camera_fb_get();

  if (useFlash) {
    diode.setPixelColor(0, diode.Color(0, 0, 0));
    diode.show();
  }

  if (!fb) { httpServer.send(503, "text/plain", "Capture failed"); return; }

  uint8_t* jpgBuf = nullptr;
  size_t   jpgLen = 0;
  bool     owned  = false;

  if (!fb_to_jpg(fb, &jpgBuf, &jpgLen, &owned, 90)) {
    esp_camera_fb_return(fb);
    httpServer.send(500, "text/plain", "Encode failed");
    return;
  }

  if (httpServer.hasArg("save") && httpServer.arg("save") == "1") {
    char path[32];
    snprintf(path, sizeof(path), "/images/img_%03d.jpg", pictureNumber);
    File f = SD_MMC.open(path, FILE_WRITE);
    if (f) {
      f.write(jpgBuf, jpgLen);
      f.close();
      EEPROM.write(0, pictureNumber);
      EEPROM.commit();
      pictureNumber++;
      Serial.printf("[web] Saved %s\n", path);
    } else {
      Serial.println("[web] SD write failed for capture");
    }
  }

  httpServer.sendHeader("Content-Disposition", "attachment; filename=\"capture.jpg\"");
  httpServer.send_P(200, "image/jpeg", (const char*)jpgBuf, jpgLen);

  if (owned) free(jpgBuf);
  esp_camera_fb_return(fb);
}

// ─────────────────────────────────────────────────────────────────────────────
//  GET /control?var=x&val=y
// ─────────────────────────────────────────────────────────────────────────────
static void handleControl() {
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");
  if (!httpServer.hasArg("var") || !httpServer.hasArg("val")) {
    httpServer.send(400, "text/plain", "Missing var/val"); return;
  }
  String var = httpServer.arg("var");
  int    val = httpServer.arg("val").toInt();

  sensor_t* s = esp_camera_sensor_get();
  if (!s) { httpServer.send(500, "text/plain", "No sensor"); return; }

  int res = 0;
  if      (var == "framesize")      res = s->set_framesize(s,      (framesize_t)val);
  else if (var == "quality")        res = s->set_quality(s,         val);
  else if (var == "brightness")     res = s->set_brightness(s,      val);
  else if (var == "contrast")       res = s->set_contrast(s,        val);
  else if (var == "saturation")     res = s->set_saturation(s,      val);
  else if (var == "sharpness")      res = s->set_sharpness(s,       val);
  else if (var == "aec")            res = s->set_exposure_ctrl(s,   val);
  else if (var == "aec_value")      res = s->set_aec_value(s,       val);
  else if (var == "aec2")           res = s->set_aec2(s,            val);
  else if (var == "ae_level")       res = s->set_ae_level(s,        val);
  else if (var == "agc")            res = s->set_gain_ctrl(s,       val);
  else if (var == "gainceiling")    res = s->set_gainceiling(s,     (gainceiling_t)val);
  else if (var == "agc_gain")       res = s->set_agc_gain(s,        val);
  else if (var == "awb")            res = s->set_whitebal(s,        val);
  else if (var == "awb_gain")       res = s->set_awb_gain(s,        val);
  else if (var == "wb_mode")        res = s->set_wb_mode(s,         val);
  else if (var == "hmirror")        res = s->set_hmirror(s,         val);
  else if (var == "vflip")          res = s->set_vflip(s,           val);
  else if (var == "lenc")           res = s->set_lenc(s,            val);
  else if (var == "dcw")            res = s->set_dcw(s,             val);
  else if (var == "bpc")            res = s->set_bpc(s,             val);
  else if (var == "wpc")            res = s->set_wpc(s,             val);
  else if (var == "raw_gma")        res = s->set_raw_gma(s,         val);
  else if (var == "special_effect") res = s->set_special_effect(s,  val);
  else if (var == "colorbar")       res = s->set_colorbar(s,        val);
  else if (var == "led_intensity") {
    // TODO: wire to your NeoPixel — e.g. diode.setBrightness(val); diode.show();
    Serial.printf("[web] led_intensity=%d\n", val);
  }
  else { httpServer.send(404, "text/plain", "Unknown: " + var); return; }

  if (res != 0) { httpServer.send(500, "text/plain", "Sensor error"); return; }
  httpServer.send(200, "text/plain", "OK");
}

// ─────────────────────────────────────────────────────────────────────────────
//  GET /status  →  JSON
// ─────────────────────────────────────────────────────────────────────────────
static void handleStatus() {
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");

  int sdFree = 0;
  uint64_t total = SD_MMC.totalBytes(), used = SD_MMC.usedBytes();
  if (total > 0) sdFree = (int)((total - used) / (1024ULL * 1024ULL));

  char json[256];
  snprintf(json, sizeof(json),
    "{\"ip\":\"%s\",\"free_heap\":%u,\"psram_free\":%u,\"uptime\":%lu,\"sd_free\":%d}",
    WiFi.softAPIP().toString().c_str(),
    ESP.getFreeHeap(),
    (unsigned)ESP.getFreePsram(),
    millis() / 1000,
    sdFree);
  httpServer.send(200, "application/json", json);
}

// ─────────────────────────────────────────────────────────────────────────────
//  GET /gallery  →  JSON array of filenames
// ─────────────────────────────────────────────────────────────────────────────
static void handleGalleryList() {
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");
  File dir = SD_MMC.open("/images");
  if (!dir || !dir.isDirectory()) {
    httpServer.send(200, "application/json", "[]"); return;
  }
  String json = "[";
  bool first = true;
  File entry;
  while ((entry = dir.openNextFile())) {
    String name = entry.name();
    int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    if (name.endsWith(".jpg") || name.endsWith(".jpeg")) {
      if (!first) json += ",";
      json += "\"" + name + "\"";
      first = false;
    }
    entry.close();
  }
  dir.close();
  json += "]";
  httpServer.send(200, "application/json", json);
}

// ─────────────────────────────────────────────────────────────────────────────
//  GET/DELETE /gallery/<n>
// ─────────────────────────────────────────────────────────────────────────────
static void handleGalleryFile() {
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");
  String name = httpServer.uri().substring(9);   // strip "/gallery/"
  if (name.length() == 0 || name.indexOf('/') >= 0) {
    httpServer.send(400, "text/plain", "Bad filename"); return;
  }
  String path = "/images/" + name;

  if (httpServer.method() == HTTP_DELETE) {
    bool removed = SD_MMC.remove(path);
    httpServer.send(removed ? 200 : 404, "text/plain", removed ? "Deleted" : "Not found");
    return;
  }
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) { httpServer.send(404, "text/plain", "Not found"); return; }
  httpServer.sendHeader("Cache-Control", "no-cache");
  httpServer.sendHeader("Content-Disposition", "attachment; filename=\"" + name + "\"");
  httpServer.sendHeader("Content-Length", String(f.size()));  // ← ADD THIS
  httpServer.streamFile(f, "image/jpeg");
  f.close();
}

// ─────────────────────────────────────────────────────────────────────────────
//  GET /palettes  →  JSON array of palette filenames
// ─────────────────────────────────────────────────────────────────────────────
static void handlePaletteList() {
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");
  File dir = SD_MMC.open("/palettes");
  if (!dir || !dir.isDirectory()) {
    httpServer.send(200, "application/json", "[]"); return;
  }
  String json = "[";
  bool first = true;
  File entry;
  while ((entry = dir.openNextFile())) {
    String name = entry.name();
    int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    if (name.endsWith(".pa")) {
      if (!first) json += ",";
      json += "\"" + name + "\"";
      first = false;
    }
    entry.close();
  }
  dir.close();
  json += "]";
  httpServer.send(200, "application/json", json);
}

// ─────────────────────────────────────────────────────────────────────────────
//  GET/DELETE /palettes/<n>
// ─────────────────────────────────────────────────────────────────────────────
static void handlePaletteFile() {
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");
  String name = httpServer.uri().substring(10);   // strip "/palettes/"
  if (name.length() == 0 || name.indexOf('/') >= 0) {
    httpServer.send(400, "text/plain", "Bad filename"); return;
  }
  String path = "/palettes/" + name;

  if (httpServer.method() == HTTP_DELETE) {
    bool removed = SD_MMC.remove(path);
    httpServer.send(removed ? 200 : 404, "text/plain", removed ? "Deleted" : "Not found");
    return;
  }
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) { httpServer.send(404, "text/plain", "Not found"); return; }
  httpServer.sendHeader("Cache-Control", "no-cache");
  httpServer.sendHeader("Content-Disposition", "attachment; filename=\"" + name + "\"");
  httpServer.sendHeader("Content-Length", String(f.size()));
  httpServer.streamFile(f, "text/plain");
  f.close();
}

// ─────────────────────────────────────────────────────────────────────────────
//  POST /upload-palette?name=x   body = "r g b\n..."
// ─────────────────────────────────────────────────────────────────────────────
static void handlePaletteUpload() {
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");
  String name = httpServer.hasArg("name") ? httpServer.arg("name") : "palette.pa";
  name.replace("/", ""); name.replace("\\", "");
  if (!name.length()) name = "palette.pa";

  // arg("plain") only works for form-encoded bodies; for a raw text/plain POST
  // we need to read directly from the client stream.
  String body = httpServer.arg("plain");
  if (!body.length()) {
    WiFiClient client = httpServer.client();
    int len = httpServer.header("Content-Length").toInt();
    if (len > 0) body = client.readString();
  }
  if (!body.length()) { httpServer.send(400, "text/plain", "Empty"); return; }

  if (!SD_MMC.exists("/palettes")) SD_MMC.mkdir("/palettes");
  File f = SD_MMC.open("/palettes/" + name, FILE_WRITE);
  if (!f) { httpServer.send(500, "text/plain", "SD write failed"); return; }
  f.print(body);
  f.close();
  Serial.printf("[web] Palette: /palettes/%s (%d bytes)\n", name.c_str(), body.length());
  httpServer.send(200, "text/plain", "OK");
}

// ─────────────────────────────────────────────────────────────────────────────
//  POST /reboot
// ─────────────────────────────────────────────────────────────────────────────
static void handleReboot() {
  httpServer.send(200, "text/plain", "Rebooting...");
  delay(200); ESP.restart();
}

// ─────────────────────────────────────────────────────────────────────────────
//  MJPEG stream — port 81
// ─────────────────────────────────────────────────────────────────────────────
static void handleStream() {
  WiFiClient client = streamServer.client();
  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: multipart/x-mixed-replace; boundary=frame");
  client.println("Access-Control-Allow-Origin: *");
  client.println("Connection: close");
  client.println();

  while (client.connected()) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) { delay(10); continue; }

    uint8_t* jpgBuf = nullptr;
    size_t   jpgLen = 0;
    bool     owned  = false;

    if (fb_to_jpg(fb, &jpgBuf, &jpgLen, &owned, 12) && jpgLen > 0) {
      client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n", jpgLen);
      client.write(jpgBuf, jpgLen);
      client.print("\r\n");
    }
    if (owned && jpgBuf) free(jpgBuf);
    esp_camera_fb_return(fb);
    delay(1);
  }
}

// ─────────────────────────────────────────────────────────────────────────────
//  404 handler — also catches DELETE /gallery/* that WebServer misses
// ─────────────────────────────────────────────────────────────────────────────
static void handleNotFound() {
  httpServer.sendHeader("Access-Control-Allow-Origin", "*");
  if (httpServer.uri().startsWith("/gallery/"))  { handleGalleryFile();  return; }
  if (httpServer.uri().startsWith("/palettes/")) { handlePaletteFile();  return; }
  httpServer.send(404, "text/plain", "Not found: " + httpServer.uri());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────────────────────────────────────
bool initLittleFS() {
  if (!LittleFS.begin(false)) {
    Serial.println("[fs] LittleFS mount failed, formatting...");
    if (!LittleFS.begin(true)) {
      Serial.println("[fs] LittleFS format failed!");
      return false;
    }
  }
  Serial.printf("[fs] LittleFS OK. Total=%u Used=%u\n",
                LittleFS.totalBytes(), LittleFS.usedBytes());
  return true;
}

void startWebServer() {
  if (webServerRunning) return;

  initLittleFS();

  WiFi.softAP(AP_SSID, (strlen(AP_PASS) > 0) ? AP_PASS : nullptr);
  delay(200);
  Serial.printf("[web] AP: SSID=%s  IP=%s\n",
                AP_SSID, WiFi.softAPIP().toString().c_str());

  httpServer.on("/",               HTTP_GET,  handleRoot);
  httpServer.on("/capture",        HTTP_GET,  handleCapture);
  httpServer.on("/control",        HTTP_GET,  handleControl);
  httpServer.on("/status",         HTTP_GET,  handleStatus);
  httpServer.on("/gallery",        HTTP_GET,  handleGalleryList);
  httpServer.on("/gallery/",       HTTP_GET,  handleGalleryFile);
  httpServer.on("/palettes",       HTTP_GET,  handlePaletteList);
  httpServer.on("/palettes/",      HTTP_GET,  handlePaletteFile);
  httpServer.on("/upload-palette", HTTP_POST, handlePaletteUpload);
  httpServer.on("/reboot",         HTTP_POST, handleReboot);
  httpServer.onNotFound(handleNotFound);

  // Collect Content-Length so handlePaletteUpload can fall back to stream-read
  const char* hdrs[] = { "Content-Length" };
  httpServer.collectHeaders(hdrs, 1);

  httpServer.begin();
  Serial.println("[web] HTTP on port 80");

  streamServer.on("/stream", HTTP_GET, handleStream);
  streamServer.begin();
  Serial.println("[web] Stream on port 81");

  webServerRunning = true;
}

void stopWebServer() {
  if (!webServerRunning) return;
  httpServer.stop();
  streamServer.stop();
  WiFi.softAPdisconnect(true);
  webServerRunning = false;
  Serial.println("[web] Stopped");
}

void handleWebServer() {
  if (!webServerRunning) return;
  httpServer.handleClient();
  streamServer.handleClient();
}
