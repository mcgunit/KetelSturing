#include "Arduino.h"
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

#define ONE_WIRE_BUS 4 // Digital pin the ds18b20 sensor is connected to

// Define the onewire instance and pass it to the DellasTemperature instance. 
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire); // Needed for the temperature conversions
float temperatureValueRaw = 0;  // Used for first reading
float temperatureValue = 0; // Store the valid value
float maxTemp = 90.0;
float tempToKeep = 65.0; // Lower then this temperature we need to try to ignite 
float minTemp = 32.0; // Below this temperature we will not try to keep temperature 

LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 16 chars and 2 line display
unsigned long clearLCDTime = 250;
unsigned long lastClearLCDTime = 0;

const int heater = 2;     // Switch for external heating element
const int relay2 = 3;     // Switch for external components
const int startButton = 5;
const int stopButton = 6;
const int jogButton = 7;

/* Button States */
bool startButtonState = false;
bool stopButtonState = false;
bool jogButtonState = false;

/* Motor Driver */
const int fwdDrive = 8;
bool driveActive = false;
const int frequencyReadPin = A0;
int frequencyValueRaw = 0;
const int minValidFreqActive = 50;
const int minValidFreqInActive = 10; 

/* Modes */
bool startMode = false;
bool stopMode = false;
bool keepingTemperature = false; // During stand-by mode if its trying to keep temperature this mode is active

/* Timers */
unsigned long startDuration = 15 * 60000;
//unsigned long startDuration = 6000;
unsigned long lastTimeStartActivated = 0;
//unsigned long stopDuration = 5 * 60000;
unsigned long stopDuration = 5000;
unsigned long lastTimeStopActivated = 0;
unsigned long timeRequestTemp = 1000;          // Do not fetch faster then every 1 sec
unsigned long timeLastRequestedTemp = 0;

void setup() {
    
    sensors.begin(); 
    Serial.begin(9600);

    pinMode(heater, OUTPUT);
    pinMode(relay2, OUTPUT);
    pinMode(fwdDrive, OUTPUT);

    pinMode(startButton, INPUT);
    pinMode(stopButton, INPUT);
    pinMode(jogButton, INPUT);

    // initialize the LCD
	lcd.begin();

	// Turn on the blacklight and print a message.
	lcd.backlight();
    lcd.setCursor(1,0);
    lcd.print("Ketel-sturing");
    lcd.setCursor(1,1);
    lcd.print("3000");

    delay(2000);
    lcd.clear();
}


void loop() {
    
    if(millis() - timeLastRequestedTemp >= timeRequestTemp) {
        frequencyValueRaw = analogRead(frequencyReadPin);
        timeLastRequestedTemp = millis();

        sensors.requestTemperatures();
        temperatureValueRaw = sensors.getTempCByIndex(0); // It is possible to have more ds18b20 sensors on the same bus

        if(temperatureValueRaw > -30) {
            temperatureValue = temperatureValueRaw;
        }

        //Serial.print(temperatureValue);
        //Serial.println(" C°");
    }
    

    startButtonState = digitalRead(startButton);
    stopButtonState = digitalRead(stopButton);
    jogButtonState = digitalRead(jogButton);


    /*
    Serial.print(startButtonState);
    Serial.print(",");
    Serial.print(stopButtonState);
    Serial.print(",");
    Serial.println(jogButtonState);
    */

    if(millis() - lastClearLCDTime > clearLCDTime) {
        lcd.clear();
        lastClearLCDTime = millis();
    }

    lcd.setCursor(1,0);
    lcd.print("Temp: ");
    lcd.print(temperatureValue);

    lcd.setCursor(1,1);

    if(temperatureValue >= maxTemp) {
            
        digitalWrite(fwdDrive, LOW);
        
        digitalWrite(heater, LOW);

        lcd.print("Max Temp");
    } else {


        if(frequencyValueRaw > minValidFreqActive) {
            driveActive = true;
        } else if(frequencyValueRaw < minValidFreqInActive) {
            driveActive = false;
        }


        // Stand-by mode (Try to maintain temperature)
        if(!startMode && !stopMode) {
            
            

            if(jogButtonState) {
                lcd.print("Jog");
                digitalWrite(fwdDrive, HIGH);
            } else if(!jogButtonState && !keepingTemperature) {
                if(temperatureValue < minTemp) {
                    lcd.print("Temp to low");
                } else if(temperatureValue > tempToKeep) {
                    lcd.print("Temp Reached");
                } else {
                    lcd.print("Stand-by");
                }
                
                digitalWrite(fwdDrive, LOW);
            }

            if(temperatureValue <= tempToKeep && temperatureValue >= minTemp && !jogButtonState) {
                lcd.print("Keeping Temp");
                keepingTemperature = true;
                if(!driveActive) {
                    digitalWrite(fwdDrive, HIGH);
                }
            }

            if(temperatureValue > tempToKeep && keepingTemperature && !jogButtonState) {
                keepingTemperature = false;
                if(!driveActive) {
                    digitalWrite(fwdDrive, LOW);
                }
            }


            if(temperatureValue < minTemp && keepingTemperature && !jogButtonState) {
                keepingTemperature = false;
                if(driveActive) {
                    digitalWrite(fwdDrive, LOW);
                }
            }

            

        }
        

        if(startButtonState && !startMode && !stopMode) {
            startMode = true;
            lastTimeStartActivated = millis();
            
        }

        if(stopButtonState && startMode && !stopMode) {
            stopMode = true;
            startMode = false;
            lastTimeStopActivated = millis();
           
        }


        if(startMode) {
        
            lcd.print("Start ");
            long unsigned relativeTime = startDuration - (millis() - lastTimeStartActivated);
            lcd.print(relativeTime/1000);
            digitalWrite(heater, HIGH);

            if(relativeTime <= startDuration/2) {
                if(!driveActive) {
                    digitalWrite(fwdDrive, HIGH);
                    driveActive = true;
                }
                

            } else {
                digitalWrite(fwdDrive, LOW);
                driveActive = false;
            }

        } else if(!startMode) {
            digitalWrite(heater, LOW);

            if(driveActive && !jogButtonState && !keepingTemperature) {
                digitalWrite(fwdDrive, LOW);
                driveActive = false;
            }
            
        }

        if(stopMode) {
            
            lcd.print("Stop ");
            long unsigned relativeTime = stopDuration - (millis() - lastTimeStopActivated);
            lcd.print(relativeTime/1000);
            digitalWrite(heater, LOW);

            if(driveActive) {
                digitalWrite(fwdDrive, LOW);
                driveActive = false;
            }
        }

        if(startMode && (millis() - lastTimeStartActivated > startDuration)) {
            startMode = false;
        }

        if(stopMode && (millis() - lastTimeStopActivated > stopDuration)) {
            stopMode = false;
        }
    }

    
    
}