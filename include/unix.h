/* This file contains some macros to replace various library functions
	that are not available in the Unix version of the PEW */

#ifndef UNIX
#define INCLUDED_UNIX		/* Don't include if not Unix */
#endif

#ifndef INCLUDED_UNIX
#define INCLUDED_UNIX

/* The following are copied from screen.h */

/* Attribute Classes */

#define SCR_BLANK_ATTR  0x00	/* Black on black (CGA); blank (mono)     */
#define SCR_ULINE_ATTR  0x01	/* Blue on black  (CGA); underlined (mono) */
#define SCR_NORML_ATTR  0x07	/* White on black (CGA); normal (mono)    */
#define SCR_INVRS_ATTR  0x70	/* Black on white (CGA); inverse (mono)   */

/* Colours 0-7 can be used for background only */

#define BLACK           0
#define BLUE            1
#define GREEN           2
#define CYAN            3
#define RED             4
#define MAGENTA         5
#define BROWN           6
#define LIGHTGRAY       7
#define DARKGRAY        8
#define LIGHTBLUE       9
#define LIGHTGREEN      10
#define LIGHTCYAN       11
#define LIGHTRED        12
#define LIGHTMAGENTA    13
#define YELLOW          14
#define WHITE           15

#define Scr_ColorAttr(fg,bg)    ((uchar)(((bg)<<4) + (fg)))

/* Attribute Modifiers; bitwise-OR with attribute class */

#define SCR_BLINK_CHAR  0x80
#define SCR_BOLD_CHAR   0x08

/*****************/
/* Cursor Shapes */
/*****************/

enum cursorshapes
{
    SCR_BLOKCURSOR = 1, SCR_LINECURSOR
};

/******************/
/* Text Positions */
/******************/

enum positions
{
    SCR_LEFT = 1, SCR_RIGHT, SCR_TOP, SCR_BOTTOM, SCR_CENTRE
};

/****************/
/* Window Flags */
/****************/

#define WIN_NOBORDER    0	/* No border            */
#define WIN_BORDER      1	/* Bit mask for border  */
#define WIN_BLOKCURSOR  2	/* Block cursor mode    */
#define WIN_CURSOROFF   4	/* Is cursor on?        */
#define WIN_SCROLL      8	/* Scroll mode          */
#define WIN_WRAP        0x10	/* Line wrap mode       */
#define WIN_CLIP        0x20	/* Border 'clip' mode   */
#define WIN_NONDESTRUCT 0x40	/* Save window preimage? */
#define WIN_ZOOM        0x80	/* Zoom type windows?   */
#define WIN_TILED       0x100	/* Maintain active image */
#define WIN_FLASH       0x200	/* Tiled, plus restores */
 /*
  * preimage when deactivated; cannot be NONDESTRUCT
  */


#define WINPTR			int
#define Win_SetAttribute(a)
#define Win_Activate(w)
#define Win_PutChar(c)		putchar(c)
#define Win_Error(w,l,b,b1,b2)	fprintf(stderr,"Error (%s) %s\n",l,b),0
#define Win_Printf		printf
#define Win_PutString		puts

#endif				/* INCLUDED_UNIX */
