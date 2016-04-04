#!/bin/bash

#
# Use High Voltage Serial Programming (HVSP) to clear the "reset disabled"
# fuse. This fuse can be set to use the reset pin as an ordinary I/O pin,
# but prevents further programming via ISP (HVSP required).
# This will put it back to a state where ISP is again possible.
#

avrdude -c stk500hvsp -p attiny85 -U hfuse:w:0xdf:m
