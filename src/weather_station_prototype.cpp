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

const char ssid[] = "xxxxxxxxxxxxxxxxx";
const char password[]= "xxxxxxxxxxxxxxxxxxxxxxxxxxx";
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

void arrayOfValuesToJsonResponseHelper(char valuesJson[MEASURE_BUFFER_SIZE * 50]){
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
  server.on("/temperature", handleTemperature);
  server.on("/humidity", handleHumidity);
  server.on("/pressure", handlePressure);
  server.on("/temperaturearray", handleTemperatureArray);
  server.on("/humidityarray", handleHumidityArray);
  server.on("/pressurearray", handlePressureArray);
  server.onNotFound(handleNotFound);

  // post request example returns what was passed as page arg
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
  char html[] = "<!DOCTYPE html>\r\n<html>\r\n<head>\r\n\t<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\r\n\t<style>\r\n\tbody {\r\n\t\tbackground: #CDD;\r\n\t}\r\n\r\n\t.content {\r\n\t\tmax-width: 500px;\r\n\t\tmargin: auto;\r\n\t\tbackground: white;\r\n\t\tpadding: 10px;\r\n\t}\r\n\t</style>\r\n\t<script src=\"https://cdnjs.cloudflare.com/ajax/libs/Chart.js/2.5.0/Chart.min.js\"></script>\r\n\t<script src=\"https://code.jquery.com/jquery-3.1.1.min.js\"></script>\r\n\t<script>\r\n\tfunction getData(color, quantityName, path){\r\n\t    //const baseurl = \"http://10.72.69.101/\";\r\n\t    const baseurl = \"\";\r\n\r\n\t\t$.ajax({\r\n\t\t\turl: baseurl + path,\r\n\t\t\ttype: \'GET\',\r\n\t\t\tdataType: \'json\',\r\n\t\t\tcors: true ,\r\n\t\t\tcontentType:\'application/json\',\r\n\t\t\tsecure: true,\r\n\t\t\theaders: {\r\n\t\t\t\t\'Access-Control-Allow-Origin\': \'*\',\r\n\t\t\t},\r\n\t\t\tbeforeSend: function (xhr) {\r\n\t\t\t  xhr.setRequestHeader (\"Authorization\", \"Basic \" + btoa(\"\"));\r\n\t\t\t},\r\n\t\t\tsuccess: function (data){\r\n\t\t\t\tconsole.log(JSON.stringify(data));\r\n\t\t\t\tvar yValues = data[\"values\"];\r\n\t\t\t\tvar xValues = new Array();\r\n\t\t\t\t\r\n\t\t\t\t//construct X axis labels\r\n                var samplingPeriodSec = data[\"samplingPeriodSec\"];\r\n\t\t\t\tvar lastSampleSec = data[\"lastSampleSec\"];\r\n\t\t\t\tvar today = new Date();\r\n\t\t\t\ttoday.setSeconds(today.getSeconds() - lastSampleSec);\r\n\t\t\t\tfor (let i = 0; i < yValues.length; i++) {\r\n\t\t\t\t\ttoday.setSeconds(today.getSeconds() - samplingPeriodSec);\r\n\t\t\t\t\tconsole.log(today)\r\n\t\t\t\t\txValues.push(today.toLocaleTimeString(\'en-GB\'));\r\n\t\t\t\t}\r\n\t\t\t\tconsole.log(xValues)\r\n\t\t\t\tnew Chart(quantityName, {\r\n\t\t\t\t\ttype: \"line\",\r\n\t\t\t\t\tdata: {\r\n\t\t\t\t\t\tlabels: xValues,\r\n\t\t\t\t\t\tdatasets: [{\r\n\t\t\t\t\t\t\tlabel: quantityName,\r\n\t\t\t\t\t\t\tdata: data[\"values\"],\r\n\t\t\t\t\t\t\tborderColor: color,\r\n\t\t\t\t\t\t\tfill: false\r\n\t\t\t\t\t\t}]\r\n\t\t\t\t\t},\r\n\t\t\t\t\toptions: {\r\n\t\t\t\t\t\tlegend: {\r\n\t\t\t\t\t\t\tdisplay: true\r\n\t\t\t\t\t\t}\r\n\t\t\t\t\t}\r\n\t\t\t\t});\r\n\t\t\t}\r\n\t\t})\r\n\t}\r\n\t\r\n\t$(document).ready(function(){\r\n\t  $(\"button\").click(function(){\r\n          getData(\"red\", \"Temperature\", \"temperaturearray\")\r\n\t\t  getData(\"blue\", \"Humidity\", \"humidityarray\")\r\n\t\t  getData(\"green\", \"Pressure\", \"pressurearray\")\r\n\t  });\r\n\t});\r\n\t\r\n    function getDataPeriodic(){\r\n          getData(\"red\", \"Temperature\", \"temperaturearray\")\r\n\t\t  getData(\"blue\", \"Humidity\", \"humidityarray\")\r\n\t\t  getData(\"green\", \"Pressure\", \"pressurearray\")\r\n\t}\r\n\tgetDataPeriodic()\r\n\tsetInterval(getDataPeriodic, 30000);\r\n\r\n\t</script>\r\n</head>\r\n<body>\r\n\t<button>R</button>\r\n    <div class=\"content\">\r\n\t\t<canvas id=\"Temperature\" style=\"width:100%;max-width:600px\"></canvas>\r\n\t\t<canvas id=\"Humidity\" style=\"width:100%;max-width:600px\"></canvas>\r\n\t\t<canvas id=\"Pressure\" style=\"width:100%;max-width:600px\"></canvas>\r\n\t</div>\r\n</body>\r\n</html>";
  server.send(200, "text/html", html);
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

void handleTemperatureArray(){
  char valuesBuffer[MEASURE_BUFFER_SIZE * 100];
  valuesTemperature.bufferToJson(valuesBuffer);
  arrayOfValuesToJsonResponseHelper(valuesBuffer);
}

void handleHumidityArray(){
  char valuesBuffer[MEASURE_BUFFER_SIZE * 100];
  valuesHumidity.bufferToJson(valuesBuffer);
  arrayOfValuesToJsonResponseHelper(valuesBuffer);
}

void handlePressureArray(){
  char valuesBuffer[MEASURE_BUFFER_SIZE * 100];
  valuesPressure.bufferToJson(valuesBuffer);
  arrayOfValuesToJsonResponseHelper(valuesBuffer);
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
    //valuesTemperature.printVals();
    //char buffer[MEASURE_BUFFER_SIZE * 100];
    //valuesTemperature.bufferToJson(buffer);
    //Serial.println(buffer);
  }

  // Call functions after 5 seconds
  static int displayRefreshTimer = millis();
  if (millis() - displayRefreshTimer >= 5000) {
    displayRefreshTimer = millis();
    printToLcd();
  }
}
