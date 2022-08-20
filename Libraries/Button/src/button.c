/* This is class' functions for different kinds of buttons
*/

// Includes --------------------------------------------------------------------
#include "button.h"

// Constants -------------------------------------------------------------------
#ifndef PRESS_PER_VAL
#	define PRESS_PER_VAL 				3
#endif // PRESS_PER_VAL

#ifndef UNPRESS_PER_VAL
#	define UNPRESS_PER_VAL 				5
#endif // UNPRESS_PER_VAL

#ifndef LONG_PRESS_PER_VAL
#	define LONG_PRESS_PER_VAL 			10
#endif // LONG_PRESS_PER_VAL

// Public functions -----------------------------------------------------------
void InitHoldButtonState(struct HoldButton* button)
{
	button->m_hold_counter = 0;
	button->m_pressed = false;
	
#ifdef WATCH_FOR_INPUT_CHANGES
	button->m_stateChanged = false;
#endif // WATCH_FOR_INPUT_CHANGES
}

void SetHoldButtonState(struct HoldButton* button, bool currState)
{
	if(!(button->m_pressed))
	{
		if(currState)
		{
			button->m_hold_counter++;
			if(button->m_hold_counter >= PRESS_PER_VAL)
			{
				button->m_hold_counter = 0;
				button->m_pressed = true;
				
#ifdef WATCH_FOR_INPUT_CHANGES
				button->m_stateChanged = true;
#endif // WATCH_FOR_INPUT_CHANGES
			}
		}
		else button->m_hold_counter = 0;
	}
	else
	{
		if(!currState)
		{
			button->m_hold_counter++;
			if(button->m_hold_counter >= UNPRESS_PER_VAL)
			{
				button->m_hold_counter = 0;
				button->m_pressed = false;
				
#ifdef WATCH_FOR_INPUT_CHANGES
				button->m_stateChanged = true;
#endif // WATCH_FOR_INPUT_CHANGES
			}
		}
		else button->m_hold_counter = 0;
	}
}

bool HoldButtonIsPressed(struct HoldButton* button)
{
	return button->m_pressed;
}

#ifdef WATCH_FOR_INPUT_CHANGES
bool HoldButtonStateChanged(struct HoldButton* button)
{
	if(button->m_stateChanged)
	{
		button->m_stateChanged = false;
		return true;
	}
	return false;
}
#endif // WATCH_FOR_INPUT_CHANGES

//------------------------------------------------------------------------------
void InitLockButtonState(struct LockButton* button)
{
	button->m_hold_counter = 0;
	button->m_pressed = false;
	button->m_state = false;
	
#ifdef WATCH_FOR_INPUT_CHANGES
	button->m_stateChanged = false;
#endif // WATCH_FOR_INPUT_CHANGES
}

void SetLockButtonState(struct LockButton* button, bool currState)
{
	if(!(button->m_pressed))
	{
		if(currState)
		{
			button->m_hold_counter++;
			if(button->m_hold_counter >= PRESS_PER_VAL)
			{
				button->m_hold_counter = 0;
				button->m_pressed = true;
				button->m_state = !(button->m_state);
				
#ifdef WATCH_FOR_INPUT_CHANGES
				button->m_stateChanged = true;
#endif // WATCH_FOR_INPUT_CHANGES
			}
		}
		else button->m_hold_counter = 0;
	}
	else
	{
		if(!currState)
		{
			button->m_hold_counter++;
			if(button->m_hold_counter >= UNPRESS_PER_VAL)
			{
				button->m_hold_counter = 0;
				button->m_pressed = false;
				
#ifdef WATCH_FOR_INPUT_CHANGES
				button->m_stateChanged = true;
#endif // WATCH_FOR_INPUT_CHANGES
			}
		}
		else button->m_hold_counter = 0;
	}
}

bool LockButtonIsPressed(struct LockButton* button)
{
	return button->m_state;
}

void SetLockButtonOnState(struct LockButton* button)
{
	button->m_hold_counter = 0;
	button->m_pressed = true;
	button->m_state = true;
}

void SetLockButtonOffState(struct LockButton* button)
{
	button->m_hold_counter = 0;
	button->m_pressed = true;
	button->m_state = false;
}

#ifdef WATCH_FOR_INPUT_CHANGES
bool LockButtonStateChanged(struct LockButton* button)
{
	if(button->m_stateChanged)
	{
		button->m_stateChanged = false;
		return true;
	}
	return false;
}
#endif // WATCH_FOR_INPUT_CHANGES

//------------------------------------------------------------------------------
void InitPressEventButtonState(struct PressEventButton* button)
{
	button->m_hold_counter = 0;
	button->m_pressed = false;
	button->m_was_pressed = false;
	
#ifdef WATCH_FOR_INPUT_CHANGES
	button->m_stateChanged = false;
#endif // WATCH_FOR_INPUT_CHANGES
}

void SetPressEventButtonState(struct PressEventButton* button, bool currState)
{
	if(!(button->m_pressed))
	{
		if(currState)
		{
			button->m_hold_counter++;
			if(button->m_hold_counter >= PRESS_PER_VAL)
			{
				button->m_hold_counter = 0;
				button->m_pressed = true;
				button->m_was_pressed = true;
				
#ifdef WATCH_FOR_INPUT_CHANGES
				button->m_stateChanged = true;
#endif // WATCH_FOR_INPUT_CHANGES
			}
		}
		else button->m_hold_counter = 0;
	}
	else
	{
		if(!currState)
		{
			button->m_hold_counter++;
			if(button->m_hold_counter >= UNPRESS_PER_VAL)
			{
				button->m_hold_counter = 0;
				button->m_pressed = false;
				
#ifdef WATCH_FOR_INPUT_CHANGES
				button->m_stateChanged = true;
#endif // WATCH_FOR_INPUT_CHANGES
			}
		}
		else button->m_hold_counter = 0;
	}
}

bool PressEventButtonWasPressed(struct PressEventButton* button)
{
	if(button->m_was_pressed)
	{
		button->m_was_pressed = false;
		return true;
	}
	return false;
}

#ifdef WATCH_FOR_INPUT_CHANGES
bool PressEventButtonStateChanged(struct PressEventButton* button)
{
	if(button->m_stateChanged)
	{
		button->m_stateChanged = false;
		return true;
	}
	return false;
}
#endif // WATCH_FOR_INPUT_CHANGES

//------------------------------------------------------------------------------
void InitPressHoldEventButtonState(struct PressHoldEventButton* button)
{
	button->m_hold_counter = 0;
	button->m_unhold_counter = 0;
	button->m_was_pressed = false;
	button->m_was_long_pressed = false;
	button->m_catch_unpress = false;
	
#ifdef WATCH_FOR_INPUT_CHANGES
	button->m_stateChanged = false;
#endif // WATCH_FOR_INPUT_CHANGES
}

void SetPressHoldEventButtonState(struct PressHoldEventButton* button,
								 bool currState)
{
	if(currState)
	{
		// Check only for pressing
		button->m_unhold_counter = 0;
	
		if(button->m_hold_counter < LONG_PRESS_PER_VAL) 
		{
			button->m_hold_counter ++;
				
#ifdef WATCH_FOR_INPUT_CHANGES
			if(button->m_hold_counter == PRESS_PER_VAL)
			{
				button->m_stateChanged = true;
			}
#endif // WATCH_FOR_INPUT_CHANGES
		}
		else if(button->m_hold_counter == LONG_PRESS_PER_VAL)
		{
			button->m_hold_counter ++;
			button->m_was_long_pressed = true;
			button->m_catch_unpress = true;
		}
	}
	else
	{
		if(button->m_unhold_counter < UNPRESS_PER_VAL) 
			button->m_unhold_counter ++;
		else if(button->m_unhold_counter == UNPRESS_PER_VAL)
		{
			button->m_unhold_counter ++;
			if(button->m_catch_unpress) button->m_catch_unpress = false;
			else if(button->m_hold_counter >= PRESS_PER_VAL) 
				button->m_was_pressed = true;
			button->m_hold_counter = 0;
			
#ifdef WATCH_FOR_INPUT_CHANGES
			button->m_stateChanged = true;
#endif // WATCH_FOR_INPUT_CHANGES
		}
	}
}

bool PressHoldEventButtonIsHolded(struct PressHoldEventButton* button)
{
	if(button->m_hold_counter == (LONG_PRESS_PER_VAL + 1)) return true;
	return false;
}

bool PressHoldEventButtonWasPressed(struct PressHoldEventButton* button)
{
	if(button->m_was_pressed)
	{
		button->m_was_pressed = false;
		return true;
	}
	return false;
}

bool PressHoldEventButtonWasLongPressed(struct PressHoldEventButton* button)
{
	if(button->m_was_long_pressed)
	{
		button->m_was_long_pressed = false;
		return true;
	}
	return false;
}

#ifdef WATCH_FOR_INPUT_CHANGES
bool PressHoldEventButtonStateChanged(struct PressHoldEventButton* button)
{
	if(button->m_stateChanged)
	{
		button->m_stateChanged = false;
		return true;
	}
	return false;
}
#endif // WATCH_FOR_INPUT_CHANGES
