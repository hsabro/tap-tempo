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

#ifndef __SWITCHING_H__
#define __SWITCHING_H__

//
// Millisecond count before a mode switch depress is interpreted as a reset.
//

#define MODE_RESET_MIN_TIME         2000

//
// Public function prototypes.
//

void InitializeSwitching();
void DebounceSwitches();
void CalculateSwitchStates();
uint8_t SwitchWasClosed(uint8_t pins);
uint8_t SwitchWasOpened(uint8_t pins);

void SetNextSelectionMode();
void ModifyCurrentSelectionMode(int8_t change_value);
void ResetCurrentSelectionMode();

#endif // __SWITCHING_H__
