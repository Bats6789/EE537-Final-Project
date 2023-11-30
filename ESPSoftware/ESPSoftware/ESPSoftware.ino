/****************************************************
 * File: ESPSoftware.ino                            *
 * Group: The Embedded Dragon (T.E.D.)              *
 * Members: Blake Wingard     - cwingard@uab.edu    *
 *          Gaurav Lunagariya - glunagar@uab.edu    *
 *          Joe Wong          - jawong1@uab.edu     *
 ****************************************************
 * Desc: The purpose of this file is to drive the   *
 * scale's functionality. There are a few items     *
 * that need to be set before the code will         *
 * function properly.                               *
 *      1: Set WiFi config (username/password)      *
 *      2: Set ThinkSpeak (Api/channel)             *
 *      3: Set calibration factor for the scale.    *
 ****************************************************
 * Note: If you do not have a calibration factor,   *
 * please load ESPCalibration.ino first.            *
 ****************************************************/

//=============================================================================
// SETUP (DO THIS BEFORE LOADING)
// Scale settings
#define calibration_factor_custom <factor>

// WiFi settings
#define NETWORK_NAME "<WiFi-Name>"
#define NETWORK_PASS "<WiFi-Password>"

// ThingSpeak settings
#define API_KEY "<API-KEY>"
#define CHANNEL_ID <channel-ID>

//=============================================================================
// INCLUDES & DEFINES:
// General includes:
#include "HX711.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ThingSpeak.h>
#include <WiFi.h>

// LCD_I2C pin defines:
#define SDA_PIN SDA
#define SDA_PORT PORTC
#define SCL_PIN SCL
#define SCL_PORT PORTC

// Load Cell pin defines:
#define LOADCELL_DOUT_PIN 2
#define LOADCELL_SCK_PIN 0

// Interrupt pin defines:
#define BUTTON_PIN_1 26
#define BUTTON_PIN_2 27
#define BUTTON_PIN_3 33
#define LED_PIN 13

// BLE defines - UUID Generator: https://www.uuidgenerator.net/
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID_1 "1c95d5e3-d8f7-413a-bf3d-7a2e5d7be87e"

// Serial port settings:
#define BAUD_RATE 115200

// Timer settings
#define TIME_CONDITION 15000         //   15000ms == 15s
#define WAIT_FOR_WEIGHING 5000       //    5000ms ==  5s
#define WAIT_FOR_IDLE 2000000        // 2000000us ==  2s
#define WAIT_FOR_DEEP_SLEEP 2000000  // 2000000us ==  2s
#define LCD_PRINT_DELAY 1000         //    1000ms ==  1s
#define CONNECTION_DELAY 60000       //   60000ms == 60s

// Scale settings
#define WEIGHT_THRESHOLD 2

// DEBUG INFO
#define SERIAL_DEBUG  // Serial debug information
// #define LCD_DEBUG  // LCD debug information
#define WIFI_DEBUG
#define BUTTON1_DEBUG  // Button1 enabled
#define BUTTON2_DEBUG  // Button2 enabled
#define BUTTON3_DEBUG  // Button3 enabled
//=============================================================================
// TYPEDEFS
typedef enum unitType_t {
  imperial,
  metric
} unitType_t;

typedef enum state_t {
  idle,
  active,
  done,
  asleep,
  calibrating,
  deepSleep
} state_t;
//=============================================================================
// FUNCTION PROTOTYPES
void setUnits(unitType_t unitType);
void IRAM_ATTR onIdle(void);
void IRAM_ATTR onSleep(void);
//=============================================================================
// VARIABLES AND OBJECTS SETUP:
// Interrupt set up:
#ifdef BUTTON1_DEBUG
volatile bool button1Pressed = false;  // Flag to indicate button 1 press
#endif
#ifdef BUTTON2_DEBUG
volatile bool button2Pressed = false;  // Flag to indicate button 2 press
#endif
#ifdef BUTTON3_DEBUG
volatile bool button3Pressed = false;  // Flag to indicate button 3 press
#endif

// Timer interrupts
hw_timer_t* idleTimer = NULL;
hw_timer_t* deepSleepTimer = NULL;

// State variables
#ifdef BUTTON1_DEBUG
bool button1State = false;  // State associated with button 1 press
#endif
unitType_t unitType = imperial;
state_t state = idle;

// Non-blocking delay setup:
unsigned long timeSinceLastPrint = millis();
unsigned long timeSinceLastStateChange = millis();
unsigned long timeSinceLastConnection = millis();

// Load cell sensor
HX711 scale;

// LCD
//                lcd(addr,en,rw,rs,d4,d5,d6,d7,bl, blpol)
LiquidCrystal_I2C lcd(0x3F, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

// BLE - pointers setup:
BLEServer* server = NULL;
BLECharacteristic* characteristic = NULL;

// BLE - callback for when a client connects or disconnects
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) {
    // Determine which units to use from the phone
    std::string rxValue = characteristic->getValue();

    if (rxValue[0] == 'K') {
      unitType = metric;
    } else if (rxValue[0] == 'L') {
      unitType = imperial;
    }

    setUnits(unitType);

#ifdef SERIAL_DEBUG
    Serial.print("Message from the phone (getValue): ");
    Serial.println(rxValue.c_str());
#endif
  }

  void onDisconnect(BLEServer* server) {
    // give the bluetooth stack the chance to get things ready
    delay(500);
    server->startAdvertising();
#ifdef SERIAL_DEBUG
    Serial.println("start advertising");
#endif
  }
};

// Cloud
WiFiClient client;

//=============================================================================
// SETUP FUNCTION:
void setup() {

#ifdef SERIAL_DEBUG
  Serial.begin(BAUD_RATE);
#endif

  // BLE - Setup:
  BLEDevice::init("T.E.D.");

  server = BLEDevice::createServer();
  server->setCallbacks(new MyServerCallbacks());

  BLEService* Service = server->createService(SERVICE_UUID);

  characteristic = Service->createCharacteristic( CHARACTERISTIC_UUID_1,
    BLECharacteristic::PROPERTY_READ |
	BLECharacteristic::PROPERTY_WRITE |
	BLECharacteristic::PROPERTY_NOTIFY);

  Service->start();

  // BLE - Start advertising server
  BLEAdvertising* Advertising = BLEDevice::getAdvertising();
  Advertising->addServiceUUID(SERVICE_UUID);
  Advertising->setScanResponse(false);
  Advertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
#ifdef SERIAL_DEBUG
  Serial.println("Waiting a client connection to notify...");
#endif

  // Scale Set Up:
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale(calibration_factor_custom);
  scale.tare();

  // LCD Set Up & Verification:
  lcd.begin(20, 4);
#ifdef LCD_DEBUG
  for (int i = 0; i < 3; i++) {  // 3 quick blinks of backlight
    lcd.backlight();
    delay(200);
    lcd.noBacklight();
    delay(200);
  }
  lcd.backlight();
  lcd.clear();
  lcd.write("Testing 1.. 2..");
#else
  lcd.backlight();
  lcd.clear();
  lcdPrintIdle();
#endif

  // Interrupts Set Up:
#ifdef BUTTON1_DEBUG
  // Set button pin with a pull-up resistor
  pinMode(BUTTON_PIN_1, INPUT_PULLUP);                                          
  // Triggers on a falling edge (button press)
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN_1), button1Press, FALLING);  
#endif

#ifdef BUTTON2_DEBUG
  // Set button pin with a pull-up resistor
  pinMode(BUTTON_PIN_2, INPUT_PULLUP);                                          
  // Triggers on a falling edge (button press)
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN_2), button2Press, FALLING);  
#endif

#ifdef BUTTON3_DEBUG
  // Set button pin with a pull-up resistor
  pinMode(BUTTON_PIN_3, INPUT_PULLUP);                                          
  // Triggers on a falling edge (button press)
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN_3), button3Press, FALLING);  
#endif

  // LED Pin Set Up:
#ifdef BUTTON1_DEBUG
  pinMode(LED_PIN, OUTPUT);
#endif

  // TIMER interrupts
  // TIMER<n>, prescaller = 80, counting up
  idleTimer = timerBegin(0, 80, true);
  deepSleepTimer = timerBegin(2, 80, true);

  // Attach the interrupts to the timers
  timerAttachInterrupt(idleTimer, &onIdle, true);
  timerAttachInterrupt(deepSleepTimer, &onSleep, true);

  // Set when the event should fire (time in us)
  timerAlarmWrite(idleTimer, WAIT_FOR_IDLE, true);
  timerAlarmWrite(deepSleepTimer, WAIT_FOR_DEEP_SLEEP, true);

  // Enable generation of timer events
  timerAlarmEnable(idleTimer);
  timerAlarmEnable(deepSleepTimer);

  // Stop the timer for now
  timerStop(idleTimer);
  timerStop(deepSleepTimer);

  // Cloud
  WiFi.mode(WIFI_STA);
  WiFi.begin(NETWORK_NAME, NETWORK_PASS);
  ThingSpeak.begin(client);
  timeSinceLastConnection = millis();
}
//=============================================================================
// LOOP:
void loop() {
  unsigned long timeNow = millis();

  // Check WiFi connection status
  if (WiFi.status() != WL_CONNECTED &&
  timeNow - timeSinceLastConnection > CONNECTION_DELAY) {
    timeSinceLastConnection = timeNow;
#if defined SERIAL_DEBUG && defined WIFI_DEBUG
    Serial.println("Attempting to connect");
#endif
    WiFi.begin(NETWORK_NAME, NETWORK_PASS);
#if defined SERIAL_DEBUG && defined WIFI_DEBUG
    if (WiFi.status() == WL_CONNECTED)
      Serial.println("Connected");
#endif
  }
#ifdef LCD_DEBUG
  if (timeNow - timeSinceLastPrint > LCD_PRINT_DELAY) {

    // Scale Output via LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Reading: ");
    lcd.print(int(scale.get_units()), 1);
    lcdPrintUnitSuffix(unitType);

    lcd.setCursor(0, 2);
    lcd.print("Raw Data: ");
    lcdPrintRawData(unitType);

    // Resets timer
    timeSinceLastPrint = timeNow;
  }
#endif

  // State management
  int readWeight = (int)scale.get_units();

  // Check if the weight value is below the threshold
  if (readWeight < WEIGHT_THRESHOLD) {
    // idle action
    if (state == active) {
      timeSinceLastStateChange = timeNow;
      state = calibrating;
      // calibrate
      scale.tare();
#ifndef LCD_DEBUG
      lcdPrintCalibrating();
      timerRestart(idleTimer);
      timerStart(idleTimer);
#endif
    }

	// done weighing
    if (state == done) {
      timeSinceLastStateChange = timeNow;
      state = idle;
#ifndef LCD_DEBUG
      lcdPrintIdle();
#endif
    }
  } else {
  // wake from sleep
    if (state == asleep || state == deepSleep) {
      wakeDevice();
      timeSinceLastStateChange = timeNow;
      state = active;
    }

    // active action
    if (state == idle) {
      timeSinceLastStateChange = timeNow;
      state = active;
    }

#ifndef LCD_DEBUG
	// flash weight every second until done
    if (state == active && timeNow - timeSinceLastPrint > LCD_PRINT_DELAY) {
      // Scale Output via LCD
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(int(scale.get_units()), 1);
      lcdPrintUnitSuffix(unitType);
      timeSinceLastPrint = timeNow;
    }
#endif
  }

  // sleep action
  if (state == idle && timeNow - timeSinceLastStateChange >= TIME_CONDITION) {
    timeSinceLastStateChange = timeNow;
    sleepDevice();
    state = asleep;
    timerRestart(deepSleepTimer);
    timerStart(deepSleepTimer);
  } else if (state == idle) {
    lcdPrintIdle();
    timerStop(idleTimer);
  }

  // deep sleep
  if (state == deepSleep) {
    lcd.noBacklight();
    lcd.noDisplay();
    timerStop(deepSleepTimer);
  }

  // wait for user to stay on scale for 5 seconds
  if (state == active &&
  timeNow - timeSinceLastStateChange >= WAIT_FOR_WEIGHING) {
    timeSinceLastStateChange = timeNow;
    state = done;
    // display weight
#ifndef LCD_DEBUG
    lcdPrintWeight(readWeight, unitType);
#endif
    // write to BLE
    String txValue = "R";
    characteristic->setValue(txValue.c_str());
    characteristic->notify();
    delay(50);

    txValue = String(readWeight);
    characteristic->setValue(txValue.c_str());
    characteristic->notify();

    // write to cloud
    {
      int response = ThingSpeak.writeField(CHANNEL_ID, 1, readWeight, API_KEY);

#if defined SERIAL_DEBUG && defined WIFI_DEBUG
      if (response == 200) {
        Serial.println("Message sent to cloud");
      } else {
        Serial.println("Other error");
        lcd.setCursor(0, 1);
        lcd.print("ERROR: ");
        lcd.print(response);
      }
#endif
    }
  }

  // Interrupts:
  // Toggle the sleep or wake when BUTTON 1 is pressed
#ifdef BUTTON1_DEBUG
  if (button1Pressed) {
    button1State = !button1State;         // toggle button 1 state
    digitalWrite(LED_PIN, button1State);  // visual of button 1 state

    // JUST EXPERIMENTING:
    String txValue = "R";
    characteristic->setValue(txValue.c_str());
    characteristic->notify();
    delay(50);

    txValue = String(scale.get_units());
    characteristic->setValue(txValue.c_str());
    characteristic->notify();

    button1Pressed = false;  // Reset the button1Pressed flag
  }
#endif

  // Toggle scale units between lbs & kgs when BUTTON 2 is pressed.
  // Kilograms when unit state is true, else pounds when false.
#ifdef BUTTON2_DEBUG
  if (button2Pressed) {
    unitType = unitType == metric ? imperial : metric;
    setUnits(unitType);
    button2Pressed = false;  // Reset the button2Pressed flag
  }
#endif

  // Turns on LCD screen when BUTTON 3 is pressed
#ifdef BUTTON3_DEBUG
  if (button3Pressed) {
    wakeDevice();
    button3Pressed = false;  // Reset the button3Pressed flag
  }
#endif
}

//=============================================================================
// FUNCTIONS:
void button1Press() {
  button1Pressed = true;  // Set the flag to indicate button 1 press
}

#ifdef BUTTON2_DEBUG
void button2Press() {
  button2Pressed = true;  // Set the flag to indicate button 2 press
}
#endif

void button3Press() {
  button3Pressed = true;  // Set the flag to indicate button 3 press
}

// Set the units for the scale
void setUnits(unitType_t unitType) {
  switch (unitType) {
    case metric:
      scale.set_scale(2.2 * calibration_factor_custom);
      break;
    case imperial:
    default:
      scale.set_scale(calibration_factor_custom);
      break;
  }
}

// Print the units on the LCD
void lcdPrintUnitSuffix(unitType_t unitType) {
  switch (unitType) {
    case metric:
      lcd.print(" [kgs]");
      break;
    case imperial:
    default:
      lcd.print(" [lbs]");
      break;
  }
}

// Print raw data to LCD
void lcdPrintRawData(unitType_t unitType) {
  switch (unitType) {
    case metric:
      lcd.print(abs(scale.read()));
      break;
    case imperial:
    default:
      lcd.print(abs(scale.read()));
      break;
  }
}

// Start putting the device to sleep
void sleepDevice(void) {
#ifdef SERIAL_DEBUG
  Serial.println("Going to sleep now...");
#endif
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Sleeping...");
}

// Wake the device
void wakeDevice(void) {
#ifdef SERIAL_DEBUG
  Serial.println("I'm awake!");
#endif
  lcd.backlight();
  lcd.display();
}

// Print idle message to LCD
void lcdPrintIdle(void) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Waiting...");
}

// Print Calibration message to LCD
void lcdPrintCalibrating(void) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Calibrating...");
}

// Print weight to LCD
void lcdPrintWeight(int weight, unitType_t unitType) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(weight);
  lcdPrintUnitSuffix(unitType);
}

// Idle interrupt
void IRAM_ATTR onIdle(void) {
  state = idle;
}

// sleep interrupt
void IRAM_ATTR onSleep(void) {
  state = deepSleep;
}
