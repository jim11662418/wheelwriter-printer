// For the Keil C51 compiler.

#ifndef __WHEELWRITER_H__
#define __WHEELERITER_H__

void ww_print_letter(unsigned char letter,attribute);
void ww_backspace(void);                        
void ww_micro_backspace(void);
void ww_space(void);
void ww_carriage_return(void);
void ww_spin(void);
void ww_horizontal_tab(unsigned char spaces);
void ww_erase_letter(unsigned char letter);
void ww_linefeed(void);
void ww_reverse_linefeed(void);
void ww_paper_up(void);                    
void ww_paper_down(void);
void ww_micro_up(void);
void ww_micro_down(void);
void ww_init(void);
void ww_put_data(unsigned int wwCommand);
bit ww_data_avail(void);
unsigned int ww_get_data(void);

#endif