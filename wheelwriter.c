#include <reg420.h>

#define FALSE 0
#define TRUE  1
#define ON 0                                        // 0 turns the amber LED on
#define OFF 1                                       // 1 turns the amber LED off
#define BSIZE 8                                     // Must be one of these powers of 2 (2,4,8,16,32,64,128)

#if BSIZE < 2
#error BSIZE is too small. BSIZE may not be less than 2.
#elif BSIZE > 128
#error BSIZE is too large. BSIZE may not be greater than 128.
#elif ((BSIZE & (BSIZE-1)) != 0)
#error BSIZE must be a power of 2.
#endif

unsigned char uSpacesPerChar = 10;                  // micro spaces per character (8 for 15cpi, 10 for 12cpi and PS, 12 for 10cpi)
unsigned char uLinesPerLine = 16;                   // micro lines per line (12 for 15cpi; 16 for 10cpi, 12cpi and PS)
unsigned int  uSpaceCount = 0;                      // number of micro spaces on the current line (for carriage return)

sbit amberLED = P0^5;                               // amber LED connected to pin 34 0=on, 1=off
sbit WWbus = P1^2;		  			                // P1.2, (RXD1, pin 3) used to monitor the Wheelwriter BUS

///////////////////////////// Serial 1 interface to Wheelwriter ////////////////////////////
volatile unsigned char data rx1_head;       	    // receive interrupt index for serial 1
volatile unsigned char data rx1_tail;       	    // receive read index for serial 1
volatile unsigned int data rx1_buf[BSIZE];          // receive buffer for serial 1 
volatile bit tx1_ready;                             // set when ready to transmit
volatile bit waitingForAcknowledge = 0;             // TRUE when expecting the acknowledge pulse from Wheelwriter

// ---------------------------------------------------------------------------
// Serial 1 interrupt service routine
// ---------------------------------------------------------------------------
void uart1_isr(void) interrupt 7 using 3 {
	unsigned int wwBusData;
	static char count = 0;

    // serial 1 transmit interrupt
    if (TI1) {                                      // transmit interrupt?
	   TI1 = FALSE;                                 // clear transmit interrupt flag
	   tx1_ready = TRUE;						    // transmit buffer is ready for a new character
    }
    
    //serial 1 receive interrupt
    if(RI1) {                                	    // receive interrupt?
       RI1 = 0;                             	    // clear receive interrupt flag
       wwBusData = SBUF1;                           // retrieve the lower 8 bits
       if (RB81) wwBusData |= 0x0100;               // ninth bit is in RB81

       // discard the acknowledge pulse (all zeros)
       if (waitingForAcknowledge) {                 // just transmitted a command, waiting for acknowledge...
          waitingForAcknowledge = FALSE;            // clear the flag
          if (wwBusData) {                          // if it's not acknowledge (all zeros) ...
             rx1_buf[rx1_head++ & (BSIZE-1)] = wwBusData;// save it in the buffer
          }
	   }
       else {                                       // not waiting for acknowledge...
          if (wwBusData == 0x121) {
              count = 1; 
          }
          else {
             ++count;
          }

	      if (wwBusData || (count%2)) {             // if wwBusData is not zero or if it's the second zero...
             rx1_buf[rx1_head++ & (BSIZE-1)] = wwBusData;// save it in the buffer
	      }
       }
    }   
}

// ---------------------------------------------------------------------------
//  Initialize serial 1 for mode 2. Mode 2 is an asynchronous mode that transmits and receives
//  a total of 11 bits: 1 start bit, 9 data bits, and 1 stop bit. The ninth bit to be 
//  transmitted is determined by the value in TB8 (SCON1.3). When the ninth bit is received, 
//  it is stored in RB8 (SCON1.2). the baud rate for mode 2 is a function only of the oscillator
//  frequency. It is either the oscillator input divided by 32 or 64 as programmed by the SMOD 
//  doubler bit for the associated UART. The SMOD_1 baud-rate doubler bit for serial port 1 
//  is located at WDCON.7. In this case, the SMOD_1 baud-rate doubler bit is zero. Thus, the
//  12MHz clock divided by 64 gives a bit rate for serial 1 of 187500 bps.
// ---------------------------------------------------------------------------
void ww_init(void) {
    rx1_head = 0;                   			    // initialize serial 1 head/tail pointers.
    rx1_tail = 0;
    SMOD_1 = FALSE;                                 // SMOD_1=0 therefor Serial 1 baud rate is oscillator freq (12 MHz) divided by 64 (187500 bps)
    SM01 = TRUE;                                    // SM01=1, SM11=0, SM21=0 sets serial mode 2
    SM11 = FALSE;
    SM21 = FALSE;
    REN1 = TRUE;                    			    // enable receive characters.
    TI1 = TRUE;                     			    // set TI of SCON1 to Get Ready to Send
    RI1  = FALSE;                   			    // clear RI of SCON1 to Get Ready to Receive
    ES1 = TRUE;                    				    // enable serial interrupt.
}

// ---------------------------------------------------------------------------
// sends an unsigned integer to the Wheelwriter as 11 bits (start bit, 9 data bits, stop bit)
// ---------------------------------------------------------------------------
void ww_put_data(unsigned int wwCommand) {
   while (!tx1_ready);		 					    // wait until transmit buffer is empty
   tx1_ready = 0;                                   // clear flag   
   while(!WWbus);                                   // wait until the Wheelwriter bus goes high
   REN1 = FALSE;                                    // disable reception
   TB8_1 = (wwCommand & 0x100);                     // ninth bit
   SBUF1 = wwCommand & 0xFF;                        // lower 8 bits
   while(!tx1_ready);                               // wait until finished transmitting
   REN1 = TRUE;                                     // enable reception
   waitingForAcknowledge = TRUE;                    // just transmitted a command, now waiting for acknowledge
   while(!WWbus);                                   // wait until the Wheelwriter bus goes high
   while(WWbus);                                    // wait until the Wheelwriter bus goes low (acknowledge)
   while(!WWbus);                                   // wait until the Wheelwriter bus goes high again
}

// ---------------------------------------------------------------------------
// returns TRUE if there is an unsigned integer from the Wheelwriter waiting in the serial 1 receive buffer.
// ---------------------------------------------------------------------------
bit ww_data_avail(void) {
   return (rx1_head != rx1_tail);                   // not equal means there's something in the buffer
}

//----------------------------------------------------------------------------
// returns the next unsigned integer from the Wheelwriter in the serial 1 receive buffer.
// waits for an integer to become available if necessary.
//----------------------------------------------------------------------------
unsigned int ww_get_data(void) {
    unsigned int buf;

    while (rx1_head == rx1_tail);     			    // wait until a word is available
    buf = rx1_buf[rx1_tail++ &0x07];                // retrieve the word from the buffer
    return(buf);
}

//------------------------------------------------------------------------------------------------
// ASCII to Wheelwriter printwheel translation table
// The Wheelwriter code indicates the position of the character on the printwheel. “a” (code 01) 
// is at the 12 o’clock position of the printwheel. Going counter clockwise, “n” (code 02) is next 
// character on the printwheel followed by “r” (code 03), “m” (code 04), “c” (code 05), “s” (code 06),
// “d” (code 07), “h” (code 08), and so on.
//------------------------------------------------------------------------------------------------
char code printwheelChar[160] =  
// col: 00    01    02    03    04    05    06    07    08    09    0A    0B    0C    0D    0E    0F    row:
//      sp     !     "     #     $     %     &     '     (     )     *     +     ,     -     .     /
      {0x00, 0x49, 0x4b, 0x38, 0x37, 0x39, 0x3f, 0x4c, 0x23, 0x16, 0x36, 0x3b, 0x0c, 0x0e, 0x57, 0x28, // 20
//       0     1     2     3     4     5     6     7     8     9     :     ;     <     =     >     ?
       0x30, 0x2e, 0x2f, 0x2c, 0x32, 0x31, 0x33, 0x35, 0x34, 0x2a ,0x4e, 0x50, 0x00, 0x4d, 0x00, 0x4a, // 30
//       @     A     B     C     D     E     F     G     H     I     J     K     L     M     N     O
       0x3d, 0x20, 0x12, 0x1b, 0x1d, 0x1e, 0x11, 0x0f, 0x14, 0x1F, 0x21, 0x2b, 0x18, 0x24, 0x1a, 0x22, // 40
//       P     Q     R     S     T     U     V     W     X     Y     Z     [     \     ]     ^     _   
       0x15, 0x3e, 0x17, 0x19, 0x1c, 0x10, 0x0d, 0x29, 0x2d, 0x26, 0x13, 0x41, 0x00, 0x40, 0x00, 0x4f, // 50
//       `     a     b     c     d     e     f     g     h     i     j     k     l     m     n     o
       0x00, 0x01, 0x59, 0x05, 0x07, 0x60, 0x0a, 0x5a, 0x08, 0x5d, 0x56, 0x0b, 0x09, 0x04, 0x02, 0x5f, // 60
//       p     q     r     s     t     u     v     w     x     y     z     {     |     }     ~    DEL  
       0x5c, 0x52, 0x03, 0x06, 0x5e, 0x5b, 0x53, 0x55, 0x51, 0x58, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, // 70
//     
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 80      
//     
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 90      
//                   ¢                             §                                                  
       0x00, 0x00, 0x3A, 0x00, 0x00, 0x00, 0x00, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // A0      
//       °     ±     ²     ³                 ¶                                   ¼     ½              
       0x44, 0x3C, 0x42, 0x43, 0x00, 0x00, 0x46, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48, 0x47, 0x00, 0x00};// B0


// backspace, no erase. decreases micro space count by uSpacesPerChar.
void ww_backspace(void) {                        
    amberLED = ON;                          // turn on amber LED    
    ww_put_data(0x121);
    ww_put_data(0x006);                     // move the carrier horizontally
    ww_put_data(0x000);                     // bit 7 is cleared for right to left direction
    ww_put_data(uSpacesPerChar);    
    uSpaceCount -= uSpacesPerChar;
    amberLED = OFF;                         // turn off amber LED  
}

// backspace 1/120 inch. decrements micro space count
void ww_micro_backspace(void) {
    if (uSpaceCount){                       // only if the carrier is not at the left margin
        amberLED = ON;                      // turn on amber LED    
        ww_put_data(0x121);
        ww_put_data(0x006);                 // move the carrier horizontally
        ww_put_data(0x000);                 // bit 7 is cleared for right to left direction
        ww_put_data(0x001);                 // one microspace
        amberLED = OFF;                     // turn off amber LED  
        --uSpaceCount;
    }
}

// To return the carrier to the left margin, the Wheelwriter requires an eleven bit number which 
// indicates the number of micro spaces to move to reach the left margin. The upper three bits of
// the 11-bit number are sent as the 3rd word of the command, and lower 8 bits are sent as the 4th
// word. Bit 7 of the 3rd word is cleared to indicate carriage movement left direction.
// resets micro space count back to zero.
void ww_carriage_return(void) {
    amberLED = ON;                          // turn on amber LED    
    ww_put_data(0x121);
    ww_put_data(0x006);                     // move the carrier horizontallly
    ww_put_data((uSpaceCount>>8)&0x007);    // bit 7 is cleared for right to left direction, bits 0-2 = upper 3 bits of micro spaces to left margin
    ww_put_data(uSpaceCount&0xFF);          // lower 8 bits of micro spaces to left margin
    uSpaceCount = 0;                        // clear count
    amberLED = OFF;                         // turn off amber LED  
}

// ww_spins the printwheel as a visual and audible indication
void ww_spin(void) {
    amberLED = ON;                          // turn on amber LED    
    ww_put_data(0x121);
    ww_put_data(0x007);
    amberLED = OFF;                         // turn off amber LED  
}

// horizontal tab number of "spaces". updates micro space count.
void ww_horizontal_tab(unsigned char spaces) {
    unsigned int s;

    s = spaces*uSpacesPerChar;              // number of microspaces to move right
    amberLED = ON;                          // turn on amber LED    
    ww_put_data(0x121);
    ww_put_data(0x006);                     // move the carrier horizontally
    ww_put_data(((s>>8)&0x007)|0x80);       // bit 7 is set for left to right direction, bits 0-2 = upper 3 bits of micro spaces to move right
    ww_put_data(s&0xFF);                    // lower 8 bits of micro spaces to move right
    uSpaceCount += s;                       // update micro space count
    amberLED = OFF;                         // turn off amber LED  
}

// backspaces and erases "letter". updates micro space count.
// Note: erasing bold or underlined characters or characters on lines other than the current line not implemented yet.
void ww_erase_letter(unsigned char letter) {
    amberLED = ON;                          // turn on amber LED    
    ww_put_data(0x121);
    ww_put_data(0x006);                     // move the carrier horizontally
    ww_put_data(0x000);                     // bit 7 is cleared for right to left direction
    ww_put_data(uSpacesPerChar);            // number of micro spaces to move left    
    ww_put_data(0x121);
    ww_put_data(0x004);                     // print on correction tape
    ww_put_data(printwheelChar[letter-0x20]);
    ww_put_data(uSpacesPerChar);            // number of micro spaces to move right
    uSpaceCount -= uSpacesPerChar;          // update the micro space count
    amberLED = OFF;                         // turn off amber LED  
}

// paper up one line
void ww_linefeed(void) {
    amberLED = ON;                          // turn on amber LED    
    ww_put_data(0x121);
    ww_put_data(0x005);                     // vertical movement
    ww_put_data(0x080|uLinesPerLine);       // bit 7 is set to indicate paper up direction, bits 0-4 indicate number of microlines for 1 full line
    amberLED = OFF;                         // turn off amber LED  
}    

// paper down one line
void ww_reverse_linefeed(void) {
    amberLED = ON;                          // turn on amber LED    
    ww_put_data(0x121);
    ww_put_data(0x005);                     // vertical movement
    ww_put_data(0x000|uLinesPerLine);       // bit 7 is cleared to indicate paper down direction, bits 0-4 indicate number of microlines for 1 full line
    amberLED = OFF;                         // turn off amber LED  
}    

// paper up 1/2 line
void ww_paper_up(void) {                    
    amberLED = ON;                          // turn on amber LED    
    ww_put_data(0x121);
    ww_put_data(0x005);                     // vertical movement
    ww_put_data(0x080|(uLinesPerLine>>1));  // bit 7 is set to indicate up direction, bits 0-3 indicate number of microlines for 1/2 line
    amberLED = OFF;                         // turn off amber LED  
}

// paper down 1/2 line
void ww_paper_down(void) {
    amberLED = ON;                          // turn on amber LED    
    ww_put_data(0x121);
    ww_put_data(0x005);                     // vertical movement
    ww_put_data(0x000|(uLinesPerLine>>1));  // bit 7 is cleared to indicate down direction, bits 0-3 indicate number of microlines for 1/2 full line
    amberLED = OFF;                         // turn off amber LED  
}

// paper up 1/8 line
void ww_micro_up(void) {
    amberLED = ON;                          // turn on amber LED    
    ww_put_data(0x121);
    ww_put_data(0x005);                     // vertical movement
    ww_put_data(0x080|(uLinesPerLine>>3));  // bit 7 is set to indicate up direction, bits 0-3 indicate number of microlines for 1/8 full line or 1/48"
    amberLED = OFF;                         // turn off amber LED  
}

// paper down 1/8 line
void ww_micro_down(void) {
    amberLED = ON;                          // turn on amber LED    
    ww_put_data(0x121);
    ww_put_data(0x005);                     // vertical movement
    ww_put_data(0x000|(uLinesPerLine>>3));  // bit 7 is cleared to indicate down direction, bits 0-3 indicate number of microlines for 1/8 full line or 1/48"
    amberLED = OFF;                         // turn off amber LED  
}

//-----------------------------------------------------------
// Sends the code for the letter to be printed the Wheelwriter. 
// Handles bold, continuous and multiple word underline printing.
// Carrier moves to the right by uSpacesPerChar.
// Increases the micro space count by uSpacesPerChar for each letter printed.
//-----------------------------------------------------------
void ww_print_letter(unsigned char letter,attribute) {
    amberLED = ON;                          // turn on amber LED    
    ww_put_data(0x121);
    ww_put_data(0x003);
    ww_put_data(printwheelChar[letter-0x20]);// ascii character (-0x20) as index to printwheel table    
    if ((attribute & 0x06) && ((letter!=0x20) || (attribute & 0x02))){// if underlining AND the letter is not a space OR continuous underlining is on
        ww_put_data(0x000);                 // advance zero micro spaces
        ww_put_data(0x121);
        ww_put_data(0x003);
        ww_put_data(0x04F);                 // print '_' underscore
    }
    if (attribute & 0x01) {                 // if the bold bit is set   
        ww_put_data(0x001);                 // advance carriage by one micro space
        ww_put_data(0x121);
        ww_put_data(0x003);
        ww_put_data(printwheelChar[letter-0x20]);// re-print the character offset by one micro space
        ww_put_data((uSpacesPerChar)-1);    // advance carriage the remaining micro spaces
    } 
    else { // not boldprint
        ww_put_data(uSpacesPerChar);      
    }
    uSpaceCount += uSpacesPerChar;          // update the micro space count
    if (uSpaceCount > 1319) {               // 1 inch from right stop   
        ww_carriage_return();
    }
    amberLED = OFF;                         // turn off amber LED  
}

   