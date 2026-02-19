#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "esp_camera.h"
#include "secrets.h"
#include "mbedtls/base64.h"

// MQTT Topics
#define AWS_IOT_PUBLISH_TOPIC   "marrty/" THINGNAME "/recognize"
#define AWS_IOT_SUBSCRIBE_TOPIC "marrty/" THINGNAME "/result"

WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

// Camera Pins (ESP32-S3-DevKitC-1 with OV2640)
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

void messageHandler(char* topic, byte* payload, unsigned int length) {
  Serial.print("incoming: ");
  Serial.println(topic);

  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload);
  const char* message = doc["message"]; // Customize based on your result payload
  Serial.println(message);
}

void connectAWS() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.setServer(AWS_IOT_ENDPOINT, 8883);

  // Create a message handler
  client.setCallback(messageHandler);

  Serial.println("Connecting to AWS IOT");

  while (!client.connected()) {
    Serial.print(".");
    if (client.connect(THINGNAME)) {
      Serial.println("Connected!");
      client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void publishImage() {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }
  
  Serial.printf("Picture taken! Size: %d bytes\n", fb->len);

  // Buffer for Base64 (approx 1.33x size)
  size_t outputLength;
  size_t bufferSize = ((fb->len + 2) / 3) * 4 + 1;
  unsigned char* base64Buffer = (unsigned char*) malloc(bufferSize);
  
  if (!base64Buffer) {
    Serial.println("Memory allocation failed for Base64 buffer");
    esp_camera_fb_return(fb);
    return;
  }

  mbedtls_base64_encode(base64Buffer, bufferSize, &outputLength, fb->buf, fb->len);
  base64Buffer[outputLength] = '\0'; // Null-terminate

  // Construct Custom JSON Payload manually due to size
  String topic = String("marrty/") + THINGNAME + "/recognize";
  
  // Payload: {"image": "BASE64_STRING"}
  // Overhead: {"image":""} is 12 chars.
  
  // Note: PubSubClient default packet size is 256 bytes. You MUST increase MQTT_MAX_PACKET_SIZE in PubSubClient.h or platformio.ini
  // However, PubSubClient isn't great for large payloads. 
  // AWS IoT accepts up to 128KB.
  // We use beginPublish for chunked sending if supported or just ensure buffer is large enough.
  // PubSubClient's beginPublish/write/endPublish is robust for stream writing.
  
  if (client.beginPublish(topic.c_str(), outputLength + 12, false)) {
    client.print("{\"image\":\"");
    client.write(base64Buffer, outputLength);
    client.print("\"}");
    client.endPublish();
    Serial.println("Image published!");
  } else {
    Serial.println("Publish failed. Check MQTT_MAX_PACKET_SIZE in library or connection.");
  }

  free(base64Buffer);
  esp_camera_fb_return(fb);
}

void setup() {
  Serial.begin(115200);

  // Camera configuration
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
  config.frame_size = FRAMESIZE_QVGA; // QVGA = 320x240, approx 4-8KB JPEG
  config.pixel_format = PIXFORMAT_JPEG; 
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if(config.pixel_format == PIXFORMAT_JPEG){
    if(psramFound()){
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_QVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  connectAWS();
}

void loop() {
  client.loop();
  
  // Trigger logic (e.g. button press or interval)
  // For demo, run every 10s
  static unsigned long lastCapture = 0;
  if (millis() - lastCapture > 10000) {
    lastCapture = millis();
    if (client.connected()) {
      publishImage();
    }
  }
}
