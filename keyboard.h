void kb_init(void);
bit kb_scancode_avail(void);
unsigned char kb_get_scancode(void);
unsigned char kb_send_cmd(unsigned char kbcmd);
unsigned char kb_decode_scancode(unsigned char scancode);
bit kb_ctrl_pressed(void);
bit kb_alt_pressed(void);
bit kb_shift_pressed(void);
