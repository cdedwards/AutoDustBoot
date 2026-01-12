# AutoDustBoot
Code for arduino to move  a dust boot for a cnc up and down. Created with Gemini AI
# Precision Stepper Control System (T8 Lead Screw)

An Arduino-based linear motion controller featuring a **TMC2100 SilentStepStick**, opto-isolated triggering, and a custom calibration engine.

## 🚀 Features
- **Silent Operation:** Optimized for TMC2100 stealthChop.
- **Opto-Isolated Pulse Trigger:** High-pulse (+5V) toggles between Home and Max Limit.
- **Jog Mode:** Hold the encoder button to move at 8x speed.
- **Smart Sleep:** Auto-disables motor current after 5s of inactivity.
- **EEPROM Memory:** Saves calibration limits and programmed positions.
- **Soft Start:** Dynamic acceleration ramping for mechanical protection.

---

## 🛠 Hardware Configuration

### Pin Mapping
| Pin | Component | Description |
| :--- | :--- | :--- |
| **D2** | TMC2100 STEP | Step pulse output |
| **D3** | TMC2100 DIR | Direction control |
| **D4** | TMC2100 EN | Driver Enable (Active LOW) |
| **D5** | Encoder A | Rotary Phase A |
| **D6** | Encoder B | Rotary Phase B |
| **D7** | Encoder SW | Encoder Push-button |
| **D8** | Limit SW | Homing Switch (to GND) |
| **D9** | Pulse Trigger| Opto-isolated +5V Input |
| **D13** | Status LED | System Feedback |

### Power Requirements
- **Logic:** 5V DC (via onboard regulator).
- **Motor:** 12V-24V DC (connected to TMC2100 VMOT).



---

## 📖 User Manual

### 1. Homing (Auto-Startup)
On power-up, the motor moves toward the Limit Switch. Upon contact, it backs off **0.1" (2.54mm)**. The LED turns solid to indicate the system is ready at the `0.00` index.

### 2. Manual Navigation
* **Fine Adjust:** Turn the knob for 100-step increments.
* **Jog Mode:** **Hold button + Turn knob** for 800-step (rapid) increments.

### 3. Storing Positions
* **Program Position:** Move to a location and **Double-Click**.
* **Go to Programmed:** From the Home position, **Double-Click**.
* **Go Home:** **Single-Click** at any time.

### 4. External Trigger (Pulse)
The system responds to a +5V pulse on the trigger input:
* **Pulse 1:** Moves motor to the Calibrated Bottom.
* **Pulse 2:** Moves motor to Home.
* *Note: Each rising edge toggles the destination.*

### 5. Calibration Mode
To set a new custom travel limit (Safety Stop):
1. **Long Press** the button (2s) until the LED flashes rapidly.
2. Manually move the motor to the desired maximum travel point.
3. **Single-Click** to save to EEPROM.



---

## 🔧 Installation Notes
1. **Driver Current:** Adjust the TMC2100 $V_{ref}$ potentiometer to match your motor's rated current.
2. **Optocoupler:** Ensure the external pulse ground is connected to the Opto-Cathode, not the Arduino Ground, to maintain isolation.
3. **Heat Sinking:** Ensure a heatsink is attached to the TMC2100 for high-speed Jog operations.

---

## 📜 Dependencies
- [AccelStepper Library](https://www.airspayce.com/mikem/arduino/AccelStepper/)
- `EEPROM.h` (Standard Arduino Library)





## 🛠 Troubleshooting Guide

This section covers common issues related to the **TMC2100 driver**, **T8 lead screw mechanics**, and **opto-isolated signaling**.

### 1. Motor & Motion Issues
* **Motor Vibrates but Does Not Move:**
    * **Cause:** Incorrect phase wiring or insufficient current.
    * **Solution:** Verify motor pairs ($A+/A-$ and $B+/B-$). Check the **Vref** on the TMC2100; it should typically be between **0.8V and 1.2V** for NEMA 17 motors.
* **Motor Stalls during Jog Mode:**
    * **Cause:** Speed exceeds the motor's torque capacity at the current voltage.
    * **Solution:** Lower `setMaxSpeed` in the sketch or increase the motor power supply to 24V (if currently 12V) to maintain torque at higher RPMs.
* **Position "Drift" or Inaccuracy:**
    * **Cause:** Mechanical binding or missed steps due to heat.
    * **Solution:** Clean and lubricate the T8 lead screw. Ensure the TMC2100 has a heatsink and adequate airflow.



---

### 2. Driver & Power Issues
* **Driver Shuts Down (Thermal Cutoff):**
    * **Cause:** TMC2100 is overheating.
    * **Solution:** Even in SilentStepMode, these drivers get hot. Ensure the heatsink is properly seated with thermal tape. If the board is enclosed in a 40mm housing, a small 5V cooling fan is highly recommended.
* **Arduino Resets when Motor Starts:**
    * **Cause:** Voltage spikes or sags on the power rail.
    * **Solution:** Ensure a large electrolytic capacitor (**100µF to 470µF**) is installed between **VMOT** and **GND**.

---

### 3. Sensors & External Trigger
* **External Pulse Trigger Unresponsive:**
    * **Cause:** Incorrect resistor for the optocoupler LED.
    * **Solution:** The 1kΩ resistor is designed for 5V pulses. If using a **24V PLC signal**, swap the 1kΩ resistor for a **2.2kΩ** resistor to protect the PC81
    * 
