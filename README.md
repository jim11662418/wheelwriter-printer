# IBM Wheelwriter Printer
This project uses a Dallas Semiconductor [DS89C440](https://www.maximintegrated.com/en/products/microcontrollers/DS89C440.html) Microcontroller (an Intel 8052-compatible MCU) to interface a vintage IBM Wheelwriter electronic typewriter to a host computer's RS232 serial port or LPT parallel port and thus allow the Wheelwriter to act as a [Windows "Generic/Text Only"](https://youtu.be/nlqU7pKytA4) daisy wheel printer. 
<p align="center"><img src="/images/schematic.png"/>
<p align="center">Schematic</p><br>
<p align="center"><img src="/images/Wheelwriter%20Interface.jpg"/>
<p align="center">Wheelwriter Interface Built on a Piece of Perfboard</p><br>
<p align="center"><img src="/images/Serial%20Port%20Connector.jpg"/>
<p align="center">Serial Port Connector</p><br>
<p align="center"><img src="/images/Parallel%20Port%20Connector.jpg"/>
<p align="center">Parallel Port Connector</p><br>

The DS89C440 features two hardware UARTS. The first of the two UARTS is used to connect to the host computer's serial port while the second UART is used to connect to the Wheelwriter's 'BUS' on the J1 'Feature' connector located on the Wheelwriter's Printer Board. Not all Wheelwriters have this J1 connector. The early models (Wheelwriter 3, 5 and 6) do. Some, but not all, of the later models also have the connector. I know, for example, that the Wheelwriter 6 Series II has the connector but the Wheelwriter 10 and 15 do not.
<p align="center"><img src="/images/J1P%20Feature%20Connector.jpg"/>
<p align="center">Interface connection to J1P "Feature" Connector in the Wheelwriter Printer Board</p><br>

The [firmware](main.c) for the Interface was developed using the Keil C51 compiler.
