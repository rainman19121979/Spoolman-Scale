I changed the orginal code slightly.
Now I have the opportunity to weigh a spool after printing and the used filament is updated in the database. (for printers without Klipper e.g. Bambulab or similar)
The selection of which filament is on the scale currently works via a rotary encoder and is shown on the display (ID and name).
After pressing the rotary encoder, the database is updated with the current weight.

Hardware:

ESP32

HX711 Load cell (2 kg)

0.96 inch OLED module

KY-040 Rotary Encoder Module

KDC11 ON/OFF Rocker Switch

CJMCU Micro USB Breakout Board



Printed Files:

https://www.printables.com/de/model/466905-filament-scale-remix

Help needed:
The wrong weight is currently being sent to Spoolman. The respective empty coil weight would still have to be deducted, unfortunately I'm a programming noob :) and I somehow can't manage it.
