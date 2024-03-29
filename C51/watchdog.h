// For the Keil C51 compiler.

#ifndef __WATCHDOG_H__
#define __WATCHDOG_H__

void wd_clr_flags(void);  
void wd_enable_watchdog(void);
void wd_disable_watchdog(void);
void wd_reset_watchdog(void);
void wd_init_watchdog(unsigned char interval);

#endif
