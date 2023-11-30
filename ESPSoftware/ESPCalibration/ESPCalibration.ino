/****************************************************
 * File: ESPCalibration.ino                         *
 * Group: The Embedded Dragon (T.E.D.)              *
 * Members: Blake Wingard     - cwingard@uab.edu    *
 *          Gaurav Lunagariya - glunagar@uab.edu    *
 *          Joe Wong          - jawong1@uab.edu     *
 ****************************************************
 * Desc: The purpose of this file to calibrate the  *
 * HX711 for an attached scale. The instructions to *
 * calibrate the sensor are displayed in serial and *
 * on the LCD screen.                               *
 ****************************************************/

// Includes
#include "HX711.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// LCD
//                lcd(addr,en,rw,rs,d4,d5,d6,d7,bl, blpol)
LiquidCrystal_I2C lcd(0x3F, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);

// LCD_I2C pin defines:
#define SDA_PIN SDA
#define SDA_PORT PORTC
#define SCL_PIN SCL
#define SCL_PORT PORTC

// HX711 circuit wiring
#define LOADCELL_DOUT_PIN 2
#define LOADCELL_SCK_PIN  0

// Interrupt pin defines:
#define BUTTON_PIN_1 26
#define BUTTON_PIN_2 27
#define BUTTON_PIN_3 33

// Serial defines
#define BAUD_RATE 115200

volatile bool buttonPressed = false;

HX711 scale;

void setup() {
  Serial.begin(BAUD_RATE);

  // Scale Set Up:
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  // LCD Set Up:
  lcd.begin(20, 4);
  lcd.backlight();
  lcd.clear();

  // Interrupts Set Up:
  // Set button pins with a pull-up resistor
  pinMode(BUTTON_PIN_1, INPUT_PULLUP);                                         
  pinMode(BUTTON_PIN_2, INPUT_PULLUP);                                         
  pinMode(BUTTON_PIN_3, INPUT_PULLUP);                                         

  // Triggers on a falling edge (button press)
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN_1), buttonPress, FALLING);  
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN_2), buttonPress, FALLING);  
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN_3), buttonPress, FALLING);  
}

void loop() {

  if (scale.is_ready()) {
    // set scale to factory default
    Serial.println("Tare... remove any weights from the scale.");
    Serial.println("Press a button when done...");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Tare...");
    lcd.setCursor(0, 1);
    lcd.print("remove any weights");
    lcd.setCursor(4, 3);
    lcd.print("Press button");

    // wait for user to press a button
    buttonPressed = false;  // Reset the buttonPressed flag
    while (!buttonPressed);

    // clear the scale
    scale.set_scale();

    // display current readings
    long reading = scale.get_units(10);
    Serial.print("Pre-tare reading: ");
    Serial.println(reading);
    Serial.println("Press a button when done...");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Pre-tare reading: ");
    lcd.setCursor(0, 1);
    lcd.print(int(reading), 1);
    lcd.setCursor(4, 3);
    lcd.print("Press button");


    // wait for user to press a button
    buttonPressed = false;  // Reset the buttonPressed flag
    while (!buttonPressed);

    // tare scale
    scale.tare();

    Serial.println("Tare done...");
    Serial.println("Place a known weight on the scale...");
    Serial.println("Press a button when done...");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Tare done...");
    lcd.setCursor(0, 1);
    lcd.print("Place weight");
    lcd.setCursor(4, 3);
    lcd.print("Press button");

    // wait for user to press a button
	delay(10); // crude debounce
    buttonPressed = false;  // Reset the buttonPressed flag
    while (!buttonPressed)
      ;

    // report reading
    reading = scale.get_units(10);

    Serial.print("Result: ");
    Serial.println(reading);
    Serial.println("Calibration factor will be the (reading)/(known weight)");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Calibration factor:");
    lcd.setCursor(3, 1);
    lcd.print("result/weight");
    lcd.setCursor(4, 3);
    lcd.print("Press button");

    // wait for user to press a button
	delay(10); // crude debounce
    buttonPressed = false;  // Reset the buttonPressed flag
    while (!buttonPressed);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Result: ");
    lcd.setCursor(1, 1);
    lcd.print(int(reading), 1);
    lcd.setCursor(4, 2);
    lcd.print("Press button");
    lcd.setCursor(5, 3);
    lcd.print("to restart");

    // wait for user to press a button
	delay(10); // crude debounce
    buttonPressed = false;  // Reset the buttonPressed flag
    while (!buttonPressed);
  } else {
    Serial.println("HX711 not found.");
  }
  delay(1000);
}

// FUNCTIONS:
void buttonPress() {
  buttonPressed = true;
}
