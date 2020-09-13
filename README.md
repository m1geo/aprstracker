# aprstracker
An update on aprstracker-0.12 project by PE1RXQ and PE1ICQ.

On 15 September 2013, I (@m1geo) spoke with Arno (PE1ICQ) and received the code provided in the initial check in (aprstracker-0.12). Tracker version 0.11 didn't support 9600 baud GPS receivers (despite the v0.11 docs daying it does). Version 0.12 does support 9600 baud NEMA GPS receivers, but contains bugs with the altitude calculation.

You can get Hi-Tech C Compiler from ftp://ftp.microchip.com/HI-TECH%20C%20for%20PIC10-12-16%20(Standard%20v7.76-9.60)/. 
Builds with Microchip XC8 (set global options to C 90 (from default C 99).

Originally built with Hi-Tech C Compiler V8.02PL1.

Build notes:
  Builds with Hi-Tech C V9.65 (from Microchip FTP Archive)
    "C:\Program Files (x86)\HI-TECH Software\PICC\PRO\9.65\bin\picc.exe" aprstracker.c -Os -16F648A

  Builds with XC8 2.30 (see __XC defines for differences)
    "XC8 Global Options", and change "C Standard" from "C 99" to "C 90"

Changes:
  Added __XC checks for XC vs HTCC compiler