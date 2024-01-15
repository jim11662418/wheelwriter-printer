// Keyboard functions
// For the Keil C51 compiler.

#include <stdio.h>
#include <reg420.h>
#include "scancodes.h"
#include "keycodes.h"
                     
#define FALSE 0
#define TRUE  1
#define LOW   0
#define HIGH  1
#define ON  1
#define OFF 0

// pins used for PS/2 interface
sbit kb_clock_out = P1^4;       // pin 5
sbit kb_data_out = P1^5;        // pin 6
sbit kb_data_in = P1^6;         // pin 7
sbit kb_clock_in = P3^3;        // pin 13

// variables for keyboard...
bit kb_ctrl;                                // control key is pressed
bit kb_alt;                                 // alt key is pressed
bit kb_shift;                               // shift key is pressed
volatile unsigned char kb_bitcount;         // count of bits received from keyboard
volatile unsigned char kb_out;              // keyboard scancode read index.
volatile unsigned char kb_in;               // keyboard scancode write index.
volatile unsigned char xdata kb_buf[16];    // keyboard scancode queue.

// ---------------------------------------------------------------------------
// interrupt each time the keyboard clock input goes low. stores the scancode in the 
// receive buffer after all eleven bits (1 start, 9 data, 1 stop) have been received.
// ---------------------------------------------------------------------------
void kb_isr(void) interrupt 2 using 2{
    static bit kb_parity = 0;
    static unsigned char recdbits = 0;

	switch (kb_bitcount) {
		case 0:									// start bit
			if (!kb_data_in) {					// if start bit is low
				kb_bitcount++;
			}
		    break;
		case 1:									// bit 0
		case 2:									// bit 1
		case 3:									// bit 2
		case 4:									// bit 3
		case 5:									// bit 4
		case 6:									// bit 5
		case 7:									// bit 6
		case 8:									// bit 7
        	recdbits >>= 1;                     // shift bits right one position
        	if (kb_data_in) {
                recdbits |= 0x80;      			// "or" in msb
				kb_parity ^= 0x01;				// complement parity flag
			}
			kb_bitcount++;
		    break;
		case 9:									// parity bit
        	if (kb_data_in) {
				kb_parity ^= 0x01;
			}
			kb_bitcount++;
		    break;
		case 10:								// stop bit
			if (kb_data_in && kb_parity) {		// if stop bit is high and parity is odd
        		kb_buf[kb_in] = recdbits;       // store the scancode in the buffer
                kb_in = ++kb_in & 0x0F;
			}
        	kb_bitcount = 0;
        	recdbits = 0;
			kb_parity = 0;
		    break;
	}
}

// ---------------------------------------------------------------------------
// returns true if there is a keyboard scancode waiting in the buffer.
// ---------------------------------------------------------------------------
bit kb_scancode_avail(void) {
    return (kb_in != kb_out);        
}

// ---------------------------------------------------------------------------
// returns one scancode from the keyboard queue. waits until one is available if necessary.
// ---------------------------------------------------------------------------
unsigned char kb_get_scancode(void) {
    unsigned char buf;

    while(kb_in == kb_out);            // wait until a character is available
    buf = kb_buf[kb_out];
    kb_out = ++kb_out & 0x0F;
    return(buf);
}

// ---------------------------------------------------------------------------
// Send a command to the keyboard, return TRUE if acknowledge (0xFA) received from keyboard
//
// Steps the host must follow to send data to a PS/2 device:
//  1.   Bring the Clock line low for at least 100 microseconds. 
//  2.   Bring the Data line low. 
//  3.   Release the Clock line. 
//  4.   Wait for the device to bring the Clock line low. 
//  5.   Set/reset the Data line to send the first data bit 
//  6.   Wait for the device to bring Clock high. 
//  7.   Wait for the device to bring Clock low. 
//  8.   Repeat steps 5-7 for the other seven data bits and the parity bit 
//  9.   Release the Data line. 
//  10.  Wait for the device to bring Data low. 
//  11.  Wait for the device to bring Clock low. 
//  12.  Wait for the device to release Data and Clock
// ---------------------------------------------------------------------------
unsigned char kb_send_cmd(unsigned char kbcmd) {
	bit txbit,paritybit;
    unsigned char c,i;
	unsigned int k;

	while (kb_bitcount);                // don't send while a character is being received
	kb_clock_out = 0;           	    // pull the clock line low
	for (i=0;i<50;i++);	                // (50*5)+3 cycles = 253 microseconds delay
	kb_data_out = 0;				    // pull the data line low
	kb_clock_out = 1;			        // release the clock line

    for (i=0;i<5;i++);                  // (5*5)+3 cycles = 28 microseconds
    k=0;
	while (++k && kb_clock_in);	        // wait until the keyboard pulls the clock line low
	if (!k) {
		return FALSE;				    // timeout, no response from keyboard
    }

	paritybit = 1;
	for (k = 0; k < 8; k++) {           // start bit rec'd, send data bits 0 - 7
		txbit = (kbcmd & 0x01);         // next bit to send

		if (txbit) {
			kb_data_out = 1;
			paritybit ^= 0x01;
		}
		else {
			kb_data_out = 0;
		}

		kbcmd >>= 1;			        // shift bits to the right
		while(!kb_clock_in);	        // wait until clock line goes high
		while(kb_clock_in);		        // wait until clock line goes low
	}

	kb_data_out = paritybit;			// send parity bit
	while(!kb_clock_in);		        // wait until clock line goes high
	while(kb_clock_in);			        // wait until clock line goes low

	kb_data_out = 1;			        // send stop bit
	while(kb_data_in);			        //wait until data line goes low
	while(kb_clock_in);			        //wait until clock line goes low

	// wait for ack from keyboard
	while(!kb_clock_in);		        //wait until clock line goes high
	while(!kb_data_in);			        //wait until data line goes high

	k = 0;
	while(++k && (kb_in == kb_out));    // wait until the keyboard replies

	if (!k) {
		return FALSE;			        // timeout, no reply
    }
	else {
		c = kb_get_scancode();	        // get the reply from the keyboard
        return (c == 0xFA);             // is the reply "acknowledge"?
	}
}

// ---------------------------------------------------------------------------
// returns the character (if any) decoded from the scancode. not all scancodes 
// have a corresponding character. if not, returns zero.
// ---------------------------------------------------------------------------
#define SCROLL_LOCK 0x01
#define NUM_LOCK 0x02
#define CAPS_LOCK 0x04
unsigned char kb_decode_scancode(unsigned char scancode) {
    static unsigned char kb_state = 0;
    static unsigned char kb_leds = 0;
    unsigned char result;

    switch (kb_state) {
        case 0:
            switch (scancode) {
                case 0xE0:              // next scancode is an extended code
                    kb_state = 1;
                    break;
                case 0xE1:              // next scancode is an extended 1 code
                    kb_state = 6;
                    break;
                case 0xF0:              // next scancode is a release code
                    kb_state = 3;
                    break;
                case 0x01:
                    return(PS2_KEY_F1);
                case 0x03:
                    return(PS2_KEY_F5);
                case 0x04:
                    return(PS2_KEY_F3);
                case 0x05:
                    return(PS2_KEY_F1);
                case 0x06:
                    return(PS2_KEY_F2);
                case 0x07:
                    return(PS2_KEY_F12);
                case 0x09:
                    return(PS2_KEY_F10);
                case 0x0A:
                    return(PS2_KEY_F8);
                case 0x0B:
                    return(PS2_KEY_F6);
                case 0x0C:
                    return(PS2_KEY_F4);
                case 0x0D:                                  // tab
                    return(PS2_KEY_TAB);
                case 0x11:                                  // left Alt
                    kb_alt = ON;
                    break;
                case 0x12:                                  // left Shift
                    kb_shift = ON;
                    break;
                case 0x14:                                  // left Ctrl
                    kb_ctrl = ON;
                    break;
                case 0x29:
                    return(PS2_KEY_SPACE);                  // space
                case 0x58:
                    kb_leds ^= CAPS_LOCK;                   // toggle caps lock bit
			        if (kb_send_cmd(0xED))
				        kb_send_cmd(kb_leds);               // update keyboard LEDs
                    break;
                case 0x59:                                  // right Shift
                    kb_shift = ON;
                    break;
                case 0x5A:                                  // Enter
                    return(PS2_KEY_ENTER);
                case 0x66:
                    return(PS2_KEY_BACKSPACE);              // backspace
                case 0x69:
                    if (kb_leds & NUM_LOCK)                 // if num lock bit is set...
                        return('1');
                    else 
                        return(PS2_KEY_KP_END);
                case 0x6B:
                    if (kb_leds & NUM_LOCK)                 // if num lock bit is set...
                        return('4');
                    else
                        return(PS2_KEY_KP_LT_ARROW);
                case 0x6C:
                    if (kb_leds & NUM_LOCK)                 // if num lock bit is set...
                        return('7');
                    else
                        return(PS2_KEY_KP_HOME);
                case 0x70:
                    if (kb_leds & NUM_LOCK)                 // if num lock bit is set...
                        return('0');
                    else
                        return(PS2_KEY_KP_INSERT);
                case 0x71:
                    if (kb_leds & NUM_LOCK)                 // if num lock bit is set...
                        return('.');
                    else
                        return(PS2_KEY_KP_DELETE);
                case 0x72:
                    if (kb_leds & NUM_LOCK)                 // if num lock bit is set...
                        return('2');
                    else
                        return(PS2_KEY_KP_DN_ARROW);
                case 0x73:
                    return('5');
                case 0x74:
                    if (kb_leds & NUM_LOCK)                 // if num lock bit is set...
                        return('6');
                    else
                        return(PS2_KEY_KP_RT_ARROW);
                case 0x75:
                    if (kb_leds & NUM_LOCK)                 // if num lock bit is set...
                        return('8');
                    else
                        return(PS2_KEY_KP_UP_ARROW);
                case 0x76:
                    return(PS2_KEY_ESCAPE);                 // escape
                case 0x77:
                    kb_leds ^= NUM_LOCK;                    // toggle num lock bit
			        if (kb_send_cmd(0xED))
				        kb_send_cmd(kb_leds);               // update keyboard LEDs
                    break;
                case 0x78:
                    return(PS2_KEY_F11);
                case 0x79:
                    return(PS2_KEY_KP_PLUS);
                case 0x7A:
                    if (kb_leds & NUM_LOCK)                 // if num lock bit is set...
                        return('3');
                    else
                        return(PS2_KEY_KP_PGDN);
                case 0x7B:
                    return(PS2_KEY_KP_MINUS);
                case 0x7C:
                    return(PS2_KEY_KP_MULT);
                case 0x7D:
                    if (kb_leds & NUM_LOCK)                     
                        return('9');
                    else
                        return(PS2_KEY_KP_PGUP);
                case 0x7E: 
                    kb_leds ^= SCROLL_LOCK;                 // toggle scroll lock bit
			        if (kb_send_cmd(0xED))
				        kb_send_cmd(kb_leds);               // update keyboard LEDs
                    break;
                case 0x83: 
                    return(PS2_KEY_F7);
                default:
                    if (kb_shift && !(kb_leds & CAPS_LOCK)) {   // shift but not caps lock
                        return(shifted[scancode]);
                    }
                    else {
                        result = unshifted[scancode];
                        if ((kb_leds & CAPS_LOCK) && !kb_shift) {// caps lock but not shift
                            if (result > 0x60 && result < 0x7B)
                                result = result-0x20;
                        }
                        return(result);
                    }
            }  // switch (scancode)
            break; // case 0:
        case 1:                                             // 0xE0 scancode
            switch (scancode) {
                case 0xF0:                                  // release code
                    kb_state = 2;
                    break;
                case 0x11:	                                // 0xE0,0x11 = right Alt
                    kb_alt = ON;
                    kb_state = 0;
                    break;
                case 0x12:
                    kb_state = 4;
                    break;
                case 0x14:                                  // 0xE0,0x14 = right Cntl
                    kb_ctrl = ON;
                    kb_state = 0;
                    break;
                case 0x1F:	                                // left gui
                    kb_state = 0;
                    return(PS2_KEY_LT_GUI);
                case 0x27:
                    kb_state = 0;
                    return(PS2_KEY_RT_GUI);
                case 0x2F:
                    kb_state = 0;
                    return(PS2_KEY_MENU);
                case 0x4A:	
                    kb_state = 0;
                    return(PS2_KEY_KP_DIV);
                case 0x5A:	
                    kb_state = 0;
                    return(PS2_KEY_KP_ENTER);
                case 0x69:	
                    kb_state = 0;
                    return(PS2_KEY_END);
                case 0x6B:	
                    kb_state = 0;
                    return(PS2_KEY_LT_ARROW);
                case 0x6C:	
                    kb_state = 0;
                    return(PS2_KEY_HOME);
                case 0x70:	
                    kb_state = 0;
                    return(PS2_KEY_INSERT);
                case 0x71:	
                    kb_state = 0;
                    return(PS2_KEY_DELETE);
                case 0x72:	
                    kb_state = 0;
                    return(PS2_KEY_DN_ARROW);
                case 0x74:	
                    kb_state = 0;
                    return(PS2_KEY_RT_ARROW);
                case 0x75:	
                    kb_state = 0;
                    return(PS2_KEY_UP_ARROW);
                case 0x7A:	
                    kb_state = 0;
                    return(PS2_KEY_PGDN);
                case 0x7D:	
                    kb_state = 0;
                    return(PS2_KEY_PGUP);
                default:
                    kb_state = 0;
                    break;
            }
            break;
        case 2:                                 // extended key release: 0xE0,0xF0,scancode
            switch(scancode) {
               case 0x11:
                    kb_alt = OFF;               // 0xE0,0xF0,0x11 = rt alt released
                    break;
               case 0x14:                       // 0xE0,0xF0,0x14 = rt ctrl released
                    kb_ctrl = OFF;
                    break;
            }
            kb_state = 0;
            break;
        case 3:                                 // key release: 0xF0,scancode
            switch(scancode) {
               case 0x12:                       // 0xF0,0x12 = lt shift released
                    kb_shift = OFF;             
                    break;
               case 0x59:                       // 0xF0,0x59 = rt shift released
                    kb_shift = OFF;  
                    break;
               case 0x14:                       // 0xF0,0x14 = lt ctrl released
                    kb_ctrl = OFF;
                    break;
               case 0x11:                       // 0xF0,0x11 = lt alt released
                    kb_alt = OFF;
                    break;
            }
            kb_state = 0;
            break;
        case 4:                                 
            if (scancode == 0xE0)               // 0xE0,0x12,0xE0     
               kb_state = 5;
            else 
               kb_state = 0;
            break;
        case 5:
            kb_state = 0;
            if (scancode == 0x7C)               // 0xE0,0x12,0xE0,0x7C
                return(PS2_KEY_PRTSCR);               
            break;
        case 6: 
            if (scancode == 0x14)               // 0xE1,0x14,scancode     
               kb_state = 7;
            else 
               kb_state = 0;
            break;
        case 7:
            if (scancode == 0x77)                 // 0xE1,0x14,0x77
                kb_state = 8;
            else 
                kb_state = 0;
            break;
        case 8:
            if (scancode == 0xE1)                 // 0xE1,0x14,0x77,0xE1
                kb_state = 9;
            else 
                kb_state = 0;
            break;
        case 9:
            if (scancode == 0xF0)                 // 0xE1,0x14,0x77,0xE1,0xF0
                kb_state = 10;
            else 
                kb_state = 0;
            break;
        case 10:
            if (scancode == 0x14)                 // 0xE1,0x14,0x77,0xE1,0xF0,0x14
                kb_state = 11;
            else 
                kb_state = 0;
            break;
        case 11:
            if (scancode == 0xF0)                 // 0xE1,0x14,0x77,0xE1,0xF0,0x14,0xE0
                kb_state = 12;
            else 
                kb_state = 0;
            break;
        case 12:
            kb_state = 0;
            if (scancode == 0x77)                 // 0xE1,0x14,0x77,0xE1,0xF0,0x14,0xE0,0x77
                return(PS2_KEY_PAUSE);
            break;
        default:
            kb_state = 0;
    }
    return(0);
}

// ---------------------------------------------------------------------------
// initialize external interrupt 0 and keyboard vars
// ---------------------------------------------------------------------------
void kb_init(void){
   
    kb_bitcount = 0;
    kb_out = 0;
    kb_in = 0;
    kb_ctrl = 0;
    kb_alt = 0;
    kb_shift = 0;

    // initialize external interrupt 1
    IT1 = 1;                    // make external interrupt 1 edge triggered
    kb_clock_in = 1;            // make inputs high
    kb_data_in = 1;
    EX1 = 1;                    // enable external interrupt 1
}

// ---------------------------------------------------------------------------
// returns TRUE if either control key is currently pressed
// ---------------------------------------------------------------------------
bit kb_ctrl_pressed(void) {
   return kb_ctrl;
}

// ---------------------------------------------------------------------------
// returns TRUE if either alt key is currently pressed
// ---------------------------------------------------------------------------
bit kb_alt_pressed(void) {
    return kb_alt;
}

// ---------------------------------------------------------------------------
// returns TRUE if either shift key is currently pressed
// ---------------------------------------------------------------------------
bit kb_shift_pressed(void) {
    return kb_shift;
}