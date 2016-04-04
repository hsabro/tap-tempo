# tap-tempo
Version 3 of the tap-tempo LFO for the AVR attiny series of microcontrollers, now free under the GPLv3 license.

This is a small tap-tempo Low Frequency Oscillator (LFO) micro controller intended primarily for use in guitar effects and similar.

[Visit my website for earlier versions, additioanl info, veroboard projects etc.](http://www.sabrotone.com)

Harald Sabro
---

## Features
 | attiny84a | attiny85
--- | --- | ---
**Programming** | ISP | ISP or HVSP
**Tap-tempo** | Yes | Yes
**Minimum tap tempo** | 10s | 10s
**Maximum tap tempo** | 50ms | 50ms
**Base clock output** | Yes | Yes
**Actual tempo output** | Yes | No
**External clock sync** | Yes | Yes (requires HVSP)
**Single tap tempo align** | Yes | Yes
**Select waveform** | Yes | Yes
**Select multiplier** | Yes | Yes (1)
**Adjust base tempo** | Yes | No
**Reset settings** | Yes | No
**Required settings controls** | 1 | 2
**Settings control(s)** | Digital encoder | Analog potentiometers

*(1) "Triplet" multiplier is not available for the attiny85; not because it can't handle it, but because by excluding this there are an equal number of slower- and faster multipliers available meaning the analog potentiometer will (theoretically) line up with 1:1/base tempo at 12 o'clock. Would go well with a center detent pot.*
