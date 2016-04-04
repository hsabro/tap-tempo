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

//
// 
// Note: Debounce code based on article at:
//       http://www.ganssle.com/debouncing-pt2.htm
//
//       PWM DDS implementation inspired by:
//       http://interface.khm.de/index.php/lab/experiments/arduino-dds-sinewave-generator/
//
//       Rotary encoder decoding based on artivle at:
//       https://www.circuitsathome.com/mcu/rotary-encoder-interrupt-service-routine-for-avr-micros
//
// Note 2: The "divide clock by 8" fuse bit needs to be cleared for this code
//         to run at the proper speed.
//

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#include "switching.h"
#include "signaling.h"
#include "main.h"

//
// Defines and structs.
//

#define TIMER1_PRESCALER                32.0f

//
// Timer1 trigger rate in Hertz.
//

#define TIMER1_FREQUENCY                0.001f

//
// Timer1 trigger rate in milliseconds.
//

#define TIMER1_SAMPLE_RATE              1

//
// In milliseconds (i.e. 1 sec. / 1Hz)
//

#define DEFAULT_TEMPO                   1000

//
// Global variables.
//

volatile uint8_state_flags g_state;

volatile uint16_t g_tempo_ms_count;

extern volatile uint8_t g_base_table_index;
extern volatile uint32_t g_base_duty_cycle;
extern volatile uint32_t g_base_phase_accumulator;

/*====== Public functions ===================================================== 
=============================================================================*/

int main()
{
    //
    // Entry point and main loop.
    //
    
    //
    // Specify which pins are to be output pins (and consequently which ones
    // are to be input pins).
    //
    
    DDRB = (1 << SYNC_OUT) | (1 << LFO_OUT);
    
    //
    // Enable pull-up resistors on input pins and drive output pins high.
    //
    
    PORTB = 0xff;
    
    //
    // Initialize switching.
    //
    
    InitializeSwitching();
    
    //
    // Set up the random number generator for the random waveform.
    //
    
    SeedRandomNumberGenerator(0);
    UpdateRandomNumber();
    
    //
    // Initialize signaling, including setting the default base tempo.
    //
    
    SetBaseTempo(DEFAULT_TEMPO);
    
    //
    // Disable USI to conserve power.
    //
    
    PRR = (1 << PRUSI);
    
    //
    // Set up Timer0 in fast PWM mode with no prescaler and a non-inverted
    // compare.
    // This timer is used to generate the LFO PWM output signal.
    //
    
    TCCR0A = (1 << COM0A1);                 // Clear OC0A on compare match. Set OC0A at BOTTOM.
    TCCR0A |= (1 << WGM01) | (1 << WGM00);  // Fast-PWM (TOP == 0xff).
    TCCR0B = (1 << CS00);                   // No prescaler.
    TIMSK |= (1 << TOIE0);                   // Timer0 overflow interrupt.
    
    //
    // Set up Timer1 to trigger an interrupt every 1ms.
    // This timer is used to trigger the tap switch sampling routine.
    //
    // Note: In compare mode the compares are done against the OCR1C register.
    //
    
    //
    // The trigger value should come out as
    // ((8,000,000 / 32.0) * 0.001) - 1 = 249
    // which is within 8-bit range (though we could have used a 16-bit compare
    // value for this particular timer).
    // 
    
    OCR1C = ((CLOCK_FREQUENCY / TIMER1_PRESCALER) * TIMER1_FREQUENCY) - 1;
    TCCR1 = (1 << CTC1);                    // CTC mode (TOP == OCR1C).
    TCCR1 |= (1 << CS11) | (1 << CS12);     // Prescalar of 32.
    TCNT1 = 0x00;                           // Starting at 0.
    TIMSK |= (1 << OCIE1A);                 // Enable timer 1A compare.
    
    //
    // Set up ADC input pins for waveform- and multiplier selection. VCC as
    // reference voltage, and left-aligned to get an 8-bit resolution. We're
    // using a prescaler of 128 for an interrupt frequency of 62.5kHz, and free
    // running mode to get continuous samplings. We're going to use PB3 and PB4
    // as inputs, meaning we need to switch between MUX[1:0] = 11 (PB3) and
    // MUX[1:0] = 10 (PB4).
    //
    
    ADMUX = (1 << ADLAR) | (1 << MUX1) | (1 << MUX0);                   // Left-adjust, input channel = PB3.
    ADCSRA = (1 << ADATE) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // Auto-trigger, division = 64.
    ADCSRB = 0x00;                                                      // Free-running mode.
    
    ADCSRA |= (1 << ADEN) | (1 << ADIE);                                // ADC enabled, ADC interrupt enabled.
    ADCSRA |= (1 << ADSC);                                              // ADC start conversion.
    
#if ENABLE_EXT_CLK
    //
    // Set up PB5 (PCINT5)for pin change interrupts.
    //
    
    PCMSK = (1 << PCINT5);                  // Select PCINT5 (PB5) a pin change interrupt source.
    GIMSK |= (1 << PCIE);                   // Enable pin change interrupts.
#endif
    
    //
    // Enable global interrupts. No interrupts will happen without this,
    // regardless of individual flags set elsewhere.
    //
    
    sei();
    
    //
    // Main loop.
    //
    
    while (1)
    {
        //
        // Continuously poll the tap input switch.
        //
        // Note: This routine has been moved to the main loop so as not to
        //       delay the interrupt processing unnecessarily. There is enough
        //       calculation going on with the debouncing and checking that,
        //       when run inside it's ISR, the ISR that is responsible for
        //       generating the PWM/LFO signal is prevented from running at the
        //       required speed and thus starts skipping.
        //       Since we're running in main() the ISRs will take priority over
        //       the switch calculations.
        // 
        // Note 2: The code that actually updates tempo counting state etc. is
        //         wrapped in an atomic block to prevent the ISRs to modify
        //         state midways; i.e. basic multi-thread synchronization.
        //
        
        CalculateSwitchStates();
        
        if (SwitchWasClosed(1 << TAP_IN))
        {
            //
            // We've got a state change on the tap input switch pin going from
            // open to closed state, i.e. someone just stepped on the tap
            // switch.
            //  
            
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
            {
                //
                // Always reset the output signal on a manual tap.
                //
                
                if (g_state.is_counting_tempo == 0)
                {
                    ResetSignals();
                    StartTempoCount();
                }
                else
                {
                    ResetSignals();
                    StopTempoCount();
                    
                    g_state.has_received_tap_input = 1;
                }
            }
            
            //
            // Just once, use the newly entered tap tempo value to seed
            // the random number generator, so it will be different
            // each time.
            //
            
            if (g_state.has_random_seed == 0)
            {
                if (g_state.has_received_tap_input == 0)
                {
                    g_state.has_random_seed = 1;
                    
                    SeedRandomNumberGenerator(g_tempo_ms_count);
                    UpdateRandomNumber();
                }
            }
        }
    }
}

//
// Timer0 overflow interrupt handler. This is where the LFO signal is
// generated.
// Frequency: 31.25kHz
//

ISR(TIM0_OVF_vect)
{
    uint8_t previous_base_table_index = g_base_table_index;
    
    //
    // Increase the phase accumulator by a given amount based on the required
    // output signal frequency. Then use the high 8 bits (0-255) of the phase
    // accumulator to identify what part of the wave to plot.
    //
    // We need both the base tempo (for the LED and sync output signal), and
    // the actual LFO signal with applied tempo multiplier.
    //
    // Also see PlotWaveform().
    //
    
    //
    //
    // TODO: Might be a shortcut just dividing by the given multiplier rather
    //       than keeping a second set of variables.
    //
    //
    
    g_base_phase_accumulator += g_base_duty_cycle;
    g_base_table_index = (g_base_phase_accumulator & 0xff000000) >> 24;
    
    //
    // Flag whenever there's an overflow in the base table index, i.e. the base
    // tempo, has just completed a full cycle.
    //
    
    if (previous_base_table_index > g_base_table_index)
    {
        //
        // As long as we're not currently in tempo counting mode (where the LED
        // will be explicitly set) toggle the LED state.
        //
        // The state of the LED output also doubles as a base frequency clock
        // output pulse (low one complete base LFO cycle, then high one
        // complete base LFO cycle) that can be used to synchronize a second
        // (or several other) controller(s).
        //
        
        if (g_state.is_counting_tempo == 0)
        {
            PORTB ^= (1 << SYNC_OUT);   // Pull high->low or low->high.
            //AlignWaveform();
        }
    }
    
    //
    // Draw the next point on the waveform.
    //
    
    PlotWaveform();
}

//
// Timer1 compare interrupt handler. Triggers every k_timer1_frequency seconds.
// Frequency: 1kHz
//
// Note: Some code borrowed from http://www.ganssle.com/debouncing-pt2.htm.
//

ISR(TIM1_COMPA_vect)
{
    //
    // Run each switch input through a debounce routine to make sure we get rid
    // of any noise due to the switch contacts bouncing. This routine
    // DebounceSwitches all 8 input pins on PA simultaneously.
    //
    
    DebounceSwitches();
    
    //
    // Count tempo, if applicable.
    //
    
    if (g_state.is_counting_tempo == 1)
    {
        g_tempo_ms_count++; // Add a millisecond.
        
        //
        // Make sure we don't exceed the maximum tempo length / minimum LFO
        // frequency.
        //
        
        if (g_tempo_ms_count > LFO_MIN_TEMPO)
        {
            TempoCountTimeout();
        }
    }
}

ISR(ADC_vect)
{
    static uint8_t first_sampling = 0;
    uint8_t sample_value;

    sample_value = ADCH;
    
    //
    // Because we're switching between two ADC channels in free running mode
    // the first sampling is unreliable (it may belong to either channel
    // depending on exactly when the previous channel switch was done) and we
    // discard it in favor of the next one.
    //
    
    //
    // We're running at 8MHz clock frequency with a prescaler of 128, i.e.
    // 8MHz / 128 = 62.5kHz. But we also take turns sampling two different
    // channels, and furthermore we discard the first
    // sampling each time, meaning the actual sample frequency is
    // 62.5kHz / 4 = 15.625kHz. Still overkill considering we're only reading a
    // static voltage...
    //
    
    if (first_sampling == 1)
    {
        //
        // Discard sampling.
        //
        
        first_sampling = 0;
    }
    else
    {
        if (ADMUX & 0x01)   // MUX[0:0] == 1.
        {
            //
            // Sample was read from PB3; the waveform pin.
            //
            
            SetWaveform(sample_value);
        }
        else    // MUX[0:0] == 0.
        {
            //
            // Sample was read from PB4; the multiplier pin.
            //
            
            SetMultiplier(sample_value);
        }
        
        //
        // Switch the ADC input channel.
        //
        
        ADMUX ^= (1 << MUX0);   // Toggle between MUX[1:0] = 11 and MUX[1:0] = 10.
        first_sampling = 1;
    }
}

#if ENABLE_EXT_CLK
//
// Pin change interrupt handler. Reads input clock/sync pulses.
//

ISR(PCINT0_vect)
{
    static uint8_t previous_sync_input = 0;
    uint8_t sync_input = (PINB & (1 << SYNC_IN));

    //
    // Check for a change on the clock/sync input pin.
    //

    if (sync_input != previous_sync_input)
    {
        previous_sync_input = sync_input;

        //
        // Detect whether this is a falling or rising edge, and start or stop
        // the tempo counting accordingly.
        //
        
        if (sync_input)
        {
            //
            // See main loop.
            //

            StopTempoCount();
        }
        else
        {
            //
            // See main loop.
            //

            StartTempoCount();
        }
    }
}
#endif
