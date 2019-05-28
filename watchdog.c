#include <reg420.h>

//-----------------------------------------------------------
// clear watchdog timer and POR flags
//-----------------------------------------------------------
void wd_clr_flags(void) {  
	TA = 0xAA;						// timed access
	TA = 0x55;					   
	POR = 0;  						// clear power on reset flag for next time
	TA = 0xAA;						// timed access
	TA = 0x55;
	WTRF = 0; 						// clear watchdog timer reset flag for next time
}

//-----------------------------------------------------------
// enable watchdog timer reset
//-----------------------------------------------------------
void wd_enable_watchdog(void) {
	TA = 0xAA;						// timed access
	TA = 0x55;
    EWT =1;            				// enable watchdog timer reset
}

//-----------------------------------------------------------
// disable watchdog timer
//-----------------------------------------------------------
void wd_disable_watchdog(void) {
	TA = 0xAA;						// timed access
	TA = 0x55;
    EWT = 0;           				// disable watchdog timer reset
}

//-----------------------------------------------------------
// reset watchdog timer
//-----------------------------------------------------------
void wd_reset_watchdog(void) {
    TA = 0xAA;						// timed access
	TA = 0x55;
    RWT = 1;						// reset the watchdog timer
}

//-----------------------------------------------------------
// initialize watchdog timer, set watchdog interval
// 0: WD interval = (1/12MHz)*2^17 =   10.9 milliseconds
// 1: WD interval = (1/12MHz)*2^20 =   87.3 milliseconds
// 2: WD interval = (1/12MHz)*2^23 =  699.0 milliseconds
// 3: WD interval = (1/12MHz)*2^26 = 5592.4 milliseconds
//-----------------------------------------------------------
void wd_init_watchdog(unsigned char interval) {

    wd_enable_watchdog();           // enable watchdog reset
    switch (interval) {
        case 0:
            CKCON = (CKCON & 0x3F);
            break;
        case 1:
            CKCON = (CKCON & 0x3F) | 0x40;
            break;
        case 2:
            CKCON = (CKCON & 0x3F) | 0x80;
            break;
        case 3:
            CKCON = (CKCON | 0xC0);
            break;
        default:
            CKCON = (CKCON | 0xC0);
   }
   wd_reset_watchdog();              // reset watchdog timer
}
