/* ------- Setting for Blynk ------- */
#define BLYNK_DEBUG
#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL6qxqx3dpd"
#define BLYNK_TEMPLATE_NAME "Air Conditioners Efficiency Monitoring System"
#define BLYNK_AUTH_TOKEN "A8vdTMEsxM2kRDnpMaZ-ihIoQGpTMamB"

/* ------- Include libraries ------- */
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <TimeLib.h>
#include <WidgetRTC.h>
#include <cmath>

BlynkTimer timer;
WiFiClient client;
WidgetRTC rtc;

/* ------- Setting for DHT11 ------- */
#include <DHT.h>
#include <SPI.h>
#define DHTPIN 2       // What digital pin we're connected to (D4)
#define DHTTYPE DHT11  // DHT 22, AM2302, AM2321
DHT dht(DHTPIN, DHTTYPE);

/* ------- Setting for Dust Sensor -------*/
int measurePin = A0;  //Connect dust sensor to Arduino A0 pin
#define ledPower 5    // Pin D1
int samplingTime = 280;
int deltaTime = 40;
int sleepTime = 9680;

/* ------- Initialize variables ------- */
float voMeasured = 0;
float calcVoltage = 0;
float dustDensity = 0;

const char *ssid = "Bukan yang ni";
const char *pass = "Xcc-5803";

/* ------- Setting Ends ------- */

BLYNK_CONNECTED() {
  // Synchronize time on connection
  rtc.begin();
}

//Calculate the efficiency in percentage (0-100)
float calculateEfficiency(float humidity, float temperature, float dustDensity) {

  float humidityWeight = 0.33;
  float temperatureWeight = 0.33;
  float dustDensityWeight = 0.33;
  float efficiency = 0.0;

  if (humidity < 61 && temperature < 27 && dustDensity < 151) {
    efficiency = 100.0;
  } else {
    efficiency = humidityWeight * (100 - humidity) + temperatureWeight * (100 - temperature) + dustDensityWeight * (150 - dustDensity) / 3;
  }

  return efficiency;
}

void setLastAlert() {

  // Display the date and time on labels V10 and V11
  String currentTime = String(hour()) + ":" + minute() + ":" + second();
  String currentDate = String(day()) + "/" + month() + "/" + year();
  Blynk.virtualWrite(V10, currentDate);
  Blynk.virtualWrite(V11, currentTime);
}

// This function read, calculate, and send value to Blynk Cloud
void sendSensor() {
  //Read DHT11 value
  float h = dht.readHumidity();
  float t = dht.readTemperature();  // or dht.readTemperature(true) for Fahrenheit

  //Read Dust Sensor value
  digitalWrite(ledPower, LOW);  // power on the LED
  delayMicroseconds(samplingTime);

  voMeasured = analogRead(measurePin);  // read the dust value

  delayMicroseconds(deltaTime);
  digitalWrite(ledPower, HIGH);  // turn the LED off
  delayMicroseconds(sleepTime);

  // 0 - 3.3V mapped to 0 - 1023 integer values
  // recover voltage
  calcVoltage = voMeasured * (3.3 / 1024.0);

  // linear eqaution taken from http://www.howmuchsnow.com/arduino/airquality/
  // Chris Nafis (c) 2012
  dustDensity = 170 * calcVoltage - 0.1;

  String currentTime = String(hour()) + ":" + minute() + ":" + second();
  String currentDate = String(day()) + " " + month() + " " + year();

  //Check if all values are available
  if (isnan(h) || isnan(t) || isnan(dustDensity)) {
    Serial.println("Failed to read from sensors!");
    return;
  }

  //Serial.print(h); Serial.print(" "); Serial.print(t); Serial.print(" "); Serial.println(dustDensity);
  float efficiency = calculateEfficiency(h, t, dustDensity);

  // You can send any value at any time.
  // Please don't send more that 10 values per second.
  Blynk.virtualWrite(V5, t);
  Blynk.virtualWrite(V6, h);
  Blynk.virtualWrite(V7, dustDensity);

  String statusNotOK = "Abnormal";
  String statusOK = "Normal";

  // Define the optimum readings
  int tempOptimumLow = 23;
  int tempOptimumHigh = 26;
  int humOptimumLow = 40;
  int humOptimumHigh = 60;
  int dustOptimum = 150;

  // Trigger event if necessary
  if (t > tempOptimumHigh && h > humOptimumHigh && dustDensity > dustOptimum) {
    String message = "Please turn off your AC now due to inefficiency!";
    Blynk.logEvent("inefficient_alert", message);
    Blynk.virtualWrite(V8, statusNotOK);
    setLastAlert();
  } else if ((t < tempOptimumLow || t > tempOptimumHigh) && (h < humOptimumLow || h > humOptimumHigh) && dustDensity > dustOptimum) {
    String message = "Please check your AC's filter and clean if necessary!";
    Blynk.logEvent("inefficient_alert", message);
    Blynk.virtualWrite(V8, statusNotOK);
    setLastAlert();
  } else {
    Blynk.virtualWrite(V8, statusOK);
  }
  Blynk.virtualWrite(V9, efficiency);

  //Display result in Serial Monitor
  Serial.print("Humidity: ");
  Serial.print(h);
  Serial.println("%");
  Serial.print("Temperature: ");
  Serial.print(t);
  Serial.println("°C");
  Serial.print("Dust Density: ");
  Serial.print(dustDensity);
  Serial.println("ug/m3");  // unit: ug/m3
  Serial.print("Date and Time: ");
  Serial.print(currentDate);
  Serial.print(" ");
  Serial.println(currentTime);
  Serial.print("Efficiency: ");
  Serial.print(efficiency);
  Serial.println("%");
  Serial.println("");
}

void setup() {
  Serial.begin(9600);
  pinMode(ledPower, OUTPUT);

  Serial.println("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");

  //Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass, "113.210.106.161", 8080);
  Blynk.config(BLYNK_AUTH_TOKEN);
  Blynk.connect();
  Blynk.sendInternal("rtc", "sync");  //request current local time for device

  //start dht sensor
  dht.begin();

  setSyncInterval(1 * 60);  // Sync interval in seconds (1 minutes)

  // Setup a function to be called every second
  timer.setInterval(1000L, sendSensor);
}

void loop() {
  Blynk.run();
  timer.run();
}