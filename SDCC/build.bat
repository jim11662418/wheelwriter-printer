@ECHO OFF

REM compile...
sdcc -c main.c
sdcc -c keyboard.c
sdcc -c uart12.c
sdcc -c watchdog.c
sdcc -c wheelwriter.c

REM link...
sdcc main.c keyboard.rel uart12.rel watchdog.rel wheelwriter.rel

REM make Intel HEX file...
packihx main.ihx > printer.hex

REM optional cleanup...
del *.asm
del *.ihx
del *.lk
del *.lst
del *.map
del *.mem
del *.rel
del *.rst
del *.sym