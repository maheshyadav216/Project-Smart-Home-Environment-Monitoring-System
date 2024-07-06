//=============================================================================//
// Project/Tutorial       - DIY Project - Smart Home Environment Monitoring System
// Author                 - https://www.hackster.io/maheshyadav216
// Hardware               - DFRobot FireBeetle 2 ESP32 C6 Dev board, 2" LCD Display       
// Sensors                - Fermion Multifunction Env Sensor, SGP40 Air Quality
// Software               - Arduino IDE
// GitHub Repo of Project - https://github.com/maheshyadav216/Project-Smart-Home-Environment-Monitoring-System
// Code last Modified on  - 06/07/2024
//============================================================================//

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include "DFRobot_GDL.h"
#include <DFRobot_SGP40.h>
#include "DFRobot_EnvironmentalSensor.h"

// Wifi network station credentials
#define WIFI_SSID ""
#define WIFI_PASSWORD ""

// Telegram BOT Token (Get from Botfather)
#define BOT_TOKEN ""
#define CHAT_ID ""

WiFiClientSecure secured_client;

UniversalTelegramBot bot(BOT_TOKEN, secured_client);

//Display Pins
#define TFT_DC  D2
#define TFT_CS  D6
#define TFT_RST D3

/**
 * @brief Constructor Constructor of hardware SPI communication
 * @param dc Command/data line pin for SPI communication
 * @param cs Chip select pin for SPI communication
 * @param rst reset pin of the screen
 */
DFRobot_ST7789_240x320_HW_SPI screen(/*dc=*/TFT_DC,/*cs=*/TFT_CS,/*rst=*/TFT_RST);

DFRobot_SGP40    mySgp40;

DFRobot_EnvironmentalSensor environment(/*addr = */SEN050X_DEFAULT_DEVICE_ADDRESS, /*pWire = */&Wire);

float temperatureC, temperatureF, altitude, humidity, pressure;
uint16_t VOC, ultravoilet;
uint8_t smoke, CH4, Light;


String AirQuality="";

//Checks for new messages every 1 second.
const unsigned long BOT_MTBS = 1000; // mean time between scan messages
unsigned long bot_lasttime; // last time messages' scan has been done

//Check and Display Sensor Readings every 5 sec
const unsigned long sendInterval = 5000;
unsigned long previousTime = 0;


// Smoke Detector
uint8_t SmokePin = A1;
bool smokeDetectMonitor = false;

// Gas sensor
uint8_t GasPin = A2;
bool GasDetectMonitor = false;


void init_WiFi(){
  // attempt to connect to Wifi network:
  Serial.print("Connecting to Wifi SSID ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());
}


void init_EnvSensor(){
  Serial.println("Initializing ENV Sensor...");
  while(environment.begin() != 0){
    Serial.println(" Sensor initialize failed!!");
    delay(1000);
  }
  Serial.println("\nEnv Sensor initialize success!!");
}

void init_SGP40(){
  Serial.println("Initializing SGP40 Sensor...");
  /*
   * The preheating time of the sensor is 10s.
   * duration:Initialize the wait time. Unit: millisecond. Suggestion: duration > = 10000 ms
   */
  while(mySgp40.begin(/*duration = */10000) !=true){
    Serial.println("failed to init chip, please check if the chip connection is fine");
    delay(1000);
  }
  Serial.println("----------------------------------------------");
  Serial.println("sgp40 initialized successfully!");
  Serial.println("----------------------------------------------");
  /**
   * @brief  Set the temperature and humidity
   * @param  relativeHumidityRH  Current environmental relative humidity value, range 0-100, unit: %RH
   * @param  temperatureC  Current ambient temperature, range -10~50, unit: °C
   */
  //mySgp40.setRhT(/*relativeHumidity = */ 50, /*temperature = */ 20);
}

void getGasStatus(){
  uint16_t tmp = 0;
  uint16_t sensorValue = analogRead(GasPin);
  Serial.print("CH4 Gas Raw Reading = ");
  Serial.println(sensorValue);
  tmp = map(sensorValue, 430, 4096, 0, 100);
  if((tmp >= 50) && GasDetectMonitor){
    CH4 = tmp;
    bot.sendMessage(CHAT_ID, "Gas Leak Detected in the House !!", "");
  } else{
    CH4 = 0;
  }
  Serial.print("CH4 Gas Level = ");
  Serial.print(CH4);
  Serial.println(" %");
  Serial.println("\n");
}

void getSmokeStatus(){
  uint16_t tmp = 0;
  uint16_t sensorValue = analogRead(SmokePin);
  Serial.print("Smoke value Raw Reading = ");
  Serial.println(sensorValue);  
  tmp = map(sensorValue, 0, 4096, 0, 100);
  if((tmp >= 50) && smokeDetectMonitor){
    smoke = tmp;
    bot.sendMessage(CHAT_ID, "Smoke Detected in the House !!", "");
  } else{
    smoke = 0;
  }
    Serial.print("Smoke Level = ");
    Serial.print(smoke);
    Serial.println(" %");
    Serial.println("\n)");
}

void getENV_Readings(){
  Serial.println("ENV Sensor Readings :");
  //Print the data obtained from sensor
  Serial.println("-------------------------------");
  Serial.print("TempC: ");
  temperatureC = environment.getTemperature(TEMP_C);
  Serial.print(temperatureC);
  Serial.println(" ℃");

  Serial.print("TempF: ");
  temperatureF = environment.getTemperature(TEMP_F);
  Serial.print(temperatureF);
  Serial.println(" ℉");

  Serial.print("Humidity: ");
  humidity = environment.getHumidity();
  Serial.print(humidity);
  Serial.println(" %");

  Serial.print("Ultraviolet intensity: ");
  ultravoilet = environment.getUltravioletIntensity();
  Serial.print(ultravoilet);
  Serial.println(" mw/cm2");

  Serial.print("LuminousIntensity: ");
  Light = environment.getLuminousIntensity();
  Serial.print(Light);
  Serial.println(" lx");

  Serial.print("Atmospheric pressure: ");
  pressure = environment.getAtmospherePressure(KPA);
  Serial.print(pressure);
  Serial.println(" Khpa");

  Serial.print("Altitude: ");
  altitude = environment.getElevation();
  Serial.print(altitude);
  Serial.println(" m");
  Serial.println("-------------------------------");
  delay(500);
}

void getSGP40_Readings(){
  /**
   * @brief  Measure VOC index after humidity compensation
   * @note   VOC index can indicate the quality of the air directly. The larger the value, the worse the air quality.
   * @note       0-100，no need to ventilate, purify
   * @note       100-200，no need to ventilate, purify
   * @note       200-400，ventilate, purify
   * @note       400-500，ventilate, purify intensely
   * @return The VOC index measured, ranged from 0 to 500
   */
  uint16_t index = mySgp40.getVoclndex();
  VOC = index;
  if(index <= 100){
    AirQuality = "Excellent";
  } else if( (index > 100) && (index <= 200)){
    AirQuality = "Good";
  } else if( (index > 200) && (index <= 300)){
    AirQuality = "OK";
  } else if( (index > 300) && (index <= 400)){
    AirQuality = "Bad";
  } else if( (index > 400) && (index <= 500)){
    AirQuality = "Very Bad";
  }
  
  Serial.println("\nSGP40 Sensor Readings");
  Serial.println("-------------------------------");
  Serial.print("VOC index = ");
  Serial.println(VOC);
  Serial.print("Air Quality = ");
  Serial.println(AirQuality);
  Serial.println("-------------------------------");
  Serial.println("\n");
}

void draw_initScreen(){
  screen.fillScreen(COLOR_RGB565_BLACK);
  screen.drawRect(0, 0, 240, 320, COLOR_RGB565_CYAN);
  screen.fillRect(0, 0, 240, 30, COLOR_RGB565_CYAN);

  screen.setTextColor(COLOR_RGB565_BLACK);
  screen.setTextSize(2);
  screen.setCursor(18, 8);

  screen.println("House Environment");

  screen.setTextColor(COLOR_RGB565_RED);
  screen.setTextSize(2);
  screen.setCursor(15, 40);
  screen.println("Temp  = ");
  screen.setCursor(205, 40);
  screen.println("\367C ");

  screen.setTextColor(COLOR_RGB565_GREEN);
  screen.setCursor(15, 72);
  screen.println("Humi  = ");
  screen.setCursor(215, 72);
  screen.println("%");

  screen.setTextColor(COLOR_RGB565_ORANGE);
  screen.setCursor(15, 104);
  screen.println("VOC   = ");

  screen.setTextColor(COLOR_RGB565_SKYBLUE);
  screen.setCursor(15, 136);
  screen.println("Pres  = ");
  screen.setCursor(185, 136);
  screen.println("KhPa"); 

  screen.setTextColor(COLOR_RGB565_PINK);
  screen.setCursor(15, 168);
  screen.println("UV    = ");
  screen.setFont(&FreeMono9pt7b);
  screen.setTextSize(1);
  screen.setCursor(175, 178);
  screen.println("mJ/m2");

  screen.setFont();
  screen.setTextSize(2);
  screen.setTextColor(COLOR_RGB565_YELLOW);
  screen.setCursor(15, 200);
  screen.println("Light = ");
  screen.setCursor(195, 200);
  screen.println("klx");

  screen.setTextColor(COLOR_RGB565_MAGENTA);
  screen.setCursor(15, 232);
  screen.println("Gas   = ");
  screen.setCursor(215, 232);
  screen.println("%");

  screen.setTextColor(COLOR_RGB565_OLIVE);
  screen.setCursor(15, 264);
  screen.println("Smoke = ");
  screen.setCursor(215, 264);
  screen.println("%");

  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(15, 296);
  screen.println("AirQ  = ");
}

void display_readings(){

  screen.setTextWrap(false);
  screen.fillRect(105, 37, 67, 278, COLOR_RGB565_BLACK);

  screen.setTextColor(COLOR_RGB565_RED);
  screen.setCursor(110, 40);
  screen.println(temperatureC);

  screen.setTextColor(COLOR_RGB565_GREEN);
  screen.setCursor(110, 72);
  screen.println(humidity);

  screen.setTextColor(COLOR_RGB565_ORANGE);
  screen.setCursor(110, 104);
  screen.println(VOC);  

  screen.setTextColor(COLOR_RGB565_SKYBLUE);
  screen.setCursor(110, 136);
  screen.println(pressure);  

  screen.setTextColor(COLOR_RGB565_PINK);
  screen.setCursor(110, 168);
  screen.println(ultravoilet);  

  screen.setTextColor(COLOR_RGB565_YELLOW);
  screen.setCursor(110, 200);
  screen.println(Light);

  screen.setTextColor(COLOR_RGB565_MAGENTA);
  screen.setCursor(110, 232);
  screen.println(CH4);
  
  screen.setTextColor(COLOR_RGB565_OLIVE);
  screen.setCursor(110, 264);
  screen.println(smoke);

  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(110, 296);
  screen.println(AirQuality);
}

void handleNewMessages(int numNewMessages)
{
  Serial.print("handleNewMessages ");
  Serial.println(numNewMessages);
  for (int i = 0; i < numNewMessages; i++)
  {
    String chat_id = String(bot.messages[i].chat_id);
    if (chat_id != CHAT_ID )
    {
      bot.sendMessage(chat_id, "Unauthorized user", "");
    }
    else
    {
      String text = bot.messages[i].text;
      String from_name = bot.messages[i].from_name;
      if (from_name == "")
        from_name = "Guest";
      if (text == "/getReadings"){
        String readings = getReadings();
        bot.sendMessage(chat_id, readings, "");
      }

      if (text == "/EnableSmokeAlert")
      {  
          smokeDetectMonitor = true;
          bot.sendMessage(chat_id, "Smoke Detector - Enabled", "");
          Serial.println("Enable the Smoke Detector");
      }
      if (text == "/DisableSmokeAlert")
      {  
        smokeDetectMonitor = false;
        bot.sendMessage(chat_id, "Smoke Detector - Disabled", "");
        Serial.println("Disable the Smoke Detector");
      }

      if (text == "/EnableGasAlert") {
        GasDetectMonitor = true;
        bot.sendMessage(chat_id, "Gas Detector - Enabled", "");
        Serial.println("Enable the Gas Detector");
      }
    
      if (text == "/DisableGasAlert") {
        GasDetectMonitor = false;
        bot.sendMessage(chat_id, "Gas Detector - Disabled", "");
        Serial.println("Disable the Gas Detector");
      }


      if (text == "/start"){
        String welcome = "Welcome to Home Environment Assistant bot.\n\n";
        welcome += "Use the following commands to Get the Home Environment Status.\n\n";
        welcome += "/getReadings : Get Sensor Readings\n";
        welcome += "/EnableSmokeAlert : enables the Smoke Alert System\n";
        welcome += "/DisableSmokeAlert : disables the Smoke Alert System\n\n";
        welcome += "/EnableGasAlert : enables the Gas Alert System\n";
        welcome += "/DisableGasAlert : disables the Gas Alert System\n\n";
        bot.sendMessage(chat_id, welcome, "Markdown");
      }
    }
  }
}

String getReadings(){
  String message = "Temperature : " + String(temperatureC) + " ºC \n\n";
  message += "Humidity : " + String(humidity) + " % \n\n";
  message += "VOC Level : " + String(VOC) + "\n\n";
  message += "Pressure : " + String(pressure) + " khPa\n\n";
  message += "Ultravoilet Intensity : " + String(ultravoilet) + " mJ/m2\n\n";
  message += "Ambient Light Luminance : " + String(Light) + " klx\n\n";
  message += "LPG Gas/Methane CH4 Level : " + String(CH4) + " %\n\n";
  message += "Smoke Level : " + String(smoke) + " %\n\n";
  message += "Air Quality : " + AirQuality + "\n\n";
  return message;
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("Starting the House Environment Monitoring System...");
  init_EnvSensor();
  init_SGP40();
  screen.begin();
  draw_initScreen();
  init_WiFi();
}

void loop() {
  // put your main code here, to run repeatedly:
  unsigned long currentTime = millis();
  if (currentTime - previousTime >= sendInterval) {
      getSGP40_Readings();
      getENV_Readings();
      getGasStatus();
      getSmokeStatus();
      display_readings();
     /* Update the timing for the next time around */
      previousTime = currentTime;
  }

  if (millis() - bot_lasttime > BOT_MTBS)
  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages)
    {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    bot_lasttime = millis();
  }

}
