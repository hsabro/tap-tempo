//
// Tap-tempo LFO for 8-bit AVR.
// 
// Copyright (C) 2015-2016 Harald Sabro
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// Contact info
// ------------
// Website: sabrotone.com
// Email: harald (AT) website 
//

#include <avr/io.h>

#include "main.h"
#include "signaling.h"
#include "switching.h"

//
// Defines and structs.
// 

//
// Millisecond count before considering the switch state stable.
//

#define DEBOUNCE_CHECK_COUNT        10

typedef enum
{
    SelectionModeSpeed = 0,
    SelectionModeWaveform,
    SelectionModeMultiplier
} SelectionMode;

//
// Global variables.
//

volatile uint8_t g_switch_samples[DEBOUNCE_CHECK_COUNT];

volatile uint8_t g_closed_switch_state;
volatile uint8_t g_open_switch_state;
volatile uint8_t g_closed_switch_state_changed;
volatile uint8_t g_open_switch_state_changed;

volatile SelectionMode g_selection_mode = SelectionModeMultiplier;

volatile uint8_t g_speed_adjust_multiplier;
volatile uint16_t g_continuous_speed_adjustments;
volatile uint16_t g_speed_adjustment_ms_count;

/*====== Public functions ===================================================== 
=============================================================================*/

void InitializeSwitching()
{
    uint8_t count;
    
    //
    // We want to start out in an "all switches open" state; i.e. all 1's.
    //
    
    for (count = 0; count < DEBOUNCE_CHECK_COUNT; count++)
    {
        g_switch_samples[count] = 0xff;
    }
    
    g_closed_switch_state = 0xff;
    g_open_switch_state = 0xff;
    
    //
    // Start out with no switch state changes, either open or close.
    //
    
    g_closed_switch_state_changed = 0x00;
    g_open_switch_state_changed = 0x00;
    
    //
    // Set the selection mode to multiplier, and then toggle to the next mode
    // (speed adjust), also taking care of additional initialization.
    //
    
    g_selection_mode = SelectionModeMultiplier;
    SetNextSelectionMode();
    
    g_continuous_speed_adjustments = 0;
    g_speed_adjust_multiplier = 1;
}

void DebounceSwitches()
{
	static uint8_t switch_state_index = 0;
	
	//
	// Get the current state of all PA pins and store them at whatever state
    // index we're currently at, making sure to reset the state index whenever
    // we go past the max count.
	//
	
	g_switch_samples[switch_state_index] = PINA;
	
	switch_state_index++;
	if (switch_state_index >= DEBOUNCE_CHECK_COUNT)
	{
		switch_state_index = 0;
	}
}

void CalculateSwitchStates()
{
    uint8_t count;
	uint8_t accumulated_closed_switch_state = 0x00;
    uint8_t accumulated_open_switch_state = 0xff;
	uint8_t previous_closed_switch_state;
    uint8_t previous_open_switch_state;
    
    //
	// Build accumulated switch states based on DEBOUNCE_CHECK_COUNT number of
    // stored states by OR'ing and AND'ing everything together. If a particular
    // switch pin stayed "closed" (logic low) the entire time the corresponding
    // debounce state bit will remain clear, and likewise, as soon as one or
    // more reading on a particular pin reads "open" that debounce state bit
    // will be cleared. And the opposite for open state.
	//
	
	for (count = 0; count < DEBOUNCE_CHECK_COUNT; count++)
	{
		accumulated_closed_switch_state |= g_switch_samples[count];
        accumulated_open_switch_state &= g_switch_samples[count];
	}
    
    //
    // Store the new switch states, and set the corresponding changed bit
    // for each switch that changed state since last time.
    //
	
	previous_closed_switch_state = g_closed_switch_state;
    previous_open_switch_state = g_open_switch_state;
    
	g_closed_switch_state = accumulated_closed_switch_state;
    g_open_switch_state = accumulated_open_switch_state;
	
    g_closed_switch_state_changed = g_closed_switch_state ^ previous_closed_switch_state;
    g_open_switch_state_changed = g_open_switch_state ^ previous_open_switch_state;
}

uint8_t SwitchWasClosed(uint8_t pins)
{
    //
    // Return the results from the last calculated closed switch state,
    // filtered on the requested pin(s).
    //
    
    return (~g_closed_switch_state & g_closed_switch_state_changed) & pins;
}

uint8_t SwitchWasOpened(uint8_t pins)
{
    //
    // Return the results from the last calculated open switch state, filtered
    // on the requested pin(s).
    //
    
    return (g_open_switch_state & g_open_switch_state_changed) & pins;
}

void SetNextSelectionMode()
{
    //
    // Pull all mode indicator pins high to turn them off.
    //
    
    PORTA |= (1 << WAVE_MODE_OUT) | (1 << MULTI_MODE_OUT);
    PORTB |= (1 << SPEED_MODE_OUT);
    
    switch (g_selection_mode)
    {
        case SelectionModeSpeed:
        
            //
            // Switch to waveform mode and turn on the associated indicator.
            //
            
            g_selection_mode = SelectionModeWaveform;
            PORTA &= ~(1 << WAVE_MODE_OUT);
            break;
            
        case SelectionModeWaveform:
            
            //
            // Switch to multiplier mode and turn on the associated indicator.
            //
            
            g_selection_mode = SelectionModeMultiplier;
            PORTA &= ~(1 << MULTI_MODE_OUT);
            break;
        
        case SelectionModeMultiplier:
            
            //
            // Switch to speed adjust mode and turn on the associated
            // indicator.
            //
            
            g_selection_mode = SelectionModeSpeed;
            PORTB &= ~(1 << SPEED_MODE_OUT);
            break;
        
        default:
            break;
    }
}

void ModifyCurrentSelectionMode(int8_t change_value)
{
    switch (g_selection_mode)
    {
        case SelectionModeSpeed:
        
            //
            // Dynamically increase and decrease the amount of adjustment
            // applied based on whether the user is quickly or slowly making
            // adjustments.
            //
            // Note: Probably have to be tweaked for optimal "feel". Will also
            //       be affected by the detent count of the rotary encoder and
            //       the radius of the knob used.
            //
            
            if (g_speed_adjustment_ms_count < 100)
            {
                //
                // Less than 100ms since previous user input. Counting this as
                // continuous input.
                //
                
                g_continuous_speed_adjustments++;
                
                //
                // For every 10 continuous input triggers, increase the size
                // of each speed adjustment step. 
                //
                
                if ((g_continuous_speed_adjustments % 10) == 0)
                {
                    g_speed_adjust_multiplier += 10;
                }
            }
            else if (g_speed_adjustment_ms_count > 1000)
            {
                //
                // No user input in a full second. Reset any continuous
                // adjust state.
                //
                
                g_speed_adjust_multiplier = 1;
                g_continuous_speed_adjustments = 0;
            }
            
            //
            // Reset the millisecond count since last user input.
            //
            
            g_speed_adjustment_ms_count = 0;
            
            //
            // Adjust the speed by the current speed adjust multiplier.
            // Multiply by -1 to swap polarity and the direction in which speed
            // increase and decrease.
            //
        
            AdjustSpeed(g_speed_adjust_multiplier * change_value * -1);
            break;
            
        case SelectionModeWaveform:
            
            SetWaveform(change_value);
            break;
        
        case SelectionModeMultiplier:
            
            SetMultiplier(change_value);
            break;
        
        default:
            break;
    }
}

void ResetCurrentSelectionMode()
{
    switch (g_selection_mode)
    {
        case SelectionModeSpeed:
        
            ResetSpeedAdjustSetting();
            break;
            
        case SelectionModeWaveform:
            
            ResetWaveformSetting();
            break;
        
        case SelectionModeMultiplier:
            
            ResetMultiplierSetting();
            break;
        
        default:
            break;
    }
}
