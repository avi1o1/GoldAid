
/*
  1-8-24 FIxed WSD; BSD to be fixed
  Done till STEP 5
*/

#include <SPI.h>
#include <WiFi.h>
#include <LoRa.h>
#include <Wire.h>
#include <ThingSpeak.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// BSD ID (1-indexed) (keep different for different BSDs)
// TODO: Replace with the BSD ID
#define BSD_ID 1

// WiFi
// TODO: Replace with WiFi credentials
#define WiFi_SSID "WIFI_SSID"
#define WiFi_PASS "WIFI_PASS"
#define WIFI_TIMEOUT 10000

WiFiClient client;

// ThingSpeak
// TODO: Replace with ThingSpeak channel IDs and write API keys
unsigned long vitalsChannelID = 1234567;
const char *vitalsWriteAPIKey = "VITALS_WRITE_API_KEY";

unsigned long alertsChannelID = 1234567;
const char *alertsWriteAPIKey = "ALERTS_WRITE_API_KEY";

unsigned long lastThingSpeakUpdate = 0;
String lastAlertMessage = "";

// LoRa radio pins
const int csPin = 5;          // LoRa radio chip select
const int resetPin = 33;      // LoRa radio reset
const int irqPin = 32;        // Change for your board; must be a hardware interrupt pin
const long frequency = 433E6; // LoRa frequency

// OLED display pins
#define OLED_SDA 21
#define OLED_SCL 22
#define SCREEN_WIDTH 128    // OLED display width, in pixels
#define SCREEN_HEIGHT 64    // OLED display height, in pixels
#define OLED_RESET -1       // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Timing
#define BROADCAST_INTERVAL 5000   // 5 seconds
#define RECEIVE_DURATION 30000    // 1 minute
#define THINGSPEAK_INTERVAL 15000 // 15 seconds

// Storage for IDs, RSSIs, and vitals
struct TxInfo
{
  String id;
  int rssi;
  struct Vitals
  {
    float temperature;
    int diastolic;
    int systolic;
    float spo2;
    int heartRate;
    float latitude;
    float longitude;
    bool alert;
  } vitals;
} txInfo;

TxInfo txDevices[10];
int txCount = 0;

unsigned long startTime;
bool receiving = false;

void connectToWiFi()
{
  WiFi.begin(WiFi_SSID, WiFi_PASS);
  Serial.print("Connecting to WiFi");
  unsigned long start = millis();
  unsigned long now = millis();
  while (now - start < WIFI_TIMEOUT && WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
    now = millis();
  }
  if (WiFi.status() != WL_CONNECTED)
    Serial.println("WiFi Unavailable!");
  else
    Serial.println("\nWiFi connected");
}

void setup()
{
  Serial.begin(9600);
  while (!Serial)
    ;

  LoRa.setPins(csPin, resetPin, irqPin);

  if (!LoRa.begin(frequency))
  {
    Serial.println("Starting LoRa failed!");
    while (1)
      ;
  }
  LoRa.setSyncWord(0xA5);
  LoRa.setTxPower(10);
  LoRa.setSpreadingFactor(10);
  LoRa.setSignalBandwidth(62.5E3);
  LoRa.setCodingRate4(8);

  connectToWiFi();
  ThingSpeak.begin(client);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
  {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;)
      ;
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print("LoRa Rx Initialized");
  display.display();
}

void loop()
{
  startTime = millis();
  unsigned long currentMillis = startTime;
  if (!receiving && currentMillis - startTime < 5000)
  {
    Serial.println("## Inside broadcast");
    broadcastIDRequest();
    currentMillis = millis();
    receiving = true;
  }

  while (receiving && currentMillis - startTime < 30000)
  {
    // Serial.println("## Inside receiving");
    checkForAlerts();
    receiveID();
    currentMillis = millis();
    sortTxDevicesByRSSI();
  }
  displayLatestArray();
  displayTxDevices();
  sendBeaconsToTxDevices(); // Send beacons to transmitters in sorted order
  txCount = 0;
  receiving = false;
}

void checkForAlerts()
{
  int packetSize = LoRa.parsePacket();
  if (packetSize)
  {
    String message = "";
    while (LoRa.available())
      message += (char)LoRa.read();

    // If the message contains "ALERT", display it
    if (message.indexOf("ALERT") != -1)
    {

      // Extracting deviceID from "ALERT:<deviceID>"
      // char string[12];
      // message.toCharArray(string, sizeof(string));
      // char *deviceID = strtok(string, ":");
      // deviceID = strtok(NULL, ":");

      String deviceID = message.substring(6);

      Serial.println("*** " + message + " ***");
      Serial.print("From ID: ");
      Serial.println(deviceID);

      display.clearDisplay();
      display.setCursor(0, 0);
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.print("*** ALERT ***");
      display.println();
      display.println(message);
      display.print("From ID: ");
      display.println(deviceID);
      display.display();

      // Set alert flag for the corresponding device

      for (int i = 0; i < txCount; i++)
      {
        if (txDevices[i].id.compareTo(deviceID) == 0)
        {
          txDevices[i].vitals.alert = true;
          Serial.print("Alert flag set for ");
          Serial.println(deviceID);
          break;
        }
      }
    }
  }
}

void broadcastIDRequest()
{
  Serial.println("Broadcasting ID request...");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Broadcasting ID request...");
  display.display();

  LoRa.beginPacket();
  LoRa.print("Send IDs");
  LoRa.endPacket();
}

void receiveID()
{
  int packetSize = LoRa.parsePacket();
  if (packetSize)
  {
    String id = "";
    while (LoRa.available())
    {
      id += (char)LoRa.read();
    }
    int rssi = LoRa.packetRssi();
    Serial.print("Received ID: ");
    Serial.print(id);
    Serial.print(" with RSSI ");
    Serial.println(rssi);

    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Received ID: ");
    display.print(id);
    display.print(" RSSI: ");
    display.println(rssi);
    display.display();

    String numericID = id.substring(2); // Assuming ID format is "TX1", "TX2", etc.
    Serial.println(numericID);
    delay(2000);
    sendAck(numericID);
    storeTxDevice(id, rssi);
  }
}

void sendAck(String id)
{
  Serial.print("Sending ACK to ");
  Serial.println(id);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Sending ACK to ");
  display.println(id);
  display.display();

  LoRa.beginPacket();
  LoRa.print("ACK " + id);
  LoRa.endPacket();
}

void storeTxDevice(String id, int rssi)
{
  if (txCount < 10)
  {
    txDevices[txCount].id = id;
    txDevices[txCount].rssi = rssi;
    txDevices[txCount].vitals.temperature = 0;
    txDevices[txCount].vitals.diastolic = 0;
    txDevices[txCount].vitals.systolic = 0;
    txDevices[txCount].vitals.spo2 = 0;
    txDevices[txCount].vitals.heartRate = 0;
    txDevices[txCount].vitals.latitude = 0;
    txDevices[txCount].vitals.longitude = 0;
    txDevices[txCount].vitals.alert = false;
    txCount++;
  }
}

void sortTxDevicesByRSSI()
{
  for (int i = 0; i < txCount - 1; i++)
  {
    for (int j = i + 1; j < txCount; j++)
    {
      if (txDevices[i].rssi > txDevices[j].rssi)
      {
        TxInfo temp = txDevices[i];
        txDevices[i] = txDevices[j];
        txDevices[j] = temp;
      }
    }
  }
}

void displayTxDevices()
{
  Serial.println("Sorted TX Devices by RSSI:");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Sorted TX Devices:");

  for (int i = 0; i < txCount; i++)
  {
    Serial.print("ID: ");
    Serial.print(txDevices[i].id);
    Serial.print(" RSSI: ");
    Serial.println(txDevices[i].rssi);

    display.print("ID: ");
    display.print(txDevices[i].id);
    display.print(" RSSI: ");
    display.println(txDevices[i].rssi);
  }
  display.display();
}

void displayLatestArray()
{
  Serial.println("Final Array of Latest TX Devices:");
  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("Final TX Devices:");

  // Find the latest unique IDs with their smallest RSSI
  for (int i = 0; i < txCount; i++)
  {
    bool isUnique = true;
    for (int j = 0; j < i; j++)
    {
      if (txDevices[i].id == txDevices[j].id)
      {
        isUnique = false;
        break;
      }
    }
    if (isUnique)
    {
      Serial.print("ID: ");
      Serial.print(txDevices[i].id);
      Serial.print(" RSSI: ");
      Serial.println(txDevices[i].rssi);

      display.print("ID: ");
      display.print(txDevices[i].id);
      display.print(" RSSI: ");
      display.println(txDevices[i].rssi);
    }
  }
  display.display();
}

void sendBeaconsToTxDevices()
{
  for (int i = 0; i < txCount; i++)
  {
    sendBeacon(txDevices[i].id);
    waitForBeaconAck(txDevices[i].id, i);
  }

  sendVitalsAndAlertsToThingSpeak();
}

/*
  void sendVitalsAndAlertsToThingSpeak()
  {
  if (millis() - lastThingSpeakUpdate < THINGSPEAK_INTERVAL)
    return;

  String vitalsMessage = "";
  vitalsMessage += String(BSD_ID) + "-";

  String alertMessage = "";
  alertMessage += String(BSD_ID) + "-";

  for (int i = 0; i < txCount; i++)
  {
    vitalsMessage += txDevices[i].id + ":";
    vitalsMessage += String(txDevices[i].vitals.temperature) + ",";
    // vitalsMessage += String(txDevices[i].vitals.diastolic) + ",";
    // vitalsMessage += String(txDevices[i].vitals.systolic) + ",";
    vitalsMessage += String(txDevices[i].vitals.spo2) + ",";
    vitalsMessage += String(txDevices[i].vitals.heartRate) + ",";
    vitalsMessage += String(txDevices[i].vitals.latitude) + ",";
    vitalsMessage += String(txDevices[i].vitals.longitude) + ";";

    if (txDevices[i].vitals.alert)
      alertMessage += txDevices[i].id + ";";
  }

  ThingSpeak.setField(BSD_ID, vitalsMessage);
  int response = ThingSpeak.writeFields(vitalsChannelID, vitalsWriteAPIKey);

  if (response == 200)
    Serial.println("ThingSpeak vitals update successful");
  else
    Serial.println("Problem updating ThingSpeak vitals");

  ThingSpeak.setField(BSD_ID, alertMessage);
  response = ThingSpeak.writeFields(alertsChannelID, alertsWriteAPIKey);

  if (response == 200)
    Serial.println("ThingSpeak alerts update successful");
  else
    Serial.println("Problem updating ThingSpeak alerts");

  lastThingSpeakUpdate = millis();
  }
*/

void sendVitalsAndAlertsToThingSpeak()
{
  if (millis() - lastThingSpeakUpdate < THINGSPEAK_INTERVAL)
  {
    return;
  }
  String vitalsMessage = "";
  vitalsMessage += String(BSD_ID) + "-";

  String alertMessage = "";
  alertMessage += String(BSD_ID) + "-";

  for (int i = 0; i < txCount; i++)
  {
    vitalsMessage += txDevices[i].id + ":";
    vitalsMessage += String(txDevices[i].vitals.temperature) + ",";
    // Uncomment these lines if diastolic and systolic are required
    // vitalsMessage += String(txDevices[i].vitals.diastolic) + ",";
    // vitalsMessage += String(txDevices[i].vitals.systolic) + ",";
    vitalsMessage += String(txDevices[i].vitals.spo2) + ",";
    vitalsMessage += String(txDevices[i].vitals.heartRate) + ",";
    vitalsMessage += String(txDevices[i].vitals.latitude) + ",";
    vitalsMessage += String(txDevices[i].vitals.longitude) + ";";

    if (txDevices[i].vitals.alert)
      alertMessage += txDevices[i].id + ";";
  }

  ThingSpeak.setField(BSD_ID, vitalsMessage);
  int response = ThingSpeak.writeFields(vitalsChannelID, vitalsWriteAPIKey);

  if (response == 200)
    Serial.println("ThingSpeak vitals update successful");
  else
    Serial.println("Problem updating ThingSpeak vitals");

  if (lastAlertMessage != alertMessage)
  {
    ThingSpeak.setField(BSD_ID, alertMessage);
    response = ThingSpeak.writeFields(alertsChannelID, alertsWriteAPIKey);

    if (response == 200)
    {
      Serial.println("ThingSpeak alerts update successful");
      lastAlertMessage = alertMessage;
    }
    else
      Serial.println("Problem updating ThingSpeak alerts");
  }

  lastThingSpeakUpdate = millis();
}

void sendBeacon(String id)
{
  Serial.print("Sending Beacon to: ");
  Serial.println(id);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("Sending Beacon to: ");
  display.println(id);
  display.display();

  LoRa.beginPacket();
  LoRa.print("Beacon " + id);
  LoRa.endPacket();
}

/*
  void waitForBeaconAck(String id) {
  unsigned long startWait = millis();
  while (millis() - startWait < 5000) { // Wait for up to 5 seconds for the acknowledgment
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      String message = "";
      while (LoRa.available()) {
        message += (char)LoRa.read();
      }

      if (message == "Beacon received " + id) {
        Serial.print("Beacon received by: ");
        Serial.println(id);

        display.clearDisplay();
        display.setCursor(0, 0);
        display.print("Beacon received by: ");
        display.println(id);
        display.display();
        return;
      }
    }
  }
  Serial.print("No Beacon ACK from: ");
  Serial.println(id);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("No Beacon ACK from: ");
  display.println(id);
  display.display();
  }
*/

void waitForBeaconAck(String id, int i)
{
  unsigned long startWait = millis();
  while (millis() - startWait < 5000)
  { // Wait for up to 5 seconds for the acknowledgment
    int packetSize = LoRa.parsePacket();
    if (packetSize)
    {
      String message = "";
      while (LoRa.available())
        message += (char)LoRa.read();

      // Check if the message is an acknowledgment with the expected format
      // if (message.startsWith("Beacon received " + id)) {
      // String tx_string = message.substring(("Beacon received " + id).length() + 1); // Extract the TX string

      Serial.println(message);

      display.clearDisplay();
      display.setCursor(0, 0);
      display.println(message);
      display.display();

      // message format: "temperature_dia_sys_spo2_hr_lat,lon_"
      txDevices[i].vitals.temperature = message.substring(0, message.indexOf("_")).toFloat();
      message.remove(0, message.indexOf("_") + 1);
      txDevices[i].vitals.diastolic = message.substring(0, message.indexOf("_")).toInt();
      message.remove(0, message.indexOf("_") + 1);
      txDevices[i].vitals.systolic = message.substring(0, message.indexOf("_")).toInt();
      message.remove(0, message.indexOf("_") + 1);
      txDevices[i].vitals.spo2 = message.substring(0, message.indexOf("_")).toInt();
      message.remove(0, message.indexOf("_") + 1);
      txDevices[i].vitals.heartRate = message.substring(0, message.indexOf("_")).toInt();
      message.remove(0, message.indexOf("_") + 1);
      txDevices[i].vitals.latitude = message.substring(0, message.indexOf(",")).toFloat();
      message.remove(0, message.indexOf(",") + 1);
      txDevices[i].vitals.longitude = message.substring(0, message.indexOf("_")).toFloat();

      return;

      /*
        Serial.print("Beacon received by: ");
        Serial.println(id);
        Serial.print("TX String: ");
        Serial.println(tx_string);

        display.clearDisplay();
        display.setCursor(0, 0);
        display.print("Beacon received by: ");
        display.println(id);
        display.print("TX String: ");
        display.println(tx_string);
        display.display();
        return;
      */
    }
  }

  // If no acknowledgment is received within the time frame
  Serial.print("No Beacon ACK from: ");
  Serial.println(id);

  display.clearDisplay();
  display.setCursor(0, 0);
  display.print("No Beacon ACK from: ");
  display.println(id);
  display.display();
}
