/*
  John Taylor
  BLE Server for ADXL343 accelerometer
  SparkFun ESP32 Thing Plus C
  ADXL343 accelerometer
*/

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL343.h>
#include <FastLED.h>

// BLE server name
#define bleServerName "ESP32_ADXL343"

// RGB LED parameters
#define LED_PIN     2
#define COLOR_ORDER GRB
#define CHIPSET     WS2812
#define NUM_LEDS    1
#define BRIGHTNESS  25
CRGB leds[NUM_LEDS];

Adafruit_ADXL343 adxl = Adafruit_ADXL343(12345);

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 5000;

bool deviceConnected = false;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
#define SERVICE_UUID "c2d80b23-524b-4df7-9bb1-e5c305833633"

// Characteristics and Descriptors
BLECharacteristic accelerometerXCharacteristic(
  "ba9f3faa-7939-436b-8197-7ccd1e66be33",
  BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor accelerometerXDescriptor(BLEUUID((uint16_t)0x2902));

BLECharacteristic accelerometerYCharacteristic(
  "0e1f1e2e-7381-4f30-8cff-7ff84eb93026",
  BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor accelerometerYDescriptor(BLEUUID((uint16_t)0x2902));

BLECharacteristic accelerometerZCharacteristic(
  "1930a6a0-25ae-4ae4-a309-9d8c230c2358",
  BLECharacteristic::PROPERTY_NOTIFY);
BLEDescriptor accelerometerZDescriptor(BLEUUID((uint16_t)0x2902));

bool IsNotifySet(BLECharacteristic* bleCharacteristic)
{
  BLE2902 *p2902 = (BLE2902*)bleCharacteristic->getDescriptorByUUID((uint16_t)0x2902);
  if (p2902 != nullptr && p2902->getNotifications())
  {
    return true;
  }
  return false;
}

void SetNotify(BLECharacteristic* bleCharacteristic, bool flag)
{
  BLE2902 *p2902 = (BLE2902*)bleCharacteristic->getDescriptorByUUID((uint16_t)0x2902);
  if (p2902 != nullptr)
  {
    p2902->setNotifications(flag);
  }
}

// Setup callbacks onConnect and onDisconnect
class MyServerCallbacks: public BLEServerCallbacks
{
  void onConnect(BLEServer* pServer)
  {
    Serial.println("onConnect");
    deviceConnected = true;
    accelerometerXCharacteristic.setNotifyProperty(false);
    accelerometerYCharacteristic.setNotifyProperty(false);
    accelerometerZCharacteristic.setNotifyProperty(false);
  }

  void onDisconnect(BLEServer* pServer)
  {
    Serial.println("onDisconnect");
    deviceConnected = false;
    SetNotify(&accelerometerXCharacteristic, false);
    SetNotify(&accelerometerYCharacteristic, false);
    SetNotify(&accelerometerZCharacteristic, false);
    pServer->getAdvertising()->start();
  }
};

void initADXL()
{
  if (!adxl.begin())
  {
    Serial.println("Could not find a valid ADXL343 sensor");
    while (1);
  }

  adxl.setRange(ADXL343_RANGE_4_G);
  adxl.printSensorDetails();
}

void setup()
{
  // Start serial communication
  Serial.begin(115200);

  // Initialize RGB LED
  FastLED.addLeds<CHIPSET, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  // Init ADXL Sensor
  initADXL();

  // Create the BLE Device
  BLEDevice::init(bleServerName);

  // Create the BLE Server
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *adxlService = pServer->createService(SERVICE_UUID);

  // Create the BLE Characteristics and Create a BLE Descriptor
  // X axis
  adxlService->addCharacteristic(&accelerometerXCharacteristic);
  accelerometerXDescriptor.setValue("ADXL X Value");
  accelerometerXCharacteristic.addDescriptor(&accelerometerXDescriptor);

  // Y axis
  adxlService->addCharacteristic(&accelerometerYCharacteristic);
  accelerometerYDescriptor.setValue("ADXL Y Value");
  accelerometerYCharacteristic.addDescriptor(&accelerometerYDescriptor);

  // Z axis
  adxlService->addCharacteristic(&accelerometerZCharacteristic);
  accelerometerZDescriptor.setValue("ADXL Z Value");
  accelerometerZCharacteristic.addDescriptor(&accelerometerZDescriptor);

  // Start the service
  adxlService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pServer->getAdvertising()->start();
  Serial.println("Advertising");
}

void loop()
{
  if (deviceConnected)
  {
    leds[0] = CRGB::Blue;
    FastLED.show();
  }
  else
  {
    leds[0] = CRGB::Yellow;
    FastLED.show();
  }

  if (deviceConnected)
  {
    if ((millis() - lastTime) > timerDelay)
    {
      // Read accelerometer data
      sensors_event_t event;
      adxl.getEvent(&event);

      // Notify X axis
      static char xAxisStr[8];
      dtostrf(event.acceleration.x, 8, 4, xAxisStr);
      accelerometerXCharacteristic.setValue(xAxisStr);
      accelerometerXCharacteristic.notify();
      Serial.print("X: ");
      Serial.print(xAxisStr);
      Serial.print(" m/s^2");

      // Notify Y axis
      static char yAxisStr[8];
      dtostrf(event.acceleration.y, 8, 4, yAxisStr);
      accelerometerYCharacteristic.setValue(yAxisStr);
      accelerometerYCharacteristic.notify();
      Serial.print("  Y: ");
      Serial.print(yAxisStr);
      Serial.print(" m/s^2");

      // Notify Z axis
      static char zAxisStr[8];
      dtostrf(event.acceleration.z, 8, 4, zAxisStr);
      accelerometerZCharacteristic.setValue(zAxisStr);
      accelerometerZCharacteristic.notify();
      Serial.print("  Z: ");
      Serial.print(zAxisStr);
      Serial.println(" m/s^2");

      lastTime = millis();
    }
  }
}