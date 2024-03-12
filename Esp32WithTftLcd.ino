/*
  Live Image Classification on ESP32-CAM and ST7735 TFT display 
  using MobileNet v1 from Edge Impulse
  Modified from https://github.com/edgeimpulse/example-esp32-cam.

  Note: 
  The ST7735 TFT size has to be at least 120x120.
  Do not use Arduino IDE 2.0 or you won't be able to see the serial output!
*/

#include <esp32-cam-cat-dog_inferencing.h>  // replace with your deployed Edge Impulse library

#define CAMERA_MODEL_AI_THINKER

#include "img_converters.h"
#include "image_util.h"
#include "esp_camera.h"
#include "camera_pins.h"
//#include "esp_attr.h"  // for IRAM_ATTR

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735

#define TFT_SCLK 14 // SCL
#define TFT_MOSI 13 // SDA
#define TFT_RST  12 // RES (RESET)
#define TFT_DC    2 // Data Command control pin
#define TFT_CS   15 // Chip select control pin
                    // BL (back light) and VCC -> 3V3

#define BTN       4 // button (shared with flash led)

dl_matrix3du_t *resized_matrix = NULL;
ei_impulse_result_t result = {0};

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

int interruptPin = BTN;

// Interrupt Service Routine (IRS) callback function, declare as IRAM_ATTR means put it in RAM (increase meet rate)
// Note: don't know why... isr need locate above setup()
void IRAM_ATTR isr_Callback() {  
  int StartTime, EndTime;

  // capture a image and classify it
  Serial.println("Start classify.");
  StartTime = millis();
  String result = classify();
  EndTime = millis();
  Serial.printf("End classify. spend time: %d ms\n", StartTime - EndTime);

  // display result
  Serial.printf("Result: %s\n", result);
  tft_drawtext(4, 120 - 16, result, 2, ST77XX_GREEN);

  // wait for next press button to exit ISR (continue show screen)
  while (!digitalRead(BTN));
}

// setup
void setup() {
  Serial.begin(115200);

  // button
  pinMode(BTN, INPUT);

  // TFT display init
  tft.initR(INITR_GREENTAB); // you might need to use INITR_REDTAB or INITR_BLACKTAB to get correct text colors
  tft.setRotation(3);  // 1表示逆時針旋轉90度，2表示旋轉180度，3表示旋轉270度, org: 0
  tft.fillScreen(ST77XX_BLACK);

  // cam config
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
  config.frame_size =  FRAMESIZE_240X240;
  config.jpeg_quality = 10;
  config.fb_count = 1;

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1); // flip it back
    s->set_brightness(s, 1); // up the brightness just a bit
    s->set_saturation(s, 0); // lower the saturation
  }

  // set interrupt service routine for button (GPIO 4), trigger: LOW/HIGH/CHANGE/RISING/FALLING, FALLING: when release button 
  attachInterrupt(digitalPinToInterrupt(interruptPin), isr_Callback, FALLING);  

  Serial.println("Camera Ready!...(standby, press button to start)");
  tft_drawtext(4, 4, "Standby", 1, ST77XX_BLUE);
}

// main loop
void loop() {
  int StartTime, EndTime;
  // wait until the button is pressed
  //while (!digitalRead(BTN)) {
    Serial.println("Start show screen.");
    StartTime = millis();
    showScreen();
    EndTime = millis();
    Serial.printf("End show screen. spend time: %d ms\n", StartTime - EndTime);
  //};
  //tft.fillScreen(ST77XX_BLACK);
  //delay(1000);
/*
  // capture a image and classify it
  Serial.println("Start classify.");
  StartTime = millis();
  String result = classify();
  EndTime = millis();
  Serial.printf("End classify. spend time: %d ms\n", StartTime - EndTime);

  // display result
  Serial.printf("Result: %s\n", result);
  tft_drawtext(4, 120 - 16, result, 2, ST77XX_GREEN);

  // wait for next press button to continue show screen
  while (!digitalRead(BTN));
  delay(1000);
*/
}

void showScreen() {
  int StartTime, EndTime;
  //capture_quick();
  camera_fb_t *fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return;
  }

  // --- Convert frame to RGB565 and display on the TFT ---
  Serial.println("  Converting to RGB565 and display on TFT...");
  uint8_t *rgb565 = (uint8_t *) malloc(240 * 240 * 3);
  //uint8_t *rgb565 = (uint8_t *) malloc(96 * 96 * 3); 
  StartTime = millis();
  jpg2rgb565(fb->buf, fb->len, rgb565, JPG_SCALE_2X); // scale to half size
  EndTime = millis();
  Serial.printf("  jpg2rgb565() spend time: %d ms\n", StartTime - EndTime);
  //jpg2rgb565(fb->buf, fb->len, rgb565, JPG_SCALE_NONE); // scale to half size
  tft.drawRGBBitmap(0, 0, (uint16_t*)rgb565, 120, 120);
  //tft.drawRGBBitmap(32, 16, (uint16_t*)rgb565, 96, 96);

  // --- Free memory ---
  //rgb565 = NULL;
  free(rgb565);
  esp_camera_fb_return(fb);
}

// classify labels
String classify() {
  int StartTime, EndTime;
  // run image capture once to force clear buffer
  // otherwise the captured image below would only show up next time you pressed the button!
  capture_quick();

  // capture image from camera
  if (!capture()) return "Error";
  tft_drawtext(4, 4, "Classifying...", 1, ST77XX_CYAN);

  Serial.println("  Getting image...");
  signal_t signal;
  signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_WIDTH;
  signal.get_data = &raw_feature_get_data;

  Serial.println("  Run classifier...");
  // Feed signal to the classifier
  StartTime = millis();
  EI_IMPULSE_ERROR res = run_classifier(&signal, &result, false /* debug */);
  EndTime = millis();
  Serial.printf("  run_classifier() spend time: %d ms\n", StartTime - EndTime);
  // --- Free memory ---
  dl_matrix3du_free(resized_matrix);

  // --- Returned error variable "res" while data object.array in "result" ---
  ei_printf("run_classifier returned: %d\n", res);
  if (res != 0) return "Error";

  // --- print the predictions ---
  ei_printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
            result.timing.dsp, result.timing.classification, result.timing.anomaly);
  int index;
  float score = 0.0;
  for (size_t ix = 0; ix < EI_CLASSIFIER_LABEL_COUNT; ix++) {
    // record the most possible label
    if (result.classification[ix].value > score) {
      score = result.classification[ix].value;
      index = ix;
    }
    ei_printf("    %s: \t%f\r\n", result.classification[ix].label, result.classification[ix].value);
    tft_drawtext(4, 12 + 8 * ix, String(result.classification[ix].label) + " " + String(result.classification[ix].value * 100) + "%", 1, ST77XX_ORANGE);
  }

#if EI_CLASSIFIER_HAS_ANOMALY == 1
  ei_printf("    anomaly score: %f\r\n", result.anomaly);
#endif

  // --- return the most possible label ---
  return String(result.classification[index].label);
}

// quick capture (to clear buffer)
void capture_quick() {
  camera_fb_t *fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) return;
  esp_camera_fb_return(fb);
}

// capture image from cam
bool capture() {

  Serial.println("Capture image...");
  esp_err_t res = ESP_OK;
  camera_fb_t *fb = NULL;
  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return false;
  }

  // --- Convert frame to RGB888  ---
  Serial.println("Converting to RGB888...");
  // Allocate rgb888_matrix buffer
  dl_matrix3du_t *rgb888_matrix = dl_matrix3du_alloc(1, fb->width, fb->height, 3);
  fmt2rgb888(fb->buf, fb->len, fb->format, rgb888_matrix->item);

  // --- Resize the RGB888 frame to 96x96 in this example ---
  Serial.println("Resizing the frame buffer...");
  resized_matrix = dl_matrix3du_alloc(1, EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT, 3);
  image_resize_linear(resized_matrix->item, rgb888_matrix->item, EI_CLASSIFIER_INPUT_WIDTH, EI_CLASSIFIER_INPUT_HEIGHT, 3, fb->width, fb->height);

  // --- Convert frame to RGB565 and display on the TFT ---
  Serial.println("Converting to RGB565 and display on TFT...");
  uint8_t *rgb565 = (uint8_t *) malloc(240 * 240 * 3);
  //uint8_t *rgb565 = (uint8_t *) malloc(96 * 96 * 3); 
  jpg2rgb565(fb->buf, fb->len, rgb565, JPG_SCALE_2X); // scale to half size
  //jpg2rgb565(fb->buf, fb->len, rgb565, JPG_SCALE_NONE); // scale to half size
  tft.drawRGBBitmap(0, 0, (uint16_t*)rgb565, 120, 120);
  //tft.drawRGBBitmap(32, 16, (uint16_t*)rgb565, 96, 96);

  // --- Free memory ---
  //rgb565 = NULL;
  free(rgb565);
  dl_matrix3du_free(rgb888_matrix);
  esp_camera_fb_return(fb);

  return true;
}

int raw_feature_get_data(size_t offset, size_t out_len, float *signal_ptr) {

  size_t pixel_ix = offset * 3;
  size_t bytes_left = out_len;
  size_t out_ptr_ix = 0;

  // read byte for byte
  while (bytes_left != 0) {
    // grab the values and convert to r/g/b
    uint8_t r, g, b;
    r = resized_matrix->item[pixel_ix];
    g = resized_matrix->item[pixel_ix + 1];
    b = resized_matrix->item[pixel_ix + 2];

    // then convert to out_ptr format
    float pixel_f = (r << 16) + (g << 8) + b;
    signal_ptr[out_ptr_ix] = pixel_f;

    // and go to the next pixel
    out_ptr_ix++;
    pixel_ix += 3;
    bytes_left--;
  }

  return 0;
}

// draw test on TFT
void tft_drawtext(int16_t x, int16_t y, String text, uint8_t font_size, uint16_t color) {
  tft.setCursor(x, y);
  tft.setTextSize(font_size); // font size 1 = 6x8, 2 = 12x16, 3 = 18x24
  tft.setTextColor(color);
  tft.setTextWrap(true);
  tft.print(strcpy(new char[text.length() + 1], text.c_str()));
}


