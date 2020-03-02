//------------------------------------------------------------------------------------------
// Print characters received from the parallel port, serial port or PS/2 keyboard on the Wheelwriter.
//
// Control for the earliest Wheelwriter models (3, 5 and 6) is performed by two circuit boards: 
// the Function Board and the Printer Board. Each of these boards has an Intel MCS-51 type microcontroller
// (8031 on the Function Board and 8051 on the Printer Board). The Function Board scans the keyboard and
// sends commands over a serial communications link to the Printer Board (which controls the printing mechanism)
// to print the characters associated with each key press. The key to making a Wheelwriter act as a printer is to
// connect to this serial link and send commands to the Printer Board like it would normally receive coming from
// the Function Board.
//
// Flashing instructions:
//      Configure TeraTerm for 4800 bps, N-8-1. Install the jumper on the SBC to enable bootloader mode. Press Enter
//      to start the the DS89C440 bootloader.  At the bootloader prompt, enter "K" to erase the flash memory followed
//      by "LB" to load the object code. Use the TeraTerm "Send File" function to send the Intel hex object code to the
//      SBC. After the object code has finished loading, change the TeraTerm serial port baud rate to match the application.
//      Remove the jumper to disable the bootloader and start the application.
//
// For the Keil C51 compiler.
//
// Version 1.0 - Initial Arduino version
// Version 1.1 - LPT port added to Adruino version
// Version 2.0 - Initial DS89C440 Keil C51 "bit bang" version
// Version 3.0 - DS89C440 serial mode 2 version
// Version 3.1 - Limited Diablo 630 emulation added
// Version 3.2 - Detect printwheel and Wheelwriter model on start-up
// Version 3.3 - PS/2 keyboard added
// Version 3.4 - Various optimizations and clean-ups
//
// switch 1    off - linefeed only upon receipt of linefeed character (0x0A)
//             on  - auto linefeed; linefeed is performed with each carriage return (0x0D)
// switch 2    not used
//
// switch 3    not used
//
// switch 4    not used
//
//------------------------------------------------------------------------------------------

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <reg420.h>
#include <intrins.h>
#include "uart12.h"
#include "watchdog.h"
#include "control.h"
#include "keyboard.h"
#include "keycodes.h"
#include "wheelwriter.h"

#define BAUDRATE 9600                   // serial console at 9600 bps

#define FALSE 0
#define TRUE  1
#define LOW 0
#define HIGH 1
#define ON 0                            // 0 turns the LEDs on
#define OFF 1                           // 1 turns the LEDs off

// 12,000,000 Hz/12 = 1,000,000 Hz = 1.0 microsecond clock period
// 50 milliseconds per interval/1.0 microseconds per clock = 50,000 clocks per interval
#define RELOADHI (65536-50000)/256
#define RELOADLO (65536-50000)&255
#define ONESEC 20                       // 20*50 milliseconds = 1 second

sbit switch1 =  P0^0;                   // dip switch connected to pin 39 0=on, 1=off (auto LF after CR if on)  
sbit switch2 =  P0^1;                   // dip switch connected to pin 38 0=on, 1=off (not used)
sbit switch3 =  P0^2;                   // dip switch connected to pin 37 0=on, 1=off (not used)
sbit switch4 =  P0^3;                   // dip switch connected to pin 36 0=on, 1=off (not used)  

sbit redLED =   P0^4;                   // red LED connected to pin 35 0=on, 1=off
sbit amberLED = P0^5;                   // amber LED connected to pin 34 0=on, 1=off
sbit greenLED = P0^6;                   // green LED connected to pin 33 0=on, 1=off

sbit ackPin =   P1^0;                   // Acknowledge for LPT port pin 10
sbit busyPin =  P1^1;                   // Busy for LPT port pin 11

bit errorLED = FALSE;                   // makes the red LED flash when TRUE
bit centered = FALSE;                   // automatic centering between left and right margins
bit initializing = TRUE;                // makes all three LEDs flash during initialization

unsigned char attribute = 0;            // bit 0=bold, bit 1=continuous underline, bit 2=multiple word underline
unsigned char column = 1;               // current print column (1=left margin)
unsigned char tabStop = 5;              // horizontal tabs every 5 spaces (every 1/2 inch)
unsigned char wheelwriterModel = 0;     // Wheelwriter 3 or 6
unsigned char printWheel = 0;
volatile unsigned char timeout = 0;     // decremented every 50 milliseconds, used for detecting timeouts
volatile unsigned char hours = 0;       // uptime hours
volatile unsigned char minutes = 0;     // uptime minutes
volatile unsigned char seconds = 0;     // uptime seconds

extern unsigned char uSpacesPerChar;    // defined in wheelwriter.c
extern unsigned char uLinesPerLine;
extern unsigned int  uSpaceCount;

// uninitialized variables in xdata RAM, contents unaffected by reset
volatile unsigned char xdata wdResets _at_ 0x3F0;// count of watchdog resets
volatile unsigned char xdata pwrFail  _at_ 0x3F1;// flag set by power-fail interrupt ISR

code char banner[]     = "Wheelwriter Printer Version 3.4.3\n"
                         "for Maxim DS89C440 MCU\n"
                         "Compiled on " __DATE__ " at " __TIME__"\n"
                         "Copyright 2019-2020 Jim Loos\n";

code char help1[]     = "\n\nControl characters:\n"
                        "BEL 0x07        spins the printwheel\n"
                        "BS  0x08        non-destructive backspace\n"
                        "TAB 0x09        horizontal tab\n"
                        "LF  0x0A        paper up one line\n"
                        "VT  0x0B        paper up one line\n"
                        "CR  0x0D        returns carriage to left margin\n"
                        "ESC 0x1B        see Diablo 630 commands below...\n"
                        "\nDiablo 630 commands emulated:\n"
                        "<ESC><O>        selects bold printing\n"
                        "<ESC><&>        cancels bold printing\n"
                        "<ESC><E>        selects continuous underlining\n"
                        "<ESC><R>        cancels underlining\n"
                        "<ESC><X>        cancels both bold and underlining\n"
                        "<ESC><U>        half line feed\n"
                        "<ESC><D>        reverse half line feed\n"
                        "<ESC><BS>       backspace 1/120 inch\n"
                        "<ESC><LF>       reverse line feed\n"
                        "<Space> for more, <ESC> to exit...";
code char help2[]     = "\n\nPrinter control not part of the Diablo 630 emulation:\n"
                        "<ESC><u>        selects micro paper up\n"
                        "<ESC><d>        selects micro paper down\n"
                        "<ESC><b>        selects broken underlining\n"
                        "<ESC><l><n>     auto linefeed on or off\n"
                        "<ESC><p>        selects Pica pitch\n"
                        "<ESC><e>        selects Elite pitch\n"
                        "<ESC><m>        selects Micro Elite pitch\n"
                        "\nDiagnostics/debugging:\n"
                        "<ESC><^Z><a>    show version information\n"
                        "<ESC><^Z><c>    show the current column\n"
                        "<ESC><^Z><e><n> turn flashing red error LED on or off\n"
                        "<ESC><^Z><m>    monitor communications between boards\n"
                        "<ESC><^Z><p><n> show the value of Port n (0-3)\n"
                        "<ESC><^Z><r>    reset the Wheelwriter\n"
                        "<ESC><^Z><s>    show the micro-space count\n"
                        "<ESC><^Z><u>    show the uptime\n"
                        "<ESC><^Z><w>    show the number of watchdog resets\n\n";

//------------------------------------------------------------
// Timer 0 ISR: interrupt every 50 milliseconds, 20 times per second
//------------------------------------------------------------
void timer0_isr(void) interrupt 1 using 1{
    static char ticks = 0;

    TL0 = RELOADLO;     			// load timer 0 low byte
    TH0 = RELOADHI;     			// load timer 0 high byte

    if (timeout)                    // countdown value for detecting timeouts
        --timeout;
    
    if (initializing) {             // flash all three LEDs while initializing
       amberLED = greenLED = redLED = (ticks < 10);
    }

    if(++ticks == 20) { 		    // if 20 ticks (one second) have elapsed...
        ticks = 0;

        if (errorLED)               // if there's an error 
           redLED = !redLED;        // toggle the red LED once each second

        if (++seconds == 60) {		// if 60 seconds (one minute) has elapsed...
            seconds = 0;
            if (++minutes == 60) {	// if 60 minutes (one hour) has elapsed...
                minutes = 0;
                ++hours;
            }
        }
    }
}

//------------------------------------------------------------
// Power-Fail ISR: interrupt when Vcc falls below about 4.2 volts
//------------------------------------------------------------
void pwr_fail_isr(void) interrupt 6 using 1{
    PFI = FALSE;                    // clear power-fail interrupt flag
    redLED = ON;                    // turn on the red error LED
    while (PFI) {                   // loop here while voltage is still too low...
        PFI = FALSE;                // clear power-fail interrupt flag
    }
    redLED = OFF;                   // turn off the red error LED
    pwrFail = 0xAA;                 // set the flag for the main loop to see
    wd_reset_watchdog();            // reset the watchdog before exiting the isr
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
void ex0_isr(void) interrupt 0 using 2 {
  	IE0 = 0;					    // clear EX0 interrupt flag
    busyPin = HIGH;                 // when strobe goes low, indicate busy
}

//------------------------------------------------------------------------------------------
// The Wheelwriter prints the character and updates column.
// Carriage return cancels bold and underlining.
// Linefeeds automatically printed with carriage return if switch 1 is on.
// The character printed by the Wheelwriter is echoed to the serial port (for monitoring).
//
// Control characters:
//  BEL 0x07    spins the printwheel
//  BS  0x08    non-destructive backspace
//  TAB 0x09    horizontal tab to next tab stop
//  LF  0x0A    moves paper up one line
//  VT  0x0B    moves paper up one line
//  CR  0x0D    returns carriage to left margin, if switch 1 is on, moves paper up one line (linefeed)
//  ESC 0x1B    see Diablo 630 commands below...
//
// Diablo 630 commands emulated:
// <ESC><O>  selects bold printing for one line
// <ESC><&>  cancels bold printing
// <ESC><E>  selects continuous underlining  (spaces between words are underlined) for one line
// <ESC><R>  cancels underlining
// <ESC><X>  cancels both bold and underlining
// <ESC><U>  half line feed (paper up 1/2 line)
// <ESC><D>  reverse half line feed (paper down 1/2 line)
// <ESC><BS> backspace 1/120 inch
// <ESC><LF> reverse line feed (paper down one line)
//
// printer control not part of the Diablo 630 emulation:
// <ESC><u>  selects micro paper up (1/8 line or 1/48")
// <ESC><d>  selects micro paper down (1/8 line or 1/48")
// <ESC><b>  selects broken underlining (spaces between words are not underlined)
// <ESC><p>  selects Pica pitch (10 characters/inch or 12 point)
// <ESC><e>  selects Elite pitch (12 characters/inch or 10 point)
// <ESC><m>  selects Micro Elite pitch (15 characters/inch or 8 point)
//
// diagnostics/debugging:
// <ESC><^Z><c> print (on the serial console) the current column
// <ESC><^Z><r> reset the DS89C440 microcontroller
// <ESC><^Z><s> print (on the serial console) the current micro-space count
// <ESC><^Z><u> print (on the serial console) the uptime as HH:MM:SS
// <ESC><^Z><w> print (on the serial console) the number of watchdog resets
// <ESC><^Z><e><n> turn flashing red error LED on or off (n=1 is on, n=0 is off)
// <ESC><^Z><p><n> print (on the serial console) the value of Port n (0-3) as 2 digit hex number
//-------------------------------------------------------------------------------------------
void print_character(unsigned char charToPrint) {
    static char escape = 0;                                 // escape sequence state
    char i,t;

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
                    ww_print_letter(charToPrint,attribute);
                    putchar(charToPrint);                   // echo the character to the console
                    ++column;                               // update column
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
                case 'a':
                    printf("\n%s\n",banner);
                    break;
                case 'c':                                   // <ESC><^Z><c> print current column
                    printf("%s %u\n","Column:",(int)column);
                    escape = 0;
                    break;
                case 'e':                                   // <ESC><^Z><e> toggle red error LED
                    escape = 4;
                    break;
                case 'p':                                   // <ESC><^Z><p> print port values
                    escape = 3;
                    break;
                case 'r':                                   // <ESC><^Z><r> system reset
					TA = 0xAA;	        					// timed access
					TA = 0x55;
                    FCNTL = 0x0F;		        			// use the FCNTL register to preform a system reset
                    break; 
                case 's':
                    printf("%s %u\n","uSpaces:",uSpaceCount);
                    escape = 0;
                    break;
                case 'u':                                   // <ESC><^Z><u> print uptime
                    printf("%s %02u%c%02u%c%02u\n","Uptime:",(int)hours,':',(int)minutes,':',(int)seconds);
                    escape = 0;
                    break; 
                case 'w':                                   // <ESC><^Z><w> print watchdog resets
                    printf("%s %u\n","Watchdog resets:",(int)wdResets);
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
			else if (charToPrint == ESC) {					// if it's ESCAPE, exit
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
// control C - toggles automatic centering (not implemented yet)
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
    static unsigned char idata keybuffer[64];                   // 64 character buffer used for ps/2 keyboard input
    static unsigned char keybufptr = 0;                         // pointer into keybuffer
    unsigned char i,t;

    if (kb_ctrl_pressed()) {                                    // is the control key pressed?
        switch (key) {
            case 'b':
            case 'B':                                           // control B or b toggles bold printing
                ww_spin();                                      // spin the printwheel
                attribute ^= 0x01;                              // toggle bold print flag
                break;
            case 'c':
            case 'C':      
                ww_spin();                                     // control C or c toggles automatic centering
                centered ^= 0x01;                               // toggle centered flag                 
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
                putchar(BS);                                        // erase the last character on the Teraterm screen
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
	unsigned int loopcounter,WWdata;
    unsigned char state = 0;

    wd_disable_watchdog();                                      // disable wwtchdog timer reset

    PMR |= 0x01;                                                // enable internal SRAM MOVX memory

    amberLED = OFF;                                             // turn off the amber LED
    greenLED = OFF;                                             // turn off the green LED
    redLED = OFF;                                               // turn off the red LED

    busyPin = LOW;                                              // set LPT Busy low: ready to receive 
    ackPin = HIGH;                                              // set LPT Acknowledge high
    P2 = 0xFF;                                                  // set all pins of port 2 high to act as input

    EPFI = 1;                                                   // enable power fail interrupt
	IT0 = 1;	                                        		// configure interrupt 0 for falling edge on INT0 (P3.2)
	EX0 = 1;   			                                   		// enable EX0 Interrupt
	TL0 = RELOADLO;                        	                	// load timer 0 low byte
	TH0 = RELOADHI;            	                            	// load timer 0 high byte
	TMOD = (TMOD & 0xF0) | 0x01;                           	    // configure timer 0 for mode 1 - 16 bit timer
	ET0 = 1;                     	                            // enable timer 0 interrupt
	TR0 = 1;                     	                            // run timer 0

    kb_init();                                                  // initialize ps/2 keyboard
    uart_init(BAUDRATE);                                        // initialize serial 0 for N-8-1 at 9600bps, RTS-CTS handshaking
    ww_init();                                                  // initialize serial 1 for the Wheelwriter

	EA = TRUE;                     	                            // global interrupt enable

    printf("\n%s\n",banner);

    switch (WDCON & 0x44) {
        case 0x00:
            if (pwrFail == 0xAA) 
                printf("Power failure reset\n\n");
            else 
                printf("External reset\n\n");
            pwrFail = 0;
            wdResets = 0;
            break;
        case 0x04:
            printf("%s %u\n\n","Watchdog resets:",(int)++wdResets);
            break;
        case 0x40:
            printf("Power on reset\n\n");
            printf("Initializing...\n");
            pwrFail = 0;
            wdResets = 0;
            timeout = 7*ONESEC;                                 // allow up to 7 seconds in case the carrier is at the right margin
            while(timeout) {                                    // loop for 7 seconds waiting for data from the Wheelwriter
                while (ww_data_avail()){
                    WWdata = ww_get_data(); 
                    switch (state) {
                        case 0:
                            if (WWdata == 0x121) 
                                state = 1;
                            break;
                        case 1:
                            if (WWdata == 0x001)                // 0x121,0x001 is the "reset" command
                                state = 2;
                            else
                                state = 0;
                            break; 
                        case 2:
                            printWheel = WWdata;                // the word following 0x121,0x001 is returned by the Wheelwriter to indicate the printwheel pitch
                            state = 3;
                            break;
                        case 3:                                 // timeout and exit the loop 1 second after the last word is received
                            timeout = ONESEC;
                            break;
                    } 
                }
            }
            break;
    } // switch (WDCON & 0x44)

    while(kb_scancode_avail()) {                                // any input from the ps/2 keyboard?
        //printf("0x%02X\n",(unsigned int)(kb_get_scancode() & 0xFF));
        kb_get_scancode();        
    }

    if (kb_send_cmd(0xFF)) {                                    // send reset command to keyboard
        timeout = ONESEC;
        while (timeout);                                        // one second delay for keyboard to finish sending codes...
        while(kb_scancode_avail()){      	                    // if any ps/2 keyboard input is waiting in queue...
            //printf("0x%02X\n",(unsigned int)(kb_get_scancode() & 0xFF));
            if (kb_get_scancode() == 0xAA)
                printf("PS/2 keyboard\n");        
        }
    }

    wd_clr_flags();                                             // clear watchdog reset and POR flags for next start up
    wd_init_watchdog(3);                                        // WD interval = (1/12MHz)*2^26 = 5592.4 milliseconds

    if (!printWheel) {                                          // if the pitch of the printwheel is not yet known...
        amberLED = ON;                                          // turn on the amber LED
        ww_put_data(0x121);                         
        ww_put_data(0x001);                                     // send the "reset" command
        amberLED = OFF;                                         // turn off the LED
        timeout = 3*ONESEC;                                     // 3 seconds timeout
        while(!printWheel){                                     // while the pitch of the printwheel is yet unknown...
            wd_reset_watchdog();
            if (!timeout) {                                     // timeout after 3 seconds!
                printf("Unable to determine printwheel, assuming 12P.\n");
                printWheel = 0x020;
                errorLED = TRUE;
                break;
            }
	        if (ww_data_avail())                              	// if there's data from the Wheelwriter...
                printWheel = ww_get_data();                     // Wheelwriter reports printwheel pitch
	    }
        amberLED = ON;                                          // turn on the amber LED
        ww_put_data(0x121);                         
        ww_put_data(0x006);                                     // move the carrier horizontally from the left hand stop to the left margin
        ww_put_data(0x080);                                     // bit 7 is set for left to right direction
        ww_put_data(0x078);                                     // 120 microspaces (10 characters) from the left hand stop to the left margin
        amberLED = OFF;                                         // turn off the LED

        timeout = ONESEC;
        while (timeout);                                        // wait one second while the carrier moves

    } // while(!pitch)

    switch (printWheel) {
        case 0x008:
            uSpacesPerChar = 10;
            uLinesPerLine = 16;
            tabStop = 6;                                        // tab stops every 6 characters (every 1/2 inch)
            printf("PS printwheel\n");
            break;
        case 0x010:
            uSpacesPerChar = 8;
            uLinesPerLine = 12;
            tabStop = 7;                                        // tab stops every 7 characters (every 1/2 inch)
            printf("15P printwheel\n");
            break;
        case 0x020:
            uSpacesPerChar = 10;
            uLinesPerLine = 16;
            tabStop = 6;                                        // tab stops every 6 characters (every 1/2 inch)
            printf("12P printwheel\n");
            break;
        case 0x021:
            uSpacesPerChar = 10;
            uLinesPerLine = 16;
            tabStop = 6;                                        // tab stops every 6 characters (every 1/2 inch)
            printf("No printwheel\n");
            break;
        case 0x040:
            uSpacesPerChar = 12;
            uLinesPerLine = 16;
            tabStop = 5;                                        // tab stops every 5 characters (every 1/2 inch)
            printf("10P printwheel\n");
            break;
		default:
            uSpacesPerChar = 10;
            uLinesPerLine = 16;
            tabStop = 6;                                        // tab stops every 6 characters (every 1/2 inch)        
            printf("Unable to determine printwheel\n");
    }

    amberLED = ON;                                              // turn on the amber LED
    ww_put_data(0x121);
    ww_put_data(0x000);
    amberLED = OFF;                                             // turn off the amber LED
    timeout=ONESEC/2;                                           // 1/2 second timeout
    while(!wheelwriterModel){                                   // while the model is yet unknown...
        wd_reset_watchdog();
        if (!timeout) {                                         // timeout!
           printf("Unable to determine model\n");
           wheelwriterModel=0xFF;    
           errorLED=TRUE;               
           break;                  
        }
	    if (ww_data_avail()) {                                 	// if there's data from the Wheelwriter...
            wheelwriterModel=ww_get_data();
            switch (wheelwriterModel) {
                case 0x006:
                    printf("Wheelwriter 3\n");
                    break;
                case 0x025:
                    printf("Wheelwriter 5\n");
                    break;
                case 0x026:
                    printf("Wheelwriter 6\n");
                    break;
                default:
                    printf("Unknown Wheelwriter model\n");
                    //printf("0x%02X\n",(int) wheelwriterModel);
                    errorLED=TRUE;               
                    break;
            }
	    }
    } // while(!wheelwriterModel)
    
    initializing = FALSE;    
    printf("ESC H for help\n");
    printf("Ready\n");

    //----------------- loop here forever -----------------------------------------
    while(TRUE) {

        wd_reset_watchdog();                                    // "pet" the watchdog every time through the loop

        if (++loopcounter==0)                                  // every 65536 times through the loop (at about 2Hz)
            greenLED = !greenLED;                               // toggle the green LED

        if (pwrFail == 0xAA) {
            pwrFail = 0;                                        // if there's been a power failure detected...
            printf("Power failure!\n");
        }

        if (uart_char_avail())                                 // if there is a character in the serial 0 receive buffer...
            print_character(uart_getchar());                    // retrieve it and make the Wheelwriter print it

        if (busyPin) {                                          // if there is a character from the parallel port...
            print_character(P2);                                // print the character from the parallel port (port 2)
            ackPin = LOW;                                       // set Acknowledge pin low
            _nop_();                                            // 3 microseconds delay...
            _nop_();
            _nop_();
            ackPin = HIGH;                                      // set Acknowledge pin high
            busyPin = LOW;                                      // set Busy pin low, ready for next character
        }

        if (kb_scancode_avail())      		                    // if there is a scancode from the ps/2 keyboard...
            handle_key(kb_decode_scancode(kb_get_scancode()));  // decode the scancode from the keyboard
    }
}