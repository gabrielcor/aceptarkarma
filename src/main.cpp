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

// Converted with https://rop.nl/truetype2gfx/
// #include "include/NotoSansDevanagari_Regular20pt7b.h"
// #include "include/NotoSansDevanagari_Regular5pt7b.h"

// Handle local control API
AsyncWebServer server(80);         // to handle the published API
bool gameStarted = false; // estado del "juego" o del control. 
bool onAnimation=true; // Indica si tengo que poner la animación
bool enJuegoFinal = false; // Indica si estoy en el juego final
int spriteFinalenUso = 1; // Indica si el sprite final que se está mostrando es el de proceder(1) o cancelar (2)
long oldPosition = -999;
long newPosition = -999;

static constexpr int TFT_MYORANGE      = 0xFA00; 

const int COLOR_PROCEDER = TFT_MYORANGE;
const int COLOR_CANCELAR = TFT_SKYBLUE;
const int COLOR_SELLO = TFT_PURPLE;
const int K_M5VOLUME = 255; // volumen del buzzer 0-255

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
const int epd_bitmap_allArray_LEN = 15;

const unsigned char* epd_bitmap_allArray[15] = {
	epd_bitmap_250px_32_Asmoday_seal, epd_bitmap_01_Bael_seal, epd_bitmap_250px_09_Paimon_seal01, epd_bitmap_62_Valac_seal, epd_bitmap_250px_68_Belial_seal,
  epd_bitmap_250px_32_Asmoday_seal_Accept, epd_bitmap_01_Bael_seal_Accept, epd_bitmap_250px_09_Paimon_seal01_Accept, epd_bitmap_62_Valac_seal_Accept, epd_bitmap_250px_68_Belial_seal_Accept,
  epd_bitmap_250px_32_Asmoday_seal_Cancel, epd_bitmap_01_Bael_seal_Cancel, epd_bitmap_250px_09_Paimon_seal01_Cancel, epd_bitmap_62_Valac_seal_Cancel, epd_bitmap_250px_68_Belial_seal_Cancel      
};


const char *ssid ="blackcrow_prod01";
const char *password = "e2aVwqCtfc5EsgGE852E";
const char *ssid2 = "blackcrow_01";
const char *password2 = "8001017170";
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
long prevValue = -999;
int changeDirection = -1; // 0-clockwise, 1-counterclockwise

lgfx::LGFX_Sprite sprite(&M5Dial.Lcd);
 
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


void LoadSpriteKarma(int whichDevice, int whichColor)
{

  // Sprite def
  // screen size
  int scrS = 240; // M5Dial
  // bitmap width/height
  int bmpW = 200;
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

  int tftColor = whichColor;
  // Sprite

  const unsigned char *bitmapToShow;
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


/// @brief Handle the API call
/// @param request full request
/// @param data JSON data
void postRule(AsyncWebServerRequest *request, uint8_t *data)
{
  size_t len = request->contentLength();
  Serial.println("Data: ");
  Serial.write(data, len); // Correctly print the received data
  Serial.println("\nLength: ");
  Serial.println(len);

  // Construct the received data string with the specified length
  String receivedData = "";
  for (size_t i = 0; i < len; i++)
  {
    receivedData += (char)data[i];
  }

  Serial.println("Received Data String: " + receivedData);

  if (receivedData.indexOf("start") != -1)
  {
    gameStarted = true;
    onAnimation = true;
    oldPosition = -999;
    newPosition = -999;

    request->send(200, "application/json", "{\"status\":\"game started\"}");
    Serial.println("Command received: start");
  }
  else if (receivedData.indexOf("shutdown") != -1)
  {
    gameStarted = false;
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
    esp_restart();
    return; // technically not needed, but explicit
  }
  else if (receivedData.indexOf("finalgame") != -1)
  {
    gameStarted = true;
    startPositionEndGame = oldPosition; // Guardar la posición de inicio del juego final
    enJuegoFinal = true;
    request->send(200, "application/json", "{\"status\":\"finalgame started\"}");
    Serial.println("Command received: finalgame");
    // TODO: poner en spriteFinalEnUso el que corresponda a cada sello
    LoadSpriteKarma(deviceId+(spriteFinalenUso*5),COLOR_PROCEDER);
   
  }
  else
  {
    request->send(400, "application/json", "{\"status\":\"invalid command\"}");
    Serial.println("Invalid command");
  }
}

void playMatrixRain() {

  M5Dial.Lcd.clearDisplay(TFT_BLACK);
  matrixSprite.setPsram(true);
  matrixSprite.createSprite(matrixWidth, matrixHeight);
  matrixSprite.fillScreen(TFT_BLACK);
  // matrixSprite.setFreeFont(&NotoSansDevanagari_Regular5pt7b); // Not using this one because converters did not convert all the characters
  matrixSprite.setTextSize(2);
  matrixSprite.setTextFont(1);  // basic built-in font
  matrixSprite.setTextColor(COLOR_SELLO, TFT_BLACK);

  
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

    playMatrixRain();
    Serial.println("End animation...");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    Serial.print("MAC Adress:");
    Serial.println(WiFi.macAddress());
    Serial.print("Device Id:");
    Serial.println(deviceId);

    LoadSpriteKarma(deviceId,COLOR_SELLO);
}

void setup() {
    Serial.begin(115200);
    // delay(50);
    WiFi.begin(ssid, password);
    int retries = 0;
    int wifiSsid = 1;
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(1000);
      if (retries == 5)
      {
        WiFi.begin(ssid2,password2);
        wifiSsid = 2;
      }
      Serial.print("Connecting to WiFi ");
      Serial.println(WiFi.SSID());
      retries++;
    }
    Serial.println("Connected to WiFi");
    String macAddress = WiFi.macAddress();
    String toDisplay = " ";

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
    // 
    // Setup the deviceId
    // 
    // 34:B7:DA:54:DE:50
    if (macAddress == "34:B7:DA:54:DE:50") {
        deviceId = 0;
        toDisplay = "起";
        // Origin
        // "起源"; // Japanese
        //  "世";
    } else if (macAddress == "34:B7:DA:56:17:90") {
        deviceId = 1;
        toDisplay = "幻"; // Hallucination
        // "幻覚"; // Hallucination
        // "こ"
    } else if (macAddress == "34:B7:DA:56:17:54") {
        deviceId = 2;
        positionAdjustment = 79;
        toDisplay = "鏡"; // Kagami
    } else if (macAddress == "34:B7:DA:56:12:F8") {
        deviceId = 3;
        toDisplay = "奇"; // Miracle
        //  "奇跡"; // Miracle
        // "ち";
    } else if (macAddress == "34:B7:DA:56:15:EC") {
        deviceId = 4;
        toDisplay ="反"; // Hansha - Reflection
        // "反射"; // Hansha - Reflection
        // toDisplay = "は";
    } else if (macAddress == "B0:81:84:97:1B:C4") {
        deviceId = 4;
        positionAdjustment =79;
        toDisplay = "世"; // Otra cosa
        // "こ"
    } else {
        // Default case if no match
        deviceId = -1; // Or any other default value
    }
   

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



    // Start API server
    server.on("/api/command", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              { postRule(request, data); });

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
        if (spriteFinalenUso != 1) {
          Serial.println("Cambio sprite a proceder");
          spriteFinalenUso = 1; // sprite de proceder
          LoadSpriteKarma(deviceId + (spriteFinalenUso * 5), COLOR_PROCEDER);
        }
      } else {
        if (spriteFinalenUso != 2) {
          Serial.println("Cambio sprite a cancelar");
          spriteFinalenUso = 2; // sprite de cancelar
          LoadSpriteKarma(deviceId + (spriteFinalenUso * 5), COLOR_CANCELAR);
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
  if (spriteFinalenUso == 2) {
    colorNum = COLOR_CANCELAR;
  } else {
    colorNum = COLOR_PROCEDER;
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
      touchCounting = true;
      touchStartMs = millis();
      lastShownCount = 0;
      // Si veníamos de un blink anterior (por seguridad), lo apagamos
      blinkEnabled = false;
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
      blinkEnabled = true;
      freezePositionAtSelect = oldPosition;   // congela la rotación en el ángulo actual
      blinkState = true;                      // empieza mostrándose
      lastBlinkToggleMs = millis();
      showFrozenSprite();                     // mostrar el sprite (estado ON)
    }

  } else {
    // Soltó el toque: limpiar estados y restaurar vista
    if (touchCounting) {
      touchCounting = false;
      blinkEnabled = false;
      // Dejar de mostrar número o pantalla negra y volver a la vista normal
      M5.Speaker.stop();  // tono OFF
      restoreRotatedSprite();
    }
  }
}

int mostreCore = 0;
void loop() {
    M5Dial.update();
    ArduinoOTA.handle();
    if (mostreCore == 0)
    {
      Serial.print("loop() running on core ");
      Serial.println(xPortGetCoreID());
      mostreCore = 1;
    }

    if (onAnimation)
    {
      onAnimation=false;
      sprite.deleteSprite();
      gameStarted = false;
      AnimateAndWaitForStart();
      gameStarted = true; // Si hizo la animación y entró en lo normal, hay que mandar los mensajes
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