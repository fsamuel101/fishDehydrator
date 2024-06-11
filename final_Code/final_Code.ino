#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>
#include "HX711.h"
#include <EEPROM.h>

//this is for GSM900a
SoftwareSerial myGSM(8,9); //rx, tx

//this is for the pin of  load sensor
uint8_t dataPin = 4;
uint8_t clockPin = 5;

//this is for the button
#define BUTTON_PIN_GREEN 3 //for the button
#define BUTTON_PIN_YELLOW 2 //for the button
#define BUTTON_PIN_RED 10 // for the red button

//this is for the Temperature sensor
#define DHTPIN 12
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
#define TEMPERATURE_READ_INTERVAL 5000
unsigned long lastTempReadTime = 0;

//this is for relays
#define RELAY_PIN1 6 // heater 
#define RELAY_PIN2 7


//this is for the display
LiquidCrystal_I2C lcd(0x27, 20, 4);


//this is for the data in loadcell
HX711 scale;
const int calVal_eepromAdress = 0;
float w1, w2, previous = 0;
float initialWeight;

//this is for the button
int GREENButtonState;
int YELLOWButtonState;
int REDButtonState;


void setup() {
  lcd.begin(20, 4);
  lcd.backlight();

  pinMode(BUTTON_PIN_GREEN, INPUT_PULLUP);
  pinMode(BUTTON_PIN_YELLOW, INPUT_PULLUP);
  pinMode(BUTTON_PIN_RED, INPUT_PULLUP);
  pinMode(RELAY_PIN1, OUTPUT);
  pinMode(RELAY_PIN2, OUTPUT);
  myGSM.begin(9600); //baud rate of GSM Module or set to 9600
  delay(100);

  customSetup();
  
  dht.begin();

  scale.begin(dataPin, clockPin);
  float calibrationValue;
  EEPROM.get(calVal_eepromAdress, calibrationValue);
  scale.set_scale(calibrationValue);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("    Drying Setup");
  lcd.setCursor(0, 2);
  lcd.print("      Taring..");
  lcd.setCursor(0, 3);
  lcd.print("      Do not touch");
  delay(5000);

  scale.tare();

  lcd.clear();
  lcd.setCursor(0, 2);
  lcd.print("       Wait a moment..");
  lcd.setCursor(0, 3);
  lcd.print("       Do not touch");
  delay(2000);

  dryingSetup();
  sendFirst();
  
}

void loop(){

  unsigned long time = millis();
  unsigned long hours = time / 3600000UL;
  unsigned long minutes = (time % 3600000UL) / 60000UL;
  float fractionOfHour = (time % 60000UL) / 3600000.0;
  float temp = dht.readTemperature();
  float humidity = dht.readHumidity();

  digitalWrite(RELAY_PIN2, HIGH);
  
  w1 = scale.get_units(10);
  delay(100);
  w2 = scale.get_units();
  while (abs(w1 - w2) > 10)
  {
     w1 = w2;
     w2 = scale.get_units();
     delay(100);
  }

  w1 /= 1000;

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("      Drying...");
  lcd.setCursor(0,1);
  lcd.print("Weight: ");
  lcd.print(w1);
  lcd.print("kg");

  if (time - lastTempReadTime >= TEMPERATURE_READ_INTERVAL) {
    if (hours < 4) {
        // Control temperature for the first four hours (38-43°C)
        if (temp < 37) {
            digitalWrite(RELAY_PIN1, HIGH);           
        } else if (temp > 42) {
            digitalWrite(RELAY_PIN1, LOW);      
        }
    } else {
        // Control temperature for the remaining time (54-59°C)
        if (temp < 53) {
            digitalWrite(RELAY_PIN1, HIGH);           
        } else if (temp > 58) {
            digitalWrite(RELAY_PIN1, LOW);
        }
    }
    lastTempReadTime = time;  // Update the last reading time
  }
  lcd.setCursor(0,2);
  lcd.print("Temp:");
  lcd.print(temp);
  lcd.print("C");
  lcd.setCursor(0,3);
  lcd.print("Humidity:");
  lcd.print(humidity);
  lcd.print("%");


  if(w1<initialWeight*0.45 || hours > 17){
    lcd.clear();
    digitalWrite(RELAY_PIN1, LOW);
    digitalWrite(RELAY_PIN2, LOW);
    sendMessage(w1);

    while(true){

      lcd.setCursor(0, 0);
      lcd.print("      Done!  ");

      lcd.setCursor(0, 1);
      lcd.print("Time:");
      lcd.print(hours);
      lcd.print("h ");
      lcd.print(minutes);
      lcd.print("min");

      lcd.setCursor(0, 2);
      lcd.print("Before: ");
      lcd.print(initialWeight);
      lcd.print("kg");

      lcd.setCursor(0, 3);
      lcd.print("After: ");
      lcd.print(w1);
      lcd.print("kg");
    }
  }
}

void calibrate() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("     Calibration");
  scale.begin(dataPin, clockPin);
  lcd.setCursor(0, 1);
  lcd.print("    Please wait");

  delay(5000);
  float known_weight = 500;
  uint32_t new_weight = 0;
  
  scale.tare(20);
  uint32_t offset = scale.get_offset();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("     Calibration");
  lcd.setCursor(0, 1);
  lcd.print("1.Remove all weights");
  lcd.setCursor(0, 2);
  lcd.print("2.Place 500g weight");
  lcd.setCursor(0, 3);
  lcd.print("3.Push Green button");

  // Wait for the GREEN button to be pressed
  while (digitalRead(BUTTON_PIN_GREEN) == HIGH) {
    delay(50);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("    Calibration");
  lcd.setCursor(0, 1);
  lcd.print("Calibrating...");
  delay(5000);

  new_weight = known_weight;
  
  scale.calibrate_scale(new_weight, 20);
  float newCalibrationValue = scale.get_scale();
  EEPROM.put(calVal_eepromAdress, newCalibrationValue);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("     Calibration");

  lcd.setCursor(0, 1);
  lcd.print("*****Success!*****");
  delay(1000);
  
  while (digitalRead(BUTTON_PIN_RED) == HIGH) {
    delay(50);
    lcd.setCursor(0, 2);
    lcd.print("Press Red Button");
    lcd.setCursor(0, 3);
    lcd.print("to exit");
  }
  customSetup();
}

void customSetup() {
  lcd.clear();
  while (true) {
    lcd.setCursor(0, 0);
    lcd.print("      Setup");
    lcd.setCursor(0, 1);
    lcd.print("Please Press");
    lcd.setCursor(0, 2);
    lcd.print("Yellow - Calibrate");
    lcd.setCursor(0, 3);
    lcd.print("Green - Start Drying");

    GREENButtonState = digitalRead(BUTTON_PIN_GREEN);
    YELLOWButtonState = digitalRead(BUTTON_PIN_YELLOW);

    if (GREENButtonState == LOW) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("        Choose");
      lcd.setCursor(0, 1);
      lcd.print("        Setup");
      lcd.setCursor(0, 2);
      lcd.print("   Start Drying");
      delay(2000);
      break;
    } else if (YELLOWButtonState == LOW) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("        Setup");
      lcd.setCursor(0, 0);
      lcd.print("   Calibrate");
      delay(2000);
      calibrate();
    }
  }
}

void dryingSetup(){

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("      Setup");

  lcd.setCursor(0, 1);
  lcd.print("1. Insert Fish");

  lcd.setCursor(0, 2);
  lcd.print("2. Press green button");
  lcd.setCursor(0, 3);
  lcd.print("   when done.");

  while (digitalRead(BUTTON_PIN_GREEN) == HIGH) {
    delay(50);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("      Setup");

  lcd.setCursor(0, 1);
  lcd.print("    Please Wait");
  delay(5000);
  
  w1 = scale.get_units(10);
  delay(100);
  w2 = scale.get_units();
  while (abs(w1 - w2) > 10)
  {
     w1 = w2;
     w2 = scale.get_units();
     delay(100);
  }

  w1 /= 1000;
  initialWeight = w1;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("      Setup");

  lcd.setCursor(0, 1);
  lcd.print("Weight: ");
  lcd.print( initialWeight);
  lcd.print("kg");
  lcd.setCursor(0, 3);
  lcd.print("Please wait");
  delay(3000);
}

void sendMessage(double weight) {
 // Convert the double value to string
  String message = "Drying is done, Final Weight: " + String(weight);
  myGSM.println("AT+CMGF=1");
  delay(1000);
  myGSM.println("AT+CMGS=\"09123456789\"\r");
  delay(1000);
  // Send the message
  myGSM.println(message);
  delay(1000);
  myGSM.println((char)26);
  delay(1000);
}

void sendFirst(){
  myGSM.println("AT+CMGF=1");
  delay(1000);
  myGSM.println("AT+CMGS=\"+639123456789\"\r");
  delay(1000);
  myGSM.println("The drying will begin");
  delay(1000);
  myGSM.println((char)26);
  delay(1000);
}
