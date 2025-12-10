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

static DriveDirections CurrentDriveDirection = DriveDirections::CLOSE;
static bool isDriving = false;
static unsigned long DriveStartTime = 0;



// the setup function runs once when you press reset or power the board
void setup() 
{
    pinMode(PIN_INH, OUTPUT);
    pinMode(PIN_IN1, OUTPUT);
    pinMode(PIN_IN2, OUTPUT);
    pinMode(INPUT_START_BUTTON, INPUT_PULLUP);
    pinMode(INPUT_END_BUTTON, INPUT);
    pinMode(PIN_ERR, INPUT);

    DriveStop();
    LoadDriveDirectionFromEEPROM();
}

// the loop function runs over and over again until power down or reset
void loop() 
{
    static bool doOnce = false;

    // STOP: DO NOT CONTINUE while IC is in ERR STATE;
    while (TLE4207_Error())
    {
        DriveStop();
        delay(WATCHDOG_RELIEVE_TLE4207_ERR_STATE);
    }

    if (!doOnce)
    {
        if (SHOULD_RECOVER_AT_BOOT == 1 && !EndReached())
        {

            if (SHOULD_RECOVER_TO_CLOSE_AT_BOOT == 1)
            {
                SetDesiredDriveDirection(DriveDirections::CLOSE);
            }

            AttemptDrive();
        }

        doOnce = true;
    }

    if (CanAttemptNewDrive())
    {
        AttemptDrive();
    }
}

bool AttemptDrive()
{
    if (isDriving)
    {
        // for some reason isDriving is set to true, abort;
        DriveStop();
        return false;
    }

    // ---------------------------------------------------------------------
    // End-switch handling:
    // If the button is already pressed at start, we ignore that state
    // until we have seen it RELEASED once.
    // ---------------------------------------------------------------------
    bool wasEndButtonPressedAtBeginning = EndReached();
    bool LogicalEndDependsOnEndButtonReleased = wasEndButtonPressedAtBeginning;

    bool reachedLogicalEnd = false; // only when the button is pressed we reach the end. the end however should not be considered as true if the button is already pressed at start 
    int AttemptsBeforeSafteyStop = 1;

    LogTimeAndDriveMotor();

    while (isDriving && !reachedLogicalEnd)
    {
        if (TLE4207_Error())
        {
            DriveStop();
            return false;
        }

        if (wasEndButtonPressedAtBeginning) 
        {
            if (!EndReached() && END_BUTTON_RELEASED_TIMER_BEFORE_CONSIDER_SUCCESS) 
            {
                LogicalEndDependsOnEndButtonReleased = false;
            }
            else if (ShouldConsiderDriveAttemptSuccessOnLockedButton()) 
            {
                DriveStop();
                reachedLogicalEnd = true;
                break;
            }
        }
        else if(EndReached() && !LogicalEndDependsOnEndButtonReleased)
        {
                DriveStop();
                reachedLogicalEnd = true;
                break;
        }
        // -----------------------------------------------------------------
        // OVERTIME LOGIC - if the drive can not reach its desired end at given time, reverse drive direction and try to reach other end, inverse as many times as given
        // -----------------------------------------------------------------

        if (GetDriveTimeOut() && MAX_TRYS_BEFORE_SAFETY_STOP > 1) 
        {
            if (AttemptsBeforeSafteyStop < MAX_TRYS_BEFORE_SAFETY_STOP)
            {
                AttemptsBeforeSafteyStop++;
                SwitchDriveDirection();
                LogTimeAndDriveMotor();
            }
            else
            {
                DriveStop();
                return false;
            }
        }
    }

    if (reachedLogicalEnd)
    {
        return true;
    }
    else
    {
        return false;
    }
}


// ---------------------------------------------------------------------------
// STATE HELPERS
// ---------------------------------------------------------------------------

void SetDriveStatus(bool bNewDriveStatus)
{
    isDriving = bNewDriveStatus;
}

void LogTimeAndDriveMotor()
{
    DriveStartTime = millis();
    (CurrentDriveDirection == CLOSE) ? DriveClose() : DriveOpen();   // starts driving in new direction
}

// ---------------------------------------------------------------------------
// DIRECTION CONTROL
// ---------------------------------------------------------------------------

void SwitchDriveDirection()
{
    (CurrentDriveDirection == CLOSE) 
        ? SetDesiredDriveDirection(DriveDirections::OPEN)
        : SetDesiredDriveDirection(DriveDirections::CLOSE);
}

void SetDesiredDriveDirection(DriveDirections NewDriveDirection)
{
    CurrentDriveDirection = NewDriveDirection;

    SaveDriveDirectionToEEPROM();
}

// ---------------------------------------------------------------------------
// TIMEOUT LOGIC � millis() rollover-safe
// ---------------------------------------------------------------------------

bool GetDriveTimeOut()
{
    unsigned long allowedTime = MAX_DRIVE_TIMEOUT;

    return (millis() - DriveStartTime) > allowedTime;
}

bool ShouldConsiderDriveAttemptSuccessOnLockedButton()
{
    return (millis() - DriveStartTime) > END_BUTTON_LOCKTIME_CONSIDER_SUCCESS;
}

// ---------------------------------------------------------------------------
// EEPROM SAVE/LOAD
// ---------------------------------------------------------------------------

void SaveDriveDirectionToEEPROM()
{
    EEPROMBitAccess::writeBit(ADDRESS_LastDriveDirection, ADDRESS_BIT_LastDriveDirection, CurrentDriveDirection);
}

void LoadDriveDirectionFromEEPROM()
{
    CurrentDriveDirection = 
        EEPROMBitAccess::readBit(ADDRESS_LastDriveDirection, ADDRESS_BIT_LastDriveDirection) == 1
        ? DriveDirections::OPEN
        : DriveDirections::CLOSE;
}

// ---------------------------------------------------------------------------
// MOTOR CONTROL
// ---------------------------------------------------------------------------

void DriveClose()
{
    SetDriveStatus(true);

    digitalWrite(PIN_IN1, HIGH);
    digitalWrite(PIN_IN2, LOW);
    digitalWrite(PIN_INH, HIGH);
}

void DriveOpen()
{
    SetDriveStatus(true);

    digitalWrite(PIN_IN1, LOW);
    digitalWrite(PIN_IN2, HIGH);
    digitalWrite(PIN_INH, HIGH);
}

void DriveStop()
{
    digitalWrite(PIN_IN1, LOW);
    digitalWrite(PIN_IN2, LOW);
    digitalWrite(PIN_INH, LOW);

    SetDriveStatus(false);
}

// ---------------------------------------------------------------------------
// SIGNALS
// ---------------------------------------------------------------------------

bool TLE4207_Error()
{
    return false;
    // inverse as LOW is OK!
    return !digitalRead(PIN_ERR);
}

bool StartButtonPressed()
{
    return !digitalRead(INPUT_START_BUTTON);
}

bool EndReached()
{
    // inverse as LOW is PRESSED!
    return !digitalRead(INPUT_END_BUTTON);
}

// ---------------------------------------------------------------------------
// INPUT CONTROL � FIXED: button release timeout
// ---------------------------------------------------------------------------

bool CanAttemptNewDrive()
{
    if (!isDriving && StartButtonPressed())
    {
        unsigned long button_pressed_time = millis();
        while (StartButtonPressed())
        {
            if (millis() - button_pressed_time > StartButtonDeadlockRelease)
            {
                break;
            }
        }

        SwitchDriveDirection();
        return true;
    }

    return false;
}

