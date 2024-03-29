// For the SDCC compiler.

#ifndef __SCANCODES_H__
#define __SCANCODES_H__

unsigned char __code unshifted[128] = {
0,       // 00
0,       // 01 - F9
0,       // 02
0,       // 03 - F5
0,       // 04 - F3
0,       // 05 - F1
0,       // 06 - F2
0,       // 07 - F12
0,       // 08 
0,       // 09 - F10
0,       // 0a - F8
0,       // 0b - F6
0,       // 0c - F4
9,       // 0d - Tab
'`',     // 0e
0,       // 0f 
0,       // 10
0,       // 11 - Left Alt
0,       // 12 - Left Shift
0,       // 13
0,       // 14 - Left Control
'q',     // 15
'1',     // 16
0,       // 17
0,       // 18
0,       // 19
'z',     // 1a
's',     // 1b
'a',     // 1c
'w',     // 1d
'2',     // 1e
0,       // 1f
0,       // 20
'c',     // 21
'x',     // 22
'd',     // 23
'e',     // 24
'4',     // 25
'3',     // 26
0,       // 27
0,       // 28
' ',     // 29 - Space
'v',     // 2a
'f',     // 2b
't',     // 2c
'r',     // 2d
'5',     // 2e
0,       // 2f
0,       // 30
'n',     // 31
'b',     // 32
'h',     // 33
'g',     // 34
'y',     // 35
'6',     // 36
0,       // 37
0,       // 38
0,       // 39
'm',     // 3a
'j',     // 3b
'u',     // 3c
'7',     // 3d
'8',     // 3e
0,       // 3f
0,       // 40
',',     // 41
'k',     // 42
'i',     // 43
'o',     // 44
'0',     // 45
'9',     // 46
0,       // 47
0,       // 48
'.',     // 49
'/',     // 4a
'l',     // 4b
';',     // 4c
'p',     // 4d
'-',     // 4e
0,       // 4f
0,       // 50
0,       // 51
39,      // 52
0,       // 53
'[',     // 54
'=',     // 55
0,       // 56
0,       // 57
0,       // 58 - Caps Lock
0,       // 59 - Right Shift   
13,      // 5a - Enter
']',     // 5b
0,       // 5c
92,      // 5d - "\"
0,       // 5e
0,       // 5f
0,       // 60
0,       // 61
0,       // 62
0,       // 63
0,       // 64
0,       // 65
8,       // 66 - Backspace
0,       // 67
0,       // 68
0,       // 69 - Keypad 1
0,       // 6a
0,       // 6b - Keypad 4
0,       // 6c - Keypad 7
0,       // 6d
0,       // 6e
0,       // 6f
0,       // 70 - Keypad 0
127,     // 71 - Keypad .
0,       // 72 - Keypad 2
0,       // 73 - Keypad 5
0,       // 74 - Keypad 6
0,       // 75 - Keypad 8
27,      // 76 - Escape
0,       // 77 - Numlock
0,       // 78 - F11
'+',     // 79 - Keypad +
0,       // 7a - Keypad 3
'-',     // 7b - Keypad -
'*',     // 7c - Keypad *
0,       // 7d - Keypad 9
0,       // 7e - Scroll Lock
0};      // 7f

unsigned char __code shifted[128] = { 
0,       // 00
0,       // 01 - F9
0,       // 02 - F7
0,       // 03 - F5
0,       // 04 - F3
0,       // 05 - F1
0,       // 06 - F2
0,       // 07 - F12
0,       // 08 -
0,       // 09 - F10
0,       // 0a - F8
0,       // 0b - F6
0,       // 0c - F4
9,       // 0d - Tab
'~',     // 0e -
0,       // 0f -
0,       // 10
0,       // 11 - Left Alt
0,       // 12 - Left Shift
0,       // 13
0,       // 14 - Left Control
'Q',     // 15
'!',     // 16
0,       // 17
0,       // 18
0,       // 19
'Z',     // 1a
'S',     // 1b
'A',     // 1c
'W',     // 1d
'@',     // 1e
0,       // 1f
0,       // 20
'C',     // 21
'X',     // 22
'D',     // 23
'E',     // 24
'$',     // 25
'#',     // 26
0,       // 27
0,       // 28
' ',     // 29
'V',     // 2a
'F',     // 2b
'T',     // 2c
'R',     // 2d
'%',     // 2e
0,       // 2f
0,       // 30
'N',     // 31
'B',     // 32
'H',     // 33
'G',     // 34
'Y',     // 35
'^',     // 36
0,       // 37
0,       // 38
0,       // 39
'M',     // 3a
'J',     // 3b
'U',     // 3c
'&',     // 3d
'*',     // 3e
0,       // 3f
0,       // 40
'<',     // 41
'K',     // 42
'I',     // 43
'O',     // 44
')',     // 45
'(',     // 46
0,       // 47
0,       // 48
'>',     // 49
'?',     // 4a
'L',     // 4b
':',     // 4c
'P',     // 4d
'_',     // 4e
0,       // 4f
0,       // 50
0,       // 51
34,      // 52
0,       // 53
'{',     // 54
'+',     // 55
0,       // 56
0,       // 57
0,       // 58 - Caps Lock
0,       // 59
13,      // 5a
'}',     // 5b
0,       // 5c
'|',     // 5d
0,       // 5e
0,       // 5f
0,       // 60
0,       // 61
0,       // 62
0,       // 63
0,       // 64
0,       // 65
8,       // 66
0,       // 67
0,       // 68
0,       // 69
0,       // 6a
0,       // 6b
0,       // 6c
0,       // 6d
0,       // 6e
0,       // 6f
0,       // 70
127,     // 71 Keypad Delete
0,       // 72
0,       // 73
0,       // 74
0,       // 75
27,      // 76 - Escape
0,       // 77 - Numlock
0,       // 78
'+',     // 79
0,       // 7a
'_',     // 7b
'*',     // 7c
0,       // 7d
0,       // 7e
0};      // 7f

#endif