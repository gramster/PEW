/************************/
/* Name: screen.h	*/
/*			*/
/* Author: G. Wheeler	*/
/*			*/
/* Date: Dec 1988	*/
/*			*/
/* Version: 1.0		*/
/*  (but based on older */
/*   bits of code)	*/
/*			*/
/************************/

#ifndef INCLUDED_SCREEN
#define INCLUDED_SCREEN

#ifndef Malloc
#define Malloc 		malloc
#define Free 		free
#define Farmalloc	farmalloc
#define Farfree		farfree
#define Far		far
#endif

/* The following definitions are to allow the window library to
	be reconfigured to use farmallocs if desired */

#ifdef UNIX
#define _wfar
#define _wmalloc	malloc
#define _wfree(p)	if (p!=NULL) free(p)
#else
#ifdef USEFARMALLOC
#define _wfar		Far
#define _wmalloc	Farmalloc
#define _wfree(p)	if (p!=(void far *)NULL) Farfree(p)
#else
#ifdef USEMALLOC
#define _wfar
#define _wmalloc	Malloc
#define _wfree(p)	if (p!=(void *)NULL) Free(p)
#else
#define _wfar		Far
#define _wmalloc(m)	Mem_Malloc(m,99)
#define _wfree(p)	Mem_Free(p)
#endif
#endif
#endif

/*********************/
/* Screen Attributes */
/*********************/

/* Attribute Classes */

#define SCR_BLANK_ATTR	0x00	/* Black on black (CGA); blank (mono)	 */
#define SCR_ULINE_ATTR	0x01	/* Blue on black  (CGA); underlined (mono) */
#define SCR_NORML_ATTR	0x07	/* White on black (CGA); normal (mono) 	 */
#define SCR_INVRS_ATTR	0x70	/* Black on white (CGA); inverse (mono)	 */

/* Colours 0-7 can be used for background only */

#define BLACK		0
#define BLUE		1
#define GREEN		2
#define CYAN		3
#define RED		4
#define MAGENTA		5
#define BROWN		6
#define LIGHTGRAY	7
#define DARKGRAY	8
#define LIGHTBLUE	9
#define LIGHTGREEN	10
#define LIGHTCYAN	11
#define LIGHTRED	12
#define LIGHTMAGENTA	13
#define YELLOW		14
#define WHITE		15

#define Scr_ColorAttr(fg,bg)	((uchar)(((bg)<<4) + (fg)))

/* Attribute Modifiers; bitwise-OR with attribute class */

#define SCR_BLINK_CHAR	0x80
#define SCR_BOLD_CHAR	0x08

/* Macro to create 16-bit screen words from 8-bit char, attr pairs */

/*
#define SCR_CHAR(c,a)	((((ushort)a)<<8) + ((ushort)c))
*/
#define SCR_CHAR(c,a)	((a)*256+((unsigned)(c&0xFF)))

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

/************************/
/* Basic Constants	*/
/************************/

#define SCR_COLUMNS	80
#define SCR_LINES	24
#define SCR_MONOBASE	0xB0000000L
#define SCR_CGABASE	0xB8000000L

/****************/
/* Window Flags	*/
/****************/

#define WIN_NOBORDER	0	/* No border		 */
#define WIN_BORDER	1	/* Bit mask for border	 */
#define WIN_BLOKCURSOR	2	/* Block cursor mode	 */
#define WIN_CURSOROFF	4	/* Is cursor on?	 */
#define WIN_SCROLL	8	/* Scroll mode		 */
#define WIN_WRAP	0x10	/* Line wrap mode	 */
#define WIN_CLIP	0x20	/* Border 'clip' mode	 */
#define WIN_NONDESTRUCT	0x40	/* Save window preimage? */
#define WIN_ZOOM 	0x80	/* Zoom type windows?	 */
#define WIN_TILED	0x100	/* Maintain active image */
#define WIN_FLASH	0x200	/* Tiled, plus restores */
 /*
  * preimage when deactivated; cannot be NONDESTRUCT
  */

/************************/
/* Type definitions	*/
/************************/

typedef struct Window
{
    short x, y, w, h;		/* Top left corner and size	 */
    void _wfar *preimage;	/* Pointer to save area for	 */
    /* non-destructive windows 	 */
    void _wfar *current;	/* Pointer to save area for	 */
    /* current window.		 */
    ushort far *start;		/* Pointer to top-left corner	 */
    /* of video memory		 */
    short r, c;			/* Current cursor position 	 */
    unsigned char ba;		/* Border attribute		 */
    unsigned char ca;		/* Current attribute		 */
    unsigned char ia;		/* Inverse attribute		 */
    ushort flags;		/* Various characteristics	 */
    ushort tabsize;		/* Tab stop size		 */
}
    WINDOW;

typedef WINDOW _wfar *WINPTR;

/*********************/
/* Common use macros */
/*********************/

#ifdef UNIX
#define WIN_X		(0)
#define WIN_Y		(0)
#define WIN_WIDTH	(0)
#define WIN_HEIGHT	(0)
#define WIN_ROW		(0)
#define WIN_COL		(0)
#define WIN_START	(0)
#else
#define WIN_X		(Win_Current->x)
#define WIN_Y		(Win_Current->y)
#define WIN_WIDTH	(Win_Current->w)
#define WIN_HEIGHT	(Win_Current->h)
#define WIN_ROW		(Win_Current->r)
#define WIN_COL		(Win_Current->c)
#define WIN_START	(Win_Current->start)
#endif

/*************************************/
/* Screen & Window macro 'functions' */
/*************************************/

#define Win_SetTabs(t)		Win_Current->tabsize = t
#define Win_GetTabs()		(Win_Current->tabsize)

#ifdef UNIX

#define Win_SetAttribute(a)
#define Win_SetInverse(w,a)
#define Win_Width(W)		W
#define Win_Attribute(w)	w
#define Win_InverseAttr(w)	w
#define Win_BorderAttr(w)	w

#else

#define Win_SetAttribute(a)	Win_Current->ca = a
#define Win_SetInverse(w,a)	w->ia = a
#define Win_Width(W)		W->w
#define Win_Attribute(w)	w->ca
#define Win_InverseAttr(w)	w->ia
#define Win_BorderAttr(w)	w->ba

#endif

#define Win_PlaceCursor()	Scr_PutCursor(WIN_Y+WIN_ROW, WIN_X+WIN_COL)
#define Win_BlockCursor()	Win_SetFlags(WIN_BLOKCURSOR);
#define Win_LineCursor()	Win_ClearFlags(WIN_BLOKCURSOR);
#define Scr_SaveScreen(p)	p = Scr_BlockSave(scr_base,NULL,25,80)
#define Scr_RestoreScreen(p)	Scr_BlockRestore(p,scr_base,25,80)
#define Scr_ClearAll()		clrscr()

/********************/
/* Global variables */
/********************/

#ifndef GLOBAL
#define GLOBAL extern
#endif

GLOBAL WINPTR Win_Current;	/* Current active window	 */
GLOBAL ushort scr_segbase;	/* base segment of video mamory	 */
GLOBAL ushort far *scr_base;	/* base address of video memory */

/******************************/
/* Screen function prototypes */
/******************************/

#ifndef UNIX
void Scr_InstallFatal(void (*f) (char *));
ushort _wfar *
Scr_BlockSave(ushort far * from, ushort _wfar * to,
	      ushort rows, ushort cols);
void 
Scr_BlockRestore(ushort _wfar * from, ushort far * to,
		 ushort rows, ushort cols);
void Scr_InitSystem(void);
void Scr_EndSystem(void);
void Scr_SetCursorShape(enum cursorshapes shape);
void Scr_PutCursor(int row, int col);
void Scr_CursorOn(void);
void Scr_CursorOff(void);
int Scr_GetCursor(int *row, int *col);
void Scr_PutChar(int row, int col, char c, ushort attr);
void Scr_PutString(STRING str, ushort attr);
void Scr_SetAttribute(ushort attr);
void Scr_PutC(char c);
void Scr_PutS(STRING str);

/******************************/
/* Window Function Prototypes */
/******************************/

void Win_1LineBorderSet(void);
void Win_2LineBorderSet(void);
void 
Win_BorderSet(char tl, char tr, char bl, char br,
	      char th, char bh, char rv, char lv);
WINPTR 
Win_Make(ushort flags, int left, int top, int width, int height,
	 unsigned char boxattr, unsigned char attr);
void 
Win_DrawBorder(ushort far * start, ushort width,
	       ushort height, uchar attr);
void Win_PutTitle(STRING title, enum positions place, enum positions align);
void Win_Free(WINPTR win);
void Win_PutCursor(int row, int col);
void Win_Activate(WINPTR win);
void Win_SetFlags(ushort mask);
void Win_ClearFlags(ushort mask);
void Win_PutChar(unsigned char c);
void Win_FastPutS(int row, int col, STRING str);
void Win_PutString(STRING str);
void Win_PutMarkedStr(int line, STRING txt, short bstart, short bend);
void Win_Clear(void);
void Win_ClearLine(int line);
void Win_DelLine(int line);
void Win_InsLine(int line);
void Win_InsColumn(int row, int col, int height);
void Win_InsChar(void);
void Win_DelColumn(int row, int col, int height);
void Win_DelChar(void);
void Win_ScrollUp(void);
void Win_ScrollDown(void);
void Win_ScrollLeft(void);
void Win_ScrollRight(void);
void 
Win_PutAttribute(unsigned char attr, int fromx, int fromy,
		 int w, int h);
ushort 
Win_Error(WINPTR errwin, STRING class, STRING message,
	  BOOLEAN justwarn, BOOLEAN wait);
void Win_Printf(STRING format,...);

#else				/* Unix stuff - mostly we just map window
				 * function onto stdio */

#define Scr_CursorOn()
#define Scr_CursorOff()
#define Scr_EndSystem()
#define Scr_InitSystem()
#define Scr_PutChar(r,c,ch,attr)
#define Scr_PutCursor(r,c)
#define Scr_PutS(s)
#define Scr_PutString(s,a)

#define Win_Make(f,l,t,w,h,b,a)	((WINPTR)-1)
#define Win_Activate(w)
#define Win_PutChar(c)          putchar(c)
#define Win_Error(w,l,b,b1,b2)  fprintf(stderr,"Error (%s) %s\n",l,b),0
#define Win_Printf              printf
#define Win_PutString(s)        puts(s)
#define Win_Clear()
#define Win_ClearLine(l)
#define Win_Free(w)
#define Win_PutCursor(r,c)
#define Win_1LineBorderSet()
#define Win_FastPutS(r,c,s)	puts(s)
#define Win_PutTitle(t,p,a)
#define Win_PutAttribute(a,x,y,w,h)

#endif

#endif				/* INCLUDED_SCREEN */
