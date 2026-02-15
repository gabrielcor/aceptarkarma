/**
 * @file EncoderSend2ApiDualCore
 * @author Gabriel Cor - gabrielcor@gmail.com
 * @brief M5 Dials to control the pentagram puzle runing on Unity. 5 dials control 1 laser dome and 4 mirrors.
 * @version 0.5
 * @date 2025-01-11
 *
 *
 * @Hardwares: M5Dial
 * @Platform Version: Arduino M5Stack Board Manager v2.0.7
 * @Dependent Library:
 * M5GFX: https://github.com/m5stack/M5GFX
 * M5Unified: https://github.com/m5stack/M5Unified
 * IMPORTANTE: para compilar y hacer despliegue, usar PSRAM: OPI PsRam, Flash Size: 8MB, Partition: 8MB with spiffs
 */
#include <Arduino.h>
#include <ArduinoOTA.h>
#include "M5Dial.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include  <WiFiUdp.h>
#include <ctype.h>
#include "../include/Bael_seal.h"
#include "../include/Bael_seal_accept.h"
#include "../include/Bael_seal_cancel.h"
#include "../include/Valac_seal.h"
#include "../include/Valac_seal_accept.h"
#include "../include/Valac_seal_cancel.h"
#include "../include/Asmoday_seal.h"
#include "../include/Asmoday_seal_accept.h"
#include "../include/Asmoday_seal_cancel.h"
#include "../include/Belial_seal.h"
#include "../include/Belial_seal_accept.h"
#include "../include/Belial_seal_cancel.h"
#include "../include/Paimon_seal.h"
#include "../include/Paimon_seal_accept.h"
#include "../include/Paimon_seal_cancel.h"
#include "../include/puzlehint/90_all_pngs.h"
#include "../include/puzlehint/91_all_pngs.h"


// Includes for MQTT Discovery
#include <ArduinoHA.h>
#define BROKER_ADDR     IPAddress(192,168,70,113)
#define BROKER_USERNAME     "mqttuser" 
#define BROKER_PASSWORD     "A1234567#"
// uint8_t uniqueId[6] = {0, 0, 0, 0, 0, 0}; // to store the Unique Id of the device this is NOT a MAC address
// uint8_t uniqueId[6] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED}; // for test purposes
// HADevice device(uniqueId, sizeof(uniqueId));
HADevice device;
WiFiClient netClient;                 // <-- WiFi client (replaces EthernetClient)
HAMqtt mqtt(netClient, device);
// "myInput" is unique ID of the sensor. You should define you own ID.

HABinarySensor sensorgameStarted("gameStarted");
HABinarySensor sensorenJuegoFinal("enJuegoFinal");
HABinarySensor sensorTouchCounting("touchCounting");
HABinarySensor sensorBlinkEnabled("blinkEnabled");
HABinarySensor sensorShowingAnimation("showingAnimation");
HASensor sensorIPAddress("deviceIPAddress");

String deviceName;

// Converted with https://rop.nl/truetype2gfx/
// #include "include/NotoSansDevanagari_Regular20pt7b.h"
// #include "include/NotoSansDevanagari_Regular5pt7b.h"

// Handle local control API
AsyncWebServer server(80);         // to handle the published API
bool gameStarted = false; // estado del "juego" o del control. 
bool onAnimation=true; // Indica si tengo que poner la animación
bool showingAnimation=false; // Indica si estoy mostrando la animación
bool enJuegoFinal = false; // Indica si estoy en el juego final
int defaultSpriteFinalEnUso = 1; // Indica el sprite final por defecto que se muestra al iniciar el juego (1=proceder, 2=cancelar)
int spriteFinalenUso = defaultSpriteFinalEnUso; // Indica si el sprite final que se está mostrando es el de proceder(1) o cancelar (2)
long oldPosition = -999;
long newPosition = -999;

static constexpr int TFT_MYORANGE      = 0xFA00; 

const int COLOR_PROCEDER = TFT_MYORANGE;
const int COLOR_CANCELAR = TFT_SKYBLUE;
const int COLOR_SELLO = TFT_PURPLE;
const int COLOR_VASSAGO = TFT_GREEN;
const int COLOR_APAGADO = TFT_BLACK;

const int K_M5VOLUME = 255; // volumen del buzzer 0-255

int enVassagoMode = 0; // 0-no, 1-sí

static constexpr int TFT_PAIMON = 0x9EFF;  // RGB(148,241,255)
static constexpr int TFT_ASMODAY = TFT_ORANGE; // RGB(255,255,106)
static constexpr int TFT_BAEL = 0xFEC9;    // RGB(255,255,220)
static constexpr int TFT_BELIAL = TFT_RED;  // RGB(255,215,136)
static constexpr int TFT_VALAC = 0xF99F;   // RGB(255,131,255)

// --- Contador táctil en juego final ---
bool touchCounting = false;
uint32_t touchStartMs = 0;
int lastShownCount = 0;
int secondsToSelect = 6; // segundos que hay que mantener el toque para seleccionar

// --- Blink a 6s ---
bool blinkEnabled = false;          // ¿estamos parpadeando?
bool blinkState = false;            // ON (se ve) / OFF (oculto)
uint32_t lastBlinkToggleMs = 0;     // última vez que se invirtió el estado
int freezePositionAtSelect = 0;     // posición congelada para mostrar el sprite fijo mientras blinquea
const uint32_t BLINK_INTERVAL_MS = 1000;  // 1 segundo on/off


// Matrix animation
lgfx::LGFX_Sprite matrixSprite(&M5Dial.Lcd);

const int matrixWidth = 240;
const int matrixHeight = 240;
const int charSize = 10;  // 10×10 pixel cells
const int cols = matrixWidth / charSize;
const int rows = matrixHeight / charSize;

int dropY[cols];  // current position of head per column

// end animation

// ARRAY with all bitmaps
// total 15: 5 devices x 3 states (normal, accept, cancel)
// memory used: 200x200/8*15 = 75000 bytes aprox
// 2026-02 agrego 5 más para la pista del puzle líquidos.

const unsigned char* epd_bitmap_allArray[25] = {
	epd_bitmap_250px_32_Asmoday_seal, epd_bitmap_01_Bael_seal, epd_bitmap_250px_09_Paimon_seal01, epd_bitmap_62_Valac_seal, epd_bitmap_250px_68_Belial_seal,
  epd_bitmap_250px_32_Asmoday_seal_Accept, epd_bitmap_01_Bael_seal_Accept, epd_bitmap_250px_09_Paimon_seal01_Accept, epd_bitmap_62_Valac_seal_Accept, epd_bitmap_250px_68_Belial_seal_Accept,
  epd_bitmap_250px_32_Asmoday_seal_Cancel, epd_bitmap_01_Bael_seal_Cancel, epd_bitmap_250px_09_Paimon_seal01_Cancel, epd_bitmap_62_Valac_seal_Cancel, epd_bitmap_250px_68_Belial_seal_Cancel,
   epd_bitmap_90_crystalcalibrator_1mc, epd_bitmap_90_frequency_1, epd_bitmap_90_modeselector_afc, epd_bitmap_90_scalevalue_absolute, epd_bitmap_90_waveform_sine,
   epd_bitmap_91_crystalcalibrator_1mc, epd_bitmap_91_frequency_1, epd_bitmap_91_modeselector_afc, epd_bitmap_91_scalevalue_absolute, epd_bitmap_91_waveform_sine

  };



// Para producción, solamente que intente conctarse a producción.
const char *ssid ="blackcrow_prod01";
const char *password = "e2aVwqCtfc5EsgGE852E";
const char *ssid2 = "blackcrow_prod01";
const char *password2 = "e2aVwqCtfc5EsgGE852E";

int activeWifiCredential = 1;
int wifiReconnectAttempts = 0;
uint32_t lastWifiReconnectAttemptMs = 0;
const uint32_t WIFI_RECONNECT_INTERVAL_MS = 5000;

bool lastTouchPressed = false;
int emergencyTapCount = 0;
uint32_t lastEmergencyTapMs = 0;
const uint32_t TAP_DEBOUNCE_MS = 180;
const uint32_t TAP_WINDOW_MS = 15000;

// const char *ssid2 = "blackcrow_01";
// const char *password2 = "8001017170";
int deviceId;

// const char *url2SendResult = "http://192.168.70.172:8080";
// const char *url2SendResult = "http://192.168.1.149:8080";
// const char* udpAddress = "192.168.50.224";  // Replace with your Unity  server IP
const char* udpAddress = "192.168.70.172";  // Replace with your Unity server IP
const int udpPort = 8080;

WiFiUDP udp;


long startPositionEndGame = -999;

int positionAdjustment = 0; // cuanto se ajusta lo que el dial tiene como valor hacia el envío para que coincida con la posición unity
int hasChanged = 0;
long value2Send = 0;
long prevValue = 0;
int changeDirection = -1; // 0-clockwise, 1-counterclockwise

lgfx::LGFX_Sprite sprite(&M5Dial.Lcd);

static constexpr int BITMAP_WIDTH = 200;
static constexpr int BITMAP_HEIGHT = 200;
static constexpr size_t MANUAL_BITMAP_BYTES = (BITMAP_WIDTH * BITMAP_HEIGHT) / 8;
uint8_t manualBitmapBuffer[MANUAL_BITMAP_BYTES];

int hexDigitToInt(char c)
{
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
  if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
  return -1;
}

size_t parseHexBitmapBytes(const String &imageData, uint8_t *output, size_t maxBytes)
{
  size_t outCount = 0;
  int i = 0;
  int n = imageData.length();

  while (i < n && outCount < maxBytes)
  {
    if ((imageData[i] == '0') && (i + 1 < n) && (imageData[i + 1] == 'x' || imageData[i + 1] == 'X'))
    {
      i += 2;

      int value = 0;
      int digitCount = 0;
      while (i < n)
      {
        int d = hexDigitToInt(imageData[i]);
        if (d < 0) break;
        if (digitCount < 2)
          value = (value << 4) | d;
        digitCount++;
        i++;
      }

      if (digitCount > 0)
        output[outCount++] = (uint8_t)(value & 0xFF);
    }
    else
    {
      i++;
    }
  }

  return outCount;
}

size_t parseHexBitmapBytesFromIndex(const String &imageData, int startIndex, uint8_t *output, size_t maxBytes)
{
  size_t outCount = 0;
  int i = startIndex;
  int n = imageData.length();

  while (i < n && outCount < maxBytes)
  {
    if ((imageData[i] == '0') && (i + 1 < n) && (imageData[i + 1] == 'x' || imageData[i + 1] == 'X'))
    {
      i += 2;

      int value = 0;
      int digitCount = 0;
      while (i < n)
      {
        int d = hexDigitToInt(imageData[i]);
        if (d < 0) break;
        if (digitCount < 2)
          value = (value << 4) | d;
        digitCount++;
        i++;
      }

      if (digitCount > 0)
        output[outCount++] = (uint8_t)(value & 0xFF);
    }
    else
    {
      i++;
    }
  }

  return outCount;
}

bool extractJsonStringField(const String &json, const char *fieldName, String &outValue)
{
  String key = String("\"") + fieldName + "\"";
  int keyPos = json.indexOf(key);
  if (keyPos < 0) return false;

  int colonPos = json.indexOf(':', keyPos + key.length());
  if (colonPos < 0) return false;

  int valueStart = colonPos + 1;
  while (valueStart < json.length() && isspace((unsigned char)json[valueStart]))
    valueStart++;

  if (valueStart >= json.length() || json[valueStart] != '"')
    return false;

  valueStart++; // skip opening quote
  outValue = "";
  bool escaping = false;

  for (int i = valueStart; i < json.length(); i++)
  {
    char c = json[i];
    if (escaping)
    {
      outValue += c;
      escaping = false;
      continue;
    }

    if (c == '\\')
    {
      escaping = true;
      continue;
    }

    if (c == '"')
      return true;

    outValue += c;
  }

  return false;
}
 
void sendEncoderToApi(int device, int step_mode, int step_size, int transition_time)
{

  if (WiFi.status() == WL_CONNECTED)
  {

    String payload = "{\"device\": \""+String(device)+"\", \"step_mode\": \""+String(step_mode)+"\", \"step_size\": \""+String(step_size)+"\",\"transition_time\": \""+String(transition_time)+"\"}";
    udp.beginPacket(udpAddress, udpPort);
    udp.print(payload);
    udp.endPacket();
  }
  else
  {
    Serial.println("WiFi not connected");
  }
}


TaskHandle_t Task1; // Definir el Handle para que compile
void Task1code( void * parameter) {
  Serial.print("Task1code() running on core ");
  Serial.println(xPortGetCoreID());

  for (;;) {
    // put your main code here, to run repeatedly:
    if (hasChanged == 1)
    {
      hasChanged = 0;
      if (gameStarted && !enJuegoFinal)
      {
        int envioValor = value2Send; // lo guardo porque entre que lo envío y lo guardo como previo, puede cambiar
        sendEncoderToApi(deviceId,changeDirection,envioValor,prevValue);
        prevValue = envioValor;
      }
    }
    delay(50);
  }
  
}

int GetColorSello(int deviceId)
{
  int tftColor = COLOR_SELLO;
  if (enVassagoMode == 1)
    return COLOR_VASSAGO;
  if (enVassagoMode == 2)
    return COLOR_APAGADO;

  switch (deviceId)
  {
  case 0: // Asmoday
    tftColor = TFT_ASMODAY;
    break;
  case 1: // Bael
    tftColor = TFT_BAEL;
    break;
  case 2: // Paimon
    tftColor = TFT_PAIMON;
    break;
  case 3: // Valac
    tftColor = TFT_VALAC;
    break;
  case 4: // Belial
    tftColor = TFT_BELIAL;
    break;  
  default:
    tftColor = COLOR_SELLO;
    break;
  }
  return tftColor;
}

void LoadSpriteKarma(int whichDevice, int spriteEnUso, const unsigned char *bitmapOverride = nullptr)
{

  if (enVassagoMode == 2)
  {
    M5Dial.Lcd.setBrightness(0);
    M5Dial.Lcd.fillScreen(TFT_BLACK);
    return;
  }
  M5Dial.Lcd.setBrightness(255);
  

  // Sprite def
  // screen size
  int scrS = 240; // M5Dial
  // bitmap width/height
  int bmpW = BITMAP_WIDTH;
  int bmpH = bmpW;
  // sprite width / height
  int sprW = 200;
  int sprH = sprW;
  // bitmap x,y
  int bmpX = (sprW - bmpW) / 2;
  int bmpY = (sprH - bmpH) / 2;
  // sprite placement
  int sppX = (scrS - sprW) / 2;
  int sppY = (scrS - sprH) / 2;

  int tftColor = COLOR_PROCEDER;
  if (spriteEnUso == 2)
    tftColor = COLOR_CANCELAR;
  else if (spriteEnUso == 9)
    tftColor = GetColorSello(deviceId);
  // Sprite

  const unsigned char *bitmapToShow;
  if (bitmapOverride != nullptr)
    bitmapToShow = bitmapOverride;
  else
    bitmapToShow = epd_bitmap_allArray[whichDevice];

  M5Dial.Lcd.clearDisplay(TFT_BLACK);
  sprite.setPsram(true);           // Optional if using large sprites
  sprite.createSprite(sprW, sprH); // Create a sprite of size 200x50
  sprite.fillScreen(TFT_BLACK);    // Clear sprite background
  sprite.drawBitmap(bmpX, bmpY, bitmapToShow, bmpW, bmpH, TFT_BLACK, tftColor);
  sprite.pushSprite(sppX, sppY);
  // Rotate and display the sprite at an arbitrary angle
  float angle = 0; // Angle in degrees
  sprite.pushRotated(angle);
  oldPosition = -999;
}

void ChangegameStarted(bool value)
{
  gameStarted = value;
  sensorgameStarted.setState(gameStarted);
}
void ChangeenJuegoFinal(bool value)
{
  enJuegoFinal = value;
  sensorenJuegoFinal.setState(enJuegoFinal);
}
// add for touchcounting and blinkenabled
void ChangeTouchCounting(bool value)
{
  touchCounting = value;
  sensorTouchCounting.setState(touchCounting);
}
void ChangeBlinkEnabled(bool value)
{
  blinkEnabled = value;
  sensorBlinkEnabled.setState(blinkEnabled);
}
void ChangeOnAnimation(bool value)
{
  onAnimation = value;
}
void ChangeShowingAnimation(bool value)
{
  showingAnimation = value;
  sensorShowingAnimation.setState(showingAnimation);
}

/// @brief Handle the API call
/// @param request full request
/// @param data JSON data
void postRule(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
{
  if (index == 0)
  {
    String *buffer = new String();
    if (total > 0)
      buffer->reserve(total);
    request->_tempObject = buffer;
  }

  String *buffer = reinterpret_cast<String *>(request->_tempObject);
  if (buffer == nullptr)
  {
    request->send(500, "application/json", "{\"status\":\"request buffer error\"}");
    return;
  }

  for (size_t i = 0; i < len; i++)
    *buffer += (char)data[i];

  if ((index + len) < total)
    return;

  auto releaseRequestBuffer = [&]() {
    if (request->_tempObject != nullptr)
    {
      delete reinterpret_cast<String *>(request->_tempObject);
      request->_tempObject = nullptr;
    }
  };

  String &receivedData = *buffer;

  Serial.println("\nPOST body length:");
  Serial.println(receivedData.length());

  String commandValue;
  bool hasCommand = extractJsonStringField(receivedData, "command", commandValue);

  if (receivedData.indexOf("start") != -1)
  {
    ChangegameStarted(true);
    ChangeOnAnimation(true);
    ChangeenJuegoFinal(false);
    oldPosition = -999;
    newPosition = -999;

    request->send(200, "application/json", "{\"status\":\"game started\"}");
    Serial.println("Command received: start");
  }
  else if (receivedData.indexOf("shutdown") != -1)
  {
    ChangegameStarted(false);
    ChangeenJuegoFinal(false);
    M5Dial.Lcd.clearDisplay(TFT_BLACK);
    request->send(200, "application/json", "{\"status\":\"game shutdown\"}");
    Serial.println("Command received: shutdown");
  }
  else if (receivedData.indexOf("status") != -1)
  {
    if (gameStarted)
    {
      request->send(200, "application/json", "{\"status\":\"game started\"}");
    }
    else
    {
      request->send(200, "application/json", "{\"status\":\"game shutdown\"}");
    }
    Serial.println("Command received: status");
  }
  else if (receivedData.indexOf("reboot") != -1) {
    request->send(200, "application/json", "{\"status\":\"restarting...\"}");
    Serial.println("Command received: reboot");
    delay(100); // allow time for response to be sent
    releaseRequestBuffer();
    esp_restart();
    return; // technically not needed, but explicit
  }
  else if (receivedData.indexOf("vassagomodesd") != -1)
  {
    enVassagoMode = 2; // modo de apagado
    LoadSpriteKarma(deviceId,9); // show the seal in vassago mode
    request->send(200, "application/json", "{\"status\":\"vassago mode shutdown enabled\"}");
    Serial.println("Command received: vassagomodeshutdown");
  }
  else if (receivedData.indexOf("vassagomodeon") != -1)
  {
    enVassagoMode = 1;
    LoadSpriteKarma(deviceId,9); // show the seal in vassago mode
    request->send(200, "application/json", "{\"status\":\"vassago mode enabled\"}");
    Serial.println("Command received: vassagomodeon");
  }
  else if (receivedData.indexOf("vassagomodeoff") != -1)
  {
    enVassagoMode = 0;
    LoadSpriteKarma(deviceId,9); // show the default sprite
    request->send(200, "application/json", "{\"status\":\"vassago mode disabled\"}");
    Serial.println("Command received: vassagomodeoff");
  }
  else if (receivedData.indexOf("hintliquidpuzle_a") != -1)
  {
    enVassagoMode = 1;
    LoadSpriteKarma(deviceId+15,9); // show the hint sprite for the liquid puzzle in vassago mode
    request->send(200, "application/json", "{\"status\":\"hint mode enabled\"}");
    Serial.println("Command received: hintliquidpuzle_a");
  }
  else if (receivedData.indexOf("hintliquidpuzle_b") != -1)
  {
    enVassagoMode = 1;
    LoadSpriteKarma(deviceId+20,9); // show the hint sprite for the liquid puzzle in vassago mode
    request->send(200, "application/json", "{\"status\":\"hint mode enabled\"}");
    Serial.println("Command received: hintliquidpuzle_b");
  }
  else if ((receivedData.indexOf("hintliquidpuzle_manual") != -1) || (hasCommand && commandValue == "hintliquidpuzle_manual"))
  {
    int imageKeyPos = receivedData.indexOf("\"image\"");
    if (imageKeyPos < 0)
    {
      request->send(400, "application/json", "{\"status\":\"missing image field\"}");
      Serial.println("Command received: hintliquidpuzle_manual (missing image)");
      releaseRequestBuffer();
      return;
    }

    int imageColonPos = receivedData.indexOf(':', imageKeyPos);
    if (imageColonPos < 0)
    {
      request->send(400, "application/json", "{\"status\":\"invalid image field\"}");
      Serial.println("Command received: hintliquidpuzle_manual (invalid image field)");
      releaseRequestBuffer();
      return;
    }

    size_t parsedBytes = parseHexBitmapBytesFromIndex(receivedData, imageColonPos + 1, manualBitmapBuffer, MANUAL_BITMAP_BYTES);
    if (parsedBytes != MANUAL_BITMAP_BYTES)
    {
      String errorJson = "{\"status\":\"invalid image size\",\"expected_bytes\":" + String(MANUAL_BITMAP_BYTES) + ",\"parsed_bytes\":" + String(parsedBytes) + "}";
      request->send(400, "application/json", errorJson);
      Serial.println("Command received: hintliquidpuzle_manual (invalid image size)");
      releaseRequestBuffer();
      return;
    }

    enVassagoMode = 1;
    LoadSpriteKarma(deviceId, 9, manualBitmapBuffer);

    String okJson = "{\"status\":\"hint manual loaded\",\"bytes\":" + String(parsedBytes) + "}";
    request->send(200, "application/json", okJson);
    Serial.println("Command received: hintliquidpuzle_manual");
  }
  else if (receivedData.indexOf("finalgame") != -1)
  {
    ChangegameStarted(true);
    startPositionEndGame = oldPosition; // Guardar la posición de inicio del juego final
    ChangeenJuegoFinal(true);
    request->send(200, "application/json", "{\"status\":\"finalgame started\"}");
    Serial.println("Command received: finalgame");
    spriteFinalenUso = defaultSpriteFinalEnUso; // Al iniciar el juego final, mostrar el sprite por defecto
    LoadSpriteKarma(deviceId+(spriteFinalenUso*5),spriteFinalenUso);
  
  }
  else
  {
    request->send(400, "application/json", "{\"status\":\"invalid command\"}");
    Serial.println("Invalid command");
  }

  releaseRequestBuffer();
}

void playMatrixRain() {

  M5Dial.Lcd.clearDisplay(TFT_BLACK);
  matrixSprite.setPsram(true);
  matrixSprite.createSprite(matrixWidth, matrixHeight);
  matrixSprite.fillScreen(TFT_BLACK);
  // matrixSprite.setFreeFont(&NotoSansDevanagari_Regular5pt7b); // Not using this one because converters did not convert all the characters
  matrixSprite.setTextSize(2);
  matrixSprite.setTextFont(1);  // basic built-in font
  matrixSprite.setTextColor(GetColorSello(deviceId), TFT_BLACK);

  
  // matrixSprite.drawString("नमस्ते", 10, 10);

  // Initialize drop positions randomly
  for (int i = 0; i < cols; i++) {
    dropY[i] = random(rows);
  }

  long initialDial = M5Dial.Encoder.read();

    
//   for (int frame = 0; frame < frames; frame++) {
    while (true) {
      M5Dial.update();  // Read hardware
      ArduinoOTA.handle(); // In case OTA comes and we're showing the animation
      mqtt.loop();       // In case MQTT messages come and we're showing the animation
      // Check dial movement
      long currentDial = M5Dial.Encoder.read();
      if (currentDial != initialDial) {
        break;
      }
    // Fade entire screen slightly (tail effect)

    for (int y = 0; y < matrixHeight; y++) {
      for (int x = 0; x < matrixWidth; x++) {
        uint16_t color = matrixSprite.readPixel(x, y);

        // Fade pixel to black
        uint8_t r = ((color >> 11) & 0x1F) << 3;
        uint8_t g = ((color >> 5) & 0x3F) << 2;
        uint8_t b = (color & 0x1F) << 3;

        r = r > 5 ? r - 5 : 0;
        g = g > 15 ? g - 15 : 0;
        b = b > 5 ? b - 5 : 0;

        uint16_t faded = matrixSprite.color565(r, g, b);
        matrixSprite.drawPixel(x, y, faded);
      }
    }

    // Draw new characters
    for (int i = 0; i < cols; i++) {
      int x = i * charSize;
      int y = dropY[i] * charSize;

      char c = 33 + random(30);  // Random printable ASCII
      matrixSprite.setCursor(x, y);
      matrixSprite.print(c);

      // Move drop down
      dropY[i]++;
      if (dropY[i] * charSize > matrixHeight || random(100) > 98) {
        dropY[i] = 0;
      }
    }

    // Show frame
    matrixSprite.pushSprite(0, 0);
    delay(50);
  }

  matrixSprite.deleteSprite();
}


/*
void playStartupRainAnimation(int frames, lgfx::LGFX_Sprite animSprite, int sppX, int sppY) {
  const int WIDTH = 200;
  const int HEIGHT = 200;

  for (int frame = 0; frame < frames; frame++) {
    // 1. Save bottom row
    uint16_t bottomRow[WIDTH];
    for (int x = 0; x < WIDTH; x++) {
      bottomRow[x] = animSprite.readPixel(x, HEIGHT - 1);
    }

    // 2. Shift all pixels down by 1
    for (int y = HEIGHT - 1; y > 0; y--) {
      for (int x = 0; x < WIDTH; x++) {
        uint16_t color = animSprite.readPixel(x, y - 1);
        animSprite.drawPixel(x, y, color);
      }
    }

    // 3. Move bottom row to top
    for (int x = 0; x < WIDTH; x++) {
      animSprite.drawPixel(x, 0, bottomRow[x]);
    }

    // 4. Show it
    animSprite.pushSprite(sppX, sppY);
    // delay(20);  // Adjust speed
  }

  animSprite.deleteSprite();
}
*/

void AnimateAndWaitForStart()
{
    Serial.println("Starting animation...");
    sprite.deleteSprite();        // seal sprite (if any)
    matrixSprite.deleteSprite();  // in case a previous run left it allocated
    ChangeShowingAnimation(true);
    playMatrixRain();
    ChangeShowingAnimation(false);

    Serial.println("End animation...");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("MAC Adress:");
    Serial.println(WiFi.macAddress());
    Serial.print("Device Id:");
    Serial.println(deviceId);

    LoadSpriteKarma(deviceId,9);
}

void EnsureWiFiConnected()
{
    if (WiFi.status() == WL_CONNECTED)
    {
      wifiReconnectAttempts = 0;
      return;
    }

    uint32_t now = millis();
    if (now - lastWifiReconnectAttemptMs < WIFI_RECONNECT_INTERVAL_MS)
      return;

    lastWifiReconnectAttemptMs = now;
    wifiReconnectAttempts++;

    if (wifiReconnectAttempts % 6 == 0)
      activeWifiCredential = (activeWifiCredential == 1) ? 2 : 1;

    const char *targetSsid = (activeWifiCredential == 1) ? ssid : ssid2;
    const char *targetPassword = (activeWifiCredential == 1) ? password : password2;

    Serial.printf("WiFi disconnected. Reconnect attempt %d to SSID: %s\n", wifiReconnectAttempts, targetSsid);
    WiFi.disconnect();
    WiFi.begin(targetSsid, targetPassword);
}

void HandleEmergencyRestartByTap()
{
  auto touch = M5Dial.Touch.getDetail();
  bool isPressed = touch.isPressed();
  uint32_t now = millis();

  if (isPressed && !lastTouchPressed)
  {
    if (now - lastEmergencyTapMs < TAP_DEBOUNCE_MS)
    {
      lastTouchPressed = isPressed;
      return;
    }

    if ((lastEmergencyTapMs == 0) || (now - lastEmergencyTapMs > TAP_WINDOW_MS))
      emergencyTapCount = 0;

    emergencyTapCount++;
    lastEmergencyTapMs = now;
    Serial.printf("Emergency tap count: %d/10\n", emergencyTapCount);

    if (emergencyTapCount >= 10)
    {
      Serial.println("Emergency restart requested by 10 taps.");
      delay(100);
      esp_restart();
      return;
    }
  }

  lastTouchPressed = isPressed;
}

void setup() {
    Serial.begin(115200);

    // Initialize Wi-Fi in station mode but don't connect yet
    WiFi.mode(WIFI_STA);

    // Get MAC address before connecting
    String macAddress = WiFi.macAddress();
    Serial.print("MAC Address: ");
    Serial.println(macAddress);

    defaultSpriteFinalEnUso = 1; // Indica el sprite final por defecto que se muestra al iniciar el juego (1=proceder, 2=cancelar)
    String toDisplay = " ";
    int deviceIPAddress = 300; // invalid IP address to detect errors

    // 
    // Setup the deviceId
    // and deviceName
    // 
    // 34:B7:DA:54:DE:50

    if (macAddress == "34:B7:DA:54:DE:50") {
        deviceId = 1; // old 0
        toDisplay = "起";
        deviceName = "M5Dial-Bael";
        deviceIPAddress = 51;
        // Origin
        // "起源"; // Japanese
        //  "世";
    } else if (macAddress == "34:B7:DA:56:17:90") {
        deviceId = 2; // old 1
        toDisplay = "幻"; // Hallucination
        deviceName = "M5Dial-Paimon";
        deviceIPAddress = 52;
        // "幻覚"; // Hallucination
        // "こ"
        defaultSpriteFinalEnUso = 2; // Indica el sprite final por defecto que se muestra al iniciar el juego (1=proceder, 2=cancelar)
    } else if (macAddress == "34:B7:DA:56:17:54") {
        deviceId = 0; // old 2
        positionAdjustment = 0;
        deviceName = "M5Dial-Asmoday";
        toDisplay = "鏡"; // Kagami
        deviceIPAddress = 53;
    } else if (macAddress == "34:B7:DA:56:12:F8") {
        deviceId = 4; // old 3
        toDisplay = "奇"; // Miracle
        deviceName = "M5Dial-Belial";
        deviceIPAddress = 54;
        //  "奇跡"; // Miracle
        // "ち";
        defaultSpriteFinalEnUso = 2; // Indica el sprite final por defecto que se muestra al iniciar el juego (1=proceder, 2=cancelar)

    } else if (macAddress == "34:B7:DA:56:15:EC") {
        deviceId = 3; // old 4
        toDisplay ="反"; // Hansha - Reflection
        deviceName = "M5Dial-Valac";
        deviceIPAddress = 55;
        // "反射"; // Hansha - Reflection
        // toDisplay = "は";
    } else if (macAddress == "B0:81:84:97:1B:C4") {
        deviceId = 1;
        defaultSpriteFinalEnUso = 2; // Indica el sprite final por defecto que se muestra al iniciar el juego (1=proceder, 2=cancelar)
        positionAdjustment =0;
        toDisplay = "世"; // Otra cosa
        deviceName = "M5Dial-Testing";
        deviceIPAddress = 50;
        // "こ"
    } else {
        // Default case if no match
        deviceId = -1; // Or any other default value
    }

    spriteFinalenUso = defaultSpriteFinalEnUso;


    // delay(50);
    // Fixed IP configuration
    if (macAddress == "B0:81:84:97:1B:C4")
    {
    }
    else
    {
      IPAddress local_IP(192, 168, 70, deviceIPAddress);
      IPAddress gateway(192, 168, 70, 1);
      IPAddress subnet(255, 255, 255, 0);    
      if (!WiFi.config(local_IP, gateway, subnet)) {
        Serial.println("Failed to configure Static IP");
      }    
    }    
    WiFi.begin(ssid, password);
    int retries = 0;
    activeWifiCredential = 1;
    while ((WiFi.status() != WL_CONNECTED) && (retries < 10))
    {
      delay(1000);
      if (retries == 5)
      {
        WiFi.begin(ssid2,password2);
        activeWifiCredential = 2;
      }
      Serial.print("Connecting to WiFi ");
      Serial.println(WiFi.SSID());
      retries++;
    }
    // Si no se conectó finalmente, reiniciar el ESP
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("Failed to connect to WiFi");
      delay(500);
      esp_restart();
    }
    Serial.println("Connected to WiFi");

    
    // OTA
    // Hostname (optional)
    String hostname = "M5Dial-" + String(deviceId);
    ArduinoOTA.setHostname(hostname.c_str());

    // Password protection (optional)
    ArduinoOTA.setPassword("myOTAPassword");

    // OTA event handlers (optional but useful for debug)
    ArduinoOTA.onStart([]() {
      Serial.println("OTA Update Start");
    });
    ArduinoOTA.onEnd([]() {
      Serial.println("\nOTA Update End");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("OTA Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      Serial.printf("OTA Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

    // Start OTA
    ArduinoOTA.begin();
    Serial.println("OTA Ready");    


    // Print the IP address
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("MAC Adress:");
    Serial.println(WiFi.macAddress());
    Serial.print("Device Id:");
    Serial.println(deviceId);

    auto cfg = M5.config();
    M5Dial.begin(cfg, true, false);
    // Audio / buzzer
    M5.Speaker.begin();
    M5.Speaker.setVolume(K_M5VOLUME);  // 0-255 (ajusta gusto; 96-160 suele ir bien)

    // setup MQTT
    // MQTT Discovery
    // optional device's details
    // uint8_t uniqueId[6];
    // WiFi.macAddress(uniqueId);
    // I want to set the deviceName as the uniquieId

    // change the deviceName to be a byte array
    uint8_t* deviceNameBytes;
    deviceNameBytes = new uint8_t[deviceName.length()];
    memcpy(deviceNameBytes, deviceName.c_str(), deviceName.length());
    device.setUniqueId(deviceNameBytes, deviceName.length() );

    // device.setUniqueId(uniqueId, sizeof(uniqueId));
    device.setName(deviceName.c_str());
    device.setSoftwareVersion("1.0.0");
    device.setManufacturer("BlackCrow");
    device.setModel("M5Dial");
    device.enableExtendedUniqueIds();

    // optional properties
    sensorShowingAnimation.setCurrentState(showingAnimation);
    sensorShowingAnimation.setName("00 Mostrando Animación");
  
    sensorgameStarted.setCurrentState(gameStarted);
    sensorgameStarted.setName("01 Juego Activo");

    sensorenJuegoFinal.setCurrentState(enJuegoFinal);
    sensorenJuegoFinal.setName("02 En Juego Final");

    // add for touchcounting and blinkenabled
    sensorTouchCounting.setCurrentState(touchCounting);
    sensorTouchCounting.setName("03 Touch detectado");

    sensorBlinkEnabled.setCurrentState(blinkEnabled);
    sensorBlinkEnabled.setName("04 Parpadeo Habilitado");

    // sensorIPAddress.setValue("WARNING"); IMPORTANTE: aunque lo inicialicemos aquí no se ve, hay que hacerlo después.

    sensorIPAddress.setName("05 IP Address");
    // WiFi.localIP().toString().c_str()
    // sensor.setIcon("mdi:fire");
    mqtt.begin(BROKER_ADDR, BROKER_USERNAME, BROKER_PASSWORD);

    // Start API server
    server.on("/api/command", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
          { postRule(request, data, len, index, total); });

    server.begin();


    xTaskCreatePinnedToCore(
          Task1code, /* Function to implement the task */
          "Task1", /* Name of the task */
          10000,  /* Stack size in words */
          NULL,  /* Task input parameter */
          0,  /* Priority of the task */
          &Task1,  /* Task handle. */
          0); /* Core where the task should run */    
}

float calculateDialAngle(int position) {
    // Calculate the angle for positive or negative positions in a 64-step dial.
    const int stepsPerRevolution = 64; // Steps for a full revolution (0 to 360 degrees)
    position = position % stepsPerRevolution;
    if (position < 0) {
        // Handle negative positions: Map them to 360 to 0 degrees (reverse direction)
        return 360 + (position * (360.0f / stepsPerRevolution));
    } else {
        // Handle positive positions: Map them to 0 to 360 degrees
        return position * (360.0f / stepsPerRevolution);
    }
}

// Constantes (ajusta HYST si quieres más/menos histéresis)
const int ROT_STEPS = 64;
const int HALF = ROT_STEPS / 2;   // 32
const int SHIFT = 16;             // desplazamiento pedido
const int HYST = 0;               // 1 paso de histéresis en el borde

// Normaliza a 0..63
inline int mod64(int x) { 
  int r = x % ROT_STEPS; 
  return (r < 0) ? r + ROT_STEPS : r; 
}
void updateSprite(int newValor)
{
  // Si estoy en el juego final, el valor + 90 y el valor -90 se mantiene el sprite que había, si paso de ahí, cambio el sprite y
  if (enJuegoFinal) 
  {
  
    // Origen corrido (start - 16)
    int origin = startPositionEndGame - SHIFT;

    // Posición dentro del ciclo 0..63 tomando el origen corrido
    int pos = mod64(newValor - origin);

    // Opción: histéresis en el borde 31/32 para evitar “chatter”
    // Si estás justo en el borde, conserva el sprite actual.
    bool borde = (pos >= (HALF - HYST) && pos <= (HALF - 1 + HYST));

    if (!borde) {
      // 0..31 => proceder ; 32..63 => cancelar
      if (pos < HALF) {
        if (spriteFinalenUso != defaultSpriteFinalEnUso) {
          Serial.println("Cambio sprite");
          spriteFinalenUso = defaultSpriteFinalEnUso; // sprite de proceder
          LoadSpriteKarma(deviceId + (spriteFinalenUso * 5), spriteFinalenUso);
        }
      } else {
        if (spriteFinalenUso == defaultSpriteFinalEnUso) {
          Serial.println("Cambio sprite");
          if (defaultSpriteFinalEnUso == 1) // si el sprite por defecto es el de 1 
            spriteFinalenUso = 2; // sprite de cancelar
          else
            spriteFinalenUso = 1; // sprite de proceder
          LoadSpriteKarma(deviceId + (spriteFinalenUso * 5), spriteFinalenUso);
        }
      }
    }
  }
  int angle = calculateDialAngle(newValor);
  sprite.pushRotated(angle);
}  

/*
void sendEncoderToApiHttp(int device, int step_mode, int step_size, int transition_time)
{
   // Serial.println("sendEncoderToApi...");

  if (WiFi.status() == WL_CONNECTED)
  {
    HTTPClient http;
    http.begin(url2SendResult); // Replace with your API endpoint
    http.addHeader("Content-Type", "application/json");

    String payload = "{\"device\": \""+String(device)+"\", \"step_mode\": \""+String(step_mode)+"\", \"step_size\": \""+String(step_size)+"\",\"transition_time\": \""+String(transition_time)+"\"}";
    // Serial.println(payload);
    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0)
    {
      String response = http.getString();
      // Serial.println("HTTP Response code: " + String(httpResponseCode));
      // Serial.println("Response: " + response);
    }
    else
    {
      // Serial.println("Error on sending POST: " + String(httpResponseCode));
    }
    http.end();
  }
  else
  {
    Serial.println("WiFi not connected");
  }
}
*/

// Dibuja el mismo número en 6 posiciones tipo reloj: 11,1,3,5,7,9
void drawRadialCopies(const String& s, uint16_t color) {
  const int cx = 120;              // centro pantalla 240x240
  const int cy = 120;
  const int r  = 88;               // radio (ajusta si lo quieres más adentro/afuera)

  // Usamos la misma fuente/tamaño que el número central, pero más pequeño para no tapar
  M5Dial.Lcd.setTextFont(1);       
  M5Dial.Lcd.setTextSize(3);       // tamaño radial (ajusta si quieres)
  M5Dial.Lcd.setTextColor(color, TFT_BLACK);

  int16_t tw = M5Dial.Lcd.textWidth(s);
  int16_t th = M5Dial.Lcd.fontHeight();

  auto putAt = [&](float deg){
    float rad = deg * 3.1415926535f / 180.0f;
    int x = (int)roundf(cx + r * cosf(rad));
    int y = (int)roundf(cy - r * sinf(rad));   // y invertida (arriba = menor y)
    // centrar el texto en (x,y)
    M5Dial.Lcd.setCursor(x - tw/2, y - th/2);
    M5Dial.Lcd.print(s);
  };

  // Posiciones: 11, 1, 3, 5, 7, 9 → 330°, 30°, 90°, 150°, 210°, 270°
  putAt(330.0f);
  putAt(30.0f);
  putAt(90.0f);
  putAt(150.0f);
  putAt(210.0f);
  putAt(270.0f);
}

// Dibuja un número grande centrado (sobre fondo negro)
void drawCenterNumber(int n, int newPosition) {
  M5Dial.Lcd.fillScreen(TFT_BLACK);

  // tipografía básica, grande y blanca
  // si estoy en Cancelar uso el color COLOR_CANCELAR para la letra
  int colorNum;
  if (spriteFinalenUso == 1) {
    colorNum = COLOR_PROCEDER;
  } else {
    colorNum = COLOR_CANCELAR;
  }
  M5Dial.Lcd.setTextColor(colorNum, TFT_BLACK);
  M5Dial.Lcd.setTextFont(1);      // fuente built-in
  M5Dial.Lcd.setTextSize(12);      // tamaño grande (ajusta si quieres más/menos)

  String s = String(n);
  int16_t tw = M5Dial.Lcd.textWidth(s);
  int16_t th = M5Dial.Lcd.fontHeight();

  int x = (240 - tw) / 2;
  int y = (240 - th) / 2;
  M5Dial.Lcd.setCursor(x, y);

  // Que el numero este rotado igual que el sprite
  /* Desestimado - esto rota todo y destruye el resto del movimiento
  int angle = calculateDialAngle(newPosition);

  M5Dial.Lcd.setRotation(angle);
  */
  M5Dial.Lcd.print(s);
  // Copias radiales (más pequeñas)
  drawRadialCopies(s, colorNum);
}

void showFrozenSprite() {
  updateSprite(freezePositionAtSelect);
  // tono grave ON (duración 0 = sostenido hasta stop)
  M5.Speaker.tone(110 /*Hz*/, 0 /*ms*/);  // prueba 90, 100, 110, 120 Hz
}

void hideSprite() {
  M5Dial.Lcd.fillScreen(TFT_BLACK);
  // tono OFF
  M5.Speaker.stop();
}


void restoreRotatedSprite() {
   // Redibuja el sprite rotado con el ángulo/posición que ya tengas
    updateSprite(oldPosition); 
  }

void Check4TouchToPrint(int newPosition)
{
  if (!enJuegoFinal) return;

  auto t = M5Dial.Touch.getDetail();

  if (t.isPressed()) {
    // Si recién empieza el toque, inicializa el conteo
    if (!touchCounting) {
      ChangeTouchCounting(true);
      touchStartMs = millis();
      lastShownCount = 0;
      // Si veníamos de un blink anterior (por seguridad), lo apagamos
      ChangeTouchCounting(true);
    }

    // Si ya estamos en blink, solo gestionar el parpadeo y salir
    if (blinkEnabled) {
      uint32_t now = millis();
      if (now - lastBlinkToggleMs >= BLINK_INTERVAL_MS) {
        lastBlinkToggleMs = now;
        blinkState = !blinkState;
        if (blinkState) showFrozenSprite(); else hideSprite();
      }
      return;  // no seguimos con números ni rotación
    }

    // Aún no parpadeamos: calcular segundos transcurridos
    uint32_t elapsed = millis() - touchStartMs;
    int secs = (int)(elapsed / 1000) + 1;   // 0–999ms => 1, 1000–1999 => 2, ...
    if (secs > secondsToSelect) secs = secondsToSelect;

    // Mostrar el número si cambió (1..6)
    if (secs != lastShownCount) {
      drawCenterNumber(secs, newPosition);
      lastShownCount = secs;
    }

    // ¿Llegamos al umbral? activamos blink y congelamos la posición actual
    if (secs >= secondsToSelect && !blinkEnabled) {
      ChangeBlinkEnabled(true);
      freezePositionAtSelect = oldPosition;   // congela la rotación en el ángulo actual
      blinkState = true;                      // empieza mostrándose
      lastBlinkToggleMs = millis();
      showFrozenSprite();                     // mostrar el sprite (estado ON)
    }

  } else {
    // Soltó el toque: limpiar estados y restaurar vista
    if (touchCounting) {
      ChangeTouchCounting(false);
      ChangeBlinkEnabled(false);
      // Dejar de mostrar número o pantalla negra y volver a la vista normal
      M5.Speaker.stop();  // tono OFF
      restoreRotatedSprite();
    }
  }
}

int mostreCore = 0;
void loop() {
    M5Dial.update();
  EnsureWiFiConnected();
  HandleEmergencyRestartByTap();
    mqtt.loop();
    ArduinoOTA.handle();
    if (mostreCore == 0)
    {
      Serial.print("loop() running on core ");
      Serial.println(xPortGetCoreID());
      mostreCore = 1;
      // Actualizar el sensor de IP Address
      // Si no lo hacemos en el loop, no se actualiza en el setup no sirve
      sensorIPAddress.setValue(WiFi.localIP().toString().c_str());

    }

    if (onAnimation)
    {
      ChangeOnAnimation(false);
      sprite.deleteSprite();
      ChangegameStarted(false);
      AnimateAndWaitForStart();
      ChangegameStarted(true);
    }
    newPosition = M5Dial.Encoder.read();
  
    newPosition = newPosition + positionAdjustment;

    if (newPosition != oldPosition) {
        if (gameStarted && !blinkEnabled && !touchCounting) // hacer que si está en blink o contando toque, no actualice el sprite
        {
          updateSprite(newPosition);
        }
        if (newPosition >= oldPosition)
        {
          changeDirection = 1; // clockwise
        }
        else
        {
          changeDirection = 2; // counterclockwise
        }
        oldPosition = newPosition;
        hasChanged = 1;
        
        value2Send = newPosition;
    }
    Check4TouchToPrint(newPosition);

}