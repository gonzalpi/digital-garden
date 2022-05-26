// Blynk
#define BLYNK_TEMPLATE_ID "templateId"
#define BLYNK_DEVICE_NAME "deviceName"
#define BLYNK_AUTH_TOKEN "authToken"
#define BLYNK_PRINT Serial
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include "FirebaseESP8266.h"
WiFiClient client;

// Wi-Fi settings
const char auth[] = "auth";
const char ssid[] = "ssid";
const char pass[] = "pass";

// Firebase
#define API_KEY "apiKey"
const char *FIREBASE_HOST="firebaseHost"; 
const char *FIREBASE_AUTH="firebaseAuth";
FirebaseData firebaseData;

// DHT (temperature and environmental humidity sensor)
#include "DHT.h"
#define DHTPIN D5
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// PINS
const int venti = D1; // fan
const int bomba = D2; // pump
const int echo = 13;  // ultrasonic
const int trig = 3;   // ultrasonic
const int led = 12;   // led
const int hygro = A0; // soil moisture

bool statusVenti = false; // fan status (negated)
bool statusBomba = false; // pump status (negated)
int t_ctl = 100;          // max. temperature (control)
int s_ctl = 0;            // min. soil moisture (control)
int depth = 18;           // water container depth
int count = 0;            // counter: keep diff. update freq. (Blynk, Firebase)
String node = "digital-garden"; // Firebase node
  
void setup()
{
  Serial.begin(9600);
  pinMode(echo,INPUT);
  pinMode(trig,OUTPUT);
  pinMode(led, OUTPUT);
  pinMode(hygro, INPUT);
  pinMode(bomba, OUTPUT);
  pinMode(venti, OUTPUT);

  // Enable temperature and environmental humidity sensor
  dht.begin();
  
  // Disable components
  digitalWrite(led,LOW);
  digitalWrite(venti, HIGH);
  digitalWrite(bomba, HIGH);

  //Blynk
  Blynk.begin(auth, ssid, pass);
  Serial.println(F("DHTxx test!"));
  Serial.print("Conectando...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
   
  Serial.println("Conexión OK!");
  Serial.print("IP Local: ");
  Serial.println(WiFi.localIP());

  // Firebase
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
  
  // Set gauge range on Blynk panel
  Blynk.setProperty(V5, "max", 100);
  Blynk.setProperty(V6, "max", 100);
  Blynk.setProperty(V7, "max", 100);
  Blynk.setProperty(V8, "max", 100);

  // Get stored control max. temperature from Firebase
  Firebase.getInt(firebaseData, node + "/t_ctl");
  if (firebaseData.intData()==0){
    Firebase.setInt(firebaseData, node + "/t_ctl", t_ctl+1);
  }
  else{
    t_ctl=firebaseData.intData()-1;
  }

  // Get stored control min. soil moisture from Firebase
  Firebase.getInt(firebaseData, node + "/s_ctl");
  if (firebaseData.intData()==0){
    Firebase.setInt(firebaseData, node + "/s_ctl", s_ctl+1);
  }
  else{
    s_ctl=firebaseData.intData()-1;
  }
  
  // Get stored water container depth from Firebase
  Firebase.getInt(firebaseData, node + "/depth");
  if (firebaseData.intData()==0){
    Firebase.setInt(firebaseData, node + "/depth", depth+1);
  }
  else{
    depth=firebaseData.intData()-1;
  }
  
  // Set relay and depth values to false by default on Blynk panel
  Blynk.virtualWrite(V2, 0);
  Blynk.virtualWrite(V3, 0);

  // Set water container depth and control values on Blynk panel
  Blynk.virtualWrite(V9, depth);
  Blynk.virtualWrite(V11, t_ctl);
  Blynk.virtualWrite(V12, s_ctl);
}

// Get fan status from Blynk panel
BLYNK_WRITE(V2)
{
  int V2Data = param.asInt();
  Serial.print(V2Data);
  Serial.println(" Venti");
  statusVenti = (bool)V2Data;
}

// Get pump status from Blynk panel
BLYNK_WRITE(V3)
{
  int V3Data = param.asInt();
  Serial.print(V3Data);
  Serial.println(" Bomba");
  statusBomba = (bool)V3Data;
}

// Get water container depth from Blynk panel
BLYNK_WRITE(V9)
{
  int V9Data = param.asInt();
  depth = V9Data;
  // int V1Data=param.asInt();
  // digitalWrite(led, V1Data);
}

// Get control max. temperature value
BLYNK_WRITE(V11)
{
  int V11Data = param.asInt();
  t_ctl = V11Data;
}

// Get control min. soil moisture value
BLYNK_WRITE(V12)
{
  int V12Data = param.asInt();
  s_ctl = V12Data;
}

void loop()
{
  int t;        // Temperature
  int h;        // Humidity
  long dur;       // Time difference from ultrasonic sensor
  long dist;       // Distance from ultrasonic sensor
  float s;        // Soil moisture
  int s_map;      // Soil moisture normalized
  int waterLevelPercentage;

    // Water level
  digitalWrite(trig, LOW);
  delayMicroseconds(4);
  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);
  dur = pulseIn(echo, HIGH);
  dist = ((dur * 0.034) / 2)-3;
  waterLevelPercentage = 100-(((float)dist/(depth-3))*100);
  if (waterLevelPercentage<0){
    waterLevelPercentage=0;
  }

  //waterLevelLED
  if(waterLevelPercentage<25){
    digitalWrite(led,HIGH);
  }
  else{
    digitalWrite(led,LOW);
  }
  

    // Humidity
  h = dht.readHumidity();

  // Temperature
  t = dht.readTemperature();

    // Soil moisture
  s = analogRead(hygro);
  s_map = map(s, 0, 1023, 100, 0);

    // Read failure
  if (isnan(h) || isnan(t) || isnan(s) || isnan(dur)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  // Update fan from Blynk
  if (statusVenti) {
    digitalWrite(venti, LOW);
  } else {
    digitalWrite(venti, HIGH);
  }
  Serial.print("Status venti: ");
  Serial.println(statusVenti);

  // Update pump from Blynk}
  if (statusBomba) {
    digitalWrite(bomba, LOW);
  } else {
    digitalWrite(bomba, HIGH);
  }
  Serial.print("Status bomba: ");
  Serial.println(statusBomba);
  
   // Monitor soil moisture
  if (s_map < s_ctl) {
    digitalWrite(bomba, LOW);
    delay(5000);
    digitalWrite(bomba, HIGH);
    delay(2000);
  }

  // Monitor temperature
  if (t > t_ctl) {
    digitalWrite(venti, LOW);
    delay(5000);
    digitalWrite(venti, HIGH);
    delay(2000);
  }

  // Print stuff
  Serial.print(waterLevelPercentage);
  Serial.print("%  ");
  Serial.print(dist);
  Serial.println(" cm");
  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.print(F("°C  Soil humidity: "));
  Serial.print(s_map);
  Serial.println("%");
  Serial.print("Control temperature: ");
  Serial.print(t_ctl);
  Serial.print(" °C  Control soil moisture: ");
  Serial.print(s_ctl);
  Serial.println("%");

  // Write to Blynk
  Blynk.virtualWrite(V5, h);
  Blynk.virtualWrite(V6, t);
  Blynk.virtualWrite(V7, s_map);
  Blynk.virtualWrite(V8, waterLevelPercentage);

  Firebase.setInt(firebaseData, node + "/s_ctl", s_ctl+1);
  Firebase.setInt(firebaseData, node + "/t_ctl", t_ctl+1);
  Firebase.setInt(firebaseData, node + "/depth", depth+1);

  if(count == 3){
    // push de datos Firebase
    Firebase.pushInt(firebaseData, node + "/Temperature", t);
    Firebase.pushInt(firebaseData, node + "/Humidity", h);
    Firebase.pushInt(firebaseData, node + "/Soil_Moisture", s_map);
    Firebase.pushInt(firebaseData, node + "/Water_Level", waterLevelPercentage);
    count=0;
  }
  count = count + 1;
  
  delay(5000);
}
