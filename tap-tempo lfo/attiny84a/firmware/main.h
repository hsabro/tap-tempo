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

#ifndef __MAIN_H__
#define __MAIN_H__

//
// Fuse bits set to internal 8MHz clock (default), and no clock division (not
// default).
//

#define CLOCK_FREQUENCY     			8000000UL

//
// Pin configuration defines.
//

#define TAP_IN                  		PA0     /* Tap switch input */
#define WAVE_MODE_OUT           		PA1     /* Waveform mode indicator */
#define MULTI_MODE_OUT          		PA2     /* Multiplier mode indicator */
#define MODE_IN                 		PA3     /* Mode switch input */
#define ROTARY_A_IN             		PA4     /* Rotary interrupt input A */
#define ROTARY_B_IN             		PA5     /* Rotary interrupt input B */
#define TEMPO_OUT               		PA6     /* Actual multiplier tempo indicator */
#define SYNC_OUT                		PA7     /* Base tempo indicator / sync output */
#define SPEED_MODE_OUT          		PB0     /* Speed adjust mode indicator */
#define SYNC_IN                 		PB1     /* External sync signal interrupt input */
#define LFO_OUT                 		PB2     /* OC0A PWM timer output */
#define RESET                   		PB3     /* Reset */

//
// Various boolean flags wrapped up in a single byte to save space (not sure if
// actually ends up taking less space when compiled).
//

typedef struct
{
    uint8_t is_counting_tempo:1;
    uint8_t is_counting_mode_reset_time:1;
    uint8_t is_resetting_mode:1;
    uint8_t has_random_seed:1;
    uint8_t has_received_tap_input:1;
    uint8_t reserved:3;
} uint8_state_flags;

#endif // __MAIN_H__
