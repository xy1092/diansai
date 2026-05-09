#!/usr/bin/env bash
set -euo pipefail

echo '[tty]'
ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || echo 'No ttyUSB/ttyACM devices found'
echo

echo '[usb]'
lsusb 2>/dev/null | grep -Ei 'texas instruments|xds110|xds|msp|ti ' || echo 'No common TI debug probes found'
echo

echo '[groups]'
id
