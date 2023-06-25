#include <Wire.h>
#include <ESP8266WiFi.h>
#include "MAX30105.h"
#include "ESP8266WiFi.h"
#include "heartRate.h"
#include <MQUnifiedsensor.h>
#include <ThingSpeak.h>
MAX30105 particleSensor;
String apiKey = "8IDD0U7LYZQ1KALW";     //  Enter your Write API key from ThingSpeak
const char *ssid =  "Redmi Note 7S";     // replace with your wifi ssid and wpa2 key
const char *pass =  "12121212";
const char* server = "api.thingspeak.com";
WiFiClient client;
#define         Board                   ("Esp8266")
#define         Pin                     (A0)
#define         Type                    ("MQ-9") //MQ9
#define         Voltage_Resolution      (5)
#define         ADC_Bit_Resolution      (10) // For arduino UNO/MEGA/NANO
const int         RatioMQ9CleanAir        (9.6); 
const byte RATE_SIZE = 4; //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; //Time at which the last beat occurred
float bpm;
float abpm;
MQUnifiedsensor MQ9(Board, Voltage_Resolution, ADC_Bit_Resolution, Pin, Type);
void setup()
{
  
  Serial.begin(115200);
  Serial.println("Initializing...");
   Serial.println("Connecting to ");
  Serial.println(ssid);


  WiFi.begin(ssid, pass);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  MQ9.setRegressionMethod(1);
  MQ9.init(); 
  Serial.print("Calibrating please wait.");
  float calcR0 = 0;
  for(int i = 1; i<=10; i ++)
  {
    MQ9.update(); // Update data, the arduino will read the voltage from the analog pin
    calcR0 += MQ9.calibrate(RatioMQ9CleanAir);
    Serial.print(".");
  }
  MQ9.setR0(calcR0/10);
  Serial.println("  done!.");
  
  if(isinf(calcR0)) {Serial.println("Warning: Conection issue, R0 is infinite (Open circuit detected) please check your wiring and supply"); while(1);}
  if(calcR0 == 0){Serial.println("Warning: Conection issue found, R0 is zero (Analog pin shorts to ground) please check your wiring and supply"); while(1);}

  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) //Use default I2C port, 400kHz speed
  {
    Serial.println("MAX30105 was not found. Please check wiring/power. ");
    while (1);
  }
  Serial.println("Place your index finger on the sensor with steady pressure.");

  particleSensor.setup(); //Configure sensor with default settings
  particleSensor.setPulseAmplitudeRed(0x0A); //Turn Red LED to low to indicate sensor is running
  particleSensor.setPulseAmplitudeGreen(0); //Turn off Green LED
  particleSensor.enableDIETEMPRDY(); //Enable the temp ready interrupt. This is required.
}

void loop()
{
  float temperature = particleSensor.readTemperature();
  long irValue = particleSensor.getIR();
  float temperatureF = particleSensor.readTemperatureF(); //Because I am a bad global citizen

  
    //We sensed a beat!
    long delta = millis() - lastBeat;
    lastBeat = millis();

    bpm = 60 / (delta / 1000.0);

    if (bpm < 255 && bpm > 20)
    {
      rates[rateSpot++] = (byte)bpm; //Store this reading in the array
      rateSpot %= RATE_SIZE; //Wrap variable

      //Take average of readings
      abpm = 0;
      for (byte x = 0 ; x < RATE_SIZE ; x++)
        abpm += rates[x];
      abpm /= RATE_SIZE;
    
  }
  MQ9.update(); // Update data, the arduino will read the voltage from the analog pin
  MQ9.setA(1000.5); MQ9.setB(-2.186); // Configure the equation to to calculate LPG concentration
  float LPG = MQ9.readSensor(); // Sensor will read PPM concentration using the model, a and b values set previously or from the setup

  MQ9.setA(4269.6); MQ9.setB(-2.648); // Configure the equation to to calculate LPG concentration
  float CH4 = MQ9.readSensor(); // Sensor will read PPM concentration using the model, a and b values set previously or from the setup

  MQ9.setA(599.65); MQ9.setB(-2.244); // Configure the equation to to calculate LPG concentration
  float CO = MQ9.readSensor();
  // Sensor will read PPM concentration using the model, a and b values set previously or from the setup
  Serial.print("LPG= "); 
  Serial.print(LPG);
  Serial.print(",  CH4="); 
  Serial.print(CH4);
  Serial.print(",  CO"); 
  Serial.print(CO); 
  Serial.print(",  IR=");
  Serial.print(irValue);
  Serial.print(", BPM=");
  Serial.print(bpm);
  Serial.print(", Avg BPM=");
  Serial.print(abpm);
  Serial.print("temperatureC=");
  Serial.print(temperature, 4);
  Serial.print(" temperatureF=");
  Serial.print(temperatureF, 4);

  if (irValue < 50000)
    Serial.print(" No finger?"); 

  Serial.println();
    if (client.connect(server, 80))  //   "184.106.153.149" or api.thingspeak.com
  {

    String postStr = apiKey;
    postStr += "&field1=";
    postStr += String(bpm);
    postStr += "&field2=";
    postStr += String(abpm);
    postStr += "&field3=";
    postStr += String(LPG);
    postStr += "&field4=";
    postStr += String(CH4);
    postStr += "&field5=";
    postStr += String(CO);
    postStr += "&field6=";
    postStr += String(temperature);
    postStr += "&field7=";
    postStr += String(temperatureF);
    postStr += "\r\n\r\n";

    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + apiKey + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\n\n");
    client.print(postStr);
   

    Serial.println(". Send to Thingspeak.");
  }


  client.stop();
  Serial.println("Waiting...");

  // thingspeak needs minimum 15 sec delay between updates
  delay(1000);
  
}
  
