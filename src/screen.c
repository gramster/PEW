/*********************************************************************
The GWLIBL library, its source code, and header files, are all
copyright 1987, 1988, 1989, 1990 by Graham Wheeler. All rights reserved.
**********************************************************************/

/***************************************/
/*					*/
/* Name: screen.c			*/
/*					*/
/* Author: G. Wheeler			*/
/*					*/
/* Date:  March 1988 (version 0)	*/
/*	  June 1988 (version 1.0)	*/
/*	  December 1988 (version 1.1)	*/
/*	  version 1.2 currently		*/
/*					*/
/***************************************/

#include <conio.h>
#include "misc.h"
#include "screen.h"
#include "errors.h"
#include "keybd.h"
#include <stdarg.h>

ushort FAR *scr_base;		/* base address of video memory */
ushort scr_segbase;		/* 'Fast' segment for assembly  */

static ushort scr_maxcursor;	/* Maximum scan line for cursor - 7 for CGA;
				 * 13 for mono/EGA 	 */

static int DOSrow, DOScol,	/* Original cursor attributes	 */
    DOSshape;

WINPTR Win_Current = (WINPTR) NULL;

static char TLCORNER = 218, TRCORNER = 191,	/* Window Border drawing	 */
    BLCORNER = 192, BRCORNER = 217,	/* character set		 */
    THLINE = 196, BHLINE = 196, RVLINE = 179, LVLINE = 179;

static struct _WinList		/* Linked list of windows	 */
{
    WINPTR w;
    struct _WinList *next;
    short open;
}  *_ActiveWindowList = (struct _WinList *)NULL;

#define LISTELTSZ	sizeof(struct _WinList)


static ushort _wfar *_Scr_Preimage = (ushort _wfar *) NULL;


static short _Win_ErrorLevel = 0;	/* Prevent infinite recursion of
					 * errors by Win_Error if memory is
					 * exhausted */

/**********************************************************************

	Low level Functions

***********************************************************************/

ushort _wfar *
Scr_BlockSave(ushort far * from, ushort _wfar * to,
	      ushort rows, ushort cols)
{
    ushort _wfar *rtn;
    if (to == (ushort _wfar *) NULL)
	to = (ushort _wfar *) _wmalloc(2 * rows * cols);

    rtn = to;
    if (to != (ushort _wfar *) NULL)
	while (rows--)
	{
	    movedata(SEGMENT(from), OFFSET(from),
		     SEGMENT((void far *)to), OFFSET(to), 2 * cols);
	    from += 80;
	    to += cols;
	}
    else
	(void)Win_Error(NULL, "Scr_BlockSave", "malloc failed", FALSE, FALSE);
    return (rtn);
}

void 
Scr_BlockRestore(ushort _wfar * from, ushort far * to,
		 ushort rows, ushort cols)
{
    if (from != (ushort _wfar *) NULL)
    {
	while (rows--)
	{
	    movedata(SEGMENT((void far *)from), OFFSET(from),
		     SEGMENT(to), OFFSET(to), 2 * cols);
	    from += cols;
	    to += 80;
	}
    }
}


static void NEAR 
_Block_Clear(ushort ulx, ushort uly,
	     ushort brx, ushort bry, char attr)
{
    _CH = uly;
    _CL = ulx;
    _DH = bry;
    _DL = brx;
    _BH = attr;
    _AX = 0x0600;
    geninterrupt(0x10);
}

static void NEAR 
_BlockMove(ushort from_offset, ushort to_offset,
	   ushort width, ushort height)
{
    from_offset *= 2;
    to_offset *= 2;
    width *= 2;
    if (height > 0)
	while (height--)
	{
	    movedata(scr_segbase, from_offset, scr_segbase, to_offset, width);
	    from_offset += 160;
	    to_offset += 160;
	}
}


/*****************************************************************

	Initialisation and Termination Functions

******************************************************************/

static int system_initialised = FALSE;

void 
Scr_InitSystem(void)
{
    union REGS rgs;
    if (!system_initialised)
    {
	int86(0x11, &rgs, &rgs);/* BIOS equipment flag service */
	if ((rgs.x.ax & 0x0030) != 0x0030)	/* CGA? */
	{
	    scr_base = (ushort far *) SCR_CGABASE;
	    scr_segbase = 0xB800;
	    scr_maxcursor = 7;
	} else
	{
	    scr_base = (ushort far *) SCR_MONOBASE;
	    scr_segbase = 0xB000;
	    scr_maxcursor = 13;
	}
	system_initialised = TRUE;
	DOSshape = Scr_GetCursor(&DOSrow, &DOScol);
	Scr_SaveScreen(_Scr_Preimage);
    }
}


void 
Scr_EndSystem(void)
{
    struct _WinList *wlnow = _ActiveWindowList;
    struct _WinList *wltmp;
    if (system_initialised)
    {
	while (wlnow)
	{
	    Win_Free(wlnow->w);
	    wlnow = wlnow->next;
	}
	wlnow = _ActiveWindowList;
	while (wlnow)
	{
	    wltmp = wlnow;
	    wlnow = wlnow->next;
	    free(wltmp);
	}
	Scr_RestoreScreen(_Scr_Preimage);
	_wfree(_Scr_Preimage);
	Scr_PutCursor(DOSrow, DOScol);
	Scr_SetCursorShape(DOSshape);
	system_initialised = FALSE;
    }
}

/**************************************************************

	General Screen Functions

**************************************************************/

void 
    Scr_SetCursorShape(enum cursorshapes shape)
    {
	unsigned char scantop;
	switch (shape)
	{
	    case SCR_BLOKCURSOR:scantop = 0;
	    break;
	    case SCR_LINECURSOR:scantop = scr_maxcursor - 2;
	    break;
	    default:scantop = shape;
	}
	    _CL = scr_maxcursor - 1;	/* End line	 */
	_CH = scantop;
	_AH = 1;
	geninterrupt(0x10);
    }




void 
Scr_PutCursor(int row, int col)
{
    _DH = row;			/* row		 */
    _DL = col;			/* column	 */
    _AH = 2;			/* BIOS service	 */
    _BH = 0;			/* video page	 */
    geninterrupt(0x10);
}





int 
Scr_GetCursor(int *row, int *col)
{
    _AH = 3;			/* BIOS service	 */
    _BH = 0;			/* video page	 */
    geninterrupt(0x10);
    *row = _DH;
    *col = _DL;
    return (_CH ? SCR_LINECURSOR : SCR_BLOKCURSOR);
}

void 
Scr_CursorOn(void)
{
    _AH = 3;
    _BH = 0;
    geninterrupt(0x10);
    _CX &= (ushort) 0xDFFF;
    _AH = 1;
    geninterrupt(0x10);
}

void 
Scr_CursorOff(void)
{
    _AH = 3;
    _BH = 0;
    geninterrupt(0x10);
    _CX |= (ushort) 0x2000;
    _AH = 1;
    geninterrupt(0x10);
}

void 
Scr_PutChar(int row, int col, char c, ushort attr)
{
    register ushort far *scr_tmp;
    scr_tmp = (ushort far *) FAR_PTR(scr_segbase, 160 * row + col * 2);
    *scr_tmp = SCR_CHAR(c, attr);
}

void 
Scr_PutString(STRING str, ushort attr)
{
    int r, c;
    register ushort far *scr_tmp;
    Scr_GetCursor(&r, &c);
    scr_tmp = (ushort far *) FAR_PTR(scr_segbase, 160 * r + c * 2);
    while (*str)
	*scr_tmp++ = SCR_CHAR(*str++, attr);
}

static ushort Scr_CurrentAttr = Scr_ColorAttr(LIGHTGRAY, BLACK);

void 
Scr_SetAttribute(ushort attr)
{
    Scr_CurrentAttr = attr;
}

void 
Scr_PutC(char ch)
{
    int r, c;
    Scr_GetCursor(&r, &c);
    *((ushort far *) FAR_PTR(scr_segbase, 160 * r + c * 2)) = SCR_CHAR(ch, Scr_CurrentAttr);
    c++;
    if (c == 80)
    {
	c = 0;
	r++;
	if (r == 25)
	{
	    c = 79;
	    r = 24;
	}
    }
    Scr_PutCursor(r, c);
}

void 
Scr_PutS(STRING str)
{
    while (*str)
	Scr_PutC(*str++);
}

/*******************************************************************

		Window Routines

*********************************************************************/


/********************************************
	Save or restore window to/from heap or
	user-supplied buffer area.
	if 'current' is true the current image
		is used, else the preimage.
	if 'save' is true the image is saved
		from the screen, else it is
		restored to the screen.
*********************************************/

static void NEAR 
_Win_MoveImage(WINPTR win, int current, int save)
{
    if (win)
    {
	ushort far *start = win->start;
	ushort h = win->h, w = win->w;
	if (win->flags & WIN_BORDER)
	{
	    start -= 81L;
	    h += 2;
	    w += 2;
	}
	if (!current && (win->flags & (WIN_FLASH | WIN_NONDESTRUCT)))
	    if (save)
		win->preimage = Scr_BlockSave(start, win->preimage, h, w);
	    else
		Scr_BlockRestore(win->preimage, start, h, w);
	else if (current && (win->flags & (WIN_FLASH | WIN_TILED)))
	    if (save)
		win->current = Scr_BlockSave(start, win->current, h, w);
	    else
		Scr_BlockRestore(win->current, start, h, w);
    }
}

#define _Win_SavePreimage(w)	_Win_MoveImage(w,FALSE,TRUE)
#define _Win_RestorePreimage(w)	_Win_MoveImage(w,FALSE,FALSE)
#define _Win_SaveCurrent(w)	_Win_MoveImage(w,TRUE,TRUE)
#define _Win_RestoreCurrent(w)	_Win_MoveImage(w,TRUE,FALSE)

/* Check that current window is valid before doing ops */

static int NEAR 
_Win_Valid(void)
{
    if (Win_Current == NULL)
    {
	(void)Win_Error(NULL, NULL, "Window operation fails: no active window!", FALSE, TRUE);
	return 0;
    } else
	return 1;
}

/***********************************************/
/* Screen and Window Border Drawing	       */
/***********************************************/

void 
Win_1LineBorderSet(void)
{
    TLCORNER = 218;
    TRCORNER = 191;
    BLCORNER = 192;
    BRCORNER = 217;
    THLINE = BHLINE = 196;
    RVLINE = LVLINE = 179;
}



void 
Win_2LineBorderSet(void)
{
    TLCORNER = 201;
    TRCORNER = 187;
    BLCORNER = 200;
    BRCORNER = 188;
    THLINE = BHLINE = 205;
    RVLINE = LVLINE = 186;
}



void 
Win_BorderSet(char tl, char tr, char bl, char br,
	      char th, char bh, char rv, char lv)
{
    TLCORNER = tl;
    TRCORNER = tr;
    BLCORNER = bl;
    BRCORNER = br;
    THLINE = th;
    BHLINE = bh;
    RVLINE = rv;
    LVLINE = lv;
}




void 
Win_DrawBorder(ushort far * start, ushort width,
	       ushort height, uchar attr)
{
    register int i;
    int d1 = width + 1;
    int d2 = 80 - d1;
    register ushort far *scr_tmp = start - 81L;

    *scr_tmp++ = SCR_CHAR(TLCORNER, attr);
    for (i = 0; i < width; ++i)
	*scr_tmp++ = SCR_CHAR(THLINE, attr);
    *scr_tmp = SCR_CHAR(TRCORNER, attr);
    scr_tmp += d2;
    for (i = 0; i < height; ++i)
    {
	*scr_tmp = SCR_CHAR(LVLINE, attr);
	scr_tmp += d1;
	*scr_tmp = SCR_CHAR(RVLINE, attr);
	scr_tmp += d2;
    }
    *scr_tmp++ = SCR_CHAR(BLCORNER, attr);
    for (i = 0; i < width; ++i)
	*scr_tmp++ = SCR_CHAR(BHLINE, attr);
    *scr_tmp = SCR_CHAR(BRCORNER, attr);
}


void 
    Win_PutTitle(STRING title, enum positions place, enum positions align)
    {
	ushort far *scr_tmp = (ushort far *) (Win_Current->start);

	if  (_Win_Valid() && title != NULL)
	{
	    register uchar attr = Win_Current->ba;
	    switch (place)
	    {
		case SCR_TOP:scr_tmp -= 80L;
		break;
		case SCR_BOTTOM:scr_tmp += Win_Current->h * 80;
		default:break;
	    }
	    switch (align)
	    {
		case SCR_RIGHT:scr_tmp += Win_Current->w - strlen(title);
	    case SCR_LEFT:
		break;
	    case SCR_CENTRE:
	    default:
		scr_tmp += (Win_Current->w - strlen(title)) / 2;
		break;
	    }
	    while (*title)
		*scr_tmp++ = SCR_CHAR(*title++, attr);
	}
    }

/********************************/
/* Create and Free Windows	*/
/********************************/

WINPTR 
Win_Make(ushort flags, int left, int top, int width, int height,
	 uchar boxattr, uchar attr)
{
    WINPTR win = (WINPTR) NULL;
    struct _WinList *wl_tmp = (struct _WinList *)NULL;
    int maxx, minx, maxy, miny;
    unsigned tmp;
    maxy = top + height - 1;
    miny = top;
    maxx = left + width - 1;
    minx = left;
    if (flags & WIN_BORDER)
    {
	maxx++;
	maxy++;
	minx--;
	miny--;
    }
    if (maxx > 79 || minx < 0 || maxy > 24 || miny < 0)
    {
	(void)Win_Error(NULL, "Win_Make", "window will not fit on screen", FALSE, FALSE);
	return (WINPTR) NULL;
    }
    if ((flags & WIN_NONDESTRUCT) && (flags & WIN_FLASH))
    {
	(void)Win_Error(NULL, "Win_Make", "NONDESTRUCT and FLASH flags are mutually exclusive", FALSE, FALSE);
	return (WINPTR) NULL;
    }
    if (((win = (WINPTR) _wmalloc(sizeof(WINDOW))) != NULL) &&
	((wl_tmp = (struct _WinList *)malloc(LISTELTSZ)) != NULL))
    {
	/* Insert at FRONT of list */
	wl_tmp->next = _ActiveWindowList;
	wl_tmp->w = win;
	wl_tmp->open = 1;
	_ActiveWindowList = wl_tmp;

	win->x = left;
	win->y = top;
	win->h = height;
	win->w = width;
	win->start = (ushort far *)
	FAR_PTR(scr_segbase, 160 * top + left * 2);
	win->preimage = win->current = (void _wfar *)NULL;
	win->r = win->c = 0;	/* Initially cursor is at top left */
	win->ba = boxattr;
	win->ca = attr;
	tmp = attr & 16;
	if (tmp > 7)
	    tmp = 0;		/* Make background black if illegal */
	tmp = tmp * 16 + attr / 16;
	win->ia = tmp;
	win->tabsize = 8;
	win->flags = flags;

	/* save current window image prior to creating new one... */
	_Win_SaveCurrent(Win_Current);
	_Win_SavePreimage(win);
	_Block_Clear((ushort) left,
		     (ushort) top,
		     (ushort) (left + width - 1),
		     (ushort) (top + height - 1),
		     attr);
	if ((flags & WIN_BORDER) == WIN_BORDER)
	{
	    if (flags & WIN_ZOOM)
	    {
		ushort w = 1, h = 1, i;
		if  (width < height)
		        h = 2 * ((height - width) / 2) + 1;
		else
		        w = 2 * ((width - height) / 2) + 1;
		while (w < width && h < height)
		{
		    ushort t = top + (height - h) / 2, l = left + (width - w) / 2;
		    for (i = 0; i < 10; i++)	/* Delay */
		    {
			Win_DrawBorder((ushort far *) FAR_PTR(scr_segbase, 160 * t + l * 2),
				       w, h, boxattr);
			_Block_Clear(l - 1, t - 1, l + w, t + h, attr);
		    }
		        w += 2;
		    h += 2;
		}
	    }
	    Win_DrawBorder(win->start, width, height, boxattr);
	}
	_Win_SaveCurrent(win);	/* Save image of new window */
	/* If a flash window, restore screen */
	if (win->flags & WIN_FLASH)
	    _Win_RestorePreimage(win);
	/* If current window is tiled, restore its active image */
	_Win_RestoreCurrent(Win_Current);
	/*
	 * Note that the two lines above may be overkill, but only when we
	 * are creating a FLASH window and the active window is a FLASH or
	 * TILED window. In any other case, at most one of these lines will
	 * have any effect.
	 */
    } else
	(void)Win_Error(NULL, "Win_Make", "malloc failed", FALSE, FALSE);

    return win;
}

/*******************************************************/

void 
Win_Free(WINPTR win)
{
    struct _WinList *wl_tmp = _ActiveWindowList;
    if (win == NULL)
	return;
    while (wl_tmp)
    {
	if (wl_tmp->w == win)
	{
	    if (wl_tmp->open)
	    {
		if ((win->flags & (WIN_ZOOM | WIN_BORDER)) == (WIN_ZOOM | WIN_BORDER))
		{
		    short w = win->w, h = win->h, y = win->y, x = win->x,
		        i;
		    while (w > 0 && h > 0)
		    {
			for (i = 0; i < 10; i++)
			{
			    Win_DrawBorder((ushort far *) FAR_PTR(scr_segbase, 160 * y + x * 2), w, h, win->ba);
			    _Block_Clear(x - 1, y - 1, x + w, y + h, win->ca);
			}
			x++;
			y++;
			w -= 2;
			h -= 2;
		    }
		}
		_Win_RestorePreimage(win);
		_wfree(win->preimage);
		_wfree(win->current);
		_wfree(win);
		wl_tmp->open = 0;
	    }
	    break;
	}
	wl_tmp = wl_tmp->next;
    }
    if (win == Win_Current)
	Win_Current = NULL;
}

/*********************************************************************/

void 
Win_Activate(WINPTR win)
{
    if (win == NULL)
	(void)Win_Error(NULL, NULL, "Cannot activate NULL window", FALSE, TRUE);
    else if (win != Win_Current)
    {
	/* Save current active image, retore new current active image */
	if (Win_Current != NULL)
	{
	    _Win_SaveCurrent(Win_Current);
	    if (Win_Current->flags & WIN_FLASH)
		_Win_RestorePreimage(Win_Current);
	}
	if (win->flags & WIN_FLASH)
	    _Win_SavePreimage(win);
	_Win_RestoreCurrent(win);
	Win_Current = win;
	if (Win_Current->flags & WIN_BLOKCURSOR)
	    Scr_SetCursorShape(SCR_BLOKCURSOR);
	else
	    Scr_SetCursorShape(SCR_LINECURSOR);
	if (Win_Current->flags & WIN_CURSOROFF)
	    Scr_CursorOff();
	else
	    Scr_CursorOn();
	Win_PlaceCursor();
    }
}

/*****************************************************/

void 
Win_PutCursor(int r, int c)
{
    if (_Win_Valid())
    {
	WIN_ROW = r;
	WIN_COL = c;
	Win_PlaceCursor();
    }
}

void 
Win_SetFlags(ushort mask)
{
    if (_Win_Valid())
    {
	Win_Current->flags |= mask;
	if (mask & (WIN_NONDESTRUCT | WIN_BORDER | WIN_TILED | WIN_FLASH))
	    (void)Win_Error(NULL, "Win_SetFlags", "illegal argument", FALSE, FALSE);
	if ((mask & WIN_BLOKCURSOR) == WIN_BLOKCURSOR)
	    Scr_SetCursorShape(SCR_BLOKCURSOR);
	if ((mask & WIN_CURSOROFF) == WIN_CURSOROFF)
	    Scr_CursorOff();
    }
}

void 
Win_ClearFlags(ushort mask)
{
    if (_Win_Valid())
    {
	Win_Current->flags ^= mask;
	if (mask & (WIN_NONDESTRUCT | WIN_BORDER | WIN_TILED | WIN_FLASH))
	    (void)Win_Error(NULL, "Win_ClearFlags", "illegal argument", FALSE, FALSE);
	if ((mask & WIN_BLOKCURSOR) == WIN_BLOKCURSOR)
	    Scr_SetCursorShape(SCR_LINECURSOR);
	if ((mask & WIN_CURSOROFF) == WIN_CURSOROFF)
	    Scr_CursorOn();
    }
}

/*****************************************************/

void 
Win_PutChar(unsigned char c)
{
    register ushort far *scr_tmp =
    (ushort far *) (WIN_START);
    int tabspcs;

    if (_Win_Valid())
    {
	switch (c)
	{
	case KB_NEWLINE:
	    WIN_ROW++;
	    break;
	case KB_CRGRTN:
	    WIN_COL = 0;
	    break;
	case KB_TAB:
	    tabspcs = Win_Current->tabsize -
		(Win_Current->c % Win_Current->tabsize);
	    while (tabspcs--)
		Win_PutChar(' ');
	    break;
	default:
	    scr_tmp += 80 * WIN_ROW + WIN_COL;
	    *scr_tmp = SCR_CHAR(c, Win_Current->ca);
	    WIN_COL++;
	    break;
	}
	if (Win_Current->flags & WIN_WRAP)
	    if (WIN_WIDTH <= WIN_COL)
	    {
		WIN_COL = 0;
		WIN_ROW++;
	    }
	if (Win_Current->flags & WIN_SCROLL)
	    if (WIN_ROW >= WIN_HEIGHT)
	    {
		WIN_ROW = WIN_HEIGHT - 1;
		Win_ScrollUp();
	    }
	if (Win_Current->flags & WIN_CLIP)
	{
	    if (WIN_WIDTH <= WIN_COL)
		WIN_COL = WIN_WIDTH - 1;
	    if (WIN_HEIGHT <= WIN_ROW)
		WIN_ROW = WIN_HEIGHT - 1;
	} else if (WIN_WIDTH <= WIN_COL)
	{
	    Win_ScrollLeft();
	    WIN_COL = WIN_WIDTH - 1;
	}
	Win_PlaceCursor();
    }
}

/*****************************************************/

/* The Win_FastPuts function is a slightly more efficient version
	of Win_PutsString, but does not allow all of the possible
	flag combinations, nor does it affect the window cursor position. */


void 
Win_FastPutS(int r, int c, STRING str)
{
    register ushort far *scr_tmp =
    (ushort far *) (WIN_START);
    short space, slen, stmp;
    char ca = Win_Current->ca;
    if (_Win_Valid() && str != NULL)
    {
	slen = strlen(str);
	if ((Win_Current->flags & WIN_WRAP) == WIN_WRAP)
	{
	    r += c / Win_Current->w;
	    c %= Win_Current->w;
	} else if (c >= Win_Current->w)
	    return;
	scr_tmp += 80 * r + c;
	space = WIN_WIDTH - c;
	while (slen)
	{
	    stmp = min(space, slen);
	    if (stmp > 0)
	    {
		slen -= stmp;
		while (stmp--)
		    *scr_tmp++ = SCR_CHAR(*str++, ca);
	    }
	    if (slen == 0)
		break;
	    if ((Win_Current->flags & WIN_WRAP) != WIN_WRAP)
		break;
	    scr_tmp = WIN_START + 80 * (++r);
	    space = WIN_WIDTH;
	    if (r >= WIN_HEIGHT)
		if (Win_Current->flags & WIN_SCROLL)
		    Win_ScrollUp();
		else
		    break;
	}
    }
}



void 
Win_PutString(STRING str)
{
    if (_Win_Valid() && str != (STRING) NULL)
	while (*str)
	    Win_PutChar(*str++);
}



void 
Win_PutMarkedStr(int line, STRING txt, short bstart, short bend)
 /* Write string with substring in reverse video */
{
    char buff[256];
    register int substrwidth;
    int len = strlen(txt);
    if (_Win_Valid())
    {
	*buff = NUL;
	if (bstart > 0)
	{
	    if (bstart > len)
		bstart = bend = len;
	    strncpy(buff, txt, bstart);
	    buff[bstart] = NUL;
	    Win_FastPutS(line, 0, buff);
	}
	if (bend == (-1))
	    bend = len - 1;
	if (len > bstart && bend >= bstart)
	{
	    uchar oldattr = Win_Current->ca;
	    *buff = NUL;
	    Win_SetAttribute(SCR_INVRS_ATTR);
	    substrwidth = bend - bstart + 1;
	    if (bstart + substrwidth > len)
		substrwidth = len - bstart;
	    strncpy(buff, txt + bstart, substrwidth);
	    buff[substrwidth] = NUL;
	    Win_FastPutS(line, bstart, buff);
	    Win_SetAttribute(oldattr);
	    substrwidth = len - bend - 1;
	    if (substrwidth > 0)
	    {
		*buff = NUL;
		strncpy(buff, txt + bend + 1, substrwidth);
		buff[substrwidth] = NUL;
		Win_FastPutS(line, bend + 1, buff);
	    }
	}
    }
}


/*************************************************************
	Basic Editing Functions
**************************************************************/

void 
Win_Clear(void)
{
    if (_Win_Valid())
	_Block_Clear(WIN_X, WIN_Y, WIN_X + WIN_WIDTH - 1, WIN_Y + WIN_HEIGHT - 1, Win_Current->ca);
}

void 
Win_ClearLine(int l)
{
    if (_Win_Valid())
	_Block_Clear(WIN_X, WIN_Y + l, WIN_X + WIN_WIDTH - 1, WIN_Y + l, Win_Current->ca);
}

void 
Win_DelLine(int l)
{
    if (_Win_Valid())
    {
	register int tmpx = WIN_X, tmpy = WIN_Y + l;
	_BlockMove((tmpy + 1) * 80 + tmpx, tmpy * 80 + tmpx,
		   WIN_WIDTH, WIN_HEIGHT - 1 - l);
	tmpy = WIN_Y + WIN_HEIGHT - 1;
	_Block_Clear(tmpx, tmpy, tmpx + WIN_WIDTH - 1, tmpy, Win_Current->ca);
    }
}

void 
Win_InsLine(int l)
{
    if (_Win_Valid())
    {
	register int tmpx = WIN_X, tmpy = WIN_Y + l;
	_BlockMove(tmpy * 80 + tmpx, (tmpy + 1) * 80 + tmpx,
		   WIN_WIDTH, WIN_HEIGHT - l - 1);
	_Block_Clear(tmpx, tmpy, tmpx + WIN_WIDTH - 1, tmpy, Win_Current->ca);
    }
}

void 
Win_InsColumn(int row, int col, int height)
{
    if (_Win_Valid())
    {
	register int tmpx = WIN_X + col, tmpy = WIN_Y + row, tmp;
	tmp = tmpy * 80 + tmpx;
	_BlockMove(tmp, tmp + 1, WIN_WIDTH - col - 1, height);
	_Block_Clear(tmpx, tmpy, tmpx, tmpy + height - 1, Win_Current->ca);
    }
}

void 
Win_InsChar(void)
{
    Win_InsColumn(WIN_ROW, WIN_COL, 1);
}


void 
Win_DelColumn(int row, int col, int height)
{
    if (_Win_Valid())
    {
	register int tmp = (WIN_Y + row) * 80 + WIN_X + col, tmpy;
	_BlockMove(tmp + 1, tmp, WIN_WIDTH - col - 1, height);
	tmp = WIN_X + WIN_WIDTH - 1;
	tmpy = WIN_Y + col;
	_Block_Clear(tmp, tmpy, tmp, tmpy + height - 1, Win_Current->ca);
    }
}

void 
Win_DelChar(void)
{
    Win_DelColumn(WIN_ROW, WIN_COL, 1);
}

/*****************************************/
/*	Window Movement			*/
/****************************************/

void 
Win_ScrollUp(void)
{
    Win_DelLine(0);
}

void 
Win_ScrollDown(void)
{
    Win_InsLine(0);
}

void 
Win_ScrollLeft(void)
{
    Win_DelColumn(0, 0, WIN_HEIGHT);
}

void 
Win_ScrollRight(void)
{
    Win_InsColumn(0, 0, WIN_HEIGHT);
}

/*****************************************
	Change attributes in window
******************************************/

void 
Win_PutAttribute(unsigned char attr, int fromx, int fromy,
		 int w, int h)
{
    char far *scr_tmp;
    BOOLEAN bad_params = FALSE;
    int rowinc = (80 - w) << 1;
    if (_Win_Valid())
    {
	if ((fromx + w) > WIN_WIDTH)
	{
	    w = WIN_WIDTH - fromx;
	    bad_params = TRUE;
	}
	if ((fromy + h) > WIN_HEIGHT)
	{
	    h = WIN_HEIGHT - fromy;
	    bad_params = TRUE;
	}
	if (w < 0)
	{
	    w = 0;
	    bad_params = TRUE;
	}
	if (h < 0)
	{
	    h = 0;
	    bad_params = TRUE;
	}
	/*
	 * It is a common bug to pass bad parameters to Win_PutAttr, usually
	 * because a different window to expected is active. This code helps
	 * detect these.
	 */
	if (bad_params)
	    (void)Win_Error(NULL, "Win_PutAttribute", "Bad parameters in call", FALSE, FALSE);
	scr_tmp = (char far *)(WIN_START + 80 * fromy + fromx) + 1;
	while (h--)
	{
	    int wtmp = w;
	    while (wtmp--)
	    {
		*scr_tmp = attr;
		scr_tmp += 2;
	    }
	    scr_tmp += rowinc;
	}
    }
}


/***********************************************************************/


void (*_Scr_Fatality_Handler) (char *msg)= NULL;

void 
Scr_InstallFatal(void (*f) (char *))
{
    _Scr_Fatality_Handler = f;
}


ushort 
Win_Error(WINPTR Err_Win, STRING class, STRING message,
	  BOOLEAN justwarn, BOOLEAN wait)
{
    WINPTR Old_Win = Win_Current;
    int len;
    ushort rtn = 0;
    _Win_ErrorLevel++;
    Win_1LineBorderSet();
    if (class == NULL)
	class = "";
    if (Err_Win == NULL && _Win_ErrorLevel < 2)
    {
	len = strlen(message) + strlen(class) + (justwarn ? 12 : 10);
	if (len > 78)
	    len = 78;
	if (len < 18)
	    len = 18;
	Err_Win = Win_Make(WIN_CURSOROFF | WIN_NONDESTRUCT | WIN_TILED | WIN_BORDER,
		(80 - len) / 2, 10, len, 1, SCR_INVRS_ATTR, SCR_INVRS_ATTR);
    }
    if (Err_Win)
    {
	if (message)
	{
	    len = strlen(message);
	    while (len >= 0 && message[len] < 33)
		len--;		/* strip \n's etc */
	    message[++len] = 0;
	}
	if (class)
	{
	    len = strlen(class);
	    while (len >= 0 && class[len] < 33)
		len--;		/* strip \n's etc */
	    class[++len] = 0;
	}
	Win_Activate(Err_Win);
	Win_PutTitle(justwarn ? "<WARNING>" : "< ERROR >", SCR_TOP, SCR_CENTRE);
	Win_FastPutS(0, 1, class);
	Win_FastPutS(0, len + 1, justwarn ? " warning: " : " error: ");
	Win_FastPutS(0, len + (justwarn ? 11 : 9), message);
	if (wait)
	{
	    Scr_PutCursor(11, 32);
	    Scr_PutString("< Press any Key >", SCR_INVRS_ATTR);
	    rtn = Kb_RawGetC();
	} else
	    sleep(2);
	Win_Free(Err_Win);
    } else
    {
	if (farcoreleft() < 2000)
	{
	    char *msg = "NO MORE MEMORY - ABORTING";
	    if (_Scr_Fatality_Handler)
		(*_Scr_Fatality_Handler) (msg);
	    else
	    {
		Scr_EndSystem();
		cprintf("\07%s error :%s\n", class, message);
		puts(msg);
		exit(-1);
	    }
	}
	cprintf("\07%s error :%s\n", class, message);
	sleep(2);
    }
    if (Old_Win)
	Win_Activate(Old_Win);
    _Win_ErrorLevel--;
    return rtn;
}


/******************************************************************/

void 
Win_Printf(STRING format,...)
{
    char LineBuff[120];
    va_list args;
    va_start(args, format);
    vsprintf(LineBuff, format, args);
    va_end(args);
    Win_PutString(LineBuff);
}
