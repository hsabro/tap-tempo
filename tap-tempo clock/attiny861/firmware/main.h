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
#define SYNC_IN          		        PA1     /* External sync signal interrupt input */
#define TAP_ALIGN_IN                    PA2     /* Tap alignment switch input */
#define ADJUST_RESET_IN                 PA3     /* Speed adjust reset switch input */
#define INPUT_SELECT_IN                 PA4     /* Input source selection (tap/clock) */
#define SYNC_IS_2X_IN                   PA5     /* External sync signal is 2x speed */
#define ROTARY_A_IN             		PA6     /* Rotary interrupt input A */
#define ROTARY_B_IN             		PA7     /* Rotary interrupt input B */
#define SYNC_OUT                		PB0     /* Tempo indicator / sync output */
#define SYNC_2X_OUT                     PB1     /* Sync 2x output */
#define TAP_ACTIVE_OUT                  PB2     /* LED indicator when actively counting tempo */
#define UNUSED1                         PB3
#define CRYSTAL_IN1                     PB4     /* Crystal leg #1 */
#define CRYSTAL_IN2                     PB5     /* Crystal leg #2 */
#define TAP_AVERAGING_IN                PB6     /* Accumulate tap inputs, and average */
#define RESET                   		PB7     /* Reset */

//
// Various boolean flags wrapped up in a single byte to save space (not sure if
// actually ends up taking less space when compiled).
//

typedef struct
{
    uint8_t is_counting_tempo:1;
    uint8_t is_counting_speed_adjust_reset_time:1;
    uint8_t is_resetting_speed_adjust:1;
    uint8_t is_clock_input_source:1;
    uint8_t is_2x_clock_input:1;
    uint8_t is_averaging_tempo:1;
    uint8_t is_counting_2x_tempo:1;
    uint8_t reserved:1;
} state_flags;

#endif // __MAIN_H__
