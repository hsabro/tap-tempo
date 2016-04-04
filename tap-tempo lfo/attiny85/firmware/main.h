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

#ifndef __MAIN_H__
#define __MAIN_H__

//
// Fuse bits set to internal 8MHz clock (default), and no clock division (not
// default).
//

#define CLOCK_FREQUENCY     			8000000UL

//
// Pin configuration defines for attiny85.
//

#define LFO_OUT                 		PB0     /* OC0A PWM timer output */
#define SYNC_OUT                		PB1     /* Base tempo indicator / sync output */
#define TAP_IN                  		PB2     /* Tap switch input */
#define WAVEFORM_IN             		PB3     /* Waveform selection ADC input */
#define MULTIPLIER_IN                   PB4     /* Multiplier selection ADC input */

#if ENABLE_EXT_CLK
#define SYNC_IN                 		PB5     /* External sync signal interrupt input */
#else
#define RESET                   		PB5     /* Reset */
#endif

//
// Various boolean flags wrapped up in a single byte to save space (not sure if
// actually ends up taking less space when compiled).
//

typedef struct
{
    uint8_t is_counting_tempo:1;
    uint8_t has_random_seed:1;
    uint8_t has_received_tap_input:1;
    uint8_t reserved:5;
} uint8_state_flags;

#endif // __MAIN_H__
