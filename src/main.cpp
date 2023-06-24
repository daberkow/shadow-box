#include <Arduino.h>

boolean debug = false;
void easy_debug(String val) {
  if (debug) {
    Serial.print(val);
  }
}

void easy_debug_line(String val) {
  if (debug) {
    Serial.println(val);
  }
}

// SD Card Libraries
#include "FS.h"
#include "SD.h"
#include "SPI.h"

#define MODE_BUTTON 4

/*********************
* Audio libraries and settings
**********************/
// Include I2S driver
#include <driver/i2s.h>

// Connections to INMP441 I2S microphone
#define I2S_WS 15
#define I2S_SD 32
#define I2S_SCK 14

// Use I2S Processor 0
#define I2S_PORT I2S_NUM_0

// Define input buffer length
#define bufferLen 64
int16_t sBuffer[bufferLen];
void i2s_install() {
  // Set up I2S Processor configuration
  const i2s_config_t i2s_config = {
    .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 44100,
    .bits_per_sample = i2s_bits_per_sample_t(16),
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = 0,
    .dma_buf_count = 8,
    .dma_buf_len = bufferLen,
    .use_apll = false
  };

  i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
}

void i2s_setpin() {
  // Set I2S pin configuration
  const i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK,
    .ws_io_num = I2S_WS,
    .data_out_num = -1,
    .data_in_num = I2S_SD
  };

  i2s_set_pin(I2S_PORT, &pin_config);
}


/**********************
* LEDs
**********************/
// The WS2812 doesnt support this esp32 hardware spi
#define FASTLED_FORCE_SOFTWARE_SPI
#include <FastLED.h>
#define RGB_PIN 26 // LED DATA PIN
#define RGB_LED_NUM 256 // Number of LEDs
#define LEDS_PER_ROW 16 // brightness range [0..255]
#define BRIGHTNESS 100 // brightness range [0..255]
#define CHIP_SET WS2812B // types of RGB LEDs
#define COLOR_CODE GRB //sequence of colors in data stream
// Define the array of LEDs
CRGB LEDs[RGB_LED_NUM];
// define 3 byte for the random color
byte a, b, c;
#define UPDATES_PER_SECOND 100
char iByte = 0;

// Dans remapping
int mappedLeds[RGB_LED_NUM];
void map_leds() {
  for (int i = 0; i < RGB_LED_NUM; i++) {
    for (int i = 0; i < RGB_LED_NUM; i++) {
      if ((i / LEDS_PER_ROW) % 2 == 1) {
          // If the LED number (example 22) divided by the number of LEDS is odd then we are in one of these reverse LED rows
          int maxId = (LEDS_PER_ROW * ((i / LEDS_PER_ROW) + 1));
          int newId = maxId - i;
          newId = (LEDS_PER_ROW * (i / LEDS_PER_ROW)) + newId;
          // Since this counts from 0, we need to remove 1
          mappedLeds[i] = newId - 1;
      } else {
          mappedLeds[i] = i;
      }
    }
  }
}

// Blue, Green , Red
void r_g_b() {
  for (int i = 0; i < RGB_LED_NUM; i++) {
    LEDs[mappedLeds[i]] = CRGB ( 0, 0, 255);
    FastLED.show();
    delay(50);
  }
  for (int i = RGB_LED_NUM; i >= 0; i--) {
    LEDs[mappedLeds[i]] = CRGB ( 0, 255, 0);
    FastLED.show();
    delay(50);
  }
  for (int i = 0; i < RGB_LED_NUM; i++) {
    LEDs[mappedLeds[i]] = CRGB ( 255, 0, 0);
    FastLED.show();
    delay(50);
  }
  for (int i = RGB_LED_NUM; i >= 0; i--) {
    LEDs[mappedLeds[i]] = CRGB ( 0, 0, 0);
    FastLED.show();
    delay(50);
  }
}

void light_up_white() {
  Serial.println("Lighting White");
  for (int i = 0; i < RGB_LED_NUM; i++) {
    LEDs[i] = CRGB( BRIGHTNESS, BRIGHTNESS, BRIGHTNESS);
  }
  FastLED.show();
}

void disable_all_led() {
  Serial.println("Lighting White");
  for (int i = 0; i < RGB_LED_NUM; i++) {
    LEDs[i] = CRGB( 0, 0, 0);
  }
  FastLED.show();
}

int lowest_audio_seen = 0;
unsigned long timeSinceLow = millis();
int highest_audio_seen = 0;
unsigned long timeSinceHigh = millis();
int timeBetweenLowering = 5000;
int num_per_row = 16;
int num_cols = 16;

void audio_to_leds() {
  // Get I2S data and place in data buffer
  size_t bytesIn = 0;
  esp_err_t result = i2s_read(I2S_PORT, &sBuffer, bufferLen, &bytesIn, portMAX_DELAY);

  if (result == ESP_OK) {
    // Read I2S data buffer
    int16_t samples_read = bytesIn / 8;
    // Serial.print("Data:");
    if (lowest_audio_seen < -200 && (millis() - timeSinceLow) > timeBetweenLowering) {
      timeSinceLow = millis();
      lowest_audio_seen += 100;
      Serial.print("Auto adjusting lowest_audio_seen to: ");
      Serial.println(lowest_audio_seen);
    }
    if (highest_audio_seen > 200 && (millis() - timeSinceHigh) > timeBetweenLowering) {
      timeSinceHigh = millis();
      highest_audio_seen -= 100;
      Serial.print("Auto adjusting highest_audio_seen to: ");
      Serial.println(highest_audio_seen);
    }
    if (samples_read > 0) {
      for (int16_t i = 0; i < samples_read; i++) {
        if (sBuffer[i] < lowest_audio_seen) {
          lowest_audio_seen = sBuffer[i];
          Serial.print("Adjusting lowest_audio_seen to: ");
          Serial.println(lowest_audio_seen);
        }
        if (sBuffer[i] > highest_audio_seen) {
          highest_audio_seen = sBuffer[i];
          Serial.print("Adjusting highest_audio_seen to: ");
          Serial.println(highest_audio_seen);
        }
        // Serial.print(sBuffer[i]);
        // Serial.print(",");
      }
      // Serial.println(",");
      int lastFullValue = 0;
      for (int k = 0; k < num_per_row; k++) {
        int range = (lowest_audio_seen * -1) + highest_audio_seen;
        // Serial.print("Range: ");
        // Serial.println(range);
        // Serial.print("lowest_audio_seen: ");
        // Serial.println(lowest_audio_seen);
        float audioValue = 0;
        if (k % (num_per_row / samples_read) == 0) {
          if (sBuffer[k / (num_per_row / samples_read)] < 0) {
            // Negative value
            // audioValue = ((sBuffer[k / (num_per_row / samples_read)] / (-1 * lowest_audio_seen)) * BRIGHTNESS);
            audioValue = ((sBuffer[k / (num_per_row / samples_read)] * BRIGHTNESS) / ((range / 2) * -1));
          } else {
            // audioValue = ((sBuffer[k / (num_per_row / samples_read)] / highest_audio_seen) * BRIGHTNESS);
            audioValue = ((sBuffer[k / (num_per_row / samples_read)] * BRIGHTNESS) / (range / 2));
          }
          lastFullValue = k;
        } else {
          float lowerValue = ((sBuffer[k / (num_per_row / samples_read)] * BRIGHTNESS) / (range / 2));
          if (sBuffer[k / (num_per_row / samples_read)] < 0) {
            // Negative value
            lowerValue *= -1;
          }

          float nextValue = ((sBuffer[(lastFullValue + (num_per_row / samples_read)) / (num_per_row / samples_read)] * BRIGHTNESS) / (range / 2));
          if (sBuffer[(lastFullValue + (num_per_row / samples_read)) / (num_per_row / samples_read)] < 0) {
            // Negative value
            nextValue *= -1;
          }
          // float lowerValue = ((sBuffer[lastFullValue / (num_per_row / samples_read)] / (-1 * lowest_audio_seen)) * BRIGHTNESS) / range;
          // float nextValue = ((sBuffer[(lastFullValue + (num_per_row / samples_read)) / (num_per_row / samples_read)] +  (-1 * lowest_audio_seen)) * BRIGHTNESS) / range;
          float averageValue = 0;
          float total_size = (num_per_row / samples_read) / 2;
          for (int z = 0; z < (num_per_row / samples_read); z++) {
            if (z < total_size) {
              averageValue += lowerValue;
            } else {
              averageValue += nextValue;
            }
          }
          averageValue /= (num_per_row / samples_read);
          audioValue = averageValue;
        }

        // Serial.print("Setting LED ");
        // Serial.print(k);
        // Serial.print(" to ");
        // Serial.println(audioValue);
        for (int l = 0; l < num_cols; l++) {
          int ledId = k + (l * num_per_row);
          if (l % 2 == 1) {
            // Odd rows are the reverse LED order
            ledId = (((l + 1) * num_per_row) - 1) - k;
          }
          if (audioValue < 10) {
            audioValue = 0;
          }
          LEDs[mappedLeds[ledId]] = CRGB(audioValue, audioValue, audioValue);
        }
      }

      FastLED.show();
    }
  }
}

/*********************
* File System
**********************/

// Initialize SPIFFS
#include "SPIFFS.h"

void initSPIFFS() {
  if (!SPIFFS.begin(true)) {
    Serial.println("An error has occurred while mounting SPIFFS");
  }
  Serial.println("SPIFFS mounted successfully");
}

// Read File from SPIFFS
String readFile(fs::FS &fs, const char * path){
  Serial.printf("Reading file: %s\r\n", path);

  File file = fs.open(path);
  if(!file || file.isDirectory()){
    Serial.println("- failed to open file for reading");
    return String();
  }

  String fileContent;
  while(file.available()){
    fileContent = file.readStringUntil('\n');
    break;
  }
  return fileContent;
}

// Write file to SPIFFS
void writeFile(fs::FS &fs, const char * path, const char * message){
  Serial.printf("Writing file: %s\r\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file){
    Serial.println("- failed to open file for writing");
    return;
  }
  if(file.print(message)){
    Serial.println("- file written");
  } else {
    Serial.println("- write failed");
  }
}

/*********************
* Wifi
**********************/
// WIFI
#include <WiFi.h>
// #include "AsyncTCP.h"
// #include "ESPAsyncWebServer.h"


// Create AsyncWebServer object on port 80
// AsyncWebServer server(80);

// Search for parameter in HTTP POST request
// const char* PARAM_INPUT_1 = "ssid";
// const char* PARAM_INPUT_2 = "pass";
// const char* PARAM_INPUT_3 = "ip";
// const char* PARAM_INPUT_4 = "gateway";


// //Variables to save values from HTML form
// String ssid;
// String pass;
// String ip;
// String gateway;

// // File paths to save input values permanently
// const char* ssidPath = "/ssid.txt";
// const char* passPath = "/pass.txt";
// const char* ipPath = "/ip.txt";
// const char* gatewayPath = "/gateway.txt";

// IPAddress localIP;
//IPAddress localIP(192, 168, 1, 200); // hardcoded

// Set your Gateway IP address
// IPAddress localGateway;
//IPAddress localGateway(192, 168, 1, 1); //hardcoded
// IPAddress subnet(255, 255, 0, 0);

// Timer variables
// unsigned long previousMillis = 0;
// const long interval = 10000;  // interval to wait for Wi-Fi connection (milliseconds)

// bool initWiFi() {
//   if(ssid=="" || ip==""){
//     Serial.println("Undefined SSID or IP address.");
//     return false;
//   }

//   WiFi.mode(WIFI_STA);
//   localIP.fromString(ip.c_str());
//   localGateway.fromString(gateway.c_str());


//   if (!WiFi.config(localIP, localGateway, subnet)){
//     Serial.println("STA Failed to configure");
//     return false;
//   }
//   WiFi.begin(ssid.c_str(), pass.c_str());
//   Serial.println("Connecting to WiFi...");

//   unsigned long currentMillis = millis();
//   previousMillis = currentMillis;

//   while(WiFi.status() != WL_CONNECTED) {
//     currentMillis = millis();
//     if (currentMillis - previousMillis >= interval) {
//       Serial.println("Failed to connect.");
//       return false;
//     }
//   }

//   Serial.println(WiFi.localIP());
//   return true;
// }

// // Replaces placeholder with LED state value
// String processor(const String& var) {
//   // if(var == "STATE") {
//   //   if(digitalRead(ledPin)) {
//   //     ledState = "ON";
//   //   }
//   //   else {
//   //     ledState = "OFF";
//   //   }
//   //   return ledState;
//   // }
//   return String();
// }

int mode = 1;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Init");
  // https://randomnerdtutorials.com/getting-started-freenove-esp32-wrover-cam/
  // https://github.com/Freenove/Freenove_ESP32_WROVER_Board/blob/main/Datasheet/ESP32-Pinout.pdf

  // Init SD card
  // https://randomnerdtutorials.com/esp32-microsd-card-arduino/
  if(!SD.begin(5)){
    Serial.println("Card Mount Failed");
  }
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE){
    Serial.println("No SD card attached");
  }

  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC){
    Serial.println("MMC");
  } else if(cardType == CARD_SD){
    Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);

  // listDir(SD, "/", 0);
  Serial.printf("Total space: %lluMB\n", SD.totalBytes() / (1024 * 1024));
  Serial.printf("Used space: %lluMB\n", SD.usedBytes() / (1024 * 1024));

  // Init Panel
  // https://www.makerguides.com/how-to-control-ws2812b-individually-addressable-leds-using-arduino/

  map_leds();
  Serial.println("WS2812B LEDs strip Initialize");
  Serial.println("Please enter the 1 to 6 value.....Otherwise no any effect show");
  FastLED.addLeds<CHIP_SET, RGB_PIN, COLOR_CODE>(LEDs, RGB_LED_NUM);
  randomSeed(2);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.setMaxPowerInVoltsAndMilliamps(5, 500);
  FastLED.clear();
  FastLED.show();
  // r_g_b();

  // Init Mic
  // https://dronebotworkshop.com/esp32-i2s/
  Serial.println("Init Mic");
  i2s_install();
  i2s_setpin();
  i2s_start(I2S_PORT);

  // Init Wifi
  // Load values saved in SPIFFS
  // ssid = readFile(SPIFFS, ssidPath);
  // pass = readFile(SPIFFS, passPath);
  // ip = readFile(SPIFFS, ipPath);
  // gateway = readFile (SPIFFS, gatewayPath);
  // Serial.println(ssid);
  // Serial.println(pass);
  // Serial.println(ip);
  // Serial.println(gateway);

  // if(initWiFi()) {
  //   // Route for root / web page
  //   server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
  //     request->send(SPIFFS, "/index.html", "text/html", false, processor);
  //   });
  //   server.serveStatic("/", SPIFFS, "/");

  //   // Route to set GPIO state to HIGH
  //   server.on("/on", HTTP_GET, [](AsyncWebServerRequest *request) {
  //     // digitalWrite(ledPin, HIGH);
  //     request->send(SPIFFS, "/index.html", "text/html", false, processor);
  //   });

  //   // Route to set GPIO state to LOW
  //   server.on("/off", HTTP_GET, [](AsyncWebServerRequest *request) {
  //     // digitalWrite(ledPin, LOW);
  //     request->send(SPIFFS, "/index.html", "text/html", false, processor);
  //   });
  //   server.begin();
  // } else {
  //   // Connect to Wi-Fi network with SSID and password
  //   Serial.println("Setting AP (Access Point)");
  //   // NULL sets an open Access Point
  //   WiFi.softAP("ESP-WIFI-MANAGER", NULL);

  //   IPAddress IP = WiFi.softAPIP();
  //   Serial.print("AP IP address: ");
  //   Serial.println(IP);

  //   // Web Server Root URL
  //   server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
  //     request->send(SPIFFS, "/wifimanager.html", "text/html");
  //   });

  //   server.serveStatic("/", SPIFFS, "/");

  //   server.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
  //     int params = request->params();
  //     for(int i=0;i<params;i++){
  //       AsyncWebParameter* p = request->getParam(i);
  //       if(p->isPost()){
  //         // HTTP POST ssid value
  //         if (p->name() == PARAM_INPUT_1) {
  //           ssid = p->value().c_str();
  //           Serial.print("SSID set to: ");
  //           Serial.println(ssid);
  //           // Write file to save value
  //           writeFile(SPIFFS, ssidPath, ssid.c_str());
  //         }
  //         // HTTP POST pass value
  //         if (p->name() == PARAM_INPUT_2) {
  //           pass = p->value().c_str();
  //           Serial.print("Password set to: ");
  //           Serial.println(pass);
  //           // Write file to save value
  //           writeFile(SPIFFS, passPath, pass.c_str());
  //         }
  //         // HTTP POST ip value
  //         if (p->name() == PARAM_INPUT_3) {
  //           ip = p->value().c_str();
  //           Serial.print("IP Address set to: ");
  //           Serial.println(ip);
  //           // Write file to save value
  //           writeFile(SPIFFS, ipPath, ip.c_str());
  //         }
  //         // HTTP POST gateway value
  //         if (p->name() == PARAM_INPUT_4) {
  //           gateway = p->value().c_str();
  //           Serial.print("Gateway set to: ");
  //           Serial.println(gateway);
  //           // Write file to save value
  //           writeFile(SPIFFS, gatewayPath, gateway.c_str());
  //         }
  //         //Serial.printf("POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
  //       }
  //     }
  //     request->send(200, "text/plain", "Done. ESP will restart, connect to your router and go to IP address: " + ip);
  //     delay(3000);
  //     ESP.restart();
  //   });
  //   server.begin();
  // }
  delay(500);
}

boolean trackingButton = false;
int lastMode = 0;

void loop() {
  // Serial.print("Starting Loop, Mode: ");
  // Serial.println(mode);
  // Check if button is pressed
  if (digitalRead(MODE_BUTTON) == LOW && trackingButton) {
    trackingButton = false;
  }


  if (digitalRead(MODE_BUTTON) == HIGH && !trackingButton) {
    Serial.println("Changing Mode");
    // Button just pressed
    trackingButton = true;

    // Change mode if button pressed
    if (mode == 4) {
      mode = 1;
    } else {
      mode++;
    }
  }

  // Functions for modes that need ations each loop
  switch (mode) {
    case 1:
      // Mode 1 - Solid white
      // No action needed
      break;
    case 2:
      // Mode 2 - Audio
      audio_to_leds();
      break;
  }

  // Changing Mode
  if (lastMode == mode) {
    delay(50);
    return;
  }

  Serial.print("Changing Mode");
  Serial.println(mode);
  switch (mode) {
    case 1:
      // Mode 1 - Solid white
      light_up_white();
      // r_g_b();
      break;
    case 2:
      // Mode 2 - Audio
      disable_all_led();
      audio_to_leds();
      break;
  }
  lastMode = mode;

  // Mode 3 - Adhoc Wifi

  // Mode 4 - Home kit

  delay(500);
}
