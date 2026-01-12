#include <AccelStepper.h>
#include <EEPROM.h>

// --- Hardware Constants ---
const float STEPS_PER_MM = 400.0;     
const float BACKOFF_MM = 2.54;        
const long BACKOFF_STEPS = BACKOFF_MM * STEPS_PER_MM;

// --- Pins ---
#define STEP_PIN 2
#define DIR_PIN 3
#define EN_PIN 4      
#define ENC_A 5
#define ENC_B 6
#define ENC_SW 7
#define LIMIT_SW 8
#define EXT_SIGNAL_PIN 9  
#define LED_HOME 13

AccelStepper stepper(AccelStepper::DRIVER, STEP_PIN, DIR_PIN);

// --- EEPROM Addresses ---
const int ADDR_LIMIT = 0;
const int ADDR_SAVED = 10;

// --- Variables ---
long savedPosition = 0;
long customMaxLimit = 81280; 
int lastClk;

// External Trigger Logic (Pulse based)
bool lastExtSignalState = LOW;
bool targetIsBottom = false; // Toggle state: false = Home, true = Bottom

unsigned long lastDebounceTime = 0; 
const unsigned long debounceDelay = 50; 
unsigned long lastActivityTime = 0;
const unsigned long sleepDelay = 5000; 

bool isAsleep = false;
bool isCalibrating = false;
bool lastButtonState = HIGH;
int pressCount = 0;
unsigned long lastPressTime = 0;
unsigned long buttonDownTime = 0;
bool wasJogging = false; 

const int doubleClickWindow = 400;
const int longPressWindow = 2000;

void setup() {
  Serial.begin(115200);
  pinMode(ENC_A, INPUT);
  pinMode(ENC_B, INPUT);
  pinMode(ENC_SW, INPUT_PULLUP);
  pinMode(LIMIT_SW, INPUT_PULLUP);
  pinMode(EXT_SIGNAL_PIN, INPUT); // Set to INPUT for High Pulse signal
  pinMode(LED_HOME, OUTPUT);
  pinMode(EN_PIN, OUTPUT);
  
  EEPROM.get(ADDR_LIMIT, customMaxLimit);
  EEPROM.get(ADDR_SAVED, savedPosition);
  if(customMaxLimit <= 100) customMaxLimit = 81280; 

  wakeUp();
  stepper.setMaxSpeed(2000); 
  stepper.setAcceleration(400); 
  
  lastClk = digitalRead(ENC_A);
  performHoming();
}

void loop() {
  handleEncoderRotation();
  handleEncoderButton();
  handleExternalPulse(); // New logic for pulse trigger
  stepper.run();
  
  // Dynamic Acceleration
  if (abs(stepper.speed()) > 800) {
    stepper.setAcceleration(1200); 
  } else {
    stepper.setAcceleration(400);
  }

  if (isCalibrating) {
    digitalWrite(LED_HOME, (millis() / 100) % 2); 
  } else {
    checkSleepTimer();
  }
}

void wakeUp() {
  if (isAsleep) {
    digitalWrite(EN_PIN, LOW);
    isAsleep = false;
  }
  lastActivityTime = millis();
}

void checkSleepTimer() {
  if (!stepper.isRunning() && !isAsleep && !isCalibrating) {
    if (millis() - lastActivityTime > sleepDelay) {
      digitalWrite(EN_PIN, HIGH);
      isAsleep = true;
      Serial.println("Sleep Mode Active");
    }
  }
}

void handleExternalPulse() {
  bool currentSignal = digitalRead(EXT_SIGNAL_PIN);
  
  // Check for Rising Edge (LOW to HIGH)
  if (currentSignal == HIGH && lastExtSignalState == LOW) {
    if (!isCalibrating) {
      wakeUp();
      
      // Toggle the target
      targetIsBottom = !targetIsBottom; 
      
      if (targetIsBottom) {
        Serial.println("Pulse Detected: Moving to BOTTOM");
        stepper.moveTo(customMaxLimit);
      } else {
        Serial.println("Pulse Detected: Moving to HOME");
        stepper.moveTo(0);
      }
    }
    delay(50); // Simple debounce for the external pulse
  }
  lastExtSignalState = currentSignal;
}

void performHoming() {
  wakeUp();
  stepper.setSpeed(-500); 
  while (digitalRead(LIMIT_SW) == HIGH) {
    stepper.runSpeed();
  }
  stepper.setCurrentPosition(0);
  stepper.moveTo(BACKOFF_STEPS);
  while (stepper.distanceToGo() != 0) { stepper.run(); }
  stepper.setCurrentPosition(0); 
  digitalWrite(LED_HOME, HIGH);
  targetIsBottom = false; // Reset toggle state after homing
}

void handleEncoderRotation() {
  int currentClk = digitalRead(ENC_A);
  if (currentClk != lastClk && currentClk == LOW) {
    wakeUp();
    bool buttonHeld = (digitalRead(ENC_SW) == LOW);
    long newTarget = stepper.currentPosition();
    
    int moveAmount = buttonHeld ? 800 : 100;
    if (buttonHeld) wasJogging = true; 

    newTarget += (digitalRead(ENC_B) != currentClk) ? moveAmount : -moveAmount;

    if (!isCalibrating) newTarget = constrain(newTarget, 0, customMaxLimit);
    
    stepper.moveTo(newTarget);
    lastActivityTime = millis();
  }
  lastClk = currentClk;
}

void handleEncoderButton() {
  bool reading = digitalRead(ENC_SW);

  if (reading == LOW && lastButtonState == HIGH) {
    buttonDownTime = millis();
    wasJogging = false; 
    wakeUp();
  }

  if (reading != lastButtonState) {
    if (reading == HIGH) { 
      unsigned long duration = millis() - buttonDownTime;
      
      if (!wasJogging) {
        if (isCalibrating && duration < 1000) {
          customMaxLimit = stepper.currentPosition();
          EEPROM.put(ADDR_LIMIT, customMaxLimit);
          isCalibrating = false;
          digitalWrite(LED_HOME, HIGH);
          Serial.println("Calibration Saved");
        } 
        else if (!isCalibrating) {
          if (duration > longPressWindow) {
            isCalibrating = true;
            Serial.println("Mode: Calibrating");
          } else {
            pressCount++;
            lastPressTime = millis();
          }
        }
      }
    }
    lastDebounceTime = millis(); 
  }

  if (pressCount > 0 && (millis() - lastPressTime > doubleClickWindow)) {
    if (pressCount == 1) {
      stepper.moveTo(0);
      targetIsBottom = false; // Sync pulse state
    } 
    else if (pressCount >= 2) {
      if (stepper.currentPosition() == 0) {
        stepper.moveTo(savedPosition);
      } else {
        savedPosition = stepper.currentPosition();
        EEPROM.put(ADDR_SAVED, savedPosition);
        visualConfirm(); 
        Serial.println("Position Programmed");
      }
    }
    pressCount = 0;
  }
  lastButtonState = reading;
}

void visualConfirm() {
  for(int i=0; i<3; i++) {
    digitalWrite(LED_HOME, LOW); delay(80);
    digitalWrite(LED_HOME, HIGH); delay(80);
  }
}

