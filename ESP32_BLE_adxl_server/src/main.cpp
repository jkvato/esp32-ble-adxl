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
#include <SparkFun_MAX1704x_Fuel_Gauge_Arduino_Library.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>

// BLE server name
#define bleServerName "ESP32_ADXL343"

// RGB LED parameters
#define LED_PIN     2
#define COLOR_ORDER GRB
#define CHIPSET     WS2812
#define NUM_LEDS    1
#define BRIGHTNESS  0
CRGB leds[NUM_LEDS];

// SD card chip select
const int sd_cs = 5;
bool canLog = false;

// Log filename
#define LOG_FILENAME "/logfile.txt"

// Instantiate the accelerometer
Adafruit_ADXL343 adxl = Adafruit_ADXL343(12345);

// Instantiate the battery monitor
SFE_MAX1704X lipo(MAX1704X_MAX17048);

// Timer variables
unsigned long lastTime = 0;
unsigned long timerDelay = 500;

// Battery monitor variables
double voltage = 0;
double soc = 0;

sensors_event_t event;

bool deviceConnected = false;

String Data;

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

void appendFile(fs::FS &fs, const char * path, const char * message)
{
  // Serial.printf("Appending to file: %s\n", path);

  File file = fs.open(path, FILE_APPEND);
  if(!file)
  {
    Serial.println("Failed to open file for appending");
    return;
  }

  Data = String(millis()) + "," + String(message);
  message = Data.c_str();

  if(file.print(message))
  {
    // Serial.println("Message appended");
  }
  else
  {
    Serial.println("Append failed");
  }
  file.close();
}

// Setup callbacks onConnect and onDisconnect
class MyServerCallbacks: public BLEServerCallbacks
{
  void onConnect(BLEServer* pServer)
  {
    appendFile(SD, LOG_FILENAME, "Connected\r\n");
    Serial.println("onConnect");
    deviceConnected = true;
    accelerometerXCharacteristic.setNotifyProperty(false);
    accelerometerYCharacteristic.setNotifyProperty(false);
    accelerometerZCharacteristic.setNotifyProperty(false);
  }

  void onDisconnect(BLEServer* pServer)
  {
    appendFile(SD, LOG_FILENAME, "Disconnected\r\n");
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

void initMAX17048()
{
  // Set up the MAX17048 LiPo fuel gauge:
  if (lipo.begin() == false) // Connect to the MAX17048 using the default wire port
  {
    Serial.println(F("MAX17048 not detected. Please check wiring. Freezing."));
    while (1);
  }

  // Quick start restarts the MAX17043 in hopes of getting a more accurate
	// guess for the SOC.
	lipo.quickStart();
}

void logData()
{
  Data = String(voltage) + "," + String(soc) + "," + String(event.acceleration.x) + "," + 
                String(event.acceleration.x) + "," + String(event.acceleration.z) + "\r\n";
  // Data = String(millis()) + "," + String(voltage) + "," + String(soc) + "," + String(event.acceleration.x) + "," + 
  //               String(event.acceleration.x) + "," + String(event.acceleration.z) + "\r\n";
  // Serial.print("Save data: ");
  // Serial.println(Data);
  appendFile(SD, LOG_FILENAME, Data.c_str());
}

void writeFile(fs::FS &fs, const char * path, const char * message)
{
  Serial.printf("Writing file: %s\n", path);

  File file = fs.open(path, FILE_WRITE);
  if(!file)
  {
    Serial.println("Failed to open file for writing");
    return;
  }
  if(file.print(message))
  {
    Serial.println("File written");
  }
  else
  {
    Serial.println("Write failed");
  }
  file.close();
}

void initSDCard()
{
  if(!SD.begin(sd_cs))
  {
    Serial.println("Card Mount Failed");
    return;
  }
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE)
  {
    Serial.println("No SD card attached");
    return;
  }
  Serial.println("Initializing SD card...");
  if (!SD.begin())
  {
    Serial.println("SD card initialization failed!");
    return;    
  }

  File file = SD.open(LOG_FILENAME);
  if(!file)
  {
    Serial.println("File does not exist");
    Serial.println("Creating file...");
    writeFile(SD, LOG_FILENAME, "Millis, Voltage, StateOfCharge, X, Y, Z\r\n");
  }
  else
  {
    Serial.println("File exists");  
  }
  file.close();
  canLog = true;
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

  // Init lipo sensor
  initMAX17048();

  // Init SD card
  initSDCard();

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

  if ((millis() - lastTime) > timerDelay)
  {
    // Read battery monitor data
    // lipo.getVoltage() returns a voltage value (e.g. 3.93)
    voltage = lipo.getVoltage();
    // lipo.getSOC() returns the estimated state of charge (e.g. 79%)
    soc = lipo.getSOC();

    static char voltageStr[5];
    dtostrf(voltage, 4, 2, voltageStr);
    Serial.print("Voltage: ");
    Serial.print(voltageStr);
    Serial.print(" V,  ");

    static char socStr[7];
    dtostrf(soc, 4, 2, socStr);
    Serial.print("Charge: ");
    Serial.print(socStr);
    Serial.print(" %,  ");

    // Read accelerometer data
    adxl.getEvent(&event);

    // Notify X axis
    static char xAxisStr[8];
    dtostrf(event.acceleration.x, 8, 4, xAxisStr);
    if (deviceConnected)
    {
      accelerometerXCharacteristic.setValue(xAxisStr);
      accelerometerXCharacteristic.notify();
    }
    Serial.print("X: ");
    Serial.print(xAxisStr);
    Serial.print(" m/s^2");

    // Notify Y axis
    static char yAxisStr[8];
    dtostrf(event.acceleration.y, 8, 4, yAxisStr);
    if (deviceConnected)
    {
      accelerometerYCharacteristic.setValue(yAxisStr);
      accelerometerYCharacteristic.notify();
    }
    Serial.print("  Y: ");
    Serial.print(yAxisStr);
    Serial.print(" m/s^2");

    // Notify Z axis
    static char zAxisStr[8];
    dtostrf(event.acceleration.z, 8, 4, zAxisStr);
    if (deviceConnected)
    {
      accelerometerZCharacteristic.setValue(zAxisStr);
      accelerometerZCharacteristic.notify();
    }
    Serial.print("  Z: ");
    Serial.print(zAxisStr);
    Serial.println(" m/s^2");

    lastTime = millis();

    if (canLog)
    {
      logData();
    }
  }
}
