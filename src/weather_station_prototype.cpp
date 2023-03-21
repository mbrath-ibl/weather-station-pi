#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_BMP280.h>
#include "DHT.h"
#include <stdio.h>
#include <stdlib.h>

#include "pico/stdlib.h"

#include <hardware/flash.h>
uint32_t ints = save_and_disable_interrupts();

char *p = (char *)XIP_BASE;
char *first_byte = *p;
flash_range_erase (first_byte, 10);
flash_range_program (first_byte, buffer, 10);
restore_interrupts (ints);


const char ssid[] = "XXXXXXXXXXXXX";
const char password[]= "XXXXXXXXXXXXX";
WebServer server(80);

/////////////// MEASURED VALUES STORAGE ////////
#define MEASURE_BUFFER_SIZE 50
float valuesTemperature[MEASURE_BUFFER_SIZE];
int valuesTemperaturePtr = 0;
float valuesHumidity[MEASURE_BUFFER_SIZE];
int valuesHumidityPtr = 0;
float valuesPressure[MEASURE_BUFFER_SIZE];
int valuesPressurePtr = 0;

void initBuffers(){
  for(int i = 0; i < MEASURE_BUFFER_SIZE; i++){
    valuesPressure[i] = 0;
    valuesHumidity[i] = 0;
    valuesTemperature[i] = 0;
  }
}

void  bufferWrite(float storage[MEASURE_BUFFER_SIZE], int &ptr, float value){
  storage[ptr % MEASURE_BUFFER_SIZE] = value;
  ptr++; 
}

void  bufferToJson(float storage[MEASURE_BUFFER_SIZE], int &ptr, char* outBuffer, int bufferSize){
  int stringWritePtr=1;
  outBuffer[0]='[';
  for(int i = 0; i < MEASURE_BUFFER_SIZE; i++){
    // iterate back
    int idx = (ptr - 1 - i) %MEASURE_BUFFER_SIZE;
    stringWritePtr += sprintf(outBuffer + stringWritePtr, "%.1f,", storage[idx]);
  }
  outBuffer[stringWritePtr - 1] = ']';
  outBuffer[stringWritePtr] = '\0';
}

//////////////// DHT GLOBAL ////////////////////
#define DHTPIN 13
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

//////////////// BMP GLOBAL ////////////////////
#undef BMP280_ADDRESS
#define BMP280_ADDRESS 0x76
Adafruit_BMP280 bmp; // I2C

//////////////// LCD GLOBAL ////////////////////
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels
#define SCREEN_NUMBER_OF_LINES 3
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // See datasheet for Address
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//////////////// LCD ////////////////////
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

void drawText(char text[SCREEN_NUMBER_OF_LINES][25]) {
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

//////////////// DHT ////////////////////
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
  Serial.println(t);
  return t;
}

float getHumidity(){
  float h = dht.readHumidity();
  Serial.println(h);
  return h;
}

float getPressure(){
  float p = bmp.readPressure();
  Serial.println(p);
  return p;
}

//////////////// WIFI ///////////////////
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

//////////////// SERVER ///////////////////
void initServer(){
  server.begin();
  server.on("/", handleMain);
  server.on("/temperature", handleTemperature);
  server.on("/humidity", handleHumidity);
  server.on("/pressure", handlePressure);
  server.onNotFound(handleNotFound);
  // post request example returns what was passed
  server.on(
    "/upload",
    HTTP_POST,
    [] { 
      PROGMEM char buffer[640];
      server.arg("page").toCharArray(buffer, 640);

      server.sendHeader("Connection", "close");
      server.send(200, "text/plain", buffer); 
    }
  );
  server.begin();
  server.enableCORS(true);
  Serial.println("HTTP server started");
}

void handleMain(){
  char html[] = "<!DOCTYPE html>\r\n<html>\r\n<head>\r\n<script src=\"https://ajax.googleapis.com/ajax/libs/jquery/3.6.3/jquery.min.js\">\r\n<body style=\"padding-top:5px; background:#000\">\r\n<div class=\"container\">\r\n  \r\n </div>\r\n<!-- Optional JavaScript -->\r\n<!-- jQuery first, then Popper.js, then Bootstrap JS -->\r\n<script src=\"https://code.jquery.com/jquery-3.3.1.slim.min.js\" integrity=\"sha384-q8i/X+965DzO0rT7abK41JStQIAqVgRVzpbzo5smXKp4YfRvH+8abtTE1Pi6jizo\" crossorigin=\"anonymous\"></script>\r\n<script src=\"https://cdnjs.cloudflare.com/ajax/libs/popper.js/1.14.7/umd/popper.min.js\" integrity=\"sha384-UO2eT0CpHqdSJQ6hJty5KVphtPhzWj9WO1clHTMGa3JDZwrnQq4sF86dIHNDz0W1\" crossorigin=\"anonymous\"></script>\r\n<script src=\"https://stackpath.bootstrapcdn.com/bootstrap/4.3.1/js/bootstrap.min.js\" integrity=\"sha384-JjSmVgyd0p3pXB1rRibZUAYoIIy6OrQ6VrjIEaFf/nJGzIxFDsf4x0xIM+B07jRM\" crossorigin=\"anonymous\"></script></script>\r\n<script>\r\n$(document).ready(function(){\r\n  $(\"button\").click(function(){\r\n\t\r\n\r\n      $.ajax({\r\n          url: \"http://10.72.69.101/temperature\",\r\n          type: \'GET\',\r\n          dataType: \'json\',\r\n          cors: true ,\r\n          contentType:\'application/json\',\r\n          secure: true,\r\n                    headers: {\r\n                        \'Access-Control-Allow-Origin\': \'*\',\r\n                    },\r\n          beforeSend: function (xhr) {\r\n              xhr.setRequestHeader (\"Authorization\", \"Basic \" + btoa(\"\"));\r\n          },\r\n          success: function (data){\r\n            console.log(data.organizations[0].name);\r\n            var organisation = data.organizations[0].name;\r\n            $(\"#company\").text(organisation);\r\n          }\r\n        })\r\n\r\n\r\n  });\r\n});\r\n\r\n\r\n\r\n</script>\r\n</head>\r\n<body>\r\n\r\n<button>Send an HTTP GET request to a page and get the result back</button>\r\n\r\n</body>\r\n</html>";
  server.send(200, "text/html", html);
}

void handleUpload(){
  server.send(200, "text/html", "upload");
}

void handleTemperature(){
  char retBuff[20];
  sprintf(retBuff, "[%f]", getTemperature());
  server.send(200, "text/html", retBuff);
}

void handleHumidity(){
  char retBuff[20];
  sprintf(retBuff, "[%f]", getHumidity());
  server.send(200, "text/html", retBuff);
}

void handlePressure(){
  char retBuff[20];
  sprintf(retBuff, "[%f]", getHumidity());
  server.send(200, "text/html", retBuff);
}

void handleNotFound(){
  server.send(404, "text/plain", "Not found");
}

void printToLcd(){
  char lcdData [SCREEN_NUMBER_OF_LINES][25]{0};
  unsigned int ipBuffLen = WiFi.localIP().toString().length()+1;
  char ip[ipBuffLen]={0};
  WiFi.localIP().toString().toCharArray(ip, ipBuffLen);
  sprintf(lcdData[0],"Temp:%.1fC Hum:%.1f%s", getTemperature(), getHumidity(), "%");
  sprintf(lcdData[1],"Pressure: %.1f Pa", getPressure());
  sprintf(lcdData[2],"IP:  %s",ip);

  drawText(lcdData);
}

//////////////// SETUP ///////////////////
void setup() {
  Serial.begin(9600);
  initLcd();
  initSensors();
  initWifi();
  initServer();
  initBuffers();
}
//////////////// EXECUTION LOOP ///////////////////
void loop() {
  server.handleClient();


  // Call functions after certain amount of time
  static int previousMiliseconds = millis();
  if (millis() - previousMiliseconds >= 5000) {
    previousMiliseconds = millis();
    printToLcd();
    bufferWrite(valuesTemperature, valuesTemperaturePtr, getTemperature());
    bufferWrite(valuesHumidity, valuesHumidityPtr, getTemperature());
    bufferWrite(valuesPressure, valuesPressurePtr, getTemperature());
    Serial.println("xxxxxxxxxxxxxxxxxxxxx");
    char buffer[4000];
    bufferToJson(valuesTemperature, valuesTemperaturePtr, buffer, 4000);
    Serial.println(buffer);
  }
}