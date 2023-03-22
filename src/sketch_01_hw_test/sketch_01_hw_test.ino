#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BMP280.h>
#include "DHT.h"
#include <stdio.h>
#include <stdlib.h>
#define MEASURE_BUFFER_SIZE 50


static int milisecondsFromLastMeasurement = 999999;
#define SAMPLING_PERIOD_SEC 60*15

//////////////// TEMP HUMIDITY SENSOR GLOBAL ////
//init dht11 TODO

//////////////// PRESSURE SENSOR GLOBAL /////////
#undef BMP280_ADDRESS
#define BMP280_ADDRESS 0x76
Adafruit_BMP280 bmp; // I2C

//////////////// DISPLAY GLOBAL ////////////////////
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define SCREEN_NUMBER_OF_LINES 3
#define SCREEN_NUMBER_OF_CHARS 50
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // See datasheet for Address
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//////////////// DISPLAY ////////////////////
void initLcd(){
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  display.clearDisplay();
  // rotate lcd by 180 degrees
  display.setRotation(2);
  display.dim(true);
  display.display();
}

void drawText(char text[SCREEN_NUMBER_OF_LINES][SCREEN_NUMBER_OF_CHARS]) {
  //adds diagonal movement to screen for longer lifetime of display
  static int lineOffset = 0;
  if(lineOffset++ == 2)
    lineOffset = 0;

  display.clearDisplay();
  display.setTextSize(1); // Draw 2X-scale text
  display.setTextColor(SSD1306_WHITE);
  for(unsigned int i = 0; i < SCREEN_NUMBER_OF_LINES; i++)
  {
    display.setCursor(lineOffset + 0, lineOffset + i * 10);
    display.println(text[i]);
  }
  display.display();      // Show initial text
  delay(100);
}

//////////////// TEMPERATURE HUMIDITY AND PRESSURE SENSOR ////////////////////
void initSensors(){
  // init dht TODO

  // init BMP280
  unsigned status;
  status = bmp.begin(BMP280_ADDRESS);
  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                  Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                  Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                  Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                  Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
  if (!status) {
    Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                     "try a different address!"));
  }
}

// TODO implement retrieval of temperature and humidity

float getPressure(){
  float p = bmp.readPressure();
  return p;
}

////////// HELPER FUNCTIONS ///////////////////////////////////////////////
void printToLcd(){
  char lcdData [SCREEN_NUMBER_OF_LINES][SCREEN_NUMBER_OF_CHARS]{0};
  //TODO print Temperature And Humidity to first line
  sprintf(lcdData[0],"TEST");
  sprintf(lcdData[1],"Pressure: %.1f Pa", getPressure());
  sprintf(lcdData[2],"TEST");

  drawText(lcdData);
}

//////////////// SETUP /////////////////////////////////////////////////////
void setup() {
  Serial.begin(9600);
  initLcd();
  initSensors();
}

//////////////// EXECUTION LOOP ///////////////////////////////////////////
void loop() {
  // Call functions after SAMPLING_PERIOD_SEC
  if (millis() - milisecondsFromLastMeasurement >= /*SAMPLING_PERIOD_SEC **/ 1000) {
    milisecondsFromLastMeasurement = millis();
    // TODO log Temperature and Humidity to serial
    Serial.println(getPressure());
  }

  // Call functions after 5 seconds
  static int displayRefreshTimer = millis();
  if (millis() - displayRefreshTimer >= 5000) {
    displayRefreshTimer = millis();
    printToLcd();
  }
}
