#include <Keypad.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include "BluetoothSerial.h"

//Pin Def
#define SERVO_PIN 19

//Keypad setup 
const byte ROWS = 4; 
const byte COLS = 4; 
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

byte rowPins[ROWS] = {13, 14, 27, 26};
byte colPins[COLS] = {25, 33, 32, 4};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

//LCD, Servo Setup
LiquidCrystal_I2C lcd(0x27, 16, 2); 
Servo lockServo;
BluetoothSerial SerialBT;

//Lock password
String correctPIN = "1234"; 
String currentPIN = "";

//Max device
const int NUM_DEVICES = 4; 

// MAC ADDRESS of Authorized Devices
String authorizedMACs[NUM_DEVICES] = {
  "EC:46:2C:64:B0:F8", // cp ko (for testing purposes)
  "3C:B0:ED:23:B5:FA", // cp ka manghod ko (for testing purposes)
  "C0:35:32:9E:B9:4E",// akong laptop (for testing purposes)
  "12:34:56:78:90:09" // random shi
};

enum State { LOCKED, UNLOCKED, BT_REJECTED };
State currentState = LOCKED;

unsigned long unlockTimer = 0;
unsigned long rejectTimer = 0;
const unsigned long UNLOCK_DURATION = 5000;
const unsigned long REJECT_DURATION = 3000;

// Flags, buffers for Bluetooth Callback
volatile bool btCheckPending = false;
uint8_t incomingMACBytes[6]; 

//btConnection Callback
void btCallback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  if (event == ESP_SPP_SRV_OPEN_EVT) {
    for (int i = 0; i < 6; i++) {
      incomingMACBytes[i] = param->srv_open.rem_bda[i];
    }
    btCheckPending = true;
  }
}

void setup() {
  Serial.begin(115200);
  
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  
  // Initialize Servo
  lockServo.setPeriodHertz(50);
  lockServo.attach(SERVO_PIN, 500, 2400); 
  lockDoor();

  // Initialize Bluetooth
  SerialBT.register_callback(btCallback);
  SerialBT.begin("SmartLock_ESP32"); 

  displayLocked();
  Serial.println("\n--- ESP32 Smart Lock Ready ---");
}

void loop() {
  // Process bt Events
  if (btCheckPending) {
    btCheckPending = false; 
    
    char macStr[18]; 
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             incomingMACBytes[0], incomingMACBytes[1],
             incomingMACBytes[2], incomingMACBytes[3],
             incomingMACBytes[4], incomingMACBytes[5]);

    String incomingMAC = String(macStr);
    bool isAuthorized = false;

    Serial.print("Bluetooth connection attempt from: ");
    Serial.println(incomingMAC);

    for (int i = 0; i < NUM_DEVICES; i++) {
      if (incomingMAC.equalsIgnoreCase(authorizedMACs[i])) {
        isAuthorized = true;
        break;
      }
    }

    if (isAuthorized) {
      Serial.println("Device Authorized. Unlocking...");
      unlockDoor();
    } else {
      Serial.println("Device Rejected. Not in authorized list.");
      currentState = BT_REJECTED;
      rejectTimer = millis();
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Device not");
      lcd.setCursor(0, 1);
      lcd.print("in list");
    }
  }

  // Auto lock
  if (currentState == BT_REJECTED) {
    if (millis() - rejectTimer >= REJECT_DURATION) {
      currentState = LOCKED;
      displayLocked();
      currentPIN = "";
      SerialBT.disconnect(); 
    }
  } else if (currentState == UNLOCKED) {
    if (millis() - unlockTimer >= UNLOCK_DURATION) {
      Serial.println("Auto-locking door.");
      lockDoor();
      displayLocked();
      SerialBT.disconnect(); 
    }
  }

  //Keypad, Serial Input
  if (currentState == LOCKED && !btCheckPending) {
    char key = keypad.getKey(); 
    
    if (!key && Serial.available() > 0) {
      char sKey = Serial.read();
      if (sKey != '\n' && sKey != '\r') {
        key = sKey;
      }
    }

    if (key) {
      if (key == '*') {
        currentPIN = "";
        Serial.println("\nInput cleared.");
        displayLocked();
      } else if (key == '#') {
        Serial.println(); 
        if (currentPIN == correctPIN) {
          Serial.println("PIN Correct. Unlocking...");
          unlockDoor();
        } else {
          Serial.println("Wrong Code!");
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Wrong Code!");
          delay(1500);
          currentPIN = "";
          displayLocked();
        }
      } else {
        if (currentPIN.length() == 0) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("Enter code:");
        }
        if (currentPIN.length() < 16) {
          currentPIN += key;
          lcd.setCursor(currentPIN.length() - 1, 1);
          lcd.print('*'); 
        }
      }
    }
  }

  // para di sagi restart HAHAHAHHAHH
  delay(20); 
}

// functions 
void unlockDoor() {
  currentState = UNLOCKED;
  lockServo.write(90); 
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Access Granted");
  unlockTimer = millis();
  currentPIN = "";
}

void lockDoor() {
  currentState = LOCKED;
  lockServo.write(0); 
}

void displayLocked() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("*Enter Password*");
}