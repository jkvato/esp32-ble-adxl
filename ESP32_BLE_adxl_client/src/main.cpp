/*
  John Taylor
  BLE Client for ADXL343 accelerometer
  Adafruit Feather ESP32-S3 2MB PSRAM
  SSD1327 128x128 OLED display
*/

#include <Arduino.h>
#include <BLEDevice.h>
#include <Wire.h>
#include <Adafruit_SSD1327.h>
#include <Adafruit_GFX.h>
#include <Adafruit_NeoPixel.h>

// BLE Server name
#define bleServerName "ESP32_ADXL343"

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/
static BLEUUID accelerometerServiceUUID("c2d80b23-524b-4df7-9bb1-e5c305833633");

// Characteristics and Descriptors
static BLEUUID accelerometerXCharacteristicUUID("ba9f3faa-7939-436b-8197-7ccd1e66be33");
static BLEUUID accelerometerYCharacteristicUUID("0e1f1e2e-7381-4f30-8cff-7ff84eb93026");
static BLEUUID accelerometerZCharacteristicUUID("1930a6a0-25ae-4ae4-a309-9d8c230c2358");

// BLE status flags
static bool doConnect = false;
static bool connected = false;
static bool doScan = false;

// Address of the peripheral device. Address will be found during scanning
static BLEAddress *pServerAddress;

// Characteristics that we want to read
static BLERemoteCharacteristic* accelerometerXCharacteristic;
static BLERemoteCharacteristic* accelerometerYCharacteristic;
static BLERemoteCharacteristic* accelerometerZCharacteristic;

const uint8_t notificationOn[] = {0x1, 0x0};
const uint8_t notificationOff[] = {0x0, 0x0};

#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 128

#define NUM_PIXELS 1
Adafruit_NeoPixel pixels(NUM_PIXELS, PIN_NEOPIXEL, NEO_GRB + NEO_KHZ800);

Adafruit_SSD1327 display = Adafruit_SSD1327(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);
bool displayConfigured = false;
bool displayEnabled = false;
const int buttonPin = 0;

char xAxisChar[10];
char yAxisChar[10];
char zAxisChar[10];

bool newXAxis = false;
bool newYAxis = false;
bool newZAxis = false;

class MyClientCallback : public BLEClientCallbacks
{
  void onConnect(BLEClient* pClient)
  {
    Serial.println("onConnect");
    connected = true;
  }

  void onDisconnect(BLEClient* pClient)
  {
    Serial.println("onDisconnect");
    connected = false;
  }
};

static void accelerometerXNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData, size_t length, bool isNotify)
{
  // Serial.print("accelerometerXNotifyCallback: ");
  memcpy(xAxisChar, pData, length);
  newXAxis = true;
  // Serial.println(xAxisChar);
}

static void accelerometerYNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData, size_t length, bool isNotify)
{
  // Serial.print("accelerometerYNotifyCallback: ");
  memcpy(yAxisChar, pData, length);
  newYAxis = true;
  // Serial.println(yAxisChar);
}

static void accelerometerZNotifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData, size_t length, bool isNotify)
{
  // Serial.print("accelerometerZNotifyCallback: ");
  memcpy(zAxisChar, pData, length);
  newZAxis = true;
  // Serial.println(zAxisChar);
}

// Connect to the BLE Server that has the name, service and characteristics
// we are looking for
bool connectToServer(BLEAddress bleAddress)
{
  Serial.print("connectToServer: ");
  Serial.println(bleAddress.toString().c_str());
  BLEClient* pClient = BLEDevice::createClient();

  pClient->setClientCallbacks(new MyClientCallback());

  // Connect to the remote BLE server
  pClient->connect(bleAddress);
  Serial.println(" - Connected to server");

  // Obtain a reference to the service we are after in the remote BLE server
  BLERemoteService* pRemoteService = pClient->getService(accelerometerServiceUUID);
  if (pRemoteService == nullptr)
  {
    Serial.print("Failed to find our service UUID:: ");
    Serial.println(accelerometerServiceUUID.toString().c_str());
    pClient->disconnect();
    return false;
  }

  // Obtain a reference to the characteristics in the service of the remote BLE server
  accelerometerXCharacteristic = pRemoteService->getCharacteristic(accelerometerXCharacteristicUUID);
  accelerometerYCharacteristic = pRemoteService->getCharacteristic(accelerometerYCharacteristicUUID);
  accelerometerZCharacteristic = pRemoteService->getCharacteristic(accelerometerZCharacteristicUUID);

  if (accelerometerXCharacteristic == nullptr ||
    accelerometerYCharacteristic == nullptr ||
    accelerometerZCharacteristic == nullptr)
  {
    Serial.println("Failed to find our characteristic UUID");
    return false;
  }

  Serial.println(" - Found our characteristics");

  // Assign callback functions for the characteristics
  accelerometerXCharacteristic->registerForNotify(accelerometerXNotifyCallback);
  accelerometerYCharacteristic->registerForNotify(accelerometerYNotifyCallback);
  accelerometerZCharacteristic->registerForNotify(accelerometerZNotifyCallback);

  return true;
}

// Callback function that gets called when another device's advertisement has been received
class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{
  void onResult(BLEAdvertisedDevice advertisedDevice)
  {
    if (advertisedDevice.getName() == bleServerName)
    {
      // Check if the name of the advertiser matches
      advertisedDevice.getScan()->stop();
      pServerAddress = new BLEAddress(advertisedDevice.getAddress());
      doConnect = true;
      doScan = true;
      Serial.println("Device found. Connecting.");
    }
  }
};

// Print the latest readings on the OLED display
void printReadings()
{
  Serial.print("printReadings: ");

  if (displayConfigured && displayEnabled)
  {
    display.clearDisplay();

    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print("X Axis: ");
    display.setTextSize(2);
    display.setCursor(0, 10);
    display.print(xAxisChar);

    display.setTextSize(1);
    display.setCursor(0, 35);
    display.print("Y Axis: ");
    display.setTextSize(2);
    display.setCursor(0, 45);
    display.print(yAxisChar);

    display.setTextSize(1);
    display.setCursor(0, 70);
    display.print("Z Axis: ");
    display.setTextSize(2);
    display.setCursor(0, 80);
    display.print(zAxisChar);
    display.display();
  }

  Serial.print("X Axis: ");
  Serial.print(xAxisChar);
  Serial.print(" Y Axis: ");
  Serial.print(yAxisChar);
  Serial.print(" Z Axis: ");
  Serial.print(zAxisChar);
  Serial.println("");
}

void runScan(uint32_t seconds)
{
  Serial.println("Starting scan...");
  pixels.fill(0xFFFF00);
  pixels.show();
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->start(seconds);
  Serial.println("Scan ended");
  pixels.fill(0x000000);
  pixels.show();
}

void setup()
{
  // Start serial communication
  Serial.begin(115200);
  delay(2000);
  Serial.println("Starting BLE client application");

#if defined(NEOPIXEL_POWER)
  // If this board has a power control pin, we must set it to output and high
  // in order to enable the NeoPixels. We put this in an #if defined so it can
  // be reused for other boards without compilation errors
  pinMode(NEOPIXEL_POWER, OUTPUT);
  digitalWrite(NEOPIXEL_POWER, HIGH);
#endif

  pixels.begin();
  pixels.setBrightness(20);

  // OLED display setup
  if (!display.begin(0x3D))
  {
    Serial.println(F("SSD1327 allocation failed"));
    //for(;;);
    displayConfigured = false;
    displayEnabled = false;
  }
  else
  {
    displayConfigured = true;
    displayEnabled = true;
  }

  if (displayConfigured && displayEnabled)
  {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1327_WHITE, 0);
    display.setCursor(0, 25);
    display.print("BLE Client");
    display.display();
  }

  // Input button
  pinMode(buttonPin, INPUT_PULLUP);

  // Init BLE device
  BLEDevice::init("");

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 30 seconds.
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  runScan(30);
}

void loop()
{
  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are
  // connected we set the connected flag to be true.
  if (doConnect == true)
  {
    if (connectToServer(*pServerAddress))
    {
      Serial.println("We are now connected to the BLE Server.");

      accelerometerXCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      accelerometerYCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      accelerometerZCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902))->writeValue((uint8_t*)notificationOn, 2, true);
      connected = true;
    }
    else
    {
      pixels.fill(0xFF0000);
      pixels.show();
      Serial.println("We have failed to connect to the server");
    }
    doConnect = false;
  }

  // If we are connected to a peer BLE server
  if (connected)
  {
    pixels.fill(0x0000FF);
    pixels.show();
  }
  else
  {
    pixels.fill(0xFF0000);
    pixels.show();

    if (doScan)
    {
      runScan(30);
    }
  }

  // If new readings are available, print to OLED
  if (newXAxis && newYAxis && newZAxis)
  {
    newXAxis = newYAxis = newZAxis = false;
    printReadings();
  }

  if (digitalRead(buttonPin) == LOW && displayConfigured)
  {
    displayEnabled = !displayEnabled;
  }

  // delay(1000);
}
