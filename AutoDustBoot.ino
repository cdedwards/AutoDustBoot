/*
  NEMA17 Control for DRV8825
  - Jogging, Homing, and EEPROM Save/Restore
  - Hardware: Pins 2/3 for Encoder, 9/5 for Step/Dir, 6 for Enable
*/

#include <AccelStepper.h>
#include <Encoder.h>
#include <Bounce2.h>
#include <EEPROM.h>

// -------------------- PIN DEFINITIONS --------------------
const uint8_t PIN_STEP = 9;      // Changed to 9 as found in previous troubleshooting
const uint8_t PIN_DIR  = 5;      
const uint8_t PIN_EN   = 6;      // DRV8825 Enable Pin

const uint8_t PIN_ENC_A = 2;
const uint8_t PIN_ENC_B = 3;
const uint8_t PIN_BTN = 7;
const uint8_t PIN_LIMIT_LEFT = 8;
const uint8_t PIN_HOME_LED = LED_BUILTIN; //10;

// -------------------- MOTION CONFIG --------------------
const float STEPS_PER_REV = 200.0f;
const float MICROSTEPS     = 32.0f;     // DRV8825 common setting (M0,M1,M2 all HIGH)
const float LEAD_MM_PER_REV = 8.0f;     // Standard T8 lead screw
const float MM_PER_INCH = 25.4f;
const float HOME_OFFSET_INCH = 0.1f; 
const int8_t HOME_OFFSET_SIGN = +1;

const long STEPS_PER_DETENT = 32;       // Higher value for DRV8825 microstepping
const float MAX_SPEED = 1500.0f;        // steps/sec
const float ACCEL     = 2000.0f;        // steps/sec^2
const float HOMING_SPEED = 800.0f;

const uint16_t DOUBLE_PRESS_MS = 400;

// -------------------- EEPROM SAVE --------------------
const int EEPROM_ADDR_MAGIC = 0;
const int EEPROM_ADDR_POS   = 1;
const uint8_t EEPROM_MAGIC  = 0xA5;

// -------------------- OBJECTS --------------------
AccelStepper stepper(AccelStepper::DRIVER, PIN_STEP, PIN_DIR);
Encoder enc(PIN_ENC_A, PIN_ENC_B);
Bounce2::Button btn = Bounce2::Button();
Bounce2::Button limitLeft = Bounce2::Button();

// -------------------- STATE --------------------
enum Mode { MODE_HOMING, MODE_IDLE, MODE_MOVING_TO_TARGET };
Mode mode = MODE_HOMING;

long homePosSteps = 0;
long savedPosSteps = 0;
bool hasSaved = false;
long lastEncCount = 0;
bool waitingForSecondPress = false;
uint32_t firstPressTimeMs = 0;
bool toggleToSavedNext = true;

// -------------------- HELPERS --------------------
long inchesToSteps(float inches) {
  float mm = inches * MM_PER_INCH;
  float revs = mm / LEAD_MM_PER_REV;
  float steps = revs * STEPS_PER_REV * MICROSTEPS;
  return (long)lround(steps);
}

void loadSavedFromEEPROM() {
  uint8_t m = EEPROM.read(EEPROM_ADDR_MAGIC);
  if (m == EEPROM_MAGIC) {
    long p;
    EEPROM.get(EEPROM_ADDR_POS, p);
    savedPosSteps = p;
    hasSaved = true;
  }
}

void saveToEEPROM(long pos) {
  EEPROM.write(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
  EEPROM.put(EEPROM_ADDR_POS, pos);
  savedPosSteps = pos;
  hasSaved = true;
}

void startMoveTo(long targetSteps) {
  stepper.moveTo(targetSteps);
  mode = MODE_MOVING_TO_TARGET;
}

// -------------------- SETUP / LOOP --------------------
void setup() {
  Serial.begin(115200);

  pinMode(PIN_EN, OUTPUT);
  digitalWrite(PIN_EN, LOW); // DRV8825 ENABLE is active LOW

  btn.attach(PIN_BTN, INPUT_PULLUP);
  btn.interval(5);
  btn.setPressedState(LOW);

  limitLeft.attach(PIN_LIMIT_LEFT, INPUT_PULLUP);
  limitLeft.interval(5);
  limitLeft.setPressedState(LOW);

  pinMode(PIN_HOME_LED, OUTPUT);

  stepper.setMaxSpeed(MAX_SPEED);
  stepper.setAcceleration(ACCEL);

  loadSavedFromEEPROM();
  lastEncCount = enc.read();

  Serial.println("Starting Homing...");
}

void loop() {
  btn.update();
  limitLeft.update();

  if (mode == MODE_HOMING) {
    if (!limitLeft.isPressed()) {
      stepper.setSpeed(-HOMING_SPEED);
      stepper.runSpeed();
    } else {
      stepper.setCurrentPosition(0);
      homePosSteps = HOME_OFFSET_SIGN * inchesToSteps(HOME_OFFSET_INCH);
      startMoveTo(homePosSteps);
      digitalWrite(PIN_HOME_LED, HIGH);
      Serial.println("Homed. Moving to Offset...");
    }
  } else {
    stepper.run();
    if (mode == MODE_MOVING_TO_TARGET && stepper.distanceToGo() == 0) {
      mode = MODE_IDLE;
    }

    if (mode == MODE_IDLE) {
      long c = enc.read();
      if (abs(c - lastEncCount) >= 4) {
        long delta = (c - lastEncCount) / 4;
        lastEncCount = c;
        stepper.moveTo(stepper.targetPosition() + (delta * STEPS_PER_DETENT));
      }
    }
  }

  // Button Logic (Single/Double Press)
  if (mode != MODE_HOMING && btn.pressed()) {
    if (!waitingForSecondPress) {
      waitingForSecondPress = true;
      firstPressTimeMs = millis();
    } else {
      if (millis() - firstPressTimeMs <= DOUBLE_PRESS_MS) {
        waitingForSecondPress = false;
        saveToEEPROM(stepper.currentPosition());
        Serial.println("Position Saved to EEPROM!");
      }
    }
  }

  if (waitingForSecondPress && (millis() - firstPressTimeMs > DOUBLE_PRESS_MS)) {
    waitingForSecondPress = false;
    if (toggleToSavedNext && hasSaved) {
      startMoveTo(savedPosSteps);
      Serial.println("Moving to SAVED");
    } else {
      startMoveTo(homePosSteps);
      Serial.println("Moving to HOME");
    }
    toggleToSavedNext = !toggleToSavedNext;
  }
}