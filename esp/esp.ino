#include <Wire.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <Adafruit_MLX90614.h>

MAX30105 particleSensor;
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

const int ecgPin = 34;          
const int loPlusPin = 35;        
const int loMinusPin = 32;       

uint32_t irBuffer[100];
uint32_t redBuffer[100];
int32_t spo2;
int8_t validSPO2;
int32_t heartRate;
int8_t validHeartRate;

int ecgValue = 0;
float tempObject = 0.0;
float tempAmbient = 0.0;

const int filterSize = 5;
int hrHistory[filterSize] = {0};
int hrIndex = 0;
int filteredHR = 0;

unsigned long lastSensorRead = 0;
const int sensorReadInterval = 20;
unsigned long lastPrintTime = 0;
const int printInterval = 100;
unsigned long lastMAX30102Read = 0;
const int max30102Interval = 3000;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting...");
  
  Wire.begin(21, 22);
  
  Wire1.begin(33, 32);
  
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30102 not found!");
  } else {
    Serial.println("MAX30102 OK");
    particleSensor.setup(60, 8, 2, 400, 215, 4096);
    particleSensor.setPulseAmplitudeRed(0x0A);
    particleSensor.setPulseAmplitudeIR(0x1F);
  }
  
  mlx.begin();
  
  pinMode(ecgPin, INPUT);
  pinMode(loPlusPin, INPUT);
  pinMode(loMinusPin, INPUT);
  
  Serial.println("Ready!");
}

void loop() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastSensorRead >= sensorReadInterval) {
    lastSensorRead = currentTime;
    
    if (digitalRead(loPlusPin) == 1 || digitalRead(loMinusPin) == 1) {
      ecgValue = 2048;
    } else {
      ecgValue = analogRead(ecgPin);
    }
    
    readMLX90614();
  }
  
  if (currentTime - lastMAX30102Read >= max30102Interval) {
    lastMAX30102Read = currentTime;
    readMAX30102();
  }
  
  if (currentTime - lastPrintTime >= printInterval) {
    lastPrintTime = currentTime;
    
    int displayHR = (filteredHR > 0) ? filteredHR : heartRate;
    
    Serial.print(displayHR);
    Serial.print(",");
    Serial.print(spo2);
    Serial.print(",");
    Serial.print(ecgValue);
    Serial.print(",");
    Serial.print(tempObject, 1);
    Serial.print(",");
    Serial.print(tempAmbient, 1);
    Serial.print(",");
    Serial.print(validHeartRate);
    Serial.print(",");
    Serial.println(validSPO2);
  }
}

void readMLX90614() {
  uint8_t address = 0x5A;
  
  Wire1.beginTransmission(address);
  Wire1.write(0x07);
  if (Wire1.endTransmission(false) != 0) {
    tempObject = 0;
    tempAmbient = 0;
    return;
  }
  
  Wire1.requestFrom(address, (uint8_t)3);
  if (Wire1.available() >= 3) {
    uint16_t data = Wire1.read();
    data |= (Wire1.read() << 8);
    Wire1.read();
    tempObject = (data * 0.02) - 273.15;
  }
  
  Wire1.beginTransmission(address);
  Wire1.write(0x06);
  Wire1.endTransmission(false);
  Wire1.requestFrom(address, (uint8_t)3);
  
  if (Wire1.available() >= 3) {
    uint16_t data = Wire1.read();
    data |= (Wire1.read() << 8);
    Wire1.read();
    tempAmbient = (data * 0.02) - 273.15;
  }
}

void readMAX30102() {
  Serial.println("Reading MAX30102...");
  
  for (byte i = 0; i < 100; i++) {
    while (particleSensor.available() == false) {
      particleSensor.check();
      delay(1);
    }
    
    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample();
  }
  
  maxim_heart_rate_and_oxygen_saturation(irBuffer, 100, redBuffer, 
                                         &spo2, &validSPO2, 
                                         &heartRate, &validHeartRate);
  
  if (heartRate > 150 || heartRate < 40) {
    validHeartRate = 0;
    heartRate = 0;
  }
  
  if (spo2 > 100 || spo2 < 70) {
    validSPO2 = 0;
    spo2 = 0;
  }
  
  if (validHeartRate && heartRate > 0) {
    hrHistory[hrIndex] = heartRate;
    hrIndex = (hrIndex + 1) % filterSize;
    
    int sum = 0;
    int count = 0;
    for (int i = 0; i < filterSize; i++) {
      if (hrHistory[i] > 0) {
        sum += hrHistory[i];
        count++;
      }
    }
    
    if (count > 0) {
      filteredHR = sum / count;
    }
  }
  
  if (validHeartRate && validSPO2) {
    Serial.print("Heart Rate: ");
    Serial.print(filteredHR);
    Serial.print(" bpm, SpO2: ");
    Serial.print(spo2);
    Serial.println("%");
  } else {
    Serial.println("Poor signal - adjust finger");
  }
}
