# IBM Wheelwriter Printer
This project uses a Dallas Semiconductor [DS89C440](https://www.maximintegrated.com/en/products/microcontrollers/DS89C440.html) MCU (an Intel 8052-compatible microcontroller)  to interface a vintage IBM Wheelwriter electronic typewriter to a host computer's RS232 serial port or LPT parallel port and thus allow the Wheelwriter to act as a [Windows "Generic/Text Only"](https://youtu.be/nlqU7pKytA4) daisy wheel printer. 

The DS89C440 features two hardware UARTS. The first of the two UARTS is used to connect to the host computer's serial port while the second UART is used to connect to the Wheelwriter's 'BUS' on the J1 'Feature' connector located on the printer board. Not all Wheelwriters have this J1 connector. The early models (Wheelwriter 3, 5 and 6) do. Some, but not all, of the later models also have the connector. I know, for example, that the Wheelwriter 6 Series II has the connector but the Wheelwriter 10 and 15 do not.

The [firmware](main.c) for the MCU was developed using the Keil C51 compiler.
