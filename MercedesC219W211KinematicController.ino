/*
 Name:		MercedesC219W211KinematicController.ino
 Created:	12/6/2025 10:00:21 PM
 Author:	Jonas-Gabriel
*/

#include "EEPROMBitAccess.h" // need to save load from eeprom
#include "config.h"  // changes according to your setup are made there

// DRIVE DIRECTIONS
enum DriveDirections : uint8_t
{
    CLOSE = 0,
    OPEN = 1
};

enum MotorSMStates : uint8_t
{
    BOOT,
    BOOT_RECOVERY_CHECK,
    BOOT_RECOVER,
    BOOT_END,
    IDLE,
    DRIVE_START,
    DRIVE_ACTIVE,
    DRIVE_END
};

// ==========================================================
// State Variables
// ==========================================================
static MotorSMStates MotorSMState = MotorSMStates::BOOT;

static DriveDirections desiredDirection = DriveDirections::CLOSE;
static DriveDirections lastKnownDirection = DriveDirections:: CLOSE;  // stored in EEPROM

static unsigned long stateStartTime = 0;
static unsigned long driveStartTime = 0;

static bool endPressedAtStart = false;
static bool endReleasedOnce = false;

static uint8_t retryCount = 1;

// ---------------------------------------------------------------------------
// EEPROM SAVE/LOAD
// ---------------------------------------------------------------------------

void SaveDriveDirectionToEEPROM(DriveDirections DirectionToSave)
{
    EEPROMBitAccess::writeBit(ADDRESS_LastDriveDirection, ADDRESS_BIT_LastDriveDirection, DirectionToSave);
}

DriveDirections LoadDriveDirectionFromEEPROM()
{
    return
        EEPROMBitAccess::readBit(ADDRESS_LastDriveDirection, ADDRESS_BIT_LastDriveDirection) == 1
        ? DriveDirections::OPEN
        : DriveDirections::CLOSE;
}

// ---------------------------------------------------------------------------
// MOTOR CONTROL
// ---------------------------------------------------------------------------

void MotorDrive(DriveDirections DesiredDirection)
{
    if (DesiredDirection == CLOSE)
    {
        digitalWrite(PIN_IN1, HIGH);
        digitalWrite(PIN_IN2, LOW);
    }
    else
    {
        digitalWrite(PIN_IN1, LOW);
        digitalWrite(PIN_IN2, HIGH);
    }

    digitalWrite(PIN_INH, HIGH);
}

void MotorStop()
{
    digitalWrite(PIN_IN1, LOW);
    digitalWrite(PIN_IN2, LOW);
    digitalWrite(PIN_INH, LOW);
}

// ---------------------------------------------------------------------------
// SIGNALS
// ---------------------------------------------------------------------------

bool StartButtonPressed()
{
    return !digitalRead(INPUT_START_BUTTON);  // Active LOW
}

bool EndButtonPressed()
{
    return !digitalRead(INPUT_END_BUTTON);  // Active LOW
}

// ---------------------------------------------------------------------------
// SETUP
// ---------------------------------------------------------------------------

// the setup function runs once when you press reset or power the board
void setup()
{
    Serial.begin(115200);

    pinMode(PIN_INH, OUTPUT);
    pinMode(PIN_IN1, OUTPUT);
    pinMode(PIN_IN2, OUTPUT);
    pinMode(INPUT_START_BUTTON, INPUT_PULLUP);
    pinMode(INPUT_END_BUTTON, INPUT);
    pinMode(PIN_ERR, INPUT);

    MotorStop();
    lastKnownDirection = LoadDriveDirectionFromEEPROM();

    changeState(MotorSMStates::BOOT);
}

// ---------------------------------------------------------------------------
// STATE MACHINE
// ---------------------------------------------------------------------------

void changeState(MotorSMStates newState)
{
    MotorSMState = newState;
    stateStartTime = millis();
}

void SwitchDesiredDirection()
{
    desiredDirection = (desiredDirection == DriveDirections::CLOSE)
        ? DriveDirections::OPEN
        : DriveDirections::CLOSE;
}

// the loop function runs over and over again until power down or reset
void loop()
{
    switch (MotorSMState)
    {
        case BOOT:
            if (SHOULD_RECOVER_AT_BOOT == 1)
            {
                changeState(MotorSMStates::BOOT_RECOVERY_CHECK);
            }
            else
            {
                changeState(MotorSMStates::BOOT_END);
            }
            break;

        case BOOT_RECOVERY_CHECK:
            Serial.println("CHECK RECOVER AT BOOT");
            if (EndButtonPressed())
            {
                desiredDirection = lastKnownDirection;
                changeState(MotorSMStates::BOOT_END);
            }
            else
            {
                // Unknown Position of Motor -> Recovery needed
                (SHOULD_RECOVER_TO_CLOSE_AT_BOOT == 1)
                    ? desiredDirection = DriveDirections::CLOSE
                    : desiredDirection = lastKnownDirection;

                changeState(MotorSMStates::BOOT_RECOVER);
            }
            break;

        case BOOT_RECOVER:
            Serial.println("RECOVERY");

            retryCount = MAX_TRYS_BEFORE_SAFETY_STOP;
            endPressedAtStart = EndButtonPressed();
            endReleasedOnce = !endPressedAtStart;
            MotorDrive(desiredDirection);
            driveStartTime = millis();

            // same as DRIVE_START but simpler for boot recovery
            changeState(MotorSMStates::DRIVE_ACTIVE);
            break;

        case BOOT_END:
            // At a known end -> IDLE
            Serial.println("ENDING BOOT");
            changeState(MotorSMStates::IDLE);
            break;

        case IDLE:
            if (StartButtonPressed())
            {
                Serial.println("START WAS PRESSED");
                changeState(MotorSMStates::DRIVE_START);
            }
            break;

        case DRIVE_START:
            Serial.println("STARTED DRIVE");

            retryCount = 1;
            endPressedAtStart = EndButtonPressed();
            endReleasedOnce = !endPressedAtStart;
            MotorDrive(desiredDirection);
            driveStartTime = millis();


            changeState(MotorSMStates::DRIVE_ACTIVE);
            break;

        case DRIVE_ACTIVE:
        {
            unsigned long now = millis();

            bool endNow = EndButtonPressed();

            // 1. Release EndButton Detection
            if (endPressedAtStart)
            {
                if (!endNow)
                {
                    Serial.println("END_BUTTON RELEASED");
                    endReleasedOnce = true;
                    endPressedAtStart = false;
                    delay(100);
                }
                else
                {
                    // Still Pressed -> check by END_BUTTON_RELEASE_TIMER_MS
                    if (now - driveStartTime >= END_BUTTON_RELEASE_TIMER_MS)
                    {
                        Serial.println("END_BUTTON NOT RELEASED = CONSIDERING OK = LIMIT REACHED");
                        // CONSIDER = Driving in direction where we are already at the limit
                        changeState(MotorSMStates::DRIVE_END);
                        SwitchDesiredDirection();
                        SaveDriveDirectionToEEPROM(desiredDirection);
                        break;
                    }
                }
            }
            // 2. END_BUTTON detection (after valid release or no release needed)
            if (endReleasedOnce && endNow)
            {
                Serial.println("DRIVE SUCCEEDED");
                changeState(MotorSMStates::DRIVE_END);
                SwitchDesiredDirection();
                SaveDriveDirectionToEEPROM(desiredDirection);
                break;
            }
            // 3. TimeOut reached (Drive to end took to long, maybe blocked) -> Reverse Direction
            if ((now - driveStartTime >= MAX_DRIVE_TIMEOUT_MS))
            {
                if ((MAX_TRYS_BEFORE_SAFETY_STOP > 1) && MAX_TRYS_BEFORE_SAFETY_STOP >= retryCount)
                {
                    Serial.println("OVERTIME HAPPENED -> REVERSING");
                    SwitchDesiredDirection();

                    MotorDrive(desiredDirection);
                    driveStartTime = now;

                    // reset end logic for fresh movement
                    endPressedAtStart = EndButtonPressed(); // should be false;
                    endReleasedOnce = !endPressedAtStart; // should be true;

                    retryCount++;
                }
                else
                {
                    Serial.println("OVERTIME HAPPENED -> ABORTING");

                    desiredDirection = DriveDirections::OPEN; // blockage, next motion will be open for visual inspection;
                    changeState(MotorSMStates::DRIVE_END);
                    break;
                }
            }
        }

        break;

        case DRIVE_END:
            Serial.println("DRIVE END");
            MotorStop();
            delay(DRIVE_COOLDOWN_MS);
            changeState(MotorSMStates::IDLE);
            break;
    }
}
