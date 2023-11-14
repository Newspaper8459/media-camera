#include "esp_camera.h"
#include <WiFi.h>
#include <base64.h>
#include <HTTPClient.h>
#include <bits/stdc++.h>
// #include "fb_gfx.h"
// #include "fd_forward.h"

using ll = long long;

//
// WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
//            Ensure ESP32 Wrover Module or other board with PSRAM is selected
//            Partial images will be transmitted if image exceeds buffer size
//
//            You must select partition scheme from the board menu that has at least 3MB APP space.
//            Face Recognition is DISABLED for ESP32 and ESP32-S2, because it takes up from 15
//            seconds to process single frame. Face Detection is ENABLED if PSRAM is enabled as well

// ===================
// Select camera model
// ===================
//#define CAMERA_MODEL_WROVER_KIT // Has PSRAM
// #define CAMERA_MODEL_ESP_EYE // Has PSRAM
//#define CAMERA_MODEL_ESP32S3_EYE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_PSRAM // Has PSRAM
//#define CAMERA_MODEL_M5STACK_V2_PSRAM // M5Camera version B Has PSRAM
//#define CAMERA_MODEL_M5STACK_WIDE // Has PSRAM
//#define CAMERA_MODEL_M5STACK_ESP32CAM // No PSRAM
//#define CAMERA_MODEL_M5STACK_UNITCAM // No PSRAM
#define CAMERA_MODEL_AI_THINKER  // Has PSRAM
//#define CAMERA_MODEL_TTGO_T_JOURNAL // No PSRAM
//#define CAMERA_MODEL_XIAO_ESP32S3 // Has PSRAM
// ** Espressif Internal Boards **
//#define CAMERA_MODEL_ESP32_CAM_BOARD
//#define CAMERA_MODEL_ESP32S2_CAM_BOARD
//#define CAMERA_MODEL_ESP32S3_CAM_LCD
//#define CAMERA_MODEL_DFRobot_FireBeetle2_ESP32S3 // Has PSRAM
//#define CAMERA_MODEL_DFRobot_Romeo_ESP32S3 // Has PSRAM
#include "camera_pins.h"

// ===========================
// Enter your WiFi credentials
// ===========================
// const char* ssid = "Zenfone-8_3317";
const char* ssid = "aterm-b10cdc-g";
// const char* password = "e8zrya633cj8cqg";
const char* password = "9ca442f802e19";
const char* apiEndpoint = "https://detect-ml-mvrz27tnnq-an.a.run.app/api/calc/";
const char* detectApiEndpoint = "https://detect-ml-mvrz27tnnq-an.a.run.app/api/detect/";
const char* contentType = "application/json";

bool flag = false;

void startCameraServer();
void setupLedFlash(int pin);

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

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
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  pinMode(LED_GPIO_NUM, OUTPUT);

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
#if CONFIG_IDF_TARGET_ESP32S3
    config.fb_count = 2;
#endif
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t* s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // up the brightness just a bit
    s->set_saturation(s, -2);  // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

#if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

#if defined(CAMERA_MODEL_ESP32S3_EYE)
  s->set_vflip(s, 1);
#endif

// Setup LED FLash if LED pin is defined in camera_pins.h
#if defined(LED_GPIO_NUM)
  setupLedFlash(LED_GPIO_NUM);
#endif

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
}

void cameraCapture() {
  camera_fb_t* fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  String data = base64::encode(fb->buf, fb->len);

  HTTPClient http;
  http.addHeader("Content-Type", contentType);

  String postData = "{\"file\": \"" + data + "\"}";
  http.begin(apiEndpoint);

  int httpCode = http.POST(postData);

  esp_camera_fb_return(fb);
  // Serial.println(httpCode);
  if (httpCode > 0) {
    String payload = http.getString();
    Serial.println("status: " + String(httpCode));
    Serial.println("Response: " + payload);
    double brightness = payload.toFloat();
    // if(brightness < 0.1) push();
    if(flag && brightness < 0.2){
      // Serial.println(flag);
      Serial.println("processing 'push'");
      bool res = push();
      if(!res) Serial.println("push failed");
      else Serial.println("push successful");
    }
    else if(!flag && brightness > 0.2) flag = true;
  } else {
    Serial.println("failed");
  }

  http.end();

}

float median(std::vector<float> a){
  std::sort(a.begin(), a.end());
  if(a.size()%2 == 1) return a[a.size()/2];
  else return (a[a.size()/2 - 1] + a[a.size()/2])/2;
}

bool push(){
  camera_fb_t* fb = NULL;

  analogWrite(LED_GPIO_NUM, 255);
  delay(100);
  fb = esp_camera_fb_get();
  delay(100);
  analogWrite(LED_GPIO_NUM, 0);
  if (!fb) {
    Serial.println("Camera capture failed");
    return false;
  }

  String data = base64::encode(fb->buf, fb->len);

  HTTPClient http;
  http.addHeader("Content-Type", contentType);

  String postData = "{\"file\": \"" + data + "\"}";
  http.begin(detectApiEndpoint);

  int httpCode = http.POST(postData);

  esp_camera_fb_return(fb);

  if(httpCode > 0){
    flag = false;
    return true;
  } else {
    return false;
  }

  // return true;
}

void loop() {
  cameraCapture();
  // digitalWrite(LED_GPIO_NUM, HIGH);
  // analogWrite(LED_GPIO_NUM, 255);
  // Serial.println("-------------------------");
  // delay(2000);
  // digitalWrite(LED_GPIO_NUM, LOW);
  // analogWrite(LED_GPIO_NUM, 0);
  // delay(2000);
}
