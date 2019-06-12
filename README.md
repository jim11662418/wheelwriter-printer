# wheelwriter-printer
A Dallas Semiconductor DS89C440 MCU turns an IBM Wheelwriter Electronic typewriter into a Windows "Generic/Text Only" Printer. 
The Wheelwriter prints characters received from the PC's parallel port or serial port, or entered through a PS/2 keyboard.

This project only works on earlier Wheelwriter models, the ones that internally have two circuit boards: the Function Board and the Printer Board (Wheelwriter models 3, 5 and 6).

The MCU connects to the J1P "Feature" connector on the Wheelwriter's Printer Board. See the schematic for details.
