//
// Tap-tempo LFO for 8-bit AVR.
// 
// Copyright (C) 2013-2016 Harald Sabro
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

//
// Global variables.
//

volatile uint8_t g_switch_samples[DEBOUNCE_CHECK_COUNT];

volatile uint8_t g_closed_switch_state;
volatile uint8_t g_open_switch_state;
volatile uint8_t g_closed_switch_state_changed;
volatile uint8_t g_open_switch_state_changed;

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
}

void DebounceSwitches()
{
	static uint8_t switch_state_index = 0;
	
	//
	// Get the current state of all PA pins and store them at whatever state
    // index we're currently at, making sure to reset the state index whenever
    // we go past the max count.
	//
	
	g_switch_samples[switch_state_index] = PINB;
	
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
