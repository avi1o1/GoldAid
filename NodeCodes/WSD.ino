/*
***WSD Code***
  Working: BSD receives IDs from WSDs and resets the Send ID flag on WSD end by sending an ack.

***WSD Code***
  Working: BSD receives IDs from WSDs and resets the Send ID flag on WSD end by sending an ack.
  Done till STEP 3
*/

#include <Wire.h> //I2C library
#include "SPI.h" //SPI library
#include "max32664.h" //PulseOx
#include <Adafruit_MPU6050.h>
#include <LoRa.h>
#include <TinyGPS.h>
TinyGPS gps;
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define RESET_PIN 14
#define MFIO_PIN 12
#define RAWDATA_BUFFLEN 250
#define FALL_EXPECTED_COUNT 10
#define STATUS 25

const int buttonPin1 = 26; // Button connected to GPIO 26
const int buttonPin2 = 27; // Button connected to GPIO 27

Adafruit_MPU6050 mpu;
volatile bool mpuInterrupt = false;

String tx_string;

const long frequency = 433E6;  // LoRa frequency
const int csPin = 5;          // LoRa radio chip select
const int resetPin = 33;       // LoRa radio reset
const int irqPin = 32;          // change for your board; must be a hardware interrupt pin

long old_millis;
uint8_t acc_sampling_period = 1; //4ms, that means sampling freq = 250 Hz
float fall_thresh = 1; // This is the value below which the µC assumes the body is going into free-fall
float tap_thresh = 6;
boolean fall_flag = 0;
boolean fall_certain = 0;

boolean fall_confirm = 0;       // This flag confirms the fall
uint8_t fall_confirm_count = 0; // Counter for number of falls detected
long measure_count = 0;
long fall_count = 0;
long not_fall_count = 0;
long total_count = 0;
long start_millis, end_millis;
float fall_min_time = 100; //500 ms, more info is given in the above comments
uint8_t fall_confidence = 70;
uint8_t not_fall_confidence = 10;
float confidence2;
uint8_t flag_thinkspeak = 1;

static long startMillis = 0;
static long startMillis_1 = 0;
static long fallCount = 0;
static long totalCount = 0;
static boolean fallFlag = false;
static boolean fallCertain = false;
const long vitalsDuration = 25000;  // 25 seconds for sending vitalsDuration
long old_millis_pulseox;
int pulseox_sampling_period = 10000;
boolean LoRa_send_flag = 0;

long old_millis_LoRa;
int LoRa_transmit_period = 30000;

uint16_t dataReadyFlag, responseFromI2C;
float temp;
unsigned long pressStartTime = 0;

long counter = 0;
int  interval = 1000;

max32664 MAX32664(RESET_PIN, MFIO_PIN, RAWDATA_BUFFLEN);

void mfioInterruptHndlr() {
  Serial.println("i");
}

void enableInterruptPin() {
  //pinMode(mfioPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(MAX32664.mfioPin), mfioInterruptHndlr, FALLING);
}

void loadAlgomodeParameters() {

  algomodeInitialiser algoParameters;
  /*  Replace the predefined values with the calibration values taken with a reference spo2 device in a controlled environt.
      Please have a look here for more information, https://pdfserv.maximintegrated.com/en/an/an6921-measuring-blood-pressure-MAX32664D.pdf
      https://github.com/Protocentral/protocentral-pulse-express/blob/master/docs/SpO2-Measurement-Maxim-MAX32664-Sensor-Hub.pdf
  */

  algoParameters.calibValSys[0] = 120;
  algoParameters.calibValSys[1] = 122;
  algoParameters.calibValSys[2] = 125;

  algoParameters.calibValDia[0] = 80;
  algoParameters.calibValDia[1] = 81;
  algoParameters.calibValDia[2] = 82;

  algoParameters.spo2CalibCoefA = 1.5958422;
  algoParameters.spo2CalibCoefB = -34.659664;
  algoParameters.spo2CalibCoefC = 112.68987;

  MAX32664.loadAlgorithmParameters(&algoParameters);
}

String deviceID = "TX3"; // Change this ID for each Tx device
bool sendingID = false;
bool ack_flag;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }

  display.clearDisplay(); // Clear the display
  display.setTextSize(2);  // Adjust the text size as needed
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 20);
  display.print(F("GoldAid"));
  display.display();
  display.setTextSize(1);  // Adjust the text size as needed
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 40);
  display.print(F("WSD1"));
  display.display(); // Display initialization
  delay(2000);

  Serial.println("STARTED");
  Serial2.begin(9600);

  Wire.begin();

  pinMode(buttonPin1, INPUT_PULLUP); // Set buttonPin1 as input with internal pull-up resistor
  pinMode(buttonPin2, INPUT_PULLUP); // Set buttonPin2 as input with internal pull-up resistor

  /***********MPU-6050 starts***************/
  if (!mpu.begin())
  {
    Serial.println("Failed to find MPU6050 chip");
    while (1)
    {
      delay(10);
    }
  }
  mpu.setAccelerometerRange(MPU6050_RANGE_16_G); //accel.setRange(ADXL345_RANGE_16_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  Serial.println("MPU 6050 initialised");
  /***********MPU-6050 ends***************/

  /**********PulseOx setup starts**********/
  loadAlgomodeParameters();

  if (MAX32664.hubBegin() != CMD_SUCCESS)
  {
    Serial.println("MAX32664 not initialised");
    while (1);
  }

  display.clearDisplay();
  display.setCursor(0, 22);
  display.print("Caliberation Started");
  display.display();

  bool ret = MAX32664.startBPTcalibration();
  while (!ret) {
    delay(10000);
    Serial.println("failed calib, please retsart");
    //ret = MAX32664.startBPTcalibration();
  }

  delay(1000);

  Serial.println("start in estimation mode");
  ret = MAX32664.configAlgoInEstimationMode();
  while (!ret) {

    Serial.println("failed est mode");
    ret = MAX32664.configAlgoInEstimationMode();
    delay(10000);
  }

  //MAX32664.enableInterruptPin();
  Serial.println("Getting the device ready..");
  delay(1000);

  LoRa.setPins(csPin, resetPin, irqPin);
  if (!LoRa.begin(frequency)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }

  LoRa.setSyncWord(0xA5);
  LoRa.setTxPower(10);
  LoRa.setSpreadingFactor(10);
  LoRa.setSignalBandwidth(62.5E3);
  LoRa.setCodingRate4(8);

  Wire.beginTransmission(0x48);
  if (Wire.endTransmission() == 0) Serial.println("TMP117 Found");
  else Serial.println("TMP117 Not Found");

  Wire.beginTransmission(0x48);
  Wire.write(0x01);
  Wire.endTransmission(false);
  Wire.requestFrom(0x48, 2, true);
  uint8_t highbyte = Wire.read();
  uint8_t lowbyte  = Wire.read();
  highbyte |= 0b00001100; // for setting one shot mode

  Wire.beginTransmission(0x48);
  Wire.write(0x01);
  Wire.write(highbyte);
  Wire.write(lowbyte);
  Wire.endTransmission();

  delay(100);
  LoRa.beginPacket();
  //LoRa.print("Hello from WSD" + String(USER_ID));
  LoRa.endPacket();
}

void loop() {
  int packetSize = LoRa.parsePacket();

  if (fallDetection() || buttonPressed()) {
    Serial.println("****Alert generated****");
    display.clearDisplay();
    display.setCursor(0, 22);
    display.print("****Alert generated****");
    display.setCursor(0, 38);
    display.print("Alert Sent");
    display.display();
  }
  else {
    if (packetSize) {
      String message = "";
      while (LoRa.available()) {
        message += (char)LoRa.read();
      }
      Serial.print("###Received message: ");
      Serial.println(message);

      display.clearDisplay();
      display.setCursor(0, 0);
      display.print("Received msg: ");
      display.println(message);
      display.display();

      if (message == "Send IDs") {
        ack_flag = false;
        Serial.println("** Inside Send IDs condition **");
        sendingID = true;
        sendID();
      } else {
        if (message.startsWith("ACK ")) {
          Serial.println("** Inside ACK received condition **");
          String ackID = message.substring(4); // ACK TX1
          if (ackID == deviceID.substring(2)) {
            Serial.println("** Inside ACK matched condition **");
            Serial.println(ackID);
            sendingID = false;
            Serial.println("Received ACK");

            display.clearDisplay();
            display.setCursor(0, 0);
            display.print("Received ACK");
            display.display();
            ack_flag = true;
          }
          else if (!ack_flag) {
            Serial.println(sendingID);
            sendingID = true;
            sendID();
            Serial.println("else- when ACK is not received.");
          }
        }
        else if (message.startsWith("Beacon ")) {
          // New code for handling Beacon
          String beaconID = message.substring(7); // Extract the ID from the beacon message
          if (beaconID == deviceID) {
            sendBeaconAck(); // Send acknowledgment for the beacon
          }
        }
      }
    }
  }
}

void sendID() {
  if (sendingID) {
    Serial.print("Sending ID: ");
    Serial.println(deviceID);
    LoRa.beginPacket();
    LoRa.print(deviceID);
    LoRa.endPacket();

    display.clearDisplay();
    display.setCursor(0, 0);
    display.print("Sending ID: ");
    display.println(deviceID);
    display.display();

    sendingID = false;
    delay(1000); // Send ID every 1 second
  }
}

void sendBeaconAck() {
  if ((millis() - old_millis_LoRa) > LoRa_transmit_period) {
    old_millis_LoRa = millis();

    // Construct and send the data packet
    char TXLoRaChar[80];
    tx_string.toCharArray(TXLoRaChar, 80);

    Serial.print("\n\nString before conversion is:");
    Serial.println(tx_string);
    Serial.print("\n\nString to LoRa is:");
    Serial.println(TXLoRaChar);

    LoRa.beginPacket();
    LoRa.print(TXLoRaChar);
    LoRa.endPacket();
  }

  if ((millis() - old_millis_pulseox) > pulseox_sampling_period) {
    old_millis_pulseox = millis();

    Wire.beginTransmission(0x48);
    Wire.write(0x01);
    Wire.endTransmission(false);
    Wire.requestFrom(0x48, 2, true);
    responseFromI2C = ( Wire.read() << 8 | Wire.read());
    dataReadyFlag = (responseFromI2C >> 13) & 0x0001;

    if (dataReadyFlag) {
      Wire.beginTransmission(0x48);
      Wire.write(0x00);
      Wire.endTransmission(false);
      Wire.requestFrom(0x48, 2, true);
      responseFromI2C = ( Wire.read() << 8 | Wire.read());
      temp = responseFromI2C * 0.0078125; //Resolution = 0.0078125

      Wire.beginTransmission(0x48);
      Wire.write(0x01);
      Wire.endTransmission(false);
      Wire.requestFrom(0x48, 2, true);
      uint8_t highbyte = Wire.read();
      uint8_t lowbyte  = Wire.read();
      highbyte |= 0b00001100; // for setting one shot mode

      Wire.beginTransmission(0x48);
      Wire.write(0x01);
      Wire.write(highbyte);
      Wire.write(lowbyte);
      Wire.endTransmission();
    }

    uint8_t num_samples = MAX32664.readSamples();

    if (num_samples) {

      String hr = String(MAX32664.max32664Output.hr);
      String spo2 = String(MAX32664.max32664Output.spo2);
      String sys = String(MAX32664.max32664Output.sys);
      String dia = String(MAX32664.max32664Output.dia);

      float flat, flon;
      char slat[15], slon[15];
      unsigned long age;

      while (Serial2.available())  gps.encode(Serial2.read());

      gps.f_get_position(&flat, &flon, &age);

      dtostrf(flat, 6, 6, slat);
      String GPS_TX = "";
      GPS_TX = slat;
      GPS_TX += ",";

      dtostrf(flon, 6, 6, slon);
      GPS_TX += slon;

      tx_string = "";
      tx_string += String(temp);
      tx_string += "_";
      tx_string += dia;
      tx_string += "_";
      tx_string += sys;
      tx_string += "_";
      tx_string += spo2;
      tx_string += "_";
      tx_string += hr;
      tx_string += "_";
      tx_string += GPS_TX;
      tx_string += "_";

      display.clearDisplay();
      display.setTextColor(WHITE);
      display.setTextSize(2);
      display.setCursor(30, 0);
      display.print("VITALS");
      display.setTextSize(1);

      display.setCursor(0, 22);
      display.print("HR/SpO2:");
      display.print(hr);
      display.print(" BPM/");
      display.print(spo2);
      display.print(" %");

      display.setCursor(0, 38);
      display.print("Temp:");
      display.print(temp, 1);
      display.print((char)247);
      display.print("C");

      display.setCursor(0, 54);
      display.print("BP:");
      display.print(sys);
      display.print("/");
      display.print(dia);
      display.print("mmHg");
      display.display();
    }
    Serial.println(tx_string);
  }
}

bool fallDetection() {
  sensors_event_t event, g, temp;
  mpu.getEvent(&event, &g, &temp);

  float x = event.acceleration.x / 9.8;
  float y = event.acceleration.y / 9.8;
  float z = event.acceleration.z / 9.8;

  float mag = sqrt(pow(x, 2) + pow(y, 2) + pow(z, 2));

  if (mag < fall_thresh) {
    if (fall_flag == 0) {
      start_millis = millis();
      fall_count = 0;
      total_count = 0;
    }

    fall_flag = 1;
    fall_count++;

    if ((millis() - start_millis) > fall_min_time) {
      float confidence = static_cast<float>(fall_count) / static_cast<float>(total_count) * 100.0;

      if (confidence > fall_confidence) {
        fall_certain = true;
      } else {
        fall_flag = 0;
        fall_count = 0;
        total_count = 0;
      }
    }
  }
  total_count++;

  confidence2 = static_cast<float>(fall_count) / static_cast<float>(total_count) * 100.0;

  if ((confidence2 <= not_fall_confidence) && fall_flag) {
    fall_flag = 0;
    Serial.println("********NOT FALL********");
  }

  if (fall_certain && (mag > 2)) { //XXXXXXXXX previously 6 {
    fall_certain = false;
    Serial.println();
    Serial.println("*******************FALL DETECTED*******************");
    Serial.println();
    //digitalWrite(STATUS, HIGH);


    String alert = "Fall Detected from TX1";
    LoRa.beginPacket();
    LoRa.print(alert);
    LoRa.endPacket();

    display.clearDisplay();
    display.setCursor(0, 22);
    display.print("****Fall Detected****");
    display.setCursor(0, 38);
    display.print("Alert Sent");
    display.display();

    Serial.println("Message sent from TX1: Fall detected");

    long height = 0.5 * 9.8 * pow((millis() - start_millis), 2);
    float height_actual = static_cast<float>(height) / 1000000;

    String fallLoRa = "FALL DETECTED @ " + String(height_actual) + "m#";
    char fallLoRaChar[30];
    fallLoRa.toCharArray(fallLoRaChar, 30);

    Serial.println(fallLoRa);
    Serial.println(fallLoRaChar);

    flag_thinkspeak = 1;
    old_millis_LoRa = millis();


    fall_flag = 0;
    return true;
  }

  return false;
}

bool buttonPressed() {
  if (digitalRead(buttonPin1) == LOW && digitalRead(buttonPin2) == LOW) { // Check if both buttons are pressed
    if (pressStartTime == 0) { // If it's the first time both buttons are pressed
      pressStartTime = millis(); // Record the start time
    } else {
      if (millis() - pressStartTime >= 2000) { // If both buttons have been pressed for at least 2 seconds
        pressStartTime = 0; // Reset the press start time
        LoRa.beginPacket();
        LoRa.print("SOS Alert");
        LoRa.endPacket();

        display.clearDisplay();
        display.setCursor(0, 22);
        display.print("****SOS Alert****");
        display.setCursor(0, 38);
        display.print("Alert Sent");
        display.display();

        Serial.println("Buttons pressed");
        Serial.println("****SOS Alert****");
        return true; // Return true to indicate both buttons were pressed for 2 seconds
      }
    }
  } else {
    pressStartTime = 0; // Reset the press start time if buttons are not pressed together
  }
  return false; // Return false if both buttons are not pressed for 2 seconds
}
