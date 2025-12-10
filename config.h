/*
 Name:		config.h
 Created:	12/6/2025 10:00:21 PM
 Author:	Jonas-Gabriel
*/

//
// Hardware InputsOutputs ATTiny45-20
#define PIN_INH 2
#define	PIN_IN1 4
#define PIN_IN2 1
#define PIN_ERR 5
#define INPUT_START_BUTTON 3
#define INPUT_END_BUTTON 0
//

/*Hardware InputsOutputs Arduino
#define PIN_INH 2
#define	PIN_IN1 3
#define PIN_IN2 4
#define PIN_ERR 5 
#define INPUT_START_BUTTON 6
#define INPUT_END_BUTTON 7
*/

// EEPROM Adress to Save
#define ADDRESS_LastDriveDirection 2
#define ADDRESS_BIT_LastDriveDirection 0

// Drive CONFIG
#define SHOULD_RECOVER_AT_BOOT 1
#define SHOULD_RECOVER_TO_CLOSE_AT_BOOT 1
#define MAX_DRIVE_TIMEOUT 3000
#define END_BUTTON_LOCKTIME_CONSIDER_SUCCESS 1000
#define END_BUTTON_RELEASED_TIMER_BEFORE_CONSIDER_SUCCESS 500
#define MAX_TRYS_BEFORE_SAFETY_STOP 2

// Timer to release button automatically
#define StartButtonDeadlockRelease 1000

// Timer to check ERROR State (WatchDog relieve)
#define WATCHDOG_RELIEVE_TLE4207_ERR_STATE 1000