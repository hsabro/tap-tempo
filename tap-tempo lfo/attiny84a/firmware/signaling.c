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
#include <stdlib.h>

#include "main.h"
#include "signaling.h"

//
// Available waveforms.
//

typedef enum
{
    WaveformSine = 0,
    WaveformRampUp,
    WaveformRampDown,
    WaveformTriangle,
    WaveformSquare,
    WaveformRandom,
    WaveformCount           // Dummy entry to get the enum count.
} Waveform;

//
// Available tempo multipliers.
//

typedef enum
{
    MultiplierWhole = 0,
    MultiplierDottedHalf,
    MultiplierHalf,
    MultiplierDottedQuarter,
    MultiplierQuarter,
    MultiplierDottedEighth,
    MultiplierEighth,
    MultiplierDottedSixteenth,
    MultiplierTriplet,
    MultiplierSixteenth,
    MultiplierCount         // Dummy entry to get the enum count.
} Multiplier;

//
// Calculate duty cycle for multiplier[x] by taking the base duty cycle and
// multiplying by the correct ratio.
//

static const float k_multiplier_ratio[MultiplierCount] =
{
    0.25,       // Whole note.              (1/4) = 0.25 rate
    0.333334,   // Dotted half note.        (1/3) = ~0.333334 rate
    0.5,        // Half note.               (1/2) = 0.5 rate
    0.666667,   // Dotted quarter note.     (2/3) = ~0.666667 rate
    1.0,        // Quarter note.            (1/1) = 1 rate
    1.333334,   // Dotted eighth note.      (4/3) = ~1.333334 rate
    2.0,        // Eighth note.             (2/1) = 2 rate
    2.666667,   // Dotted sixteenth note.   (8/3) = ~2,666667 rate
    3.0,        // Triplet note.            (3/1) = 3 rate
    4.0         // Sixteenth note.          (4/1) = 4 rate
};

static const uint8_t k_multiplier_alignment[MultiplierCount] =
{
    4,  // Whole note.              Matches base tempo at 4/4.
    3,  // Dotted half note.        Matches base tempo at 3/4. 
    2,  // Half note.               Matches base tempo at 2/4.
    3,  // Dotted quarter note.     Matches base tempo at 3/4.
    1,  // Quarter note.            Base tempo. 
    3,  // Dotted eighth note.      Matches base tempo at 3/4.
    1,  // Eighth note.             Matches base tempo at 1/4.
    3,  // Dotted sixteenth note.   Matches base tempo at 3/4.
    2,  // Triplet note.            Matches base tempo at 1/4.
    1   // Sixteenth note.          Matches base tempo at 1/4.
};

//
// Number of base tempo counts between each time all multipliers align.
//

#define MULTIPLIER_ALIGNMENT_OFFSET     12

//
// Book keeping defines.
//

#define TEMPO_TO_FREQUENCY              1000.0f

#define WAVEFORM_RESOLUTION             256

//
// Sine wave table definition. Got this from a calculator here:
// http://www.daycounter.com/Calculators/Sine-Generator-Calculator.phtml
//
// Note: The basic sine wave has been realigned so that we're starting with
//       the lowest peak rather than the mid-point. This matches the phase
//       of the other waveforms.
//

static const uint8_t k_sine_table[WAVEFORM_RESOLUTION] =
{
      0,   0,   0,   0,   1,   1,   1,   2,   2,   3,   4,   5,   5,   6,   7,   9,
     10,  11,  12,  14,  15,  17,  18,  20,  21,  23,  25,  27,  29,  31,  33,  35,
     37,  40,  42,  44,  47,  49,  52,  54,  57,  59,  62,  65,  67,  70,  73,  76,
     79,  82,  85,  88,  90,  93,  97, 100, 103, 106, 109, 112, 115, 118, 121, 124,
    128, 131, 134, 137, 140, 143, 146, 149, 152, 155, 158, 162, 165, 167, 170, 173,
    176, 179, 182, 185, 188, 190, 193, 196, 198, 201, 203, 206, 208, 211, 213, 215,
    218, 220, 222, 224, 226, 228, 230, 232, 234, 235, 237, 238, 240, 241, 243, 244,
    245, 246, 248, 249, 250, 250, 251, 252, 253, 253, 254, 254, 254, 255, 255, 255,
    255, 255, 255, 255, 254, 254, 254, 253, 253, 252, 251, 250, 250, 249, 248, 246,
    245, 244, 243, 241, 240, 238, 237, 235, 234, 232, 230, 228, 226, 224, 222, 220,
    218, 215, 213, 211, 208, 206, 203, 201, 198, 196, 193, 190, 188, 185, 182, 179,
    176, 173, 170, 167, 165, 162, 158, 155, 152, 149, 146, 143, 140, 137, 134, 131,
    128, 124, 121, 118, 115, 112, 109, 106, 103, 100,  97,  93,  90,  88,  85,  82,
     79,  76,  73,  70,  67,  65,  62,  59,  57,  54,  52,  49,  47,  44,  42,  40,
     37,  35,  33,  31,  29,  27,  25,  23,  21,  20,  18,  17,  15,  14,  12,  11,
     10,   9,   7,   6,   5,   5,   4,   3,   2,   2,   1,   1,   1,   0,   0,   0
};

//
// Timer0 sample rate based on wave table resolution:
// 8Mhz / 256 =  31.25kHz
// 8MHz / 128 =  62.50kHz
// 8MHz /  64 = 125.00kHz
//

#define TIMER0_SAMPLE_RATE              (CLOCK_FREQUENCY / WAVEFORM_RESOLUTION)
#define DUTY_CYCLE_DIVISOR              (0x100000000 / TIMER0_SAMPLE_RATE)

#define WAVEFORM_RANDOM_STEP_COUNT      8
#define WAVEFORM_STEP_SIZE              (0xff / WAVEFORM_RANDOM_STEP_COUNT)

//
// Local function prototypes.
//

void ResetBaseTempo();
void RecalculateTempo();
void AdjustPhaseAccumulation();

//
// Global variables.
//

volatile uint8_t g_random_number;   // Used with the "random" waveform.

volatile uint16_t g_base_tempo;

volatile uint32_t g_base_duty_cycle;
volatile uint8_t g_base_table_index = 0xff;
volatile uint32_t g_base_phase_accumulator;
volatile uint32_t g_duty_cycle;
volatile uint8_t g_table_index = 0;
volatile uint32_t g_phase_accumulator;

volatile uint8_t g_multiplier_alignment_index;

volatile Waveform g_waveform = WaveformSine;
volatile Multiplier g_multiplier = MultiplierQuarter;
volatile int16_t g_tempo_adjust_offset;

extern volatile uint8_state_flags g_state;
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
    AlignWaveform();
    
    PORTA &= ~(1 << SYNC_OUT); // Pull low.
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
    AlignWaveform();
    
    PORTA |= (1 << SYNC_OUT);   // Pull high.
}

void TempoCountTimeout()
{
    //
    // Exit the tempo counting state without making any changes, discarding any
    // tempo count.
    //
    
    g_state.is_counting_tempo = 0;
    g_tempo_ms_count = 0;
}

void ResetSignals()
{
    ResetBaseTempo();
    
    g_phase_accumulator = 0;
    g_table_index = 0;
    
    g_multiplier_alignment_index = 0;
}

void SeedRandomNumberGenerator(uint32_t seed)
{
    srand(seed);
}

void UpdateRandomNumber()
{
    //
    // Generate a "random" number within the specified range.
    //
    
    g_random_number = (rand() % WAVEFORM_RANDOM_STEP_COUNT) * WAVEFORM_STEP_SIZE;
}

void PlotWaveform()
{
    uint8_t previous_table_index = g_table_index;
    
    //
    // Calculate the next waveform table index. The phase accumulator keeps
    // track of the current offset within a single waveform, and the duty cycle
    // is a fixed "step" that gets added each interrupt. The given frequency
    // will have an impact on the size of the duty cycle, and we'll complete
    // the full waveform slower or faster based on each of these steps.
    //
    
    g_phase_accumulator += g_duty_cycle;
    g_table_index = (g_phase_accumulator & 0xff000000) >> 24;
    
    //
    // Now plot a single point on the selected waveform.
    //
    
    switch (g_waveform)
    {
        case WaveformSine:
        
            //
            // Drawing this one from a table. The given index holds the plot
            // value.
            //
        
            OCR0A = k_sine_table[g_table_index];
            break;
        
        case WaveformRampUp:
        
            //
            //   /|  /|
            //  / | / |
            // /  |/  |
            //
            // Easily calculated; x = i
            //
        
            OCR0A = g_table_index;
            break;
        
        case WaveformRampDown:
        
            //
            // \  |\  |
            //  \ | \ |
            //   \|  \|
            //
            // Easily calculated; x = max - i
            //
        
            OCR0A = 0xff - g_table_index;
            break;
        
        case WaveformTriangle:
        
            //
            // \    /\    /
            //  \  /  \  /
            //   \/    \/
            //
            // Easily calculated; first half: x = 2i, second half: x = max - 2i
            //
        
            if (g_table_index < 0x80)
            {
                OCR0A = g_table_index * 2;
            }
            else
            {
                OCR0A = 0xff - ((g_table_index - 0x80) * 2);
            }
            break;
        
        case WaveformSquare:
        
            //
            // +-----+     |
            // |     |     |
            // |     +-----+
            //
            // Easily calculated; first half: x = min, second half: x = max
            //
        
            if (g_table_index < 0x80)
            {
                OCR0A = 0x00;
            }
            else
            {
                OCR0A = 0xff;
            }
            break;
        
        case WaveformRandom:
        
            //
            // Use whatever is the current random number. Make sure to change
            // this number each complete waveform cycle.
            //
            
            OCR0A = g_random_number;
            break;
        
        default:
            
            break;
    }
    
    //
    // If applicable, toggle the actual tempo indicator. 
    //
    
    if (previous_table_index > g_table_index)
    {
        PORTA ^= (1 << TEMPO_OUT);
        
        //
        // Update the random number for the random waveform.
        //
        
        UpdateRandomNumber();
    }
}

void AlignWaveform()
{
    //
    // Before proceeding, make sure the index isn't out of bounds.
    //
    
    if (g_multiplier_alignment_index >= MULTIPLIER_ALIGNMENT_OFFSET)
    {
        g_multiplier_alignment_index = 0;
    }
    
    //
    // Align the phase accumulator appropriately based on the waveform
    // multiplier. Each multiplier aligns with the base tempo at different
    // intervals.
    //
    
    if ((g_multiplier_alignment_index % k_multiplier_alignment[g_multiplier]) == 0)
    {
        g_phase_accumulator = 0;
    }
    
    g_multiplier_alignment_index++;
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

void SetWaveform(int8_t change_value)
{
    //
    // Change the current waveform to the next in line, either forward or back,
    // making sure to wrap around when necessary.
    //
    
    if ((g_waveform == WaveformSine) && (change_value < 0))
    {
        g_waveform = WaveformRandom;
    }
    else if ((g_waveform == WaveformRandom) && (change_value > 0))
    {
        g_waveform = WaveformSine;
    }
    else
    {
        g_waveform = g_waveform + change_value;
    }
}

void ResetWaveformSetting()
{
    g_waveform = WaveformSine;
}

void SetMultiplier(int8_t change_value)
{
    Multiplier multiplier;
    
    //
    // Change the current multiplier to the next in line, either forward or
    // back, stopping at either end (no wrap-around). This (and the ability to
    // reset by depressing for 2 seconds) should make it relatively easier to
    // set the desired multiplier without having a visual indicator.
    //
    
    if ((g_multiplier == MultiplierWhole) && (change_value < 0))
    {
        multiplier = MultiplierWhole;
    }
    else if ((g_multiplier == MultiplierSixteenth) && (change_value > 0))
    {
        multiplier = MultiplierSixteenth;
    }
    else
    {
        multiplier = g_multiplier + change_value;
    }
    
    //
    // Only change the multiplier if it's different.
    //
    
    if (multiplier != g_multiplier)
    {
        g_multiplier = multiplier;
        
        RecalculateTempo();
        AdjustPhaseAccumulation();
    }
}

void ResetMultiplierSetting()
{
    //
    // If not already set, switch to the 1:1 multiplier.
    //
    
    if (g_multiplier != MultiplierQuarter)
    {
        g_multiplier = MultiplierQuarter;
        
        RecalculateTempo();
        AdjustPhaseAccumulation();
    }
}

/*====== Local functions ====================================================== 
=============================================================================*/

void ResetBaseTempo()
{
    //
    // Reset phase accumulator and wave table index for the base tempo.
    //
    
    g_base_table_index = 0;
    g_base_phase_accumulator = 0;
}

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
    
    //
    // Use the base duty cycle and the current multiplier to calculate the
    // working duty cycle.
    //
    
    g_duty_cycle = g_base_duty_cycle * k_multiplier_ratio[g_multiplier];
}

void AdjustPhaseAccumulation()
{
    //
    // When the tempo multiplier has changed, the working phase accumulator
    // also have to change to reflect how far the new duty cycle would have
    // got had it been running from the start.
    //
    // By doing this the current tempo with multiplier will keep in sync with
    // the base tempo.
    //
    // Note: For multipliers faster than 1:1 tempo the 32-bit working phase
    //       accumulator will overflow and wrap around before the base one has
    //       had a chance to do so. Fortunately, multiplying by the ratio to
    //       get the new phase accumulator also overflows at 32 bits and the
    //       end result is the same (at least it seems to work that way when
    //       testing).
    //
    // Note 2: Because not all multipliers produce waveforms that align on
    //         every base cycle, the multiplier alignemnt index also has to be
    //         taken into account when finding the correct phase accumulator.
    //         See AlignWaveform() for more details on this.
    //
    
    g_phase_accumulator = g_base_phase_accumulator * (k_multiplier_ratio[g_multiplier] * g_multiplier_alignment_index);
}
