#!/bin/bash

echo "$1" > /dev/sram_control
sleep 0.2  # Wait for Arduino to respond
cat /dev/sram_control
