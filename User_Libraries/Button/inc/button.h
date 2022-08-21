#ifndef _BUTTON_H_
#define _BUTTON_H_

// Includes --------------------------------------------------------------------
#include <stdint.h>
#include <stdbool.h>
#include "settings.h"

// Structs definitions ---------------------------------------------------------
struct PressEventButton
{
	uint8_t m_hold_counter;
	volatile bool m_pressed;
	volatile bool m_was_pressed;
	
#ifdef WATCH_FOR_INPUT_CHANGES
	volatile bool m_stateChanged;
#endif // WATCH_FOR_INPUT_CHANGES
};

struct HoldButton
{	
	uint8_t m_hold_counter;
	volatile bool m_pressed;
	
#ifdef WATCH_FOR_INPUT_CHANGES
	volatile bool m_stateChanged;
#endif // WATCH_FOR_INPUT_CHANGES
};

struct PressHoldEventButton
{
	uint8_t m_hold_counter;
	uint8_t m_unhold_counter;
	volatile bool m_was_pressed;
	volatile bool m_was_long_pressed;
	volatile bool m_catch_unpress;
	
#ifdef WATCH_FOR_INPUT_CHANGES
	volatile bool m_stateChanged;
#endif // WATCH_FOR_INPUT_CHANGES
};

struct LockButton
{
	uint8_t m_hold_counter;
	volatile bool m_pressed;
	volatile bool m_state;
	
#ifdef WATCH_FOR_INPUT_CHANGES
	volatile bool m_stateChanged;
#endif // WATCH_FOR_INPUT_CHANGES
};

// Functions prototypes --------------------------------------------------------
// Functions prototypes for "PressEventButton" struct
void InitPressEventButtonState(struct PressEventButton* button);
void SetPressEventButtonState(struct PressEventButton* button, bool currState);
bool PressEventButtonWasPressed(struct PressEventButton* button);

#ifdef WATCH_FOR_INPUT_CHANGES
bool PressEventButtonStateChanged(struct PressEventButton* button);
#endif // WATCH_FOR_INPUT_CHANGES

// Functions prototypes for "HoldButton" struct
void InitHoldButtonState(struct HoldButton* button);
void SetHoldButtonState(struct HoldButton* button, bool currState);
bool HoldButtonIsPressed(struct HoldButton* button);

#ifdef WATCH_FOR_INPUT_CHANGES
bool HoldButtonStateChanged(struct HoldButton* button);
#endif // WATCH_FOR_INPUT_CHANGES

// Functions prototypes for "PressHoldEventButton" struct
void InitPressHoldEventButtonState(struct PressHoldEventButton* button);
void SetPressHoldEventButtonState(struct PressHoldEventButton* button,
								 bool currState);
bool PressHoldEventButtonWasPressed(struct PressHoldEventButton* button);
bool PressHoldEventButtonWasLongPressed(struct PressHoldEventButton* button);
bool PressHoldEventButtonIsHolded(struct PressHoldEventButton* button);

#ifdef WATCH_FOR_INPUT_CHANGES
bool PressHoldEventButtonStateChanged(struct PressHoldEventButton* button);
#endif // WATCH_FOR_INPUT_CHANGES

// Functions prototypes for "LockButton" struct
void InitLockButtonState(struct LockButton* button);
void SetLockButtonState(struct LockButton* button, bool currState);
void SetLockButtonOnState(struct LockButton* button);
void SetLockButtonOffState(struct LockButton* button);
bool LockButtonIsPressed(struct LockButton* button);

#ifdef WATCH_FOR_INPUT_CHANGES
bool LockButtonStateChanged(struct LockButton* button);
#endif // WATCH_FOR_INPUT_CHANGES

#endif // _BUTTON_H_
