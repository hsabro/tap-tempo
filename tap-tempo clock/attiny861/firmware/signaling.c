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
#include <stdlib.h>

#include "main.h"
#include "signaling.h"

//
// Book keeping defines.
//

#define TEMPO_TO_FREQUENCY              1000.0f

#define WAVEFORM_RESOLUTION             256

#define TEMPO_AVERAGE_MAX_COUNT         10

//
// Timer0 sample rate based on wave table resolution:
// 8Mhz / 256 =  31.25kHz
// 8MHz / 128 =  62.50kHz
// 8MHz /  64 = 125.00kHz
//

#define TIMER1_SAMPLE_RATE              (CLOCK_FREQUENCY / WAVEFORM_RESOLUTION)
#define DUTY_CYCLE_DIVISOR              (0x100000000 / TIMER1_SAMPLE_RATE)

//
// Local function prototypes.
//

void RecalculateTempo();
uint16_t CalculateAverageTempo(uint16_t tempo);

//
// Global variables.
//

volatile uint16_t g_base_tempo;

volatile uint32_t g_base_duty_cycle;
volatile uint8_t g_base_table_index = 0xff;
volatile uint32_t g_base_phase_accumulator;

volatile int16_t g_tempo_adjust_offset;

volatile uint16_t g_average_tempo[TEMPO_AVERAGE_MAX_COUNT];
volatile uint8_t g_average_tempo_count = 0;

extern volatile state_flags g_state;
extern volatile uint16_t g_tempo_ms_count;

/*====== Public functions ===================================================== 
=============================================================================*/

void SetBaseTempo(uint16_t milliseconds)
{
    //
    // Do some boundary checking on the requested frequency. We're only going
    // to accept frequencies in the range of 0.1Hz - 20Hz.
    //
    
    if ((milliseconds > LFO_MIN_TEMPO) || (milliseconds < LFO_MAX_TEMPO))
    {
        return;
    }
    
    //
    // If tempo averaging is enabled the current reading is passed on for
    // further calculation and will more than likely be modified somewhat.
    //

    if ((g_state.is_clock_input_source == 0) && (g_state.is_averaging_tempo == 1))
    {
        milliseconds = CalculateAverageTempo(milliseconds);
    }
    
    //
    // No need to recalculate if the new tempo count is just a few milliseconds
    // off (would be typical when running off an external clock pulse).
    //
    // 2ms +/- seems to eliminate any syncing irregularities when clocked from
    // an external tap-tempo chip.
    //
    
    if ((g_base_tempo > (milliseconds + 2)) || (g_base_tempo < (milliseconds - 2)))
    {
        //
        // Store the new base tempo and recalculate how that affects the actual
        // output.
        //
        
        g_base_tempo = milliseconds;
        g_tempo_adjust_offset = 0;
        
        RecalculateTempo();
    }        
}

void StartTempoCount()
{
    //
    // Sync the LFO output and start the tempo counting.
    //
    
    g_tempo_ms_count = 0;
    g_state.is_counting_tempo = 1;
    
    ResetBaseTempo();
    
    PORTB &= ~(1 << SYNC_OUT); // Pull low.
    PORTB &= ~(1 << SYNC_2X_OUT); // Pull low.
}

void StopTempoCount()
{
    //
    // Set the new tempo and reset the tempo counting state.
    //
    
    if (g_state.is_counting_tempo == 1)
    {
        g_state.is_counting_tempo = 0;
        
        SetBaseTempo(g_tempo_ms_count);
        g_tempo_ms_count = 0;
    }
    
    ResetBaseTempo();
    
    PORTB |= (1 << SYNC_OUT);   // Pull high.
    PORTB &= ~(1 << SYNC_2X_OUT); // Pull low.
    
    //
    // Toggle off the "tap input registration active"-indicator LED in case
    // it's on.
    //

    PORTB |= (1 << TAP_ACTIVE_OUT);
}

void TempoCountTimeout()
{
    //
    // Exit the tempo counting state without making any changes, discarding any
    // tempo count.
    //
    
    g_state.is_counting_tempo = 0;
    g_tempo_ms_count = 0;
    
    //
    // Toggle off the "tap input registration active"-indicator LED in case
    // it's on.
    //

    PORTB |= (1 << TAP_ACTIVE_OUT);
}

void ResetBaseTempo()
{
    //
    // Reset phase accumulator and wave table index for the base tempo.
    //
    
    g_base_table_index = 0;
    g_base_phase_accumulator = 0;
}

void AdjustSpeed(int16_t change_value)
{
    //
    // Make sure the result doesn't exceed either upper or lower LFO limits.
    //
    
    int16_t new_tempo_count = g_base_tempo + g_tempo_adjust_offset + change_value;
    
    if ((new_tempo_count > LFO_MIN_TEMPO) || (new_tempo_count < LFO_MAX_TEMPO))
    {
        return;
    }
    
    g_tempo_adjust_offset += change_value;
    RecalculateTempo();
}

void ResetSpeedAdjustSetting()
{
    g_tempo_adjust_offset = 0;
    RecalculateTempo();
}

/*====== Local functions ====================================================== 
=============================================================================*/

void RecalculateTempo()
{
    float new_frequency;

    //
    // Convert the new tempo from a millisecond count to a frequency.
    //
    
    new_frequency = (TEMPO_TO_FREQUENCY / (float)(g_base_tempo + g_tempo_adjust_offset));
    
    //
    // Recalculate the base duty cycle based on the new frequency.
    //
    
    g_base_duty_cycle = new_frequency * DUTY_CYCLE_DIVISOR;
}

uint16_t CalculateAverageTempo(uint16_t tempo)
{
    static uint8_t index = 0;
    uint8_t i;

    //
    // Store the current tempo reading in the correct array spot and update
    // the reading count if we haven't yet reached the maximum.
    //

    if (g_average_tempo_count < TEMPO_AVERAGE_MAX_COUNT)
    {
        index = g_average_tempo_count;
        g_average_tempo_count++;
    }

    g_average_tempo[index] = tempo;

    //
    // Update the array index making sure to wrap around the index if we reach
    // the end. This way we always overwrite the oldest recorded tempo.
    //

    index++;
    if (index >= TEMPO_AVERAGE_MAX_COUNT)
    {
        index = 0;
    }

    //
    // Add up and divide by sample count to get a basic average tempo.
    // Repurposing the input argument variable since we no longer need it. Note
    // that average count will always be at least one at this point, so there's
    // no risk dividing by zero.
    //

    tempo = 0;
    for (i = 0; i < g_average_tempo_count; i++)
    {
        tempo += g_average_tempo[i];
    }
    tempo = tempo / g_average_tempo_count;

    return tempo;
}
