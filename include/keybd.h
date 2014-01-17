/*********************************************************************
The GWLIBL library, its source code, and header files, are all
copyright 1987, 1988, 1989, 1990 by Graham Wheeler. All rights reserved.
**********************************************************************/

/************************************************/
/*                                              */
/* keybd.h                                      */
/* =======                                      */
/*                                              */
/* This file contains the 16-bit value returned */
/*	by the BIOS keyboard service. The lower */
/*	8 bits are the ASCII value for ASCII    */
/*	characters. The upper 8 bits are the    */
/*	keyboard scan code.                     */
/*                                              */
/************************************************/

#ifndef INCLUDED_KEYBD

#define INCLUDED_KEYBD

#define KB_F1		0x3B00
#define KB_F2		0x3C00
#define KB_F3		0x3D00
#define KB_F4		0x3E00
#define KB_F5		0x3F00
#define KB_F6		0x4000
#define KB_F7		0x4100
#define KB_F8		0x4200
#define KB_F9		0x4300
#define KB_F10		0x4400

#define KB_SHIFT_F1	0x5400
#define KB_SHIFT_F2	0x5500
#define KB_SHIFT_F3	0x5600
#define KB_SHIFT_F4	0x5700
#define KB_SHIFT_F5	0x5800
#define KB_SHIFT_F6	0x5900
#define KB_SHIFT_F7	0x5A00
#define KB_SHIFT_F8	0x5B00
#define KB_SHIFT_F9	0x5C00
#define KB_SHIFT_F10	0x5D00

#define KB_CTRL_F1	0x5E00
#define KB_CTRL_F2	0x5F00
#define KB_CTRL_F3	0x6000
#define KB_CTRL_F4	0x6100
#define KB_CTRL_F5	0x6200
#define KB_CTRL_F6	0x6300
#define KB_CTRL_F7	0x6400
#define KB_CTRL_F8	0x6500
#define KB_CTRL_F9	0x6600
#define KB_CTRL_F10	0x6700

#define KB_ALT_F1	0x6800
#define KB_ALT_F2	0x6900
#define KB_ALT_F3	0x6A00
#define KB_ALT_F4	0x6B00
#define KB_ALT_F5	0x6C00
#define KB_ALT_F6	0x6D00
#define KB_ALT_F7	0x6E00
#define KB_ALT_F8	0x6F00
#define KB_ALT_F9	0x7000
#define KB_ALT_F10	0x7100

#define KB_ALT_A	0x1E00
#define KB_ALT_B	0x3000
#define KB_ALT_C	0x2E00
#define KB_ALT_D	0x2000
#define KB_ALT_E	0x1200
#define KB_ALT_F	0x2100
#define KB_ALT_G	0x2200
#define KB_ALT_H	0x2300
#define KB_ALT_I	0x1700
#define KB_ALT_J	0x2400
#define KB_ALT_K	0x2500
#define KB_ALT_L	0x2600
#define KB_ALT_M	0x3200
#define KB_ALT_N	0x3100
#define KB_ALT_O	0x1800
#define KB_ALT_P	0x1900
#define KB_ALT_Q	0x1000
#define KB_ALT_R	0x1300
#define KB_ALT_S	0x1F00
#define KB_ALT_T	0x1400
#define KB_ALT_U	0x1600
#define KB_ALT_V	0x2F00
#define KB_ALT_W	0x1100
#define KB_ALT_X	0x2D00
#define KB_ALT_Y	0x1500
#define KB_ALT_Z	0x2C00
#define KB_ALT_0	0x8100
#define KB_ALT_1	0x7800
#define KB_ALT_2	0x7900
#define KB_ALT_3	0x7A00
#define KB_ALT_4	0x7B00
#define KB_ALT_5	0x7C00
#define KB_ALT_6	0x7D00
#define KB_ALT_7	0x7E00
#define KB_ALT_8	0x7F00
#define KB_ALT_9	0x8000
#define KB_ALT_MINUS    0x8200

#define KB_CTRL_A	0x0001
#define KB_CTRL_B	0x0002
#define KB_CTRL_C	0x0003
#define KB_CTRL_D	0x0004
#define KB_CTRL_E	0x0005
#define KB_CTRL_F	0x0006
#define KB_CTRL_G	0x0007
#define KB_CTRL_H	0x0008
#define KB_CTRL_I	0x0009
#define KB_CTRL_J	0x000A
#define KB_CTRL_K	0x000B
#define KB_CTRL_L	0x000C
#define KB_CTRL_M	0x000D
#define KB_CTRL_N	0x000E
#define KB_CTRL_O	0x000F
#define KB_CTRL_P	0x0010
#define KB_CTRL_Q	0x0011
#define KB_CTRL_R	0x0012
#define KB_CTRL_S	0x0013
#define KB_CTRL_T	0x0014
#define KB_CTRL_U	0x0015
#define KB_CTRL_V	0x0016
#define KB_CTRL_W	0x0017
#define KB_CTRL_X	0x0018
#define KB_CTRL_Y	0x0019
#define KB_CTRL_Z	0x001A
#define KB_CTRL_HOME	0x7700
#define KB_CTRL_END	0x7500
#define KB_CTRL_LEFT	0x7300
#define KB_CTRL_RIGHT	0x7400
#define KB_CTRL_PGUP	0x8400
#define KB_CTRL_PGDN	0x7600

#define KB_ESC		0x001B
#define KB_BACKSPC	0x0008
#define KB_TAB		0x0009
#define KB_NEWLINE	0x000A
#define KB_CRGRTN	0x000D
#define KB_BEEP		0x0007
#define KB_UP		0x4800
#define KB_DOWN		0x5000
#define KB_RIGHT	0x4D00
#define KB_LEFT		0x4B00
#define KB_HOME		0x4700
#define KB_END		0x4F00
#define KB_PGUP		0x4900
#define KB_PGDN		0x5100
#define KB_DEL		0x5300
#define KB_INS		0x5200

#ifndef GLOBAL
#define GLOBAL extern
#endif

GLOBAL short Kb_Record, Kb_PlayBack, Kb_MacNum;

#ifndef UNIX
ushort Kb_GetCursKey(void);
ushort Kb_RawGetC(void);
BOOLEAN Kb_RawLookC(ushort * rtn);
ushort Kb_GetChWithTimeout(short timeout);
ushort Kb_GetCh(void);
void Kb_Flush(void);
BOOLEAN Kb_LookC(ushort * rtn);
void Kb_RecordOn(short key);
void Kb_GetKeyName(STRING name, unsigned short scancode);

#else

#define Kb_GetCursKey()		0
#define Kb_RawGetC()		0
#define Kb_RawLookC(r)		FALSE
#define Kb_GetChWithTimeout(t)	0
#define Kb_GetCh()		0
#define Kb_Flush()
#define Kb_LookC(r)		FALSE
#define Kb_RecordOn(k)
#define Kb_GetKeyName(n,s)

#endif

#endif				/* INCLUDED_KEYBD */
