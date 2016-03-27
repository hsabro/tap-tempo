# tap-tempo
Version 3 of the tap-tempo LFO for the AVR attiny series of microcontrollers, now free under the GPLv3 license.

This is a small tap-tempo Low Frequency Oscillator (LFO) micro controller intended primarily for use in guitar effects and similar. You get a number of different waveforms and tempo multiplier settings, and the tempo can be set either manually by switch or automatically via a synchronization signal.

Visit http://www.sabrotone.com for more info.

For those who have followed this project up until now; The previous tap-tempo LFO controller has been expanded with a few additional features, which required a larger package and more pins, hence the move from attiny85 to attiny84a. The plan is to refactor the attiny85 code and allow both options (albeit with a reduced feature set). If I get around to it I'll also move over the tap-tempo clock.

Note that there are still a few kinks that needs to be ironed out. Hopefully soon.

Harald Sabro
