# MercedesC219W211KinematicController
After installing an Android HeadUnit, i was faced with a dead Kinematic button.. not to my liking... so i wanted to figure a way out to make it work again.

The Kinematic Module can be tricked to work by sending a Canbus frame with data length of 4 (30 00 00 00) with ID 0x034 on the CanBus B. However, if the Original AGW is present, it will interfer and the Kinematic Module will not open/close appropriately (by sending the msg at 100ms interval it stuttered and opened only partial). So i fixed it by replacing its logic.
This project implements a control system for the Kinematic Module on C219/W211. For testing i used this Code with an Arduino to control the (TLE4207 full-bridge driver IC) found on the kinematic PCB. The system supports automated initialization, end-position detection, recovery after power loss, and a robust fallback mechanism if the drive cannot reach its intended limit. For making this project work you should lift three INPUT Legs from the TLE4207 connecting to the Freescale IC (Big IC in the middle of the Board). Pins you should lift are INH, IN1, IN2. You can get 5V and GND from the TLE4268 IC (a DC DC 12V to 5V converter) or you can find 5V and GND around TLE4207. 

---

## Features

### Automated Initialization
- On first installation or after a cold start, the drive automatically **closes until it reaches the END_BUTTON limit switch**.  
- Once the end point is reached, the drive is considered **initialized** and ready for normal operation.

### Normal Operation
- Pressing the input button **toggles** the motor action:
  - If the drive is closed → it opens.
  - If the drive is open → it closes.

### Fallback: Failed Motion Recovery
- If the motor is unable to reach the expected end stop (e.g., obstruction or resistance):
  - The system performs **one reverse attempt** to reach the opposite end.
  - This ensures the drive is never left in an undefined or stalled state.

### Power Loss Behavior
- If power is lost during motion (e.g., the car is turned off mid-action):
  - On the next boot, the controller will **attempt to close the drive automatically**.
  - Ensures the mechanism returns to a safe baseline state.

---

## Hardware Overview

### Components
- Arduino (e.g., Nano, Pro Mini, or UNO), you can use a ATTiny45 or similar/smaller later like me
- Kinematic PCB carrying the TLE4207 and related components
- PowerSupply for testing on Desk

---
