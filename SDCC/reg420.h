//  for the Small Device C Compiler (SDCC)

#ifndef __REG420_H__
#define __REG420_H__

/*  BYTE Registers  */
__sfr __at(0x80) P0;
__sfr __at(0x81) SP;
__sfr __at(0x82) DPL;
__sfr __at(0x83) DPH;
__sfr __at(0x84) DPL1;
__sfr __at(0x85) DPH1;
__sfr __at(0x86) DPS;
__sfr __at(0x87) PCON;
__sfr __at(0x88) TCON;
__sfr __at(0x89) TMOD;
__sfr __at(0x8A) TL0;
__sfr __at(0x8B) TL1;
__sfr __at(0x8C) TH0;
__sfr __at(0x8D) TH1;
__sfr __at(0x8E) CKCON;
__sfr __at(0x90) P1;
__sfr __at(0X91) EXIF;
__sfr __at(0x96) CKMOD;
__sfr __at(0x98) SCON0;
__sfr __at(0x99) SBUF0;
__sfr __at(0x9D) ACON;
__sfr __at(0xA0) P2;
__sfr __at(0xA8) IE;
__sfr __at(0xA9) SADDR0;
__sfr __at(0xAA) SADDR1;
__sfr __at(0xB0) P3;
__sfr __at(0xB1) IP1;
__sfr __at(0xB8) IP0;
__sfr __at(0xB9) SADEN0;
__sfr __at(0xBA) SADEN1;
__sfr __at(0xC0) SCON1;
__sfr __at(0xC1) SBUF1;
__sfr __at(0xC2) ROMSIZ;
__sfr __at(0xC4) PMR;
__sfr __at(0xC5) STATUS;
__sfr __at(0xC7) TA;
__sfr __at(0xC8) T2CON;
__sfr __at(0xC9) T2MOD;
__sfr __at(0xCA) RCAP2L;
__sfr __at(0xCB) RCAP2H;
__sfr __at(0xCC) TL2;
__sfr __at(0xCD) TH2;
__sfr __at(0xD0) PSW;
__sfr __at(0xD5) FCNTL;
__sfr __at(0xD6) FDATA;
__sfr __at(0xD8) WDCON;
__sfr __at(0xE0) ACC;
__sfr __at(0xE8) EIE;
__sfr __at(0xF0) B;
__sfr __at(0xF1) EIP1;
__sfr __at(0xF8) EIP0;

/*                 */
/*  BIT Registers  */
/*                 */
 
/* P0 */
__sbit __at (0x80) P0_0 ;
__sbit __at (0x81) P0_1 ;
__sbit __at (0x82) P0_2 ;
__sbit __at (0x83) P0_3 ;
__sbit __at (0x84) P0_4 ;
__sbit __at (0x85) P0_5 ;
__sbit __at (0x86) P0_6 ;
__sbit __at (0x87) P0_7 ;

/*  TCON */
__sbit __at(0x88) IT0;
__sbit __at(0x89) IE0;
__sbit __at(0x8A) IT1;
__sbit __at(0x8B) IE1;
__sbit __at(0x8C) TR0;
__sbit __at(0x8D) TF0;
__sbit __at(0x8E) TR1;
__sbit __at(0x8F) TF1;

/* P1 */
__sbit __at (0x90) P1_0 ;
__sbit __at (0x91) P1_1 ;
__sbit __at (0x92) P1_2 ;
__sbit __at (0x93) P1_3 ;
__sbit __at (0x94) P1_4 ;
__sbit __at (0x95) P1_5 ;
__sbit __at (0x96) P1_6 ;
__sbit __at (0x97) P1_7 ;


/* SCON0 */ 
__sbit __at(0x98) RI;
__sbit __at(0x99) TI;
__sbit __at(0x9A) RB8;
__sbit __at(0x9B) TB8;
__sbit __at(0x9C) REN;
__sbit __at(0x9D) SM2;
__sbit __at(0x9E) SM1;
__sbit __at(0x9F) SM0;

__sbit __at(0x98) RI_0;
__sbit __at(0x99) TI_0;
__sbit __at(0x9A) RB8_0;
__sbit __at(0x9B) TB8_0;
__sbit __at(0x9C) REN_0;
__sbit __at(0x9D) SM2_0;
__sbit __at(0x9E) SM1_0;
__sbit __at(0x9F) SM0FE_0;

/* IE	 */
__sbit __at(0xA8) EX0;
__sbit __at(0xA9) ET0;
__sbit __at(0xAA) EX1;
__sbit __at(0xAB) ET1;
__sbit __at(0xAC) ES0;
__sbit __at(0xAD) ET2;
__sbit __at(0xAE) ES1;
__sbit __at(0xAF) EA;

/* P3   */
__sbit __at (0xb0) RXD0;
__sbit __at (0xb1) TXD0;
__sbit __at (0xb2) INT0;
__sbit __at (0xb3) INT1;
__sbit __at (0xb4) T0;
__sbit __at (0xb5) T1;
__sbit __at (0xb6) WR;
__sbit __at (0xb7) RD;


/*  IP0	*/
__sbit __at(0xB8) LPX0;
__sbit __at(0xB9) LPT0;
__sbit __at(0xBA) LPX1;
__sbit __at(0xBB) LPT1;
__sbit __at(0xBC) LPS0;
__sbit __at(0xBD) LPT2;
__sbit __at(0xBE) LPS1;

/*  SCON1  */
__sbit __at(0xC0) RI1;
__sbit __at(0xC1) TI1;
__sbit __at(0xC2) RB81;
__sbit __at(0xC3) TB81;
__sbit __at(0xC4) REN1;
__sbit __at(0xC5) SM21;
__sbit __at(0xC6) SM11;
__sbit __at(0xC7) SM01;

__sbit __at(0xC0) RI_1;
__sbit __at(0xC1) TI_1;
__sbit __at(0xC2) RB8_1;
__sbit __at(0xC3) TB8_1;
__sbit __at(0xC4) REN_1;
__sbit __at(0xC5) SM2_1;
__sbit __at(0xC6) SM1_1;
__sbit __at(0xC7) SM0FE_1;

/* T2CON */
__sbit __at(0xC8) CP_RL2;
__sbit __at(0xC9) C_T2;
__sbit __at(0xCA) TR2;
__sbit __at(0xCB) EXEN2;
__sbit __at(0xCC) TCLK;
__sbit __at(0xCD) RCLK;
__sbit __at(0xCE) EXF2;
__sbit __at(0xCF) TF2;

/* PSW	*/
__sbit __at(0xD0) P;
__sbit __at(0xD1) F1;
__sbit __at(0xD2) OV;
__sbit __at(0xD3) RS0;
__sbit __at(0xD4) RS1;
__sbit __at(0xD5) F0;
__sbit __at(0xD6) AC;
__sbit __at(0xD7) CY;

/*  WDCON   */
__sbit __at(0xD8) RWT;
__sbit __at(0xD9) EWT;
__sbit __at(0xDA) WTRF;
__sbit __at(0xDB) WDIF;
__sbit __at(0xDC) PFI;
__sbit __at(0xDD) EPFI;
__sbit __at(0xDE) POR;
__sbit __at(0xDF) SMOD_1;

/*  EIE   */
__sbit __at(0xE8) EX2;
__sbit __at(0xE9) EX3;
__sbit __at(0xEA) EX4;
__sbit __at(0xEB) EX5;
__sbit __at(0xEC) EWDI;

/*  EIP0   */
__sbit __at(0xF8) LPX2;
__sbit __at(0xF9) LPX3;
__sbit __at(0xFA) LPX4;
__sbit __at(0xFB) LPPX5;
__sbit __at(0xFC) LPWDI;

#endif
