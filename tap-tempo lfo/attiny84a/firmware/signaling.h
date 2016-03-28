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

#ifndef __SIGNALING_H__
#define __SIGNALING_H__

//
// Defines and structs.
//

//
// In milliseconds = 0.1Hz, 10 seconds
//

#define LFO_MIN_TEMPO           		10000

//
// In milliseconds = 20Hz, 5/100ths of a second
//

#define LFO_MAX_TEMPO           		50

//
// Public function prototypes.
//

void SetBaseTempo(uint16_t milliseconds);
void StartTempoCount();
void StopTempoCount();
void TempoCountTimeout();
void ResetSignals();

void SeedRandomNumberGenerator(uint32_t seed);
void UpdateRandomNumber();
void PlotWaveform();
void AlignWaveform();
void AdjustSpeed(int16_t change_value);
void ResetSpeedAdjustSetting();
void SetWaveform(int8_t change_value);
void ResetWaveformSetting();
void SetMultiplier(int8_t change_value);
void ResetMultiplierSetting();

#endif // __SIGNALING_H__
