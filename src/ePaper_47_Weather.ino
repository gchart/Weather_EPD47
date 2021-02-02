// Weather Display for the LilyGo 4.7" ePaper board
// Built by gChart
// https://github.com/gchart

// Shout out to Valentine Roland, the source of bits of this code
// https://github.com/vroland/epdiy/tree/master/examples/weather/

// Weather icons from https://github.com/kickstandapps/WeatherIcons

// Other information for the EPD47 board: https://github.com/Xinyuan-LilyGO/LilyGo-EPD47

#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM !!!"
#endif

#include <Arduino.h>
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#include <ArduinoJson.h> // https://arduinojson.org/
#include <HTTPClient.h>
#include <WiFi.h>
//#include "freertos/FreeRTOS.h"
//#include "freertos/task.h"
#include "epd_driver.h"
#include "WeatherIcons150.h"
#include "WeatherIcons72.h"
#include "MoonPhases24.h" // by Curtis Clark
#include "Roboto16.h"
#include "Roboto20.h"
#include "Roboto24.h"
#include "esp_adc_cal.h"
#include "time.h"
#include <Wire.h>
#include "config.h"

GFXfont currentFont;

#define BATT_PIN            36

#define GxEPD_WHITE 0xFF
#define GxEPD_BLACK 0x00
#define GRAY 0x88
#define LRAY 0xBB
#define DRAY 0x44

enum alignment {LEFT, RIGHT, CENTER, CCENTER};

typedef struct {
  int      Dt;
  int      Sunrise;
  int      Sunset;
  String   Icon;
  String   Condition;
  String   Description;
  float    Temperature;
  float    FeelsLike;
  float    Humidity;
  float    High;
  float    Low;
  float    Windspeed;
  float    Precip;
} WeatherRecord;

#define fcst_readings 4
WeatherRecord  WxConditions[1];
WeatherRecord  WxForecast[fcst_readings];

uint8_t *framebuffer;

bool ConnectWifi();
void DisconnectWifi();
void SetupEPD();
float GetVoltage();
bool DecodeWeather(WiFiClient& json);
bool ObtainWeatherData(WiFiClient& client);
String TranslateIcon(String icon_id, String cond_id);
String Precip(float p);
String ConvertUnixTime(int unix_time, String format);
void DisplayWeather();
void DisplayGrid();
void DisplayCurrentWeather();
void DisplayForecast(int f);
void DisplayBattery();
void DisplayMoonPhase();
void drawString(int x, int y, String text, alignment align);
void setFont(GFXfont const&font);

void setup()
{
  Serial.begin(115200);

  setenv("TZ", Timezone, 1);  //setenv()adds the "TZ" variable to the environment with a value TimeZone, only used if set to 1, 0 means no change
  tzset(); // Set the TZ environment variable

  int sleep_minutes = 2;  // default to trying again in 2 minutes
  
  unsigned long start_time = millis();
  if(ConnectWifi()) {
    Serial.printf("Wifi connection took %d ms\n", millis() - start_time);

    int Attempts = 1;
    bool RxWeather = false;
    WiFiClient client;   // wifi client object
    while (RxWeather == false && Attempts <= 2) { // Try up-to 2 time for Weather data
      RxWeather = ObtainWeatherData(client);
      Attempts++;
    }
    DisconnectWifi();
    if (RxWeather) { // Only if received Weather proceed
      SetupEPD();
      DisplayWeather();
      epd_draw_grayscale_image(epd_full_screen(), framebuffer);
      epd_poweroff_all();

      // since everything processed fine, sleep until the next hour mark
      int sleep_interval = 60; // default to 60 minute sleep time
      int current_minute = ConvertUnixTime(WxConditions[0].Dt, "%M").toInt();
      int current_hour = ConvertUnixTime(WxConditions[0].Dt, "%H").toInt();
      if(current_hour > 5 && current_hour < 22) {  // if primary waking time, sleep for 30 minutes
        sleep_interval = 30;
      }
      sleep_minutes = sleep_interval - current_minute % sleep_interval;
      if(sleep_minutes <= 2) sleep_minutes += sleep_interval; // if we're super close, go to the next interval
      Serial.print("Sleeping for "); Serial.print(sleep_minutes); Serial.println(" minutes");
    }    
  }
  Serial.printf("Total awake time: %d ms\n", millis() - start_time);
  esp_sleep_enable_timer_wakeup(sleep_minutes * 60ULL * 1000000ULL);
  esp_deep_sleep_start();
}

void loop(){}




bool ConnectWifi() {
  int ticks = 0;
  WiFi.begin(ssid,password);
  while (WiFi.status() != WL_CONNECTED && ticks < 100) {
      delay(50);
      ticks++;
      Serial.print(".");
  }

  if(WiFi.status() != WL_CONNECTED) { 
    Serial.println("Error: \nCan't connect to WiFi");
    return false;
  }
  else {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
  return true;
}


void DisconnectWifi() {
  Serial.println("Disconnecting WiFi and turning it off...");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void SetupEPD() {
  epd_init();
  framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
  if (!framebuffer) {
      Serial.println("alloc memory failed !!!");
      while (1);
  }
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);
  epd_poweron();
  epd_clear();
}

float GetVoltage() {
  int vref = 1100;
  // Correct the ADC reference voltage
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
      Serial.printf("eFuse Vref:%u mV", adc_chars.vref);
      vref = adc_chars.vref;
  }
  uint16_t v = analogRead(BATT_PIN);
  float battery_voltage = ((float)v / 4095.0) * 2.0 * 3.3 * (vref / 1000.0);
  return battery_voltage;
}


/*
https://github.com/kickstandapps/WeatherIcons
Uppercase for bold, lowercase for thin
A: cloudy
B: hail
C: haze
D: moon (clear)
E: moon (cloudy)
F: partly sunny
G: rain
H: snow
I: thunderstorm
J: sunny
K: sunrise
L: tornado

Match these up to what is listed here:
https://openweathermap.org/weather-conditions
*/

String TranslateIcon(String icon_id, String cond_id) {
  if (cond_id == "781") return "L"; // tornado
  if (icon_id == "01d") return "J"; // day, clear sky
  if (icon_id == "02d") return "F"; // day, few clouds
  if (icon_id == "03d") return "F"; // day, scattered clouds
  if (icon_id == "04d") return "F"; // day, broken clouds
  if (icon_id == "09d") return "G"; // day, shower rain
  if (icon_id == "10d") return "G"; // day, rain
  if (icon_id == "11d") return "I"; // day, thunderstorm
  if (icon_id == "13d") return "H"; // day, snow
  if (icon_id == "50d") return "C"; // day, mist
  if (icon_id == "01n") return "D"; // night, clear sky
  if (icon_id == "02n") return "E"; // night, few clouds
  if (icon_id == "03n") return "E"; // night, scattered clouds
  if (icon_id == "04n") return "E"; // night, broken clouds
  if (icon_id == "09n") return "G"; // night, shower rain
  if (icon_id == "10n") return "G"; // night, rain
  if (icon_id == "11n") return "I"; // night, thunderstorm
  if (icon_id == "13n") return "H"; // night, snow
  if (icon_id == "50n") return "C"; // night, mist
  return ""; // not found for some reason
}

String Precip(float p) {
    if(p == 0) return "";
    else return String(", " + (String)(int)(p*100) + "%");
}

String ConvertUnixTime(int unix_time, String format) {
  time_t tm = unix_time;
  struct tm *now_tm = localtime(&tm);
  char output[40];
  strftime(output, sizeof(output), format.c_str(), now_tm);
  return output;
}

bool ObtainWeatherData(WiFiClient & client) {
  client.stop(); // close any connections before sending a new request
  HTTPClient http;
  const char server[] = "api.openweathermap.org";
  String uri = "/data/2.5/onecall?lat="+lat+"&lon="+lon+"&units="+units+"&exclude=hourly,minutely,alerts&lang="+lang+"&APPID="+apikey;
  http.begin(client, server, 80, uri);
  int httpCode = http.GET();
  if(httpCode == HTTP_CODE_OK) {
    //String payload = http.getString(); Serial.println(payload); return false;  // used for debugging API problems
    if (!DecodeWeather(http.getStream())) return false;
    client.stop();
    http.end();
    return true;
  }
  else
  {
    Serial.printf("connection failed, error: %s", http.errorToString(httpCode).c_str());
    client.stop();
    http.end();
    return false;
  }
  http.end();
  return true;
}


bool DecodeWeather(WiFiClient& json) {
  Serial.print(F("\nCreating DynamicJSON object... "));
  // allocate the JsonDocument
  DynamicJsonDocument doc(6144); // recommended by https://arduinojson.org/v6/assistant/
  Serial.print(F("\nAllocated object, starting deserialization... "));
  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, json);
  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    return false;
  }
  Serial.print(F("\nDeserialization successful... "));

  // current conditions
  WxConditions[0].Dt          = doc["current"]["dt"].as<int>();
  WxConditions[0].Sunrise     = doc["current"]["sunrise"].as<int>();
  WxConditions[0].Sunset      = doc["current"]["sunset"].as<int>();
  WxConditions[0].Icon        = doc["current"]["weather"][0]["icon"].as<char*>();
  WxConditions[0].Condition   = doc["current"]["weather"][0]["id"].as<char*>();
  WxConditions[0].Description = doc["current"]["weather"][0]["main"].as<char*>();
  WxConditions[0].Temperature = doc["current"]["temp"].as<float>();
  WxConditions[0].FeelsLike   = doc["current"]["feels_like"].as<float>();
  WxConditions[0].Humidity    = doc["current"]["humidity"].as<float>();
  WxConditions[0].Windspeed   = doc["current"]["wind_speed"].as<float>();

  // daily conditions
  for (int d = 0; d < fcst_readings; d++) {
    WxForecast[d].Dt          = doc["daily"][d]["dt"].as<int>();
    WxForecast[d].Icon        = doc["daily"][d]["weather"][0]["icon"].as<char*>();
    WxForecast[d].Condition   = doc["daily"][d]["weather"][0]["id"].as<char*>();
    WxForecast[d].Description = doc["daily"][d]["weather"][0]["main"].as<char*>();
    WxForecast[d].Temperature = doc["daily"][d]["temp"].as<float>();
    WxForecast[d].High        = doc["daily"][d]["temp"]["max"].as<float>();
    WxForecast[d].Low         = doc["daily"][d]["temp"]["min"].as<float>();
    WxForecast[d].Windspeed   = doc["daily"][d]["wind_speed"].as<float>();
    WxForecast[d].Precip      = doc["daily"][d]["pop"].as<float>();
  }
  Serial.print(F("\nJSON decoded successfully... "));

  return true;
}

void drawString(int x, int y, String text, alignment align) {
  char * data  = const_cast<char*>(text.c_str());
  int  x1, y1; //the bounds of x,y and w and h of the variable 'text' in pixels.
  int w, h;
  int xx = x, yy = y;
  get_text_bounds(&currentFont, data, &xx, &yy, &x1, &y1, &w, &h, NULL);

  if (align == RIGHT)  x = x - w;
  if (align == CENTER || align == CCENTER) x = x - w / 2;

  //int cursor_y = y + h;
  int cursor_y = y;
  if (align == CCENTER) cursor_y = y + h / 2;
  
  write_string(&currentFont, data, &x, &cursor_y, framebuffer);
}

void setFont(GFXfont const &font) {
  currentFont = font;
}

void DisplayWeather() {
  DisplayGrid();
  DisplayCurrentWeather();
  for(int f = 0; f < fcst_readings; f++) { DisplayForecast(f); }
  DisplayBattery();
  DisplayMoonPhase();
}

void DisplayGrid() {
  // two lines each for double thickness
  epd_draw_vline(480, 0, 540, GxEPD_BLACK, framebuffer);
  epd_draw_vline(481, 0, 540, GxEPD_BLACK, framebuffer);
  epd_draw_hline(480, 135, 480, GxEPD_BLACK, framebuffer);
  epd_draw_hline(480, 136, 480, GxEPD_BLACK, framebuffer);
  epd_draw_hline(480, 270, 480, GxEPD_BLACK, framebuffer);
  epd_draw_hline(480, 271, 480, GxEPD_BLACK, framebuffer);
  epd_draw_hline(480, 405, 480, GxEPD_BLACK, framebuffer);
  epd_draw_hline(480, 406, 480, GxEPD_BLACK, framebuffer);
}

void DisplayCurrentWeather() {
  setFont(Roboto24);
  drawString(240, 45, TXT_CURRENT + ": " + WxConditions[0].Description, CENTER);

  setFont(Roboto20);
  drawString(240, 340, TXT_TEMPERATURE + ": " + String(round(WxConditions[0].Temperature),0) + TXT_DEGREE, CENTER);
  drawString(240, 390, TXT_FEELS_LIKE + ": " + String(round(WxConditions[0].FeelsLike),0) + TXT_DEGREE, CENTER);
  drawString(10, 440, String(round(WxConditions[0].Humidity),0) + "% " + TXT_HUMIDITY, LEFT);
  drawString(470, 440, String(round(WxConditions[0].Windspeed),0) + " " + TXT_SPEED, RIGHT);

  setFont(Roboto16);
  String conddt = ConvertUnixTime(WxConditions[0].Dt, "%I:%M");
  if(conddt.startsWith("0")) conddt.remove(0,1);
  drawString(470, 493, TXT_UPDATED, RIGHT);
  drawString(470, 530, TXT_AT + " " + conddt, RIGHT);

  String sunrise = ConvertUnixTime(WxConditions[0].Sunrise, "%I:%M");
  if(sunrise.startsWith("0")) sunrise.remove(0,1);
  String sunset = ConvertUnixTime(WxConditions[0].Sunset, "%I:%M");
  if(sunset.startsWith("0")) sunset.remove(0,1);
  drawString(10, 493, TXT_SUNRISE + ": " + sunrise, LEFT);
  drawString(10, 530, TXT_SUNSET  + ": " + sunset, LEFT);

  setFont(WeatherIcons150);
  drawString(240, 170, TranslateIcon(WxConditions[0].Icon,WxConditions[0].Condition), CCENTER);
  
}

void DisplayForecast(int f) {
  setFont(Roboto16);
  drawString(730, 60 + f*135, WxForecast[f].Description + Precip(WxForecast[f].Precip), LEFT);
  drawString(730, 105 + f*135, String(round(WxForecast[f].High),0) + TXT_DEGREE + "  /  " + String(round(WxForecast[f].Low),0) + TXT_DEGREE, LEFT);
  drawString(545, 60 + f*135, ConvertUnixTime(WxForecast[f].Dt, "%a"), CENTER);
  drawString(545, 105 + f*135, ConvertUnixTime(WxForecast[f].Dt, "%b %e"), CENTER);
  setFont(WeatherIcons72);
  drawString(655, 67 + f*135, TranslateIcon(WxForecast[f].Icon,WxForecast[f].Condition), CCENTER);
}

void DisplayBattery() {
  int batt_x = 245;  // the position of the upper-left
  int batt_y = 504;  // corner of the battery icon
  int color  = LRAY; // which color to use for the battery icon
  float voltage = GetVoltage();
  
  epd_draw_rect(batt_x,    batt_y  , 70, 26, color, framebuffer);
  epd_draw_rect(batt_x+ 1, batt_y+1, 68, 24, color, framebuffer);
  epd_fill_rect(batt_x+70, batt_y+6,  4, 14, color, framebuffer);
  if(voltage >= 3.97) epd_fill_rect(batt_x+55, batt_y+3, 12, 20, color, framebuffer); // bar 1
  if(voltage >= 3.83) epd_fill_rect(batt_x+42, batt_y+3, 12, 20, color, framebuffer); // bar 2
  if(voltage >= 3.73) epd_fill_rect(batt_x+29, batt_y+3, 12, 20, color, framebuffer); // bar 3
  if(voltage >= 3.67) epd_fill_rect(batt_x+16, batt_y+3, 12, 20, color, framebuffer); // bar 4
  if(voltage >= 3.10) epd_fill_rect(batt_x+ 3, batt_y+3, 12, 20, color, framebuffer); // bar 5
  if(voltage < 3.10) { // low battery warning slash
    epd_write_line(batt_x+44, batt_y-3, batt_x+24, batt_y+29, color, framebuffer);
    epd_write_line(batt_x+45, batt_y-3, batt_x+25, batt_y+29, color, framebuffer);
    epd_write_line(batt_x+46, batt_y-3, batt_x+26, batt_y+29, color, framebuffer);
  }
}

void DisplayMoonPhase() {
  // https://www.subsystems.us/uploads/9/8/9/4/98948044/moonphase.pdf
  int yy   = ConvertUnixTime(WxConditions[0].Dt, "%C").toInt();
  int yyyy = ConvertUnixTime(WxConditions[0].Dt, "%Y").toInt();
  int mm   = ConvertUnixTime(WxConditions[0].Dt, "%m").toInt();
  int dd   = ConvertUnixTime(WxConditions[0].Dt, "%d").toInt();
  double julian = (2 - yy + (int)(yy/4)) + dd + (365.25 * (yyyy + 4716)) + (30.6001 * (mm + 1)) - 1524.5;
  // get day diff from a known date of a full moon, then modulo the lunar cycle time
  double phase = fmod((julian - 2459225.9502), 29.530588853);
  int moon = round(28*(phase/29.530588853));
  // The new moon is mapped to 0 and the full moon is mapped to @.  Other phases range from A-Z.
  char moon_icons[] = "0ABCDEFGHIJKLM@NOPQRSTUVWXYZ";
  setFont(MoonPhases24);
  drawString(280, 470, String(moon_icons[moon]), CCENTER);
}
