/*
 Name:		MercedesC219W211KinematicController.ino
 Created:	12/6/2025 10:00:21 PM
 Author:	Jonas-Gabriel
*/


#include <EEPROM.h>

// CHANGE TO YOUR LIKING
#define IN1             2
#define IN2             3
#define INH             4
#define ERROR_PIN       5
#define START_BUTTON    6
#define END_BUTTON      7

// DRIVE DIRECTIONS
enum staticDriveDirection : uint8_t
{
    CLOSE = 0,
    OPEN = 1
};


staticDriveDirection DriveDirection = staticDriveDirection::CLOSE;
bool bLastDriveAttemptSuccessful = false;
bool isDriving = false;
unsigned long DriveStartTime = 0;
const int MAX_DRIVE_TIME_MS = 6000;

#define LastDriveAttemptSuccessfulStateAdress 1
#define DriveDirectionStateAdress 2

#define StartButtonDeadlockRelease 1000

// ---------------------------------------------------------------------------
// EEPROM handling – FIXED (store 0/1, load exact boolean)
// ---------------------------------------------------------------------------
bool loadState(int adress)
{
    return EEPROM.read(adress) == 1;
}

void saveState(int adress, bool value)
{
    EEPROM.write(adress, value ? 1 : 0);
}


// ---------------------------------------------------------------------------

void setup()
{
    Serial.begin(115200);

    pinMode(IN1, OUTPUT);
    pinMode(IN2, OUTPUT);
    pinMode(INH, OUTPUT);
    pinMode(START_BUTTON, INPUT_PULLUP);
    pinMode(END_BUTTON, INPUT);
    pinMode(ERROR_PIN, INPUT);

    DriveStop();

    LoadLastDriveAttemptStatus();
    if (!bLastDriveAttemptSuccessful)
    {
        Serial.println("Setup runs recovery");
        DriveDirection = staticDriveDirection::CLOSE;
        AttemptDrive();
    }
    else
    {
        LoadLastDriveDirection();
        Serial.println("Setup normal start");
    }
}

void loop()
{
    // do not continue while IC is in ERROR state; check DOC -> TEMP./VOLT.
    while (TLE4207_Error())
    {
        DriveStop();
        delay(1000); // watchdog relieve
    }

    if (CanAttemptNewDrive())
    {
        AttemptDrive();
    }
}


// ---------------------------------------------------------------------------
// MOTOR CONTROL
// ---------------------------------------------------------------------------

void DriveClose()
{
    SetDriveStatus(true);
    saveState(DriveDirectionStateAdress, 0);

    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    digitalWrite(INH, HIGH);

    Serial.println("void DriveClose");
}

void DriveOpen()
{
    SetDriveStatus(true);
    saveState(DriveDirectionStateAdress, 1);

    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    digitalWrite(INH, HIGH);

    Serial.println("void DriveOpen");
}

void DriveStop()
{
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, LOW);
    digitalWrite(INH, LOW);

    SetDriveStatus(false);

    Serial.println("void DriveStop");
}


// ---------------------------------------------------------------------------
// SIGNALS
// ---------------------------------------------------------------------------

bool TLE4207_Error()
{
    // inverse as LOW is GOOD
    return !digitalRead(ERROR_PIN);
}

bool StartButtonPressed()
{
    return !digitalRead(START_BUTTON);
}

bool EndReached()
{
    return !digitalRead(END_BUTTON);
}


// ---------------------------------------------------------------------------
// INPUT CONTROL – FIXED: button release timeout
// ---------------------------------------------------------------------------

bool CanAttemptNewDrive()
{
    if (!isDriving && StartButtonPressed())
    {
        unsigned long t = millis();
        while (StartButtonPressed())
        {
            if (millis() - t > StartButtonDeadlockRelease) break; // prevent deadlock
        }

        SwitchDriveDirection();

        Serial.println("Button was Pressed and Released");
        return true;
    }

    return false;
}


// ---------------------------------------------------------------------------
// DIRECTION CONTROL
// ---------------------------------------------------------------------------

void SwitchDriveDirection()
{
    DriveDirection = (DriveDirection == CLOSE) ? staticDriveDirection::OPEN : staticDriveDirection::CLOSE;

    Serial.println("Has Set New Drive Direction!");
}


// ---------------------------------------------------------------------------
// MAIN DRIVE LOGIC – FIXED: reversal actually drives motor
// ---------------------------------------------------------------------------

bool AttemptDrive()
{
    if (isDriving)
    {
        DriveStop();
        Serial.println("Abort Attempt Drive as is Driving and entering Attempt");
        return false;
    }

    SaveLastDriveAttemptSatus(false);
    bool bSecondAttempt_ = false;

    SetDriveTime();
    (DriveDirection == CLOSE) ? DriveClose() : DriveOpen();   // starts driving in new direction

    // ---------------------------------------------------------------------
    // End-switch handling:
    // If the button is already pressed at start, we ignore that state
    // until we have seen it RELEASED once.
    // ---------------------------------------------------------------------
    bool haveSeenReleased = !EndReached();
    bool reachedLogicalEnd = false;

    while (isDriving && !reachedLogicalEnd)
    {
        Serial.println("Attempt Driving");

        if (TLE4207_Error())
        {
            Serial.println("TLE ERR in AttemptDrive");
            DriveStop();
            return false;
        }

        bool endNow = EndReached();

        if (!haveSeenReleased)
        {
            if (!endNow)
            {
                haveSeenReleased = true;
                Serial.println("Left initial end switch state");
            }
        }
        else
        {
            if (endNow)
            {
                reachedLogicalEnd = true;
                Serial.println("Logical end reached for this attempt");
                break;
            }
        }

        // -----------------------------------------------------------------
        // OVERTIME LOGIC – FIXED: reversal must re-drive with new direction
        // -----------------------------------------------------------------
        bool overtime = DriveOvertime(bSecondAttempt_);

        if (overtime && !bSecondAttempt_)
        {
            bSecondAttempt_ = true;
            SetDriveTime();
            SwitchDriveDirection();

            // FIX: actually drive the motor in the new direction
            (DriveDirection == CLOSE) ? DriveClose() : DriveOpen();

            haveSeenReleased = !EndReached();

            Serial.println("Attempted Drive went to OT and Reversed");
        }
        else if (overtime && bSecondAttempt_)
        {
            DriveStop();
            Serial.println("Attempted Drive went to OT and still not reached End");
            return false;
        }
    }

    DriveStop();

    if (reachedLogicalEnd)
    {
        Serial.println("Attempt Drive was Successful");
        SaveLastDriveAttemptSatus(true);
        return true;
    }
    else
    {
        Serial.println("Attempt Drive ended without reaching end (drive stopped or external condition)");
        return false;
    }
}


// ---------------------------------------------------------------------------
// STATE HELPERS
// ---------------------------------------------------------------------------

void SetDriveStatus(bool bNewDriveStatus)
{
    isDriving = bNewDriveStatus;
    Serial.println("Drive Status was changed");
}

void SetDriveTime()
{
    DriveStartTime = millis();
    Serial.println("DriveTime was set");
}


// ---------------------------------------------------------------------------
// TIMEOUT LOGIC – FIXED: millis() rollover-safe
// ---------------------------------------------------------------------------

bool DriveOvertime(bool bNeedDoubleTimeSecondDrive)
{
    unsigned long allowedTime = bNeedDoubleTimeSecondDrive
        ? (MAX_DRIVE_TIME_MS * 2)
        : MAX_DRIVE_TIME_MS;

    return (millis() - DriveStartTime) > allowedTime;
}


// ---------------------------------------------------------------------------
// EEPROM STATE SAVE/LOAD
// ---------------------------------------------------------------------------

void SaveLastDriveAttemptSatus(bool bSuccessful)
{
    bLastDriveAttemptSuccessful = bSuccessful;
    saveState(LastDriveAttemptSuccessfulStateAdress, bLastDriveAttemptSuccessful);

    Serial.println("SAVED");
}

void LoadLastDriveAttemptStatus()
{
    bLastDriveAttemptSuccessful = loadState(LastDriveAttemptSuccessfulStateAdress);
    Serial.println("LOADED");
}

void LoadLastDriveDirection()
{
    DriveDirection = loadState(DriveDirectionStateAdress)
        ? staticDriveDirection::OPEN
        : staticDriveDirection::CLOSE;
}

