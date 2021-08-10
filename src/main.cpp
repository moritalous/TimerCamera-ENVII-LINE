#include <Arduino.h>
#include <WiFi.h>

#include "battery.h"
#include "led.h"
#include "bmm8563.h"

#include "esp_camera.h"
#include "camera_pins.h"

#include <ESP_Line_Notify.h>

#include <Wire.h>
#include "Adafruit_Sensor.h"
#include "Adafruit_BMP280.h"
#include "Adafruit_SHT31.h"

const char *ssid = "xxxxxxxxxx";
const char *password = "xxxxxxxxxx";

const char *line_token = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";

int sleep_time = 15 * 60;

#define ENV_I2C_SDA 4
#define ENV_I2C_SCL 13

#define BM8563_I2C_SDA 12
#define BM8563_I2C_SCL 14

float tmp = 0.0;
float hum = 0.0;
float pressure = 0.0;

Adafruit_BMP280 bme = Adafruit_BMP280(&Wire);
Adafruit_SHT31 sht3x = Adafruit_SHT31(&Wire);

LineNotiFyClient line;

String message = "";

void enterSleep()
{
  Serial.println("Enter Sleep! Wake Up after " + String(sleep_time) + " Sec.");
  delay(500);
  Wire.begin(BM8563_I2C_SDA, BM8563_I2C_SCL);
  delay(500);

  bmm8563_init();
  bmm8563_setTimerIRQ(sleep_time);

  bat_disable_output();

  esp_deep_sleep(sleep_time * 1000 * 1000);
  esp_deep_sleep_start();
}

void setup()
{
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  bat_init();

  led_init(CAMERA_LED_GPIO);

  Wire.begin(ENV_I2C_SDA, ENV_I2C_SCL);
  while (!bme.begin(0x76))
  {
    Serial.println("Could not find a valid BMP280 sensor, check wiring!");
  }
  while (!sht3x.begin(0x44))
  {
    Serial.println("Could not find a valid SHT3X sensor, check wiring!");
  }

  pressure = bme.readPressure();
  tmp = sht3x.readTemperature();
  hum = sht3x.readHumidity();

  message = "\r\nきおん" + String(tmp) + "℃\r\n" +
            "しつど" + String(hum) + "%\r\n" +
            "きあつ" + String((int)pressure / 100) + "hPa\r\n" +
            "バッテリー" + String(bat_get_voltage()) + "mv";

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
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_UXGA;
  config.jpeg_quality = 10;
  config.fb_count = 2;

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  //initial sensors are flipped vertically and colors are a bit saturated
  s->set_vflip(s, 1);       //flip it back
  s->set_brightness(s, 1);  //up the blightness just a bit
  s->set_saturation(s, -2); //lower the saturation

  //drop down frame size for higher initial frame rate
  s->set_framesize(s, FRAMESIZE_SXGA);

  Serial.printf("Connect to %s, %s\r\n", ssid, password);

  WiFi.begin(ssid, password);

  int count = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    count++;
    if (count > 20)
    {
      enterSleep();
    }
  }
  Serial.println("");
  Serial.println("WiFi connected");
}

/* Function to print the sending result via Serial */
void printRessult(LineNotifySendingResult result)
{
  if (result.status == LineNotify_Sending_Success)
  {
    Serial.printf("Status: %s\n", "success");
    Serial.printf("Text limit: %d\n", result.quota.text.limit);
    Serial.printf("Text remaining: %d\n", result.quota.text.remaining);
    Serial.printf("Image limit: %d\n", result.quota.image.limit);
    Serial.printf("Image remaining: %d\n", result.quota.image.remaining);
    Serial.printf("Reset: %d\n", result.quota.reset);
  }
  else if (result.status == LineNotify_Sending_Error)
  {
    Serial.printf("Status: %s\n", "error");
    Serial.printf("error code: %d\n", result.error.code);
    Serial.printf("error msg: %s\n", result.error.message.c_str());
  }
}

void loop()
{
  // put your main code here, to run repeatedly:

  camera_fb_t *fb = NULL;
  fb = esp_camera_fb_get();

  if (!fb)
  {
    Serial.println("Camera capture failed");
    return;
  }

  Serial.println("capture complete");

  line.reconnect_wifi = true;
  line.token = line_token;
  line.message = message.c_str();

  line.image.data.blob = fb->buf;
  line.image.data.size = fb->len;
  line.image.data.file_name = "camera.jpg";

  Serial.println(message);

  LineNotifySendingResult result = LineNotify.send(line);
  printRessult(result);

  esp_camera_fb_return(fb);

  enterSleep();
}