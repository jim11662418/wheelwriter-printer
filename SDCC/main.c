//---------------------------------------------------------------------------------------------------------------------------------
// Print on the Wheelwriter characters received from the serial port and, optionally, from the parallel port
// or PS/2 keyboard. Compile with the Small Device C Compiler (SDCC).
//----------------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------------
// Copyright 2020-2024 Jim Loos
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated
// documentation files (the "Software"), to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, 
// and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all copies or substantial portions
// of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED
// TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
// CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
// DEALINGS IN THE SOFTWARE.
//----------------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------------
// For use as the 'console', configure Teraterm (or other terminal emulator) for 
// 9600bps, N-8-1, RTS/CTS flow control.
//----------------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------------
// Control for the earliest Wheelwriter models (3, 5 and 6) is performed by two circuit boards: 
// the Function Board and the Printer Board. Each of these boards has an Intel MCS-51 type microcontroller
// (8031 on the Function Board and 8051 on the Printer Board). The Function Board scans the keyboard and
// sends commands over a 187500 bps serial communications link to the Printer Board (which controls the 
// printing mechanism) to print the characters associated with each key press. The key to making a Wheelwriter
// act as a printer is to connect to this serial link and send commands to the Printer Board like it would
// normally receive coming from the Function Board.
//----------------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------------
// Use the following procedure to load object code into the DS89C440 flash memory:
//  1. Configure Teraterm for 4800 bps, N-8-1, no flow control.
//  2. Close the jumper to enable the DS89C440 bootloader.
//  3. Press Enter. The board should respond with "DS89C440 LOADER VERSION 2.1...".
//  4. At the bootloader prompt, type 'K' to clear flash memory followed by 'LB' to load the object code.
//  5. Use the Teraterm 'Send file' function to send the hex object file.
//  6. The DS89C440 Loader will respond with 'G' for each record received and programmed without error.
//  7. Re-configure Teraterm for 9600 bps, N-8-1 and RTS/CTS flow control.
//  8. Remove the jumper to disable the bootloader and restart the application.
//----------------------------------------------------------------------------------------------------------
//
//----------------------------------------------------------------------------------------------------------
// Version 1.0 - Initial Arduino version
// Version 1.1 - LPT port added to Arduino version
// Version 2.0 - Initial DS89C440 Keil C51 "bit bang" version
// Version 3.0 - DS89C440 serial mode 2 version
// Version 3.1 - Limited Diablo 630 emulation added
// Version 3.2 - Detect printwheel and Wheelwriter model on start-up
// Version 3.3 - PS/2 keyboard added
// Version 3.4 - Various optimizations and clean-ups
// Version 3.5 - Reworked and simplified power-on initialization routines
// Version 3.6 - Revised wheelwriter.c for ASCII printwheel
// Version 3.7 - SDCC version. Echo keys typed on Wheelwriter to console. Show
//							 variables. Console fixed at 9600bps. Simplified initialization code.
//----------------------------------------------------------------------------------------------------------
//----------------------------------------------------------------------------------------------------------
// switch 1    off - linefeed only upon receipt of linefeed character (0x0A)
//             on  - auto linefeed; linefeed is performed with each carriage return (0x0D)
// switch 2    not used
// switch 3    not used
// switch 4    not used
//----------------------------------------------------------------------------------------------------------

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include "reg420.h"
#include "uart12.h"
#include "watchdog.h"
#include "control.h"
#include "keyboard.h"
#include "keycodes.h"
#include "wheelwriter.h"
#include "compiler.h"

#define CR    0x0D
#define LF    0x0A
#define BS    0x08
#define TAB   0x09
#define SPACE 0x20
#define DEL   0x7F

#define TRUE  1
#define FALSE 0
#define HIGH 1
#define LOW 0
#define ON 0                            // 0 turns the LEDs on
#define OFF 1                           // 1 turns the LEDs off

#define NOP() __asm NOP __endasm

// used to display byte as binary
#define PATTERN " %c%c%c%c%c%c%c%c\n"
#define TO_BINARY(byte)  \
  (byte & 0x80 ? '1' : '0'), \
  (byte & 0x40 ? '1' : '0'), \
  (byte & 0x20 ? '1' : '0'), \
  (byte & 0x10 ? '1' : '0'), \
  (byte & 0x08 ? '1' : '0'), \
  (byte & 0x04 ? '1' : '0'), \
  (byte & 0x02 ? '1' : '0'), \
  (byte & 0x01 ? '1' : '0')
  
// 12,000,000 Hz/12 = 1,000,000 Hz = 1.0 microsecond clock period
// 50 milliseconds per interval/1.0 microseconds per clock = 50,000 clocks per interval
#define RELOADHI (65536-50000)/256
#define RELOADLO (65536-50000)&255
#define ONESEC 20                         // 20*50 milliseconds = 1 second

__sbit __at (0x80) switch1;               // dip switch connected to pin 39 0=on, 1=off (auto LF after CR if on) 
__sbit __at (0x81) switch2;               // dip switch connected to pin 38 0=on, 1=off (not used)
__sbit __at (0x82) switch3;               // dip switch connected to pin 37 0=on, 1=off (not used)
__sbit __at (0x83) switch4;               // dip switch connected to pin 36 0=on, 1=off (not used)

__sbit __at (0x84) redLED;                // red   LED connected to pin 35 0=on, 1=off
__sbit __at (0x85) amberLED;              // amber LED connected to pin 34 0=on, 1=off
__sbit __at (0x86) greenLED;              // green LED connected to pin 33 0=on, 1=off

__sbit __at (0x90) ackPin;                // Acknowledge output for LPT port on pin 1
__sbit __at (0x91) busyPin;               // Busy output for LPT port on pin 2

__bit errorLED = FALSE;                 // flag that makes the red LED flash when TRUE
__bit initializing = TRUE;              // flag that makes all three LEDs flash during initialization

unsigned char attribute = 0;            // bit 0=bold, bit 1=continuous underline, bit 2=multiple word underline
unsigned char column = 1;               // current print column (1=left margin)
unsigned char tabStop = 5;              // horizontal tabs every 5 spaces (every 1/2 inch)
volatile unsigned char timeout = 0;     // decremented every 50 milliseconds, used for detecting timeouts
volatile unsigned char hours = 0;       // uptime hours
volatile unsigned char minutes = 0;     // uptime minutes
volatile unsigned char seconds = 0;     // uptime seconds

extern unsigned char uSpacesPerChar;    // defined in wheelwriter.c
extern unsigned char uLinesPerLine;     // defined in wheelwriter.c
extern unsigned int  uSpaceCount;       // defined in wheelwriter.c

// uninitialized variables in xdata RAM, contents unaffected by reset
__xdata volatile unsigned char __at(0x03F0) wdResets;    // count of watchdog resets
__xdata volatile unsigned char __at(0x03F1) printWheel;  // 0x08: PS; 0x10: 15P; 0x20: 12P; 0x40: 10P; 0x21: none

__code char banner[]   = "\n\nWheelwriter Printer Version 3.7\n"
                         "for Maxim DS89C440 MCU and SDCC\n"
                         "Compiled on " __DATE__ " at " __TIME__"\n"
                         "Copyright 2019-2024 Jim Loos\n";

__code char help1[]   = "\n\nControl characters:\n"
                        "  BEL 0x07        spins the printwheel\n"
                        "  BS  0x08        non-destructive backspace\n"
                        "  TAB 0x09        horizontal tab\n"
                        "  LF  0x0A        paper up one line\n"
                        "  VT  0x0B        paper up one line\n"
                        "  CR  0x0D        returns carriage to left margin\n"
                        "  ESC 0x1B        see Diablo 630 commands below...\n"
                        "\nDiablo 630 commands emulated:\n"
                        "  <ESC><O>        selects bold printing\n"
                        "  <ESC><&>        cancels bold printing\n"
                        "  <ESC><E>        selects continuous underlining\n"
                        "  <ESC><R>        cancels underlining\n"
                        "  <ESC><X>        cancels both bold and underlining\n"
                        "  <ESC><U>        half line feed\n"
                        "  <ESC><D>        reverse half line feed\n"
                        "  <ESC><BS>       backspace 1/120 inch\n"
                        "  <ESC><LF>       reverse line feed\n"
                        "<Space> for more, <ESC> to exit...";
__code char help2[]   = "\n\nPrinter control not part of the Diablo 630 emulation:\n"
                        "  <ESC><u>        selects micro paper up\n"
                        "  <ESC><d>        selects micro paper down\n"
                        "  <ESC><b>        selects broken underlining\n"
                        "  <ESC><l><n>     auto linefeed on or off\n"
                        "  <ESC><p>        selects Pica pitch (10 cpi)\n"
                        "  <ESC><e>        selects Elite pitch (12 cpi)\n"
                        "  <ESC><m>        selects Micro Elite pitch (15 cpi)\n"
                        "\nDiagnostics/debugging:\n"
                        "  <ESC><^Z><a>    show version information\n"
                        "  <ESC><^Z><e><n> flashing red LED on or off\n"
                        "  <ESC><^Z><p><n> show the value of Port n (0-3)\n"
                        "  <ESC><^Z><r>    reset the MCU\n"
                        "  <ESC><^Z><u>    show the uptime\n"
                        "  <ESC><^Z><v>    show variables\n";

//------------------------------------------------------------
// Timer 0 ISR: interrupt every 50 milliseconds, 20 times per second
//------------------------------------------------------------
void timer0_isr(void) __interrupt(1) __using(1) {
    static unsigned char ticks = 0;

    TL0 = RELOADLO;              // load timer 0 low byte
    TH0 = RELOADHI;                 // load timer 0 high byte

    if (timeout)                    // countdown value for detecting timeouts
        --timeout;

    if (initializing) {             // flash all three LEDs while initializing
       amberLED = greenLED = redLED = (ticks < 10);
    }

    if(++ticks == 20) {             // if 20 ticks (one second) have elapsed...
        ticks = 0;

        if (errorLED)               // if there's an error
           redLED = !redLED;        // toggle the red LED once each second

        if (++seconds == 60) {      // if 60 seconds (one minute) has elapsed...
            seconds = 0;
            if (++minutes == 60) {  // if 60 minutes (one hour) has elapsed...
                minutes = 0;
                ++hours;
            }
        }
    }
}

//------------------------------------------------------------------------------------------
// Parallel LPT port interrupt when STROBE (INT0, pin 12) goes low. The transfer starts when
// the printer indicates that it is ready to receive data by making the Busy line low. The host
// then places data on the BUS and waits 500nS (minimum) before pulsing nStrobe low for 500nS
// (minimum). The host leaves valid data on the BUS for another 500nS (minimum) after nStrobe
// returns high. Once the printer receives the data it takes the Busy line high to indicate that
// the data is being processed. When the printer has finished processing the data it pulls the
// nAck line low for a minimum of 500nS, and then returns the Busy line low.
//------------------------------------------------------------------------------------------
void ex0_isr(void) __interrupt(0) __using(2) {
   IE0 = 0;                         // clear EX0 interrupt flag
    busyPin = HIGH;                 // when the host pulls strobe low, set busy high
}

// table used by parseWWdata function below for converting printwheel characters to ASCII
const char __code printwheel2ASCII[96] = {
// a    n    r    m    c    s    d    h    l    f    k    ,    V    _    G    U
  0x61,0x6E,0x72,0x6D,0x63,0x73,0x64,0x68,0x6C,0x66,0x6B,0x2C,0x56,0x2D,0x47,0x55,
// F    B    Z    H    P    )    R    L    S    N    C    T    D    E    I    A
  0x46,0x42,0x5A,0x48,0x50,0x29,0x52,0x4C,0x53,0x4E,0x43,0x54,0x44,0x45,0x49,0x41,
// J    O    (    M    .    Y    ,    /    W    9    K    3    X    1    2    0
  0x4A,0x4F,0x28,0x4D,0x2E,0x59,0x2C,0x2F,0x57,0x39,0x4B,0x33,0x58,0x31,0x32,0x30,
// 5    4    6    8    7    *    $    #    %    ¢    +    ±    @    Q    &    ]
  0x35,0x34,0x36,0x38,0x37,0x2A,0x24,0x23,0x25,0xA2,0x2B,0xB1,0x40,0x51,0x26,0x5D,
// [    ³    ²    º    §    ¶    ½    ¼    !    ?    "    '    =    :    -    ;
  0x5B,0xB3,0xB2,0xBA,0xA7,0xB6,0xBD,0xBC,0x21,0x3F,0x22,0x60,0x3D,0x3A,0x5F,0x3B,
// x    q    v    z    w    j    .    y    b    g    u    p    i    t    o    e
  0x78,0x71,0x76,0x7A,0x77,0x6A,0x2E,0x79,0x62,0x67,0x75,0x70,0x69,0x74,0x6F,0x65};

//------------------------------------------------------------------------------------------
// parses the data stream consisting of the 9 bit words sent by the Function Board
// on the Wheelwriter BUS to the Printer Board. transmits the decoded ASCII character
// out through Serial0.
//------------------------------------------------------------------------------------------
void parseWWdata(unsigned int WWdata) {
    static char state = 0;

    switch (state) {
        case 0:                                   // waiting for first data word from Wheelwriter...
            if (WWdata == 0x121)                  // all commands must start with 0x121...
               state = 1;
            break;
        case 1:                                   // 0x121 has been received...
            switch (WWdata) {
                case 0x003:                       // 0x121,0x003 is start of alpha-numeric character sequence
                    state = 2;
                    break;
                case 0x004:                       // 0x121,0x004 is start of erase sequence
                    putchar(SPACE);               // overwrite the character with space
                    putchar(BS);
                    state = 0;
                    break;
                case 0x005:                       // 0x121,0x005 is start of vertical movement sequence
                    state = 6;
                    break;
                case 0x006:                       // 0x121,0x006 is start of horizontal movement sequence
                    state = 3;
                    break;
                default:
                    state = 0;
            } // switch (WWdata)
            break;
        case 2:                                   // 0x121,0x003 has been received...
            if (WWdata)                           // 0x121,0x003,printwheel code
               putchar(printwheel2ASCII[(WWdata-1)]);
            else
               putchar(SP);                       // 0x121,0x003,0x000 is the sequence for SPACE
            state = 0;
            break;
        case 3:                                   // 0x121,0x006 has been received...
            if (WWdata & 0x080)                   // if bit 7 is set...
               state = 4;                         // 0x121,0x006,0x080 is horizontal movement to the right...
            else                                  // else...
               state = 5;                         // 0x121,0x006,0x000 is horizontal movement to the left...
            break;
        case 4:                                    // 0x121,0x006,0x080 has been received, move carrier to the right...
            state = 0;
            if (WWdata>uSpacesPerChar)            // if more than one space, must be tab
                putchar(TAB);
            else
                putchar(SPACE);
            break;
        case 5:                                   // 0x121,0x006,0x000 has been received, move carrier to the left...
            if (WWdata == uSpacesPerChar)
               putchar(BS);
            state = 0;
            break;
        case 6:                                   // 0x121,0x005 has been received...
            if ((WWdata&0x1F) == uLinesPerLine)
               putchar(CR);                       // 0x121,0x005,0x090 is the sequence for paper up one line (for 10P, 12P and PS printwheels)
            state = 0;
    }   // switch (state)
}

//------------------------------------------------------------------------------------------
// The Wheelwriter prints the character and updates the variable 'column'.
// Carriage return cancels bold and underlining.
// Linefeeds automatically printed with carriage return if switch 1 is on.
// The character printed by the Wheelwriter is echoed to the serial port (for monitoring).
//
// Control characters:
//   BEL 0x07    spins the printwheel
//   BS  0x08    non-destructive backspace
//   TAB 0x09    horizontal tab to next tab stop
//   LF  0x0A    moves paper up one line
//   VT  0x0B    moves paper up one line
//   CR  0x0D    returns carriage to left margin, if switch 1 is on, moves paper up one line (linefeed)
//   ESC 0x1B    see Diablo 630 commands below...
//
// Diablo 630 commands emulated:
//   <ESC><O>  selects bold printing for one line
//   <ESC><&>  cancels bold printing
//   <ESC><E>  selects continuous underlining  (spaces between words are underlined) for one line
//   <ESC><R>  cancels underlining
//   <ESC><X>  cancels both bold and underlining
//   <ESC><U>  half line feed (paper up 1/2 line)
//   <ESC><D>  reverse half line feed (paper down 1/2 line)
//   <ESC><BS> backspace 1/120 inch
//   <ESC><LF> reverse line feed (paper down one line)
//
// printer control not part of the Diablo 630 emulation:
//   <ESC><u>  selects micro paper up (1/8 line or 1/48")
//   <ESC><d>  selects micro paper down (1/8 line or 1/48")
//   <ESC><b>  selects broken underlining (spaces between words are not underlined)
//   <ESC><p>  selects Pica pitch (10 characters/inch or 12 point)
//   <ESC><e>  selects Elite pitch (12 characters/inch or 10 point)
//   <ESC><m>  selects Micro Elite pitch (15 characters/inch or 8 point)
//
// diagnostics/debugging:
//   <ESC><^Z><c> print (on the serial console) the current column
//   <ESC><^Z><r> reset the DS89C440 microcontroller
//   <ESC><^Z><u> print (on the serial console) the uptime as HH:MM:SS
//   <ESC><^Z><v> print (on the serial console) variables
//   <ESC><^Z><e><n> turn flashing red error LED on or off (n=1 is on, n=0 is off)
//   <ESC><^Z><p><n> print (on the serial console) the value of Port n (0-3) as 2 digit hex number
//-------------------------------------------------------------------------------------------
void print_character(unsigned char charToPrint) {
    static char escape = 0;                                 // escape sequence state
    char c,i,t;

    switch (escape) {
        case 0:
            switch (charToPrint) {
                case NUL:
                    break;
                case BEL:
                    ww_spin();
                    putchar(BEL);
                    break;
                case BS:
                    if (column > 1){                        // only if there's at least one character on the line
                        ww_backspace();
                        --column;                           // update column
                        putchar(BS);
                    }
                    break;
                case HT:
                    t = tabStop-(column%tabStop);           // how many spaces to the next tab stop
                    ww_horizontal_tab(t);                   // move carrier to the next tab stop
                    for(i=0; i<t; i++){
                        ++column;                           // update column
                        putchar(SP);
                    }
                    break;
                case LF:
                    ww_linefeed();
                    putchar(LF);
                    break;
                case VT:
                    ww_linefeed();
                    break;
                case CR:
                    ww_carriage_return();                   // return the carrier to the left margin
                    column = 1;                             // back to the left margin
                    attribute = 0;                          // cancel bold and underlining
                    if (!switch1)                           // if switch 2 is on, automatically print linefeed
                        ww_linefeed();
                    putchar(CR);
                    break;
                case ESC:
                    escape = 1;
                    break;
                default:
                    if ((charToPrint > 0x1F) && (charToPrint < 0x80)) { // 'printable' characters 0x20-0x7F
                        ww_print_letter(charToPrint,attribute);
                        putchar(charToPrint);               // echo the character to the console
                        ++column;                           // update column
                    }
            } // switch (charToPrint)
            break;  // case 0:

        case 1:                                             // this is the second character of the escape sequence...
            switch (charToPrint) {
                case 'O':                                   // <ESC><O> selects bold printing
                    attribute |= 0x01;
                    escape = 0;
                    break;
                case '&':                                   // <ESC><&> cancels bold printing
                    attribute &= 0x06;
                    escape = 0;
                    break;
                case 'E':                                   // <ESC><E> selects continuous underline (spaces between words are underlined)
                    attribute |= 0x02;
                    escape = 0;
                    break;
                case 'R':                                   // <ESC><R> cancels underlining
                    attribute &= 0x01;
                    escape = 0;
                    break;
                case 'X':                                   // <ESC><X> cancels both bold and underlining
                    attribute = 0;
                    escape = 0;
                    break;
                case 'U':                                   // <ESC><U> selects half line feed (paper up one half line)
                    ww_paper_up();
                    escape = 0;
                    break;
                case 'D':                                   // <ESC><D> selects reverse half line feed (paper down one half line)
                    ww_paper_down();
                    escape = 0;
                    break;
                case LF:                                    // <ESC><LF> selects reverse line feed (paper down one line)
                    ww_reverse_linefeed();
                    escape = 0;
                    break;
                case BS:                                    // <ESC><BS> backspace 1/120 inch
                    ww_micro_backspace();
                    escape = 0;
                    break;
                case 'b':                                   // <ESC><b> selects broken underline (spaces between words are not underlined)
                    attribute |= 0x04;
                    escape = 0;
                    break;
                case 'e':                                   // <ESC><e> selects Elite (12 characters/inch)
                    uSpacesPerChar = 10;
                    uLinesPerLine = 16;
                    tabStop =6;                             // tab stops every 6 characters (every 1/2 inch)
                    escape = 0;
                    break;
                case 'p':                                   // <ESC><p> selects Pica (10 characters/inch)
                    uSpacesPerChar = 12;
                    uLinesPerLine = 16;
                    tabStop =5;                             // tab stops every 5 characters (every 1/2 inch)
                    escape = 0;
                    break;
                case 'm':                                   // <ESC><m> selects Micro Elite (15 characters/inch)
                    uSpacesPerChar = 8;
                    uLinesPerLine = 12;
                    tabStop =7;                             // tab stops every 7 characters (every 1/2 inch)
                    escape = 0;
                    break;
                case 'u':                                   // <ESC><u> paper micro up (paper up 1/8 line)
                    ww_micro_up();
                    escape = 0;
                    break;
                case 'd':                                   // <ESC><d> paper micro down (paper down 1/8 line)
                    ww_micro_down();
                    escape = 0;
                    break;
                case '\x1A':                                // <ESC><^Z> for remote diagnostics
                    escape = 2;
                    break;
                case 'H':
                case 'h':
                    printf(help1);                          // print the first half of the help
                    escape = 6;                             // wait for a key to be pressed...
            } // switch (charToPrint)
            break;  // case 1:

        case 2:                                             // this is the third character of the escape sequence...
            switch (charToPrint) {
                case 'A':
                case 'a':
                    printf("\n%s\n",banner);
                    break;
                case 'E':    
                case 'e':                                   // <ESC><^Z><e> toggle red error LED
                    escape = 4;
                    break;
                case 'P':
                case 'p':                                   // <ESC><^Z><p> print port values
                    escape = 3;
                    break;
                case 'R':
                case 'r':                                   // <ESC><^Z><r> system reset
                    TA = 0xAA;                                   // timed access
                    TA = 0x55;
                    FCNTL = 0x0F;                           // use the FCNTL register to preform a system reset
                    break;
                case 'U':
                case 'u':                                   // <ESC><^Z><u> print uptime
                    printf("%s %02u%c%02u%c%02u\n","Uptime:",(int)hours,':',(int)minutes,':',(int)seconds);
                    escape = 0;
                    break;
                case 'V':
                case 'v':                                   // <ESC><^Z><v> print variables
                    printf("\n");
                    printf("%s %s\n",    "switch1:        ",switch1 ? "off":"on");
                    printf("%s %s\n",    "initializing:   ",initializing ? "true":"false");
                    printf("%s" PATTERN, "attribute:      ",TO_BINARY(attribute));
                    printf("%s %d\n",    "column:         ",(int)column);
                    printf("%s %d\n",    "tabStop:        ",(int)tabStop);
                    printf("%s 0x%02X\n","printWheel:     ",(int)printWheel);
                    printf("%s %d\n",    "uSpacesPerChar: ",(int)uSpacesPerChar);
                    printf("%s %d\n",    "uLinesPerLine:  ",(int)uLinesPerLine);
                    printf("%s %d\n",    "uSpaceCount:    ",(int)uSpaceCount);
                    printf("%s %d\n",    "wdResets:       ",(int)wdResets);
                    for(c=1; c<column; c++) putchar(SP);    // return cursor to previous position on line
                    escape = 0;
                    break;
            } // switch (charToPrint)
            break;  // case 2:

        case 3:                                             // this is the fourth character of the escape sequence
            switch (charToPrint){
                case '0':
                    printf("%s 0x%02X\n","P0:",(int)P0);    // <ESC><^Z><p><0> print port 0 value
                    escape = 0;
                    break;
                case '1':
                    printf("%s 0x%02X\n","P1:",(int)P1);    // <ESC><^Z><p><1> print port 1 value
                    escape = 0;
                    break;
                case '2':
                    printf("%s 0x%02X\n","P2:",(int)P2);    // <ESC><^Z><p><2> print port 2 value
                    escape = 0;
                    break;
                case '3':
                    printf("%s 0x%02X\n","P3:",(int)P3);    // <ESC><^Z><p><3> print port 3 value
                    escape = 0;
                    break;
            } // switch (charToPrint)
            break;  // case 3:

        case 4:
            if (charToPrint & 0x01) {
                errorLED = TRUE;                            // <ESC><^Z><e><n> odd values turn of n the LED on, even values turn the LED off
            }
            else {
                errorLED = FALSE;
                redLED = OFF;                               // turn off the red LED
            }
            escape = 0;
            break;  // case 4

        case 6:
            if (charToPrint == 0x20) {                      // if it's SPACE...
                printf(help2);                              // print the second half of the help
                escape = 0;
            }
            else if (charToPrint == ESC) {                  // if it's ESCAPE, exit
               putchar(0x0D);
               escape = 0;
            }
            break;

    } // switch (escape)
}

//-----------------------------------------------------------
// handle input from the ps/2 keyboard
//
// control B - toggles bold printing
// control U - toggles continuous underlining
// control I - toggles multiple word (broken) underlining
//
// up arrow  - moves paper up
// dn arrow  - moves paper down
// lt arrow  - moves carrier left (backspaces)
// rt arrow  - moves carrier right (spaces)
//
// control up arrow  - micro paper up
// control dn arrow  - micro paper down
//
// the Delete key acts as the Correction key (erases the last character)
//
// special characters on the Wheelwriter keyboard but not on the ps/2 keyboard:
//  ¢, §, ², ³, ¶, º, ¼, ½, ±
//
// characters on the ps/2 keyboard but not on the printwheel:
//  { (left curly bracket), } (right curly bracket), ` (back tick), ~ (tilde)
//
// functions yet to be implemented: automatic centering, tab setting and clearing, margin
// release, margin setting and clearing, variable line spacing
//-----------------------------------------------------------
void handle_key(unsigned char key) {
    static unsigned char __xdata keybuffer[64];                 // 64 character buffer used for ps/2 keyboard input
    static unsigned char keybufptr = 0;                         // pointer into keybuffer
    unsigned char i,t;

    if (kb_ctrl_pressed()) {                                    // is the control key pressed?
        switch (key) {
            case 'b':
            case 'B':                                           // control B or b toggles bold printing
                ww_spin();                                      // spin the printwheel
                attribute ^= 0x01;                              // toggle bold print flag
                break;
            case 'i':
            case 'I':                                           // control I or i toggles multiple word underlining
                ww_spin();                                      // spin the printwheel
                attribute ^= 0x04;
                break;
            case 'u':
            case 'U':                                           // control U or u toggles continuous underlining
                ww_spin();                                      // spin the printwheel
                attribute ^= 0x02;
                break;
            case 'z':
            case 'Z':                                           // control Z or z
                print_character(0x1A);                          // ^Z
                break;
            case PS2_KEY_KP_UP_ARROW:
            case PS2_KEY_UP_ARROW:
                ww_micro_up();                                  // control Up Arrow = micro up
                break;
            case PS2_KEY_KP_DN_ARROW:                           // control Dn Arrow = micro down
            case PS2_KEY_DN_ARROW:
                ww_micro_down();
                break;
        } // switch (key)
    } // (kb_ctrl_pressed())

    else if (kb_alt_pressed()){                                 // alt key pressed?
        // nothing here yet
    }

    else switch (key) {                                         // check for "grey" (special) keys
        case PS2_KEY_KP_DELETE:                                 // delete erases last character
        case PS2_KEY_DELETE:
            if (keybufptr){                                     // if there's at least one character on this line
                ww_erase_letter(keybuffer[--keybufptr]);        // erase it
                --column;                                       // update column
                putchar(BS);                                    // erase the last character on the Teraterm screen
                putchar(SP);
                putchar(BS);
            }
            break;
        case PS2_KEY_TAB:
            t = tabStop-(column%tabStop);                       // how many spaces to the next tab stop
            ww_horizontal_tab(t);                               // move carrier to the next tab stop
            for(i=0; i<t; i++){
                ++column;                                       // update column
                putchar(SP);                                    // update serial console screen
                keybuffer[keybufptr++ &0x3F] = SP;              // put spaces in the key buffer
            }
            break;
        case PS2_KEY_BACKSPACE:
            if (keybufptr) {
                print_character(BS);                            // backspace
                --keybufptr;
            }
            break;
        case PS2_KEY_KP_ENTER:
        case PS2_KEY_ENTER:
            print_character(CR);                                // carriage return
            print_character(LF);                                // line feed
            keybufptr = 0;                                      // reset the pointer to the beginning
            break;
        case PS2_KEY_KP_LT_ARROW:
        case PS2_KEY_LT_ARROW:
            if (keybufptr) {
                print_character(BS);                            // backspace to move one position to the left
                --keybufptr;
            }
            break;
        case PS2_KEY_RT_ARROW:
        case PS2_KEY_KP_RT_ARROW:
            print_character(SP);                                // space to move one position to the right
            keybuffer[keybufptr++ &0x3F] = SP;
            break;
        case PS2_KEY_KP_UP_ARROW:                               // paper up
        case PS2_KEY_UP_ARROW:
            print_character(LF);                                // linefeed moves paper up
            keybufptr = 0;
            break;
        case PS2_KEY_KP_DN_ARROW:                               // paper down
        case PS2_KEY_DN_ARROW:
            print_character(ESC);
            print_character(LF);                                // <ESC><LF> = reverse linefeed moves paper down
            keybufptr = 0;
            break;
        case PS2_KEY_ESCAPE:
            print_character(ESC);
            break;
        case PS2_KEY_KP_DIV:
            keybuffer[keybufptr++ &0x3F] = '/';
            print_character('/');
            break;
        case PS2_KEY_KP_MULT:
            keybuffer[keybufptr++ &0x3F] = '*';
            print_character('*');
            break;
        case PS2_KEY_KP_MINUS:
            keybuffer[keybufptr++ &0x3F] = '-';
            print_character('-');
            break;
        case PS2_KEY_KP_PLUS:
            keybuffer[keybufptr++ &0x3F] = '+';
            print_character('+');
            break;
        default:
            if (key > 0x1F && key < 0x7F) {
                keybuffer[keybufptr++ &0x3F] = key;             // save the key in case it needs to be erased later
                print_character(key);                           // print the ASCII character
            }
    }  // else switch (key)
}

//-----------------------------------------------------------
// main(void)
//-----------------------------------------------------------
void main(void){
   unsigned int loopcounter = 0;
   unsigned int scancode, WWdata;
   unsigned char state = 0;
   unsigned char lastsec = 0;

   wd_disable_watchdog();                                   // disable wwtchdog timer reset

   PMR |= 0x01;                                             // enable internal SRAM MOVX memory

   busyPin = LOW;                                           // set LPT Busy low: ready to receive
   ackPin = HIGH;                                           // set LPT Acknowledge high
   P2 = 0xFF;                                               // set all pins of port 2 high to act as input

   IT0 = 1;                                                 // configure interrupt 0 for falling edge on INT0 (P3.2)
   EX0 = 1;                                                 // enable EX0 Interrupt
   TL0 = RELOADLO;                                          // load timer 0 low byte
   TH0 = RELOADHI;                                          // load timer 0 high byte
   TMOD = (TMOD & 0xF0) | 0x01;                             // configure timer 0 for mode 1 - 16 bit timer
   ET0 = 1;                                                 // enable timer 0 interrupt
   TR0 = 1;                                                 // run timer 0

   kb_init();                                               // initialize ps/2 keyboard
   uart_init();                                             // initialize serial 0 for N-8-1 at 9600bps, RTS-CTS handshaking
   ww_init();                                               // initialize serial 1 for the Wheelwriter

   EA = TRUE;                                               // global interrupt enable

   printf("\n%s\n",banner);

   switch (WDCON & 0x44) {
      case 0x00:
         printf("External reset\n\n");
         break;
      case 0x04:
         printf("%s %u\n\n","Watchdog resets:",(int)++wdResets);
         break;
      case 0x40:
         printf("Power on reset\n\n");
         printf("Initializing");
         lastsec = seconds;
		   timeout = ONESEC*6;										  // 6 seconds for wheelwriter to initialize
         wdResets = 0;
         printWheel = 0;
         while (!printWheel) {                             // waiting for printwheel code...
            if (lastsec != seconds) {                      // once each second...
               lastsec = seconds;
               putchar('.');                               // dots on screen to monitor progress   
            }   
            if (ww_data_avail()){
               WWdata = ww_get_data();
               //printf("%03X\n",WWdata);
               switch (state) {
                  case 0:
                     if (WWdata == 0x121)
                        state = 1;
                     break;
                  case 1:
                     if (WWdata == 0x001)                  // function board asks for printwheel from printer board
                        state = 2;
                     else
                        state = 0;
                     break;
                  case 2:
                     printWheel = WWdata;
                     switch (printWheel) {
                        case 0x008:
                           uSpacesPerChar = 10;
                           uLinesPerLine = 16;
                           tabStop = 6;                    // tab stops every 6 characters (every 1/2 inch)
                           printf("\nPS printwheel\n");
                           break;
                        case 0x010:
                           uSpacesPerChar = 8;
                           uLinesPerLine = 12;
                           tabStop = 7;                    // tab stops every 7 characters (every 1/2 inch)
                           printf("\n15P printwheel\n");
                           break;
                        case 0x020:
                           uSpacesPerChar = 10;
                           uLinesPerLine = 16;
                           tabStop = 6;                    // tab stops every 6 characters (every 1/2 inch)
                           printf("\n12P printwheel\n");
                           break;
                        case 0x021:
                           uSpacesPerChar = 10;
                           uLinesPerLine = 16;
                           tabStop = 6;                    // tab stops every 6 characters (every 1/2 inch)
                           printf("\nNo printwheel\n");
                           break;
                        case 0x040:
                           uSpacesPerChar = 12;
                           uLinesPerLine = 16;
                           tabStop = 5;                    // tab stops every 5 characters (every 1/2 inch)
                           printf("\n10P printwheel\n");
                           break;
                        default:
                           uSpacesPerChar = 10;
                           uLinesPerLine = 16;
                           tabStop = 6;                    // tab stops every 6 characters (every 1/2 inch)
                           printf("\nUnable to determine printwheel. Assuming 12P.\n");
                     } //switch (printWheel)
               } // switch (state)
            } // if (ww_data_avail())
         } // while (!printWheel)
				 if (!timeout) {
				    errorLED = TRUE;
						printf("\nWheelwriter timed out\n");
				 }
   } // switch (WDCON & 0x44)
	 
   while (ww_data_avail()) {                           		// absorb any remaining data from the Wheelwriter...
      ww_get_data();
   }	 

   while(kb_scancode_avail()) {                            // absorb any data from the ps/2 keyboard?
      //printf("0x%02X\n",(unsigned int)(kb_get_scancode() & 0xFF));
      kb_get_scancode();
   }

	 scancode = 0;
   if (kb_send_cmd(0xFF)) {                                // send reset command to keyboard
		  timeout = ONESEC;																		 // keyboard should respond within 1 second
		  while (scancode != 0xAA) {
				 scancode = kb_get_scancode();
         //printf("0x%02X\n",(unsigned int)(scancode & 0xFF));
				 if (!timeout) {																	 // no acknowledge from keyboard
					  errorLED = TRUE;
						printf("PS/2 keyboard timed out\n");
				 }
			}
      printf("PS/2 keyboard detected\n");
   }

   wd_clr_flags();                                         // clear watchdog reset and POR flags for next start up
   wd_init_watchdog(3);                                    // WD interval = (1/12MHz)*2^26 = 5592.4 milliseconds

   initializing = FALSE;
   amberLED = OFF;                                         // turn off the amber LED
   greenLED = OFF;                                         // turn off the green LED
   redLED = OFF;                                           // turn off the red LED

   printf("ESC H for help\n");
   printf("Ready\n");

   //----------------- loop here forever -----------------------------------------
   while(TRUE) {

      wd_reset_watchdog();                                // "pet" the watchdog every time through the loop

      if (++loopcounter==0)                               // every 65536 times through the loop (at about 2Hz)
         greenLED = !greenLED;                            // toggle the green LED

      if (uart_char_avail())                          	 // if there is a character in the serial 0 receive buffer...
         print_character(uart_getchar());          		 // retrieve it and make the Wheelwriter print it

      if (busyPin) {                                  	 // if there is a character from the parallel port...
         print_character(P2);                      		 // print the character from the parallel port (port 2)
         ackPin = LOW;                             		 // set Acknowledge pin low
         NOP();                                  			 // 3 microseconds delay...
         NOP();
         NOP();
         ackPin = HIGH;                            		 // set Acknowledge pin high
         busyPin = LOW;                            		 // set Busy pin low, ready for next character
      }

      if (kb_scancode_avail())                        	 // if there is a scancode from the ps/2 keyboard...
         handle_key(kb_decode_scancode(kb_get_scancode()));// decode the scancode from the keyboard

      if (ww_data_avail())                            	 // if there's data from the Wheelwriter...
         parseWWdata(ww_get_data());               		 // echo Wheelwriter keys to the console serial port
   }
}

/*------------------------------------------------------------------------------
Note that the two function below, getchar and putchar, replace the library
functions of the same name.  These functions use the interrupt-driven serial 0
I/O routines in uart12.c
------------------------------------------------------------------------------*/
// for scanf
char _getkey(void) {
    return uart_getchar();
}

// for printf
int putchar(int c)  {
   return uart_putchar(c);
}