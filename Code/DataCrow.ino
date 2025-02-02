/*  
 ** 
     DataCrow
     version 1.0
     A first experiment with Elecrow CrowPanel 3.7''.
     Code with comments :-)
     
     Loiseau créatif - https://loiseaucreatif.com - 2025
     https://github.com/loiseaucreatif/DataCrow

     Discover how I used the Elecrow CrowPanel 3.7", an e-ink display with integrated ESP32-S3, to show dynamic data.
     This DIY project fetches online the Bitcoin price and an inspirational quote, then measures local temperature and humidity with a DHT22 sensor.
     Link to the product: https://www.elecrow.com/crowpanel-esp32-3-7-e-paper-hmi-display-with-240-416-resolution-black-white-color-driven-by-spi-interface.html

     I'm not an hardcore developer, there's certainly improvements to make.
     This experiment can be a source of inspiration and information for Makers.
     Feel free to interact with me ;-)  
    
     Enjoy!
**
*/

#include <Arduino.h>
#include "EPD.h"
#include "EPD_GUI.h"
#include "DataCrow_GFX.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "DHT.h"

// Define the DHT22 sensor variables
// Sensor connected to the GPIO pin 21 of the ESP32-S3
#define DHTPIN 21
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Define the SSID and password required for WiFi connection
String ssid = "INSERT_YOUR_WIFI_SSID";
String password = "INSERT_YOUR_WIFI_PASSWORD";

// Define a buffer for displaying images
// Sized for the full screen dimension: 416 * 240 / 8
// The result is divided by 8 because the images are in 1 bit (black and white), so we are storing 8 pixels in 1 byte.
uint8_t ImageFull[12480];

// Bitcoin variables
int btcX = 128; //Starting horizontal axis
int btcY = 24;  //Starting ordinate
int btcFont = 48;
String Txt_Bitcoin;
char buffer[40];
float bitcoinPrice;
int httpResponseCode;
String payload;
// CoinGecko API URL (in order to get the Bitcoin value)
// API structure : https://docs.coingecko.com/reference/introduction
const char* apiBitcoinUrl = "https://api.coingecko.com/api/v3/simple/price?ids=bitcoin&vs_currencies=usd";

// Quote variables
int httpCode;
size_t capacity;
String thequote;
String theauthor;
String quote;
// ZenQuotes API URL (in order to get a short quote)
// API structure : https://docs.zenquotes.io/zenquotes-documentation/#api-structure
const char* apiQuoteUrl = "https://zenquotes.io/api/random";

// Time counter variables
// interval: waiting time before each data refresh via APIs
const unsigned long interval = 5 * 60 * 1000; // 5 minutes in milliseconds (300000 ms)
// interval2: Progress bar refresh frequency, which gives a visual indicator of the time before the next data refresh
const unsigned long interval2 = 10000; // 10 seconds
unsigned long previousMillis = 0;
unsigned long currentMillis = 0;
bool start = true; // flag to find out if this is the microcontroller's first start-up

// Progress bar indicator variables
const int thelineX1 = 182;
const int thelineX2 = 234;
const int totalWidth = thelineX2 - thelineX1;
const int thelineY = 85;
int actualWidth;



void setup() {
  //Serial.begin(115200);  // Initialize serial communication with a baud rate of 115200 (debug/prototyping mode)
 
  // Start connecting to the WiFi network with the specified SSID
  WiFi.begin(ssid, password);

  // Initialize the DHT22 sensor
  dht.begin();

  // Initialize and set the screen power pin to high to supply power
  pinMode(7, OUTPUT);
  digitalWrite(7, HIGH);

  // Set the POWER light pin as output mode
  pinMode(41, OUTPUT);

  EPD_GPIOInit();
  Paint_NewImage(ImageFull, EPD_W, EPD_H, Rotation, WHITE);
  Paint_Clear(WHITE); // Clear the canvas

  EPD_FastInit();
  EPD_Display_Clear();
  EPD_Update(); 

  // BootSplash screen
  EPD_FastInit();
  EPD_ShowPicture(0, 0, EPD_W, EPD_H, img_full_splash, WHITE);
  EPD_Display(ImageFull);
  EPD_Update();

  // Wait until the WiFi connection is successful
  while (WiFi.status()!= WL_CONNECTED) {
    // Flashing the power led
    digitalWrite(41, HIGH);
    delay(500);
    digitalWrite(41, LOW);
    delay(500);
  }
  digitalWrite(41, LOW);
  delay(5000); // Just to enjoy the splash screen and stabilize the WiFi

  //Serial.println("WiFi connected");  // Print WiFi connection success message.
  //Serial.println("IP address: ");  // Print IP address information.
  //Serial.println(WiFi.localIP());  // Print the locally assigned IP address.

  // Display main dashboard waiting for first datas
  Paint_NewImage(ImageFull, EPD_W, EPD_H, 180, WHITE);
  Paint_Clear(WHITE); // Clear the canvas
  EPD_FastInit();
  EPD_ShowPicture(0, 0, 416, 16, img_part_hud1, WHITE);
  EPD_ShowString(118, 106, "Getting data...", 24, BLACK);
  EPD_Display(ImageFull);
  EPD_Update();  // Update the screen display
}



void loop() {
  currentMillis = millis(); // Get the current time

  // Check if 'interval' milliseconds have elapsed or first start
  if ((currentMillis - previousMillis >= interval) || start==true) {
    previousMillis = currentMillis; // Reset the counter
    start = false;
    
    // Code to be run every 'interval' milliseconds

    draw_dashboard();

    dht_monitoring();

    if (WiFi.status() == WL_CONNECTED) {

      HTTPClient http;

    //  
    // Bitcoin data get & display in image buffer
    //  
      http.begin(apiBitcoinUrl);
      httpResponseCode = http.GET();

      if (httpResponseCode == 200) { // 200 = OK
        payload = http.getString();

        // Parse JSON
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, payload);

        if (!error) {
          bitcoinPrice = doc["bitcoin"]["usd"];
          memset(buffer, 0, sizeof(buffer));
          snprintf(buffer, sizeof(buffer), "%s$", String(int(bitcoinPrice))); // Format bitcoin price as a string
          EPD_ShowString(btcX, btcY, buffer, btcFont, BLACK);
        } else {
          EPD_ShowString(btcX, btcY+16, "JSON error", 16, BLACK);
        }
      } else {
        EPD_ShowString(btcX, btcY+16, "HTTP error", 16, BLACK);
      }
      http.end(); // End of the http request


  //
  // Quote data get
  //
      http.begin(apiQuoteUrl);
      httpCode = http.GET();

      if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
          quote = http.getString();

          capacity = JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(2) + 200; // Calculating the memory needed
          DynamicJsonDocument docquote(capacity);
          DeserializationError error = deserializeJson(docquote, quote);

          if (!error) {
            thequote = docquote[0]["q"].as<String>();
            theauthor = docquote[0]["a"].as<String>();

            Part_Text_Display(thequote.c_str(), 16, 164, 16, BLACK, 400, 212);
            Part_Text_Display(theauthor.c_str(), 40, 220, 16, BLACK, 400, 236);
          } else {
            Part_Text_Display("JSON error", 40, 2220, 16, BLACK, 400, 236);
          }
        }
      } else {
          Part_Text_Display("HTTP error", 40, 220, 16, BLACK, 400, 236);
      }
      http.end();


    } else {
      EPD_ShowString(btcX, btcY, "No WiFi!", btcFont, BLACK);
    }

    EPD_FastInit();
    EPD_Display(ImageFull);
    EPD_Update();  // Update the screen display
  }

  // Display the progress bar indicator
  // Partial refresh of the screen
  // Check if 'interval' milliseconds are not elapsed and it's not the first start
  if ((currentMillis - previousMillis < interval) && start==false) {
    actualWidth = (currentMillis - previousMillis) * totalWidth / interval; // Calculating the width of the line
    EPD_PartInit();
    EPD_DrawLine(thelineX1, thelineY, thelineX1+actualWidth, thelineY, BLACK); // Draw a horizontal line
    EPD_Display(ImageFull);
    EPD_Update();
  }

}



void clear_all()
{
  Paint_NewImage(ImageFull, EPD_W, EPD_H, Rotation, WHITE);
  Paint_Clear(WHITE);
  EPD_FastInit();
  EPD_Display_Clear();
  EPD_Update();
}


void draw_dashboard()
{
  // Display the design of the main dashboard
  Paint_Clear(WHITE); // Clear the canvas
  EPD_ShowPicture(0, 0, 416, 16, img_part_hud1, WHITE);
  EPD_ShowPicture(btcX-56, btcY, 48, 48, img_part_bitcoin, WHITE);
  EPD_ShowPicture(0, 80, 416, 16, img_part_hud2, WHITE);
  EPD_ShowPicture(42, 98, 40, 40, img_part_temp, WHITE);
  EPD_ShowPicture(258, 98, 40, 40, img_part_hum, WHITE);
  EPD_ShowPicture(0, 146, 416, 16, img_part_hud3, WHITE);
  EPD_ShowPicture(16, 220, 16, 16, img_part_author, WHITE);
}



void dht_monitoring() {
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  EPD_ShowString(94, 92, String((int)t).c_str(), btcFont, BLACK);
  EPD_ShowString(308, 92, String((int)h).c_str(), btcFont, BLACK);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    EPD_ShowString(94, 92, "--", btcFont, BLACK);
    EPD_ShowString(308, 92, "--", btcFont, BLACK);
    return;
  }
}



/*
*--Elecrow code--
*---------Function description: Display text content locally------------
*----Parameter introduction:
      content：Text content
      startX：Starting horizontal axis
      startY：Starting ordinate
      fontSize：font size
      color：font color
      endX：End horizontal axis
      endY：End vertical axis
*/
void Part_Text_Display(const char* content, int startX, int startY, int fontSize, int color, int endX, int endY) {
    int length = strlen(content);
    int i = 0;
    char line[(endX - startX) / (fontSize/2) + 1]; //Calculate the maximum number of characters per line based on the width of the area
    int currentX = startX;
    int currentY = startY;
    int lineHeight = fontSize;

    while (i < length) {
        int lineLength = 0;
        memset(line, 0, sizeof(line));

        //Fill the line until it reaches the width of the region or the end of the string
        while (lineLength < (endX - startX) / (fontSize/2) && i < length) {
            line[lineLength++] = content[i++];
        }

        // If the current Y coordinate plus font size exceeds the area height, stop displaying
        if (currentY + lineHeight > endY) {
            break;
        }
        // Display this line
        EPD_ShowString(currentX, currentY, line, fontSize, color); 

        // Update the Y coordinate for displaying the next line
        currentY += lineHeight;

        // If there are still remaining strings but they have reached the bottom of the area, stop displaying them
        if (currentY + lineHeight > endY) {
            break;
        }
    }
}