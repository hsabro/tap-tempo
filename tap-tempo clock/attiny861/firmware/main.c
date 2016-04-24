//
// Tap-tempo clock for 8-bit AVR.
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

#define TIMER0_PRESCALER                64.0f

//
// Timer0 trigger rate in Hertz.
//

#define TIMER0_FREQUENCY                0.001f

//
// Timer0 trigger rate in milliseconds.
//

#define TIMER0_SAMPLE_RATE              1

//
// In milliseconds (i.e. 1 sec. / 1Hz)
//

#define DEFAULT_TEMPO                   1000

//
// Global variables.
//

volatile state_flags g_state;

volatile uint16_t g_tempo_ms_count;
volatile uint16_t g_speed_adjust_reset_ms_count;

extern volatile uint8_t g_base_table_index;
extern volatile uint32_t g_base_duty_cycle;
extern volatile uint32_t g_base_phase_accumulator;

extern volatile uint16_t g_speed_adjust_ms_count;

extern volatile uint8_t g_average_tempo_count;

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
    
    DDRA = 0x00;
    DDRB = (1 << SYNC_OUT) | (1 << SYNC_2X_OUT) | (1 << TAP_ACTIVE_OUT);
    
    //
    // Enable pull-up resistors on input pins and drive output pins high.
    //
    
    PORTA = 0xff;
    PORTB = 0xff;
    
    //
    // Initialize switching.
    //
    
    InitializeSwitching();
    
    //
    // Initialize signaling, including setting the default base tempo.
    //
    
    SetBaseTempo(DEFAULT_TEMPO);
    
    //
    // Disable USI and ADC to conserve power.
    //
    
    PRR = (1 << PRUSI) | (1 << PRADC);
    
    //
    // Set up Timer0 to trigger an interrupt every 1ms.
    // This timer is used to trigger the tap switch sampling routine.
    //
    
    //
    // The trigger value should come out as
    // ((8,000,000 / 64.0) * 0.001) - 1 = 124
    // which is within 8-bit range (though we could have used a 16-bit compare
    // value for this particular timer).
    // 
    
    OCR0A = ((CLOCK_FREQUENCY / TIMER0_PRESCALER) * TIMER0_FREQUENCY) - 1;
    TCCR0A = (1 << CTC0);                   // Clear Timer on Compare.
    TCCR0B = (1 << CS01) | (1 << CS00);     // Prescalar of 64.
    TCNT0L = 0x00;                          // Starting at 0.
    TIMSK = (1 << OCIE0A);                  // Enable timer 0A compare.
    
    //
    // Set up Timer1 in fast PWM mode with no prescaler and a non-inverted
    // compare.
    // This timer is used to generate the LFO PWM output signal.
    //
    
    TCCR1A = (1 << PWM1B) | (1 << COM1B1);  // Enable PWM-mode on OCR1B. Clear on compare match.
    TCCR1B = (1 << CS10);                   // No prescaler.
    TCCR1D = 0x00;                          // WGM11:10 = 00 enables Fast PWM.
    TIMSK |= (1 << TOIE1);                  // Timer1 overflow interrupt.
    
    //
    // Set up PA1, PA4, PA5, PA6, PA7 and PB6 (PCINT1, PCINT4, PCINT5, PCINT6,
    // PCINT7 and PCINT14 respectively) for pin change interrupts.
    //
    
    PCMSK0 = (1 << PCINT1) | (1 << PCINT4) | (1 << PCINT5) | (1 << PCINT6) | (1 << PCINT7); // PCINT7:0 mask.
    PCMSK1 = (1 << PCINT14);                // PCINT11:8 mask.
    GIMSK = (1 << PCIE0) | (1 << PCIE1);    // Enable pin change interrupts on
                                            // both PCINT7:0 and PCINT11:8.
    
    //
    // Sample the configuration pins to get an initial reading.
    //
    
    g_state.is_clock_input_source = ((PINA & (1 << INPUT_SELECT_IN)) == 0) ? 1 : 0;
    g_state.is_2x_clock_input = ((PINA & (1 << SYNC_IS_2X_IN)) == 0) ? 1 : 0;
    g_state.is_averaging_tempo = ((PINB & (1 << TAP_AVERAGING_IN)) == 0) ? 1 : 0;
    
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
            
            if (g_state.is_clock_input_source == 0)
            {
                ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
                {
                    //
                    // Always reset the output signal on a manual tap.
                    //
                    
                    if (g_state.is_counting_tempo == 0)
                    {
                        ResetBaseTempo();
                        StartTempoCount();
                        
                        //
                        // Turn on the "tap input registration active"-indicator LED.
                        //
                        
                        PORTB &= ~(1 << TAP_ACTIVE_OUT);
                    }
                    else
                    {
                        ResetBaseTempo();
                        StopTempoCount();
                    }
                }
            }
        }
        
        if (SwitchWasClosed(1 << TAP_ALIGN_IN))
        {
            //
		    // We've got a state change on the single sync input switch pin
            // going from open to closed state, i.e. someone just stepped on
            // the single sync switch.
		    //
            
            //
            // Note that the single sync tap is only available when we're not
            // running off an external clock. It doesn't make sense to realign
            // the base tempo only to have it undone on the next clock input.
            //
            
            if (g_state.is_clock_input_source == 0)
            {
                //
                // Reset the output clock PWM signal and end the tempo counting
                // without setting a new base tempo. Pull the normal clock pin
                // high (off), but the 2x pin low (on) since this signal will
                // have completed a full- rather than half a cycle.
                //
                
                ResetBaseTempo();
		        TempoCountTimeout();
                
                PORTB |= (1 << SYNC_OUT);   // Pull high.
                PORTB &= ~(1 << SYNC_2X_OUT); // Pull low.
            }
            else
            {
                //
                // But if we are running off an external clock signal and that
                // signal happens to also be double speed, taping the single
                // sync switch causes the next output clock pulse to extend by
                // 50%. This way you can select which one of the two incoming
                // clock pulses the single output clock pulse should align
                // with.
                //
                
                if (g_state.is_2x_clock_input == 1)
                {
                    if (g_state.is_counting_2x_tempo == 1)
                    {
                        g_state.is_counting_2x_tempo = 0;
                    }
                    else
                    {
                        g_state.is_counting_2x_tempo = 1;
                    }
                }
            }
        }
        
        if (SwitchWasClosed(1 << ADJUST_RESET_IN))
        {
            g_state.is_counting_speed_adjust_reset_time = 1;
        }
        
        if (SwitchWasOpened(1 << ADJUST_RESET_IN))
        {
            //
            // We've got a state change on the mode input switch pin going from
            // open to closed state, i.e. someone just toggled the mode switch.
            //
            
            //
            // If the mode reset counter is running, and it has exceeded the
            // specified number of seconds, interpret the switch release as
            // reset of the current mode rather than a regular mode change.
            //
            
            if (g_state.is_resetting_speed_adjust == 1)
            {
                g_state.is_resetting_speed_adjust = 0;
            }
            else
            {
                g_state.is_counting_speed_adjust_reset_time = 0;
                g_speed_adjust_reset_ms_count = 0;
            }
        }
    }
}

//
// Timer0 compare interrupt handler. Triggers every k_timer0_frequency seconds.
// Frequency: 1kHz
//
// Note: Some code borrowed from http://www.ganssle.com/debouncing-pt2.htm.
//

ISR(TIMER0_COMPA_vect)
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
    
    //
    // Count reset, if applicable.
    //
    
    if (g_state.is_counting_speed_adjust_reset_time == 1)
    {
        g_speed_adjust_reset_ms_count++;
        
        //
        // Reset speed adjust if enough time has passed.
        //
        
        if (g_speed_adjust_reset_ms_count >= SPEED_ADJUST_RESET_MIN_TIME)
        {
            g_state.is_resetting_speed_adjust = 1;
            g_state.is_counting_speed_adjust_reset_time = 0;
            g_speed_adjust_reset_ms_count = 0;
            
            ResetSpeedAdjustSetting();
        }
    }
    
    //
    // Keep the speed adjustment time counter topped up, if possible.
    //
    
    if (g_speed_adjust_ms_count < 0xffff)
    {
        g_speed_adjust_ms_count++;
    }
}

//
// Timer1 overflow interrupt handler. This is where the LFO signal is
// generated.
// Frequency: 31.25kHz
//

ISR(TIMER1_OVF_vect)
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
        }
        
        //
        // Note: Contrary to the regular clock pin, the 2x clock pin need to
        //       continue even when we're currently counting tempo (see
        //       StartTempoCount() for more).
        //
        
        PORTB ^= (1 << SYNC_2X_OUT);
    }
    else if ((previous_base_table_index < 0x80) && (g_base_table_index >= 0x80))
    {
        PORTB ^= (1 << SYNC_2X_OUT);
    }
}

//
// Pin change interrupt handler. Handles the rotary encoder.
//

ISR(PCINT_vect)
{
    static const int8_t encoder_table[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
    static uint8_t encoder_samples = 3;
    static int8_t encoder_value = 0;
    static uint8_t previous_interrupt_a = 0x00;
    static uint8_t previous_interrupt_b = 0x00;
    
    uint8_t interrupt_a = PINA;
    uint8_t interrupt_b = PINB;
    
    //
    // Check for rotary encoder pin change interrupts.
    //
    
    if (((interrupt_a & (1 << ROTARY_A_IN)) != (previous_interrupt_a & (1 << ROTARY_A_IN))) ||
        ((interrupt_a & (1 << ROTARY_B_IN)) != (previous_interrupt_a & (1 << ROTARY_B_IN))))
    {
        //
        // One or more rotary encoder pins have changed since last time. 
        //
        
        //
        // Keep sampling the four latest rotary states, as pairs of bits, in a
        // shift register. Make room for the next sample (and discard the
        // oldest one).
        //
        
        encoder_samples <<= 2;
        
        //
        // Since the current rotary samples need to be added to the shift
        // register at the start, and they do not necessarily align pin-wise,
        // they must be adjusted accordingly.
        //
        
        encoder_samples |= ((interrupt_a & (1 << ROTARY_A_IN)) >> ROTARY_A_IN) | ((interrupt_a & (1 << ROTARY_B_IN)) >> (ROTARY_B_IN - 1));
        
        //
        // Update the encoder value and check the result.
        //
        
        encoder_value += encoder_table[(encoder_samples & 0x0f)];
        if (encoder_value > 3)
        {
            ModifySpeedAdjust(1);
            
            encoder_value = 0;
        }
        else if (encoder_value < -3)
        {
            ModifySpeedAdjust(-1);
            
            encoder_value = 0;
        }
    }
    
    //
    // Check for a change on the input selection pin.
    //

    if ((interrupt_a & (1 << INPUT_SELECT_IN)) != (previous_interrupt_a & (1 << INPUT_SELECT_IN)))
    {
        g_state.is_clock_input_source = ((interrupt_a & (1 << INPUT_SELECT_IN)) == 0) ? 1 : 0;

        //
        // We have a change in the input source selection. If we were in the
        // middle of counting tempo we need to stop doing that. This is
        // specially important if we're transitioning away from using the clock
        // input as source since there's a 50% chance we'll get stuck inside a
        // tempo counting state.
        //

        TempoCountTimeout();

        //
        // We also wipe any previously stored tempos in case of averaging.
        //

        g_average_tempo_count = 0;
    }
    
    //
    // Check for a change on the double speed clock input enable pin.
    //

    if ((interrupt_a & (1 << SYNC_IS_2X_IN)) != (previous_interrupt_a & (1 << SYNC_IS_2X_IN)))
    {
        g_state.is_2x_clock_input = ((interrupt_a & (1 << SYNC_IS_2X_IN)) == 0) ? 1 : 0;
    }
    
    //
    // Check for a change on the clock/sync input pin.
    //
    
    if ((interrupt_a & (1 << SYNC_IN)) != (previous_interrupt_a & (1 << SYNC_IN)))
    {
        //
        // Honor the input selection by only proceeding if clock input is
        // selected the source.
        //
        
        if (g_state.is_clock_input_source == 1)
        {
            //
            // Determine if we're running off a normal or a 2x speed clock
            // source.
            //
            
            if (g_state.is_2x_clock_input == 1)
            {
                //
                // We're running off a 2x speed clock source and need to take
                // this into account when counting tempo and generating our own
                // output clock signal.
                //
                
                if ((interrupt_a & (1 << SYNC_IN)) == 0)
                {
                    //
                    // Rather than counting tempo from start edge to stop edge
                    // (half a clock cycle really) like we normally do, we
                    // count all the way to the next start edge. This means we
                    // have to locally keep track of how to treat each start
                    // edge.
                    //

                    if (g_state.is_counting_2x_tempo == 1)
                    {
                        //
                        // See main loop.
                        //

                        StopTempoCount();

                        g_state.is_counting_2x_tempo = 0;
                    }
                    else
                    {
                        //
                        // See main loop.
                        //

                        StartTempoCount();

                        g_state.is_counting_2x_tempo = 1;
                    }
                }
            }
            else
            {
                //
                // Detect whether this is a falling or rising edge, and start
                // or stop the tempo counting accordingly.
                //
                
                if ((interrupt_a & (1 << SYNC_IN)) != 0)
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
    }
    
    //
    // Check for a change on the tap tempo averaging enable pin.
    //

    if ((interrupt_b & (1 << TAP_AVERAGING_IN)) != (previous_interrupt_b & (1 << TAP_AVERAGING_IN)))
    {
        g_state.is_averaging_tempo = ((interrupt_b & (1 << TAP_AVERAGING_IN)) == 0) ? 1 : 0;

        //
        // Invalidate any accumulated tap tempo readings when averaging is
        // disabled.
        //
        
        if (g_state.is_averaging_tempo == 0)
        {
            g_average_tempo_count = 0;
        }
    }
    
    //
    // Remember the current pin states for the next time around.
    // 
    
    previous_interrupt_a = interrupt_a;
    previous_interrupt_b = interrupt_b;
}
