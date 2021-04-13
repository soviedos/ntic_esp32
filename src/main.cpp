/**
 * @Author: Sergio Oviedo Seas
 * @Date:   2021-03-07 18:00:51
 * @Last Modified by:   Sergio Oviedo Seas
 * @Last Modified time: 2021-04-01 14:50:20
 */

/*
  ## Hardware Connections (ESP32 Arduino):
  -VIN = 3.3V
  -GND = GND
  -SDA = 21 (or SDA)
  -SCL = 22 (or SCL)

*/

// Libraries required
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <ArduinoJson.h>
#include "MAX30105.h" // sparkfun MAX3010X library
#include "heartRate.h"
#include "credentials.h"

#define I2C_SDA 21
#define I2C_SCL 22

// Azure IoT Hub Credentials
#define IOT_HUB_NAME IOT_HUB_NAME_NTic
#define DEVICE_NAME DEVICE_NAME_NTic
#define SAS_TOKEN SAS_TOKEN_NTic

// WiFi Credentials
const char* ssid     = SSID_SECRET;     // your network SSID (name of wifi network)
const char* password = PASS_SECRET; // your network password

// WiFi  object instance
WiFiClientSecure client;

// Created with openssl s_client -showcerts -connect 
// and by picking the root certificate
const char* root_ca = ROOT_CERT; // Root Certificate for Azure IoT Hub

// MAX30102 sensor instance
MAX30105 particleSensor;

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Variables to save date and time
String formattedDate;
String dayStamp;
String timeStamp;

// Heart Rate variables
const byte RATE_SIZE = 16; // Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; // Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; // Time at which the last beat occurred
float beatsPerMinute;
int beatAvg;
long countCycles = 0;
String beatAvgString;

// SpO2 variables
double avered = 0; 
double aveir = 0;
double sumirrms = 0;
double sumredrms = 0;
int i = 0;
int Num = 100;// calculate SpO2 by this sampling interval
double ESpO2 = 0;// initial value of estimated SpO2
double FSpO2 = 0.7; // filter factor for estimated SpO2
double frate = 0.95; // low pass filter for IR/red LED value to eliminate AC component
uint32_t ir, red , green;
double fred, fir;
double SpO2 = 0; // raw SpO2 before low pass filtered

// Temperature variable
float temperature = 0;

// Sub routine to read temperature
float readTemp() {

  temperature = particleSensor.readTemperature();
  return temperature;

}

// Sub routine to calculate SpO2
double readSpO2() {

  #ifdef MAX30105
  red = particleSensor.getFIFORed(); // Sparkfun's MAX30105
  ir = particleSensor.getFIFOIR();  // Sparkfun's MAX30105
  #else
  red = particleSensor.getFIFOIR(); // why getFOFOIR output Red data by MAX30102 on MH-ET LIVE breakout board
  ir = particleSensor.getFIFORed(); // why getFIFORed output IR data by MAX30102 on MH-ET LIVE breakout board
  #endif

  i++;
  fred = (double)red;
  fir = (double)ir;
  avered = avered * frate + (double)red * (1.0 - frate);// average red level by low pass filter
  aveir = aveir * frate + (double)ir * (1.0 - frate); // average IR level by low pass filter
  sumredrms += (fred - avered) * (fred - avered); // square sum of alternate component of red level
  sumirrms += (fir - aveir) * (fir - aveir);// square sum of alternate component of IR level

  if ((i % Num) == 0) {
      
    double R = (sqrt(sumredrms) / avered) / (sqrt(sumirrms) / aveir);
    SpO2 = -23.3 * (R - 0.4) + 100; // http://ww1.microchip.com/downloads/jp/AppNotes/00001525B_JP.pdf
    ESpO2 = FSpO2 * ESpO2 + (1.0 - FSpO2) * SpO2;//low pass filter
    sumredrms = 0.0; sumirrms = 0.0; i = 0;

  }

  if (ESpO2 > 100.0) {

    ESpO2 = 100;

  }

  return ESpO2;

}

String getDate() {

  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }
  // The formattedDate comes with the following format:
  // 2018-05-28T16:00:13Z
  // We need to extract date and time
  formattedDate = timeClient.getFormattedDate();

  // Extract date
  int splitT = formattedDate.indexOf("T");
  dayStamp = formattedDate.substring(0, splitT);
 
  return dayStamp;

}

String getTime() {

  while(!timeClient.update()) {
    timeClient.forceUpdate();
  }
  // The formattedDate comes with the following format:
  // 2018-05-28T16:00:13Z
  // We need to extract date and time
  formattedDate = timeClient.getFormattedDate();

  // Extract time
  int splitT = formattedDate.indexOf("T");
  timeStamp = formattedDate.substring(splitT+1, formattedDate.length()-1);

  return timeStamp;

}

//Sub Routine to send data to Azure IoT Hub
void sendRequest(String iothubName, String deviceName, String sasToken, String message) {

  // HTTPS connection to Azure IoT Hub
  if ((WiFi.status() == WL_CONNECTED)) { //Check the current connection status

    HTTPClient http;
    String url = "https://" + iothubName + ".azure-devices.net/devices/" + deviceName + "/messages/events?api-version=2016-11-14";
    http.begin(url, root_ca); // Specify the URL and certificate
    http.addHeader("Authorization", sasToken);
    http.addHeader("Content-Type", "application/json");
    int httpCode = http.POST(message);

    if (httpCode > 0) { // Check for the returning code

        String payload = http.getString();
        Serial.print("Http code = ");
        Serial.println(httpCode);
        Serial.print("Payload = ");
        Serial.println(payload);
      }

    else {
      Serial.println("Error on HTTP request");
    }

    http.end(); // Free the resources
  }

}

///////////////////////////////////////////////////////////////////////////////
void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.print("Attempting to connect to SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("Connecting WiFi...");
    delay(1000);
  }

  // MAX30102 sensor initialization
  while (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println("MAX30102 was not found ");
  }

  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x0A); //Turn Red LED to low to indicate sensor is running
  
  //  Die Temperature enable
  particleSensor.enableDIETEMPRDY();

  // Initialize a NTPClient to get time
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  // Costa Rica GMT -6 = -21600
  timeClient.setTimeOffset(-21600);

}

void loop() {

  readSpO2();
  long irValue = particleSensor.getIR();
 
  if (irValue < 50000) {

    Serial.println("Please, place your index finger on the sensor");
    countCycles = 0;
    i = 0;

  } else if(checkForBeat(irValue) == true) {
    
    long delta = millis() - lastBeat;
    lastBeat = millis();
      
    beatsPerMinute = 60 / (delta / 1000.0);
    
    if (beatsPerMinute < 255 && beatsPerMinute > 20) {

      rates[rateSpot++] = (byte)beatsPerMinute; //Store this reading in the array
      rateSpot %= RATE_SIZE; //Wrap variable
        
      //Take average of readings
      beatAvg = 0;
      for (byte x = 0 ; x < RATE_SIZE ; x++)
      beatAvg += rates[x];
      beatAvg /= RATE_SIZE;
      char buff[10];
      beatAvgString = dtostrf(beatAvg, 4, 6, buff);
      
    }

    countCycles ++;
    if (countCycles >= 16) {

      Serial.print("Ritmo cardiaco = ");
      Serial.println(beatAvg);
      Serial.print("Temperature = ");
      Serial.println(temperature);
      Serial.print("SpO2 = ");
      Serial.println(ESpO2); 
      StaticJsonDocument<256> doc;
      doc["deviceId"] = DEVICE_NAME;
      doc["body temperature"] = readTemp();
      doc["heart beat"] = beatAvg;
      doc["SpO2"] = readSpO2();
      doc["temperature Ambient"] = 25;
      doc["humidity"] = 50;
      doc["date"] = getDate();
      doc["time"] = getTime();
      String requestBody;
      serializeJson(doc, requestBody);

      sendRequest(IOT_HUB_NAME, DEVICE_NAME, SAS_TOKEN, requestBody);


    } else {
        Serial.println("Obteniendo datos...");
      }
    } 
}
