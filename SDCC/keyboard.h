//  for the Small Device C Compiler (SDCC)

#ifndef __KEYBOARD_H__
#define __KEYBOARD_H__

void kb_isr(void) __interrupt(2) __using(2);
void kb_init(void);
__bit kb_scancode_avail(void);
unsigned char kb_get_scancode(void);
unsigned char kb_send_cmd(unsigned char kbcmd);
unsigned char kb_decode_scancode(unsigned char scancode);
__bit kb_ctrl_pressed(void);
__bit kb_alt_pressed(void);
__bit kb_shift_pressed(void);

#endif

