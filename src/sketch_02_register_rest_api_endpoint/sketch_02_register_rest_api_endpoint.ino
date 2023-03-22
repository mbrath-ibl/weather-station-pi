#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BMP280.h>
#include "DHT.h"
#include <stdio.h>
#include <stdlib.h>
#define MEASURE_BUFFER_SIZE 50


static int milisecondsFromLastMeasurement = 999999;
#define SAMPLING_PERIOD_SEC 60*15
int getSecondsFromLastMeasurement(){return milisecondsFromLastMeasurement/1000; };

const char ssid[] = "xxxxxxxxxxx";
const char password[]= "xxxxxxxxxxxx";
WebServer server(80);

//////////////// TEMP HUMIDITY SENSOR GLOBAL ////
#define DHTPIN 13
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

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

//////////////// TEMPERATURE HUMIDITY SENSOR ////////////////////
void initSensors(){
  // init dht
  dht.begin();
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

float getTemperature(bool retFarenheit=false){
  float t = dht.readTemperature(retFarenheit);
  return t;
}

float getHumidity(){
  float h = dht.readHumidity();
  return h;
}

float getPressure(){
  float p = bmp.readPressure();
  return p;
}

//////////////// WIFI //////////////////////////////////
void initWifi(){
  // Operate in WiFi Station mode
  WiFi.mode(WIFI_STA);
  // Start WiFi with supplied parameters
  WiFi.begin(ssid, password);
    // Print periods on monitor while establishing connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    delay(500);
  }
  // Connection established
  Serial.println("");
  Serial.print("Pico W is connected to WiFi network ");
  Serial.println(WiFi.SSID());
  // Print IP Address
  Serial.print("Assigned IP Address: ");
  Serial.println(WiFi.localIP());
}

/////////////// MEASURED VALUES STORAGE ////////
class MeasureBuffer
{
  float values[MEASURE_BUFFER_SIZE];
  int ptr;
public:
  void clear(){
    for(int i = 0; i < MEASURE_BUFFER_SIZE; i++){
      values[i] = 0;
    }
  }

  void  write(float value){
    values[ptr] = value;
    ptr = (ptr + 1) % MEASURE_BUFFER_SIZE; 
  }

  int getPrev(int ptr){
    int idx = ptr - 1;
    if (idx < 0)
        idx = MEASURE_BUFFER_SIZE - 1;
    return idx;
  }

  void  bufferToJson(char* outBuffer){
    int stringWritePtr=1;
    outBuffer[0]='[';
    int idx = getPrev(ptr);
    for(int i = 0; i < MEASURE_BUFFER_SIZE; i++){
      stringWritePtr += sprintf(outBuffer + stringWritePtr, "%.1f, ", values[idx]);
      idx = getPrev(idx);
    }
    outBuffer[stringWritePtr - 2] = ']';
    outBuffer[stringWritePtr - 1] = '\0';
  } 

  void printVals(){
    int idx = getPrev(ptr);
    for(int i = 0; i < MEASURE_BUFFER_SIZE; i++){
      Serial.print(values[idx]);
      Serial.print(" ");
      idx = getPrev(ptr);
    }
    Serial.println(" ");
  }
};

MeasureBuffer valuesTemperature;
MeasureBuffer valuesHumidity;
MeasureBuffer valuesPressure;

// returns json object as http response with additional data
void jsonArrayOfValuesToJsonResponseHelper(char valuesJson[MEASURE_BUFFER_SIZE * 50]){
  char returnBuffer[MEASURE_BUFFER_SIZE * 100];
  sprintf(
    returnBuffer,
    "{\"samplingPeriodSec\":%d, \"lastSampleSec\": %d, \"values\": %s}",
    SAMPLING_PERIOD_SEC,
    getSecondsFromLastMeasurement(),
    valuesJson
  );

  server.send(200, "text/html", returnBuffer);
}


//////////////// SERVER ////////////////////////////////
void initServer(){
  server.begin();
  server.on("/", handleMain);
  //TODO register /temperature
  //TODO register /temperaturearray
  server.onNotFound(handleNotFound);


  server.begin();
  server.enableCORS(true);
  Serial.println("HTTP server started");
}

void handleMain(){
  char html[] = "TEST MAIN";
  server.send(200, "text/html", html);
}

// TODO return http response by server.send(200, "text/html", retBuff); with temperature value
void handleTemperature(){
}

// TODO return array of meesured values as json from valuesTemperature 
//      use jsonArrayOfValuesToJsonResponseHelper
void handleTemperatureArray(){
}

void handleNotFound(){
  server.send(404, "text/plain", "Not found");
}

void printToLcd(){
  char lcdData [SCREEN_NUMBER_OF_LINES][SCREEN_NUMBER_OF_CHARS]{0};
  unsigned int ipBuffLen = WiFi.localIP().toString().length()+1;
  char ip[ipBuffLen]={0};
  WiFi.localIP().toString().toCharArray(ip, ipBuffLen);
  sprintf(lcdData[0],"Temp:%.1fC Hum:%.1f%s", getTemperature(), getHumidity(), "%");
  sprintf(lcdData[1],"Pressure: %.1f Pa", getPressure());
  sprintf(lcdData[2],"IP:  %s",ip);

  drawText(lcdData);
}

//////////////// SETUP /////////////////////////////////////////////////////
void setup() {
  Serial.begin(9600);
  initLcd();
  initSensors();
  initWifi();
  initServer();
  valuesTemperature.clear();
  valuesPressure.clear();
  valuesHumidity.clear();
}
//////////////// EXECUTION LOOP ///////////////////////////////////////////
void loop() {
  server.handleClient();

  // Call functions after SAMPLING_PERIOD_SEC
  if (millis() - milisecondsFromLastMeasurement >= /*SAMPLING_PERIOD_SEC **/ 1000) {
    milisecondsFromLastMeasurement = millis();
    valuesTemperature.write(getTemperature());
    valuesHumidity.write(getHumidity());
    valuesPressure.write(getPressure());
  }

  // Call functions after 5 seconds
  static int displayRefreshTimer = millis();
  if (millis() - displayRefreshTimer >= 5000) {
    displayRefreshTimer = millis();
    printToLcd();
  }
}
