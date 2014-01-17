/* ed.c */

#ifndef UNIX
#if !defined(__MEDIUM__) && !defined(__LARGE__) && !defined(__HUGE__)
/* Non-ANSI preprocessors can't cope with #error, even if this code is
	excluded by conditional compilation (they still have to parse
	it in case it is a #endif)
*/
 /* #error */ You must use the medium, large or huge model to compile the editor
#endif
#else

/* Turbo C definitions unavailable under UNIX */

#define MAXDRIVE	3
#define MAXDIR		66
#define MAXFILE		9
#define MAXEXT		5
#define FILENAME	0x04
#define EXTENSION	0x02

#endif

#include "misc.h"
#include "help.h"
#include <signal.h>

#ifndef UNIX
#include <process.h>
#include <dir.h>
#include <io.h>
#include <conio.h>
#endif

#include "errors.h"
#include "keybd.h"
#include "screen.h"
#include "gen.h"
#include "menu.h"
#include "ed.h"
#include "pew.h"

/************************
*     Editor Windows	*
************************/

WINPTR Ed_Win = (WINPTR) NULL;
static WINPTR Pos_Win = (WINPTR) NULL;
WINPTR Msg_Win = (WINPTR) NULL;
WINPTR Menu_Win = (WINPTR) NULL;

#define MAXFILENAMELEN		80
char Ed_FileName[MAXFILENAMELEN];
char Ed_BaseFileName[14];	/* File name with no path */
char Ed_LoadName[MAXFILENAMELEN];
char Ed_ScrapFileName[MAXFILENAMELEN];


static BUFPTR Ed_BufNow = (BUFPTR) NULL;	/* current line */
static BUFPTR Ed_ScrapBuf = (BUFPTR) NULL;	/* scrap buffer */

/****************************************
* Macros for accessing the linked list	*
****************************************/

#define ED_NEXT		(Ed_BufNow->next)
#define ED_PREV		(Ed_BufNow->prev)
#define ED_TEXT		(Ed_BufNow->text)
#define ED_ID		(Ed_BufNow->ID)
#define ED_LEN		(Ed_BufNow->len)

#define ED_NEXTNEXT	(Ed_BufNow->next->next)
#define ED_NEXTPREV	(Ed_BufNow->next->prev)
#define ED_NEXTTEXT	(Ed_BufNow->next->text)
#define ED_NEXTID	(Ed_BufNow->next->ID)
#define ED_NEXTLEN	(Ed_BufNow->next->len)

/************************
* Key Definitions	*
************************/

typedef void (*edHndl) (void);

ushort Ed_Key[_Ed_NumKeyDefs];	/* key assignments */
edHndl Ed_Func[_Ed_NumKeyDefs];	/* and functions  */
char Ed_KeyNames[_Ed_NumKeyDefs][16];	/* Printable names of key assignments */
/*********************************
* Current file position and size *
*********************************/

/* The first group of items here will have to be included
	as part of the screen image if more than one window
	is to be used */

short Ed_LineNow = 0;
short Ed_ColNow = 0;
static BOOLEAN Ed_MustRefresh = FALSE;
short Ed_PrefCol = 0;
short Ed_LeftCol = 0;
short Ed_ScrLine = 0;		/* The screen line at which the cursor is at;
				 * used by interpreter */

static long Ed_FileSize = 0L;
short Ed_LastLine = 0;
BOOLEAN Ed_RefreshAll = FALSE;
BOOLEAN Ed_IsCompiled = FALSE;
static BOOLEAN Ed_QuietMode = FALSE;
static short Ed_UndoCol = (-1);
static BOOLEAN Ed_FileChanged = FALSE;

/****************
* Option Flags	*
****************/

BOOLEAN Ed_BackUp = TRUE;
BOOLEAN Ed_FillTabs = TRUE;
BOOLEAN Ed_WrapLines = TRUE;
BOOLEAN Ed_ConfigSave = FALSE;
ushort Ed_TabStop = 8;

/************************
*   Mark information	*
************************/

#define ED_NOMARK	0
#define ED_BLOKMARK	1
#define ED_COLMARK	2

static short Ed_MarkCol = 0;
static short Ed_MarkLine = 0;
static short Ed_MarkType = ED_NOMARK;
static short Ed_MarkLBegin = 0;
static short Ed_MarkLEnd = 0;
static short Ed_MarkCBegin = 0;
static short Ed_MarkCEnd = 0;

/************************
* Line Identifiers	*
************************/

static short Ed_IDCnt = 1;
#define 	Ed_NewID	(Ed_IDCnt++)

/***********************************
* Current Screen Image:		   *
*	Line number of first line  *
*	Lengths of each line	   *
*	IDs of each line	   *
***********************************/

#define ED_IMGSZ	22

static struct Ed_ScreenImage
{
    short firstln;		/* line number of first line on screen	 */
    short lastln;		/* line number of last line on screen	 */
    short lastlnrow;		/* last line on screen starts at row..	 */
    short len[ED_IMGSZ];	/* length of the line			 */
    long id[ED_IMGSZ];		/* identifies which line of file	 */
    BOOLEAN changed[ED_IMGSZ];	/* has line changed since last refresh?	 */
}   Ed_ScrImage =
{
    0, 0, 0,
    {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    },
    {
	0l, 0l, 0l, 0l, 0l, 0l, 0l, 0l, 0l, 0l, 0l, 0l, 0l, 0l, 0l, 0l, 0l, 0l, 0l, 0l, 0l, 0l
    },
    {
	FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
	FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
	FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE
    }
};

/****************************
* Special Screen Image IDs  *
****************************/

#define ED_NOLINE	0l
#define ED_CONTINUATION	(-1l)

/********************/
/* Undo line buffer */
/********************/

char Ed_UndoBuff[256];

/**************************/
/* Current function index */
/**************************/

enum editorcommand ed_op;

/************************
*   macro 'functions'	*
************************/

const char *mem_fatal_msg = "Memory allocation failure";

void Fatal(char *msg);

static void 
MemFatal(void)
{
    Fatal((char *)mem_fatal_msg);
}

#define Ed_Allocate(typ,ptr,siz,who)	if ((ptr=(typ)Mem_Malloc(siz,who))==NULL) MemFatal()

/********************************
* Prototypes of static functions	*
********************************/

static void NEAR Ed_FreeAll(BUFPTR * start);
static short NEAR Ed_Point(short line, BOOLEAN undoable);
static ushort NEAR Ed_CleanUp(void);
static short NEAR Ed_FindLine(long ID);
static void NEAR Ed_AllocBuff(BUFPTR * now, BUFPTR pre, BUFPTR post);
static void NEAR Ed_ReplaceLine(BUFPTR which, STRING txt, short len);
static BUFPTR NEAR Ed_RemoveLine(BUFPTR which);
static short NEAR Ed_AddLine(BUFPTR * now, STRING txt);
static void NEAR Ed_SetUpMarkCoords(void);
static short NEAR Ed_MarkStart(short line);
static short NEAR Ed_MarkStop(short line);
static BOOLEAN NEAR Ed_IsMarked(short line);
static void NEAR Ed_ImgRefresh(void);
static void NEAR Ed_WinRefresh(void);
static void NEAR Ed_ChgCol(short c);
static void NEAR Ed_SavePosition(void);
static ushort NEAR Ed_ConfirmClear(void);
static void NEAR Ed_Up(void);
static void NEAR Ed_Down(void);
static void NEAR Ed_Right(void);
static void NEAR Ed_Left(void);
static void NEAR Ed_Home(void);
static void NEAR Ed_End(void);
static void NEAR Ed_PageUp(void);
static void NEAR Ed_PageDown(void);
static void NEAR Ed_InsertCh(char ch);
static void NEAR Ed_InsertStr(STRING txt);
static void NEAR Ed_CopyCols(short from, short len);
static void NEAR Ed_CutCols(short from, short to, BOOLEAN put_in_scrap);
static void NEAR Ed_JoinLines(void);
static void NEAR Ed_BackSpace(uchar keybd_status);
static void NEAR Ed_InsertCR(void);
static ushort NEAR Ed_Command(ushort c, uchar keyb_status);
static void NEAR Ed_Initialise(void);
static void NEAR Ed_AssignKeys(void);
static void NEAR Ed_AssignFunctions(void);

/********************************************************/

static void NEAR 
Ed_Rewind(BUFPTR * b)
{
    if (*b != NULL)
	while ((*b)->prev != NULL)
	    *b = (*b)->prev;
}

/********************************
*	Ed_FreeAll		*
*	==========		*
* Free an entire linked list	*
*	of buffer elements	*
*	from current position	*
*				*
* Called from:	Ed_CleanUp	*
*		Ed_FreeScrap	*
********************************/

static void NEAR 
Ed_FreeAll(BUFPTR * start)
{
    BUFPTR btmp;
    Ed_Rewind(start);
    while (*start != NULL)
    {
	btmp = *start;
	*start = (*start)->next;
	Mem_Free(btmp->text);
	Mem_Free(btmp);
    }
}

/********************************
*	Ed_Point		*
*	========		*
* Position the buffer pointer	*
*	at a specific line,	*
*	normalising and 	*
*	returning the line	*
*	number. If told to, the	*
*	current line is also	*
*	saved to the undo buffer*
*				*
* Called from: 	Ed_CleanUp	*
*	       	Ed_ImgRefresh	*
*		Ed_WinRefresh	*
*		Ed_FixCursor	*
*		Ed_LeftSearch	*
*		Ed_RightSearch	*
*		Ed_InsertCR	*
*		Ed_BlockOp	*
*		Ed_LoadFile	*
*		Ed_WriteFile	*
*		Ed_Initialise	*
********************************/

static short NEAR 
Ed_Point(short line, BOOLEAN undoable)
{
    static short Ed_UndoLine = (-1);
    if (line < 0l)
	line = 0l;
    else if (line > Ed_LastLine)
	line = Ed_LastLine;	/* Normalise		 */
    while (Ed_LineNow > line)
    {				/* Move backward if necessary	 */
	Ed_LineNow--;
	Ed_BufNow = ED_PREV;
    }
    while (Ed_LineNow < line && Ed_LineNow < Ed_LastLine)
    {
	/* Move forward if necessary	 */
	Ed_LineNow++;
	Ed_BufNow = ED_NEXT;
    }
    if (undoable)
    {
	if (line != Ed_UndoLine)
	{			/* Only save if moved onto new line */
	    if (ED_TEXT != NULL)
		strcpy(Ed_UndoBuff, ED_TEXT);
	    else
		Ed_UndoBuff[0] = NUL;
	    Ed_UndoLine = line;
	    Ed_UndoCol = Ed_ColNow;
	}
    }
    return line;
}


/********************************
*				*
*	Ed_CleanUp		*
*	==========		*
* Free up entire buffer	and the	*
*	scrap, first checking if*
*	the file must be saved.	*
*				*
* Called from:	Ed_InitBuff	*
*		Ed_Quit		*
********************************/

static ushort NEAR 
Ed_CleanUp()
{
    ushort tmp = 0;
    if (Ed_FileChanged)
    {
	tmp = Gen_GetResponse(NULL, "File has been changed: Save?", "(YN)", TRUE, 4, HLP_FILECH);
	if (tmp == 'Y' || tmp == 'y')
	{
	    Ed_WriteFile(Ed_FileName);
	    tmp = 0;
	}
    }
    if (tmp != KB_ESC)
    {
	Ed_FreeAll(&Ed_BufNow);
	Ed_FreeAll(&Ed_ScrapBuf);
    }
    return tmp;
}


/********************************
*	Ed_AllocBuff		*
*	============		*
* Allocate a buffer element and	*
*	link it into list.	*
*				*
* Called from:	Ed_InitBuff	*
*		Ed_AddLine	*
*		Ed_AddScrap	*
*		Ed_InsertCR	*
********************************/

static void NEAR 
Ed_AllocBuff(BUFPTR * now, BUFPTR pre, BUFPTR post)
{
    Ed_Allocate(BUFPTR, *now, BUFELTSZ, 50);
    (*now)->prev = pre;
    (*now)->next = post;
    (*now)->text = (STRING) NULL;
    (*now)->ID = Ed_NewID;
    (*now)->len = (unsigned char)0;
    if (pre != NULL)
	pre->next = *now;
    if (post != NULL)
	post->prev = *now;
}

/********************************
*	Ed_GetBaseFileName	*
*	==================	*
* Strips path prefix from	*
*	Ed_FileName and puts the*
*	result in Ed_BaseFile-	*
*	Name.			*
* Called from:			*
*	Ed_InitBuff		*
*	Ed_LoadFile		*
********************************/

void 
Ed_GetBaseFileName()
{
    char drive[MAXDRIVE], dir[MAXDIR], file[MAXFILE], ext[MAXEXT];
    short rtn;
    rtn = fnsplit(Ed_FileName, drive, dir, file, ext);
    Ed_BaseFileName[0] = 0;
    if (rtn & FILENAME)
	strcpy(Ed_BaseFileName, file);
    if (rtn & EXTENSION)
	strcat(Ed_BaseFileName, ext);
}

/********************************
*	Ed_InitBuff		*
*	===========		*
* Initialise the buffer, by	*
*	clearing it first if 	*
*	necessary, setting the	*
*	file name and position,	*
*	and clearing the edit	*
*	window.			*
*				*
* Called from:	Ed_LoadFile	*
*		Pew_MainMenu	*
*		Ed_Initialise	*
********************************/

ushort 
Ed_InitBuff(STRING filename)
{
    if (Ed_BufNow != NULL)
	if (Ed_CleanUp() == KB_ESC)
	    return (ushort) KB_ESC;
    if (Ed_FileName != filename)
	strcpy(Ed_FileName, filename);
    Ed_AllocBuff(&Ed_BufNow, NULL, NULL);
    Ed_LineNow = Ed_LastLine = Ed_FileSize = 0l;
    Ed_ColNow = Ed_PrefCol = Ed_LeftCol = 0;
    Ed_IsCompiled = Ed_FileChanged = FALSE;
    Ed_GetBaseFileName();
    Win_Clear();
    Ed_ShowPos(TRUE);
    return (ushort) 0;
}

/********************************
*	Ed_ReplaceLine		*
*	==============		*
* Replace the specified line of	*
*	text with a new line.	*
*				*
* Called from:	Ed_AddLine	*
*		Ed_Undo		*
*		Ed_InsertCh	*
*		Ed_CutCols	*
*		Ed_JoinLines	*
*		Ed_InsertCR	*
********************************/

static void NEAR 
Ed_ReplaceLine(BUFPTR which, STRING txt, short len)
{
    Mem_Free(which->text);
    if (txt != NULL)
	txt[len] = NUL;
    which->text = txt;
    which->ID = Ed_NewID;
    which->len = (uchar) len;
}

/********************************************************
*							*
*		Mark Utility Functions			*
*		==== ======= =========			*
*                                                       *
* Ed_SetUpMarkCoords - initialises all the variables    *
*		associated with a mark, according to    *
*		the type of mark. The function should	*
*		be called before calling any of the	*
*		other three.				*
* 		Called from:	Ed_WinRefresh	 	*
*				Ed_BlockOp	 	*
*							*
* Ed_MarkStart - returns, for a given line, the         *
*		starting column of the current mark     *
*		within that line.                       *
*		Called from:	Ed_WinRefresh		*
*				Ed_BlockOp		*
*                                                       *
* Ed_MarkStop - returns, for a given line, the ending   *
*		column of the current mark within that  *
*		line, or -1 if the mark extends to the	*
*		end of the line.			*
*		Called from:	Ed_WinRefresh		*
*				Ed_BlockOp		*
*                                                       *
* Ed_IsMarked - returns TRUE or FALSE depending on      *
*		whether the specified line is           *
*		marked or not.				*
*		Called from:	Ed_WinRefresh		*
*							*
*********************************************************/

static void NEAR 
Ed_SetUpMarkCoords()
{
    if (Ed_MarkType != ED_NOMARK)
    {
	if (Ed_LineNow > Ed_MarkLine)
	{
	    Ed_MarkLBegin = Ed_MarkLine;
	    Ed_MarkLEnd = Ed_LineNow;
	    Ed_MarkCBegin = Ed_MarkCol;
	    Ed_MarkCEnd = Ed_ColNow;
	} else
	{
	    Ed_MarkLBegin = Ed_LineNow;
	    Ed_MarkLEnd = Ed_MarkLine;
	    Ed_MarkCBegin = Ed_ColNow;
	    Ed_MarkCEnd = Ed_MarkCol;
	}
	if (Ed_MarkType == ED_COLMARK || Ed_MarkLBegin == Ed_MarkLEnd)
	{
	    if (Ed_MarkCBegin > Ed_MarkCEnd)
	    {
		short tmp = Ed_MarkCBegin;
		Ed_MarkCBegin = Ed_MarkCEnd;
		Ed_MarkCEnd = tmp;
	    }
	}
    } else
    {
	Ed_MarkLBegin = Ed_MarkLEnd = (-1L);
    }
}

static short NEAR 
Ed_MarkStart(short line)
{
    if (line == Ed_MarkLBegin || Ed_MarkType == ED_COLMARK)
	return Ed_MarkCBegin;
    else
	return 0;
}

static short NEAR 
Ed_MarkStop(short line)
{
    if (line == Ed_MarkLEnd || Ed_MarkType == ED_COLMARK)
	return Ed_MarkCEnd;
    else
	return (-1);		/* -1 => mark to end of line */
}

static BOOLEAN NEAR 
Ed_IsMarked(short line)
{
    if (Ed_MarkType != ED_NOMARK)
	if (line >= Ed_MarkLBegin && line <= Ed_MarkLEnd)
	    return TRUE;
    return FALSE;
}


/****************************************
*	Ed_DisplayPosValue		*
*	==================		*
* Displays a string version of a value	*
*	if it has changed from the last	*
*	value or the flag 'showit' is	*
*	true. If necessary the print	*
*	field is blanked first.		*
*					*
* Called from:	Ed_ShowPos		*
****************************************/

static void NEAR 
Ed_DisplayPosValue(short col, ulong * oldval, ulong newval, STRING clr, BOOLEAN showit)
{
    char tmp[10];
    if (showit || oldval == NULL || *oldval != newval)
    {
	(void)ltoa((long)newval, tmp, 10);
	if (oldval == NULL || *oldval > newval)
	    Win_FastPutS(0, col, clr);
	Win_FastPutS(0, col, tmp);
	if (oldval != NULL)
	    *oldval = newval;
    }
}

/********************************
*	Ed_ShowPos		*
*	==========		*
* Display current editor info,	*
*	such as file size and	*
*	position.		*
*				*
* Called from:	Ed_LoadFile	*
*		Pew_MainMenu	*
*		Ed_Main		*
********************************/

void 
Ed_ShowPos(BOOLEAN showall)
{
    WINPTR Last_Win = Win_Current;
    static ulong oldlnow, oldlast, oldcol, oldsz;
    static short oldmacrec = -1, oldmacplay = -1, oldmacnum = -1;
    char tmp[10];
    Win_Activate(Pos_Win);
    if (showall)
    {
	Win_Clear();
	if (Ed_Win->w > 70)
	{
	    Win_FastPutS(0, 0, " Line:     /      Col:      Size:        Mem:                              ");
	    Win_FastPutS(0, 61, Ed_BaseFileName);
	} else
	    Win_FastPutS(0, 0, " Line:     /      Col:      Size:        Mem:                    ");
    }
    Ed_DisplayPosValue(7, &oldlnow, (ulong) Ed_LineNow + 1, "    ", showall);
    Ed_DisplayPosValue(12, &oldlast, (ulong) Ed_LastLine + 1, "    ", showall);
    Ed_DisplayPosValue(23, &oldcol, (ulong) Ed_ColNow + 1, "   ", showall);
    Ed_DisplayPosValue(34, &oldsz, (ulong) Ed_FileSize, "     ", showall);
    Ed_DisplayPosValue(46, NULL, farcoreleft(), "      ", showall);

    if (showall || oldmacrec != Kb_Record || oldmacplay != Kb_PlayBack
	|| oldmacnum != Kb_MacNum)
    {
	Win_FastPutS(0, 54, "     ");
	tmp[0] = '0' + (char)Kb_MacNum;
	tmp[1] = 0;
	if (Kb_Record)
	{
	    Win_FastPutS(0, 54, "REC");
	    Win_FastPutS(0, 57, tmp);
	} else if (Kb_PlayBack)
	{
	    Win_FastPutS(0, 54, "PLAY");
	    Win_FastPutS(0, 58, tmp);
	}
	oldmacrec = Kb_Record;
	oldmacplay = Kb_PlayBack;
	oldmacnum = Kb_MacNum;
    }
    if (Last_Win)
	Win_Activate(Last_Win);
}

/********************************
*	Ed_ShowMessage		*
*	==============		*
* Clear the message window and	*
*	display the given string*
*	in it.			*
*				*
* Called from:	Ed_Undo		*
*		Ed_Mark		*
*		Ed_BlockOp	*
*		Ed_BlockPaste	*
*		Ed_ColumnPaste	*
*		Ed_LoadFile	*
*		Ed_WriteFile	*
*		Ed_LoadScrap	*
*		Ed_WriteScrap	*
********************************/

void 
Ed_ShowMessage(STRING msg)
{
    WINPTR Last_Win = Win_Current;
    Win_Activate(Msg_Win);
    Win_Clear();
    Win_FastPutS(0, 1, msg);
    if (Last_Win)
	Win_Activate(Last_Win);
}

/*************************
* Image / Screen Updates *
*************************/

static void NEAR 
Ed_ImgRefresh()
{
    short scrline = 0, wraps, end = (int)Ed_Win->h;
    short oldfline = Ed_LineNow, bufline = Ed_ScrImage.firstln;

    while (scrline < end)
    {
	wraps = 0;
	if (bufline <= Ed_LastLine)
	{
	    (void)Ed_Point(bufline, FALSE);
	    Ed_ScrImage.lastln = bufline;
	    Ed_ScrImage.lastlnrow = scrline;
	    wraps = Ed_WrapLines ? ED_LEN / Ed_Win->w : 0;
	    if (scrline + wraps >= end)
		wraps = end - scrline - 1;
	    if (Ed_ScrImage.id[scrline] != ED_ID)
	    {
		Ed_MustRefresh = TRUE;
		Ed_ScrImage.id[scrline] = ED_ID;
		Ed_ScrImage.changed[scrline] = TRUE;

		/* Clear the necessary number of lines */
		while (wraps-- > 0)
		{
		    Ed_ScrImage.id[++scrline] = ED_CONTINUATION;
		    Ed_ScrImage.len[scrline] = 99;
		}
	    } else
	    {
		Ed_ScrImage.changed[scrline] = FALSE;
		scrline += wraps;
	    }
	} else
	{			/* If past the end of the file */
	    /* ...then if line not blank, clear it */
	    if (Ed_ScrImage.id[scrline] != ED_NOLINE)
	    {
		Ed_ScrImage.id[scrline] = ED_NOLINE;
		Ed_ScrImage.changed[scrline] = TRUE;
		Ed_MustRefresh = TRUE;
	    }
	}
	scrline++;
	bufline++;
    }
    (void)Ed_Point(oldfline, FALSE);
}


static void NEAR 
Ed_WinRefresh()
{
    short scrline = 0, wraps, end = (int)Ed_Win->h;
    short oldfline = Ed_LineNow, bline = Ed_ScrImage.firstln;
    short newleft = Ed_LeftCol;

    if (!Ed_WrapLines)
    {
	while (Ed_ColNow < newleft)
	    newleft -= 10;
	while (Ed_ColNow >= (newleft + Ed_Win->w))
	    newleft += 10;
	if (newleft < 0)
	    newleft = 0;
	else if (newleft > ED_LEN)
	    newleft = ED_LEN - ED_LEN % 10;
	if (newleft != Ed_LeftCol)
	    Ed_RefreshAll = TRUE;
	Ed_LeftCol = newleft;
	Ed_Win->c = Ed_ColNow - Ed_LeftCol;
    }
    if ((!Ed_RefreshAll && !Ed_MustRefresh) || Ed_QuietMode)
	return;

    Ed_SetUpMarkCoords();
    while (scrline < end)
    {
	(void)Ed_Point(bline, FALSE);
	wraps = (Ed_WrapLines) ? (ED_LEN / Ed_Win->w) : 0;
	if (Ed_RefreshAll || Ed_ScrImage.changed[scrline]
	    || Ed_MarkType != ED_NOMARK)
	{
	    /*
	     * Clear only the last screen line necessary for each buffer line
	     */
	    if (scrline + wraps < end && (Ed_RefreshAll ||
	    /* Ed_ScrImage.len[scrline]>ED_LEN */
					  Ed_ScrImage.changed[scrline]))
		Win_ClearLine(scrline + wraps);
	    if (Ed_ScrImage.id[scrline] != ED_NOLINE)
	    {
		Ed_ScrImage.len[scrline] = (short)ED_LEN;
		if (ED_LEN > Ed_LeftCol)
		{
		    if (Ed_IsMarked(bline))
			Win_PutMarkedStr(scrline,
					 ED_TEXT + Ed_LeftCol,
					 Ed_MarkStart(bline) - Ed_LeftCol,
					 Ed_MarkStop(bline) - Ed_LeftCol);
		    else
			Win_FastPutS(scrline, 0, ED_TEXT + Ed_LeftCol);
		}
	    } else
		Ed_ScrImage.len[scrline] = (short)0;
	    Ed_ScrImage.changed[scrline] = FALSE;
	}
	scrline += (1 + wraps);
	bline++;
    }
    (void)Ed_Point(oldfline, FALSE);
    Ed_RefreshAll = Ed_MustRefresh = FALSE;
    Win_PutCursor(Ed_Win->r, Ed_Win->c);
    Ed_ScrLine = Ed_ScrImage.firstln;
}

/****************************************
* Locate a line on the screen with	*
*	the appropriate ID, if possible *
****************************************/

static short NEAR 
Ed_FindLine(long ID)
{
    short rtn = Ed_Win->h;
    while (rtn-- > 0)
	if (Ed_ScrImage.id[rtn] == ID)
	    break;
    return (rtn);
}


/********************************************************
* Normalise the specified cursor position 		*
*	and update the actual cursor appropriately.	*
***************************************************† ö«*/

void 
Ed_FixCursor(short r, short c)
{
    short row;
    short tmp, oldfirstln = Ed_ScrImage.firstln;

    /* Normalise row and column coordinates */

    r = Ed_Point(r, TRUE);
    Ed_ColNow = max(0, min(c, (short)ED_LEN));
    Ed_ImgRefresh();

    /* Make sure line appears on screen */

    if (Ed_LineNow < Ed_ScrImage.firstln)
    {
	Ed_ScrImage.firstln = Ed_LineNow;
	Ed_ImgRefresh();
    } else
	while (Ed_LineNow > Ed_ScrImage.lastln)
	{
	    Ed_ScrImage.firstln += (Ed_LineNow - Ed_ScrImage.lastln) / 10 + 1;
	    Ed_ImgRefresh();
	}

    /* Ensure column appears on the screen */

    row = Ed_WrapLines ? (Ed_Win->h - Ed_ColNow / Ed_Win->w) : Ed_Win->h;
    while ((tmp = Ed_FindLine(ED_ID)) >= row)
    {
	Ed_ScrImage.firstln++;
	Ed_ImgRefresh();
    }

    /* Update the data structures and edit window */

    if (oldfirstln != Ed_ScrImage.firstln)
	Ed_RefreshAll = TRUE;
    Ed_Win->r = (Ed_WrapLines) ? (tmp + Ed_ColNow / Ed_Win->w) : tmp;
    Ed_Win->c = (Ed_WrapLines) ? (Ed_ColNow % Ed_Win->w) : Ed_ColNow;
    if (Ed_MarkType != ED_NOMARK)
	Ed_MustRefresh = TRUE;
    Ed_WinRefresh();
}


static void NEAR 
Ed_ChgCol(short c)
{
    if (c < 0 && Ed_LineNow > 0L)
    {
	(void)Ed_Point(Ed_LineNow - 1, TRUE);
	c = (short)ED_LEN;
    } else if (c > (short)ED_LEN && Ed_LineNow < Ed_LastLine)
    {
	(void)Ed_Point(Ed_LineNow + 1, TRUE);
	c = 0;
    }
    Ed_FixCursor(Ed_LineNow, Ed_PrefCol = c);
}

short Ed_OldLine;
short Ed_OldCol;

static void NEAR 
Ed_SavePosition()
{
    Ed_OldLine = Ed_LineNow;
    Ed_OldCol = Ed_ColNow;
}

#define Ed_RestorePosition()	Ed_FixCursor(Ed_OldLine,Ed_OldCol)

/************************
* Basic Cursor Movement *
************************/

static void NEAR Ed_Up()
{
    Ed_ChgRow(Ed_LineNow - 1);
}
static void NEAR Ed_Down()
{
    Ed_ChgRow(Ed_LineNow + 1);
}
static void NEAR Ed_Right()
{
    Ed_ChgCol(Ed_ColNow + 1);
}
static void NEAR Ed_Left()
{
    Ed_ChgCol(Ed_ColNow - 1);
}
static void NEAR Ed_Home()
{
    Ed_ChgCol(0);
}
static void NEAR Ed_End()
{
    Ed_ChgCol((short)ED_LEN);
}
void Ed_TopFile()
{
    Ed_FixCursor(0, 0);
}
void Ed_EndFile()
{
    Ed_ChgRow(Ed_LastLine);
    Ed_ChgCol((short)ED_LEN);
}
static void NEAR 
Ed_PageUp()
{
    if (Ed_LineNow != Ed_ScrImage.firstln)
	Ed_ChgRow(Ed_ScrImage.firstln);
    else
	Ed_ChgRow(Ed_LineNow - Ed_Win->h + 1);
}

static void NEAR 
Ed_PageDown()
{
    if (Ed_LineNow != Ed_ScrImage.lastln)
	Ed_ChgRow(Ed_ScrImage.lastln);
    else
	Ed_ChgRow(Ed_LineNow + Ed_Win->h - 1);
}

/******************************************************/

static short LinePlaceMarks[ED_PLACEMARKS];
static short ColPlaceMarks[ED_PLACEMARKS];
static short NumPlaceMarks = 0;
static short NextPlaceMarkIn = 0;
static short NextPlaceMarkOut = 0;

void 
Ed_MarkPlace()
{
    LinePlaceMarks[NextPlaceMarkIn] = Ed_LineNow;
    ColPlaceMarks[NextPlaceMarkIn] = Ed_ColNow;
    NextPlaceMarkOut = NextPlaceMarkIn;
    NextPlaceMarkIn++;
    NumPlaceMarks++;
    if (NumPlaceMarks > 9)
	NumPlaceMarks = 9;
    if (NextPlaceMarkIn > 9)
	NextPlaceMarkIn = 0;
    Ed_ShowMessage("Location marked");
}

void 
Ed_GotoMark()
{
    if (NumPlaceMarks)
    {
	Ed_FixCursor(LinePlaceMarks[NextPlaceMarkOut],
		     ColPlaceMarks[NextPlaceMarkOut]);
	NextPlaceMarkOut--;
	if (NextPlaceMarkOut < 0)
	    NextPlaceMarkOut = NumPlaceMarks - 1;
    } else
    {
	Ed_ShowMessage("No locations marked");
	beep();
    }
}

/***********************
* Word Cursor Movement *
************************/

void 
Ed_LeftWord()
{
    char c;
    Ed_Left();
    do
	Ed_Left();
    while (c = ED_TEXT[Ed_ColNow],
	   ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) && (Ed_LineNow + Ed_ColNow > 0));
    if (Ed_LineNow + Ed_ColNow)
	Ed_Right();
}

void 
Ed_RightWord()
{
    char c;
    Ed_Right();
    do
	Ed_Right();
    while (c = ED_TEXT[Ed_ColNow],
	   ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) &&
	   (Ed_LineNow < Ed_LastLine || Ed_ColNow < (short)ED_LEN));
    while (c = ED_TEXT[Ed_ColNow],
	   ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) &&
	   (Ed_LineNow < Ed_LastLine || Ed_ColNow < (short)ED_LEN))
	Ed_Right();

}

/****************************************
* Bracket, brace and parenthesis moves	*
*****************************************/

char LeftSearch[ED_SEARCHSETLEN] = {'[', ']', '{', '}', '(', ')', 0};
char RightSearch[ED_SEARCHSETLEN] = {'[', ']', '{', '}', '(', ')', 0};

void 
Ed_LeftSearch()
{
    Ed_SavePosition();
    Ed_Left();			/* To prevent matching at current cursor
				 * position */

    while (Ed_LineNow != 0 || Ed_ColNow != 0)
    {
	if (ED_TEXT[Ed_ColNow])
	    if (strrchr(LeftSearch, ED_TEXT[Ed_ColNow]) != NULL)
		break;
	if (--Ed_ColNow < 0)
	{
	    (void)Ed_Point(Ed_LineNow - 1, FALSE);
	    Ed_ColNow = (short)ED_LEN;
	}
    }

    /* Check line 0, col 0 if necessary */

    if (strrchr(LeftSearch, ED_TEXT[Ed_ColNow]) != NULL)
    {
	Ed_PrefCol = Ed_ColNow;
	Ed_ReCursor();
    } else
	Ed_RestorePosition();
}

void 
Ed_RightSearch()
{
    BOOLEAN found = FALSE;
    short len;
    Ed_SavePosition();
    Ed_Right();

    len = (short)ED_LEN;
    while (Ed_LineNow != Ed_LastLine || Ed_ColNow != len)
    {
	if (ED_TEXT[Ed_ColNow])
	    if (strrchr(RightSearch, ED_TEXT[Ed_ColNow]) != NULL)
	    {
		found = TRUE;
		break;
	    }
	if (++Ed_ColNow > len)
	{
	    Ed_ColNow = 0;
	    (void)Ed_Point(Ed_LineNow + 1, FALSE);
	    len = (short)ED_LEN;
	}
    }
    if (!found)
	Ed_RestorePosition();
    else
    {
	Ed_PrefCol = Ed_ColNow;
	Ed_ReCursor();
    }
}

/************************
* Search and Translate	*
************************/

char Ed_SearchText[ED_SEARCHTEXTLEN];
char Ed_ReplaceText[ED_SEARCHTEXTLEN];
char Ed_RevSearchText[ED_SEARCHTEXTLEN];
BOOLEAN Ed_Reversed = FALSE;
BOOLEAN Ed_MadeUpperCase = FALSE;
BOOLEAN Ed_CaseSensitive = TRUE;
BOOLEAN Ed_IdentsOnly = FALSE;
static BOOLEAN Ed_ForwardFound = FALSE;

void 
Ed_SearchForward()
{
    short startpos = (short)strlen(Ed_SearchText) - 1;
    short matchpos = Ed_ColNow + startpos + 1;
    Ed_ForwardFound = FALSE;
    if (!Ed_CaseSensitive && !Ed_MadeUpperCase)
    {
	(void)strupr(Ed_SearchText);
	Ed_MadeUpperCase = TRUE;
    }
    Ed_SavePosition();
    while (!Ed_ForwardFound)
    {
	while (matchpos >= ED_LEN && Ed_LineNow < Ed_LastLine)
	{
	    (void)Ed_Point(Ed_LineNow + 1, FALSE);
	    matchpos = startpos;
	}
	matchpos = Gen_IsMatch(ED_TEXT, Ed_SearchText, matchpos,
			       Ed_CaseSensitive, Ed_IdentsOnly);
	if (matchpos == -1)
	    if (Ed_LineNow == Ed_LastLine)
		break;
	    else
		matchpos = ED_LEN;
	else
	    Ed_ForwardFound = TRUE;
    }
    if (!Ed_ForwardFound)
    {
	Ed_RestorePosition();
	Ed_ShowMessage("No Match");
	beep();
    } else
	Ed_ChgCol(matchpos - startpos);
}

void 
Ed_SearchBackward()
{
    BOOLEAN found = FALSE;
    short startpos = (short)strlen(Ed_SearchText) - 1;
    short matchpos = ED_LEN - Ed_ColNow + startpos;
    char RevLine[256];
    Ed_SavePosition();
    strcpy(RevLine, ED_TEXT);
    if (!Ed_Reversed)
    {
	strcpy(Ed_RevSearchText, Ed_SearchText);
	(void)strrev(Ed_RevSearchText);
	Ed_Reversed = TRUE;
    }
    if (!Ed_CaseSensitive && !Ed_MadeUpperCase)
    {
	(void)strupr(Ed_RevSearchText);
	Ed_MadeUpperCase = TRUE;
    }
    while (!found)
    {
	while (matchpos < startpos && Ed_LineNow > 0)
	{
	    (void)Ed_Point(Ed_LineNow - 1, FALSE);
	    strcpy(RevLine, ED_TEXT);
	    matchpos = startpos;
	}
	(void)strrev(RevLine);
	matchpos = Gen_IsMatch(RevLine, Ed_RevSearchText, matchpos,
			       Ed_CaseSensitive, Ed_IdentsOnly);
	if (matchpos == -1)
	{
	    if (Ed_LineNow == 0)
		break;
	} else
	    found = TRUE;
    }
    if (!found)
    {
	Ed_RestorePosition();
	Ed_ShowMessage("No Match");
	beep();
    } else
	Ed_ChgCol(ED_LEN - 1 - matchpos);
}


void 
Ed_Translate()
{
    BOOLEAN global = FALSE;
    short numreps = 0;
    BOOLEAN doreplace;
    short cut_end = (int)strlen(Ed_SearchText) - 1;
    short rep_len = (int)strlen(Ed_ReplaceText);
    ushort reply;
    char msg[80];
    short origline = Ed_LineNow;
    short origcol = Ed_ColNow;
    while (Ed_SearchForward(), Ed_ForwardFound)
    {
	doreplace = FALSE;
	if (!global)
	{
	    Win_PutAttribute(ED_INVRS_COLOR, Ed_Win->c, Ed_Win->r, cut_end + 1, 1);
	    reply = Gen_GetResponse(Msg_Win, "Replace?", "(YNG)", TRUE, 5, HLP_TRANS);
	    Win_PutAttribute(ED_INSIDE_COLOR, Ed_Win->c, Ed_Win->r, cut_end + 1, 1);
	    if (reply == KB_ESC)
		break;
	    if (reply == 'g' || reply == 'G')
		global = TRUE;
	    if (reply == 'y' || reply == 'Y')
		doreplace = TRUE;
	} else
	    doreplace = TRUE;
	if (doreplace)
	{
	    numreps++;
	    Ed_CutCols(Ed_ColNow, Ed_ColNow + cut_end, FALSE);
	    Ed_InsertStr(Ed_ReplaceText);
	    Ed_ChgCol(Ed_ColNow + rep_len);
	}
    }
    sprintf(msg, "%d replacements ( %s / %s ) made", numreps, Ed_SearchText,
	    Ed_ReplaceText);
    Ed_ShowMessage(msg);
    Ed_FixCursor(origline, origcol);
}

/********
* Undo	*
********/

void 
Ed_Undo()
{
    char *stmp;
    short slen = (int)strlen(Ed_UndoBuff);
    Ed_FileSize += (long)(slen - ED_LEN);
    Ed_Allocate(STRING, stmp, slen + 1, 51);
    strcpy(stmp, Ed_UndoBuff);
    Ed_ReplaceLine(Ed_BufNow, stmp, slen);
    Ed_ChgCol(Ed_UndoCol);
    Ed_ShowMessage("Undone");
}

/********************************
*   Low-level Scrap Operations	*
*********************************/

static void NEAR 
Ed_CopyCols(short from, short to)
{
    short len;
    char tmp[256];
    if (to == (-1) || to >= ED_LEN)
	to = ED_LEN - 1;
    len = to - from + 1;
    if (from >= ED_LEN)
	len = 0;
    strncpy(tmp, ED_TEXT + from, (unsigned int)len);
    tmp[len] = NUL;
    (void)Ed_AddLine(&Ed_ScrapBuf, tmp);
}

static void NEAR 
Ed_CutCols(short from, short to, BOOLEAN put_in_scrap)
{
    STRING stmp;
    short len;
    if (from >= ED_LEN)
    {
	if (put_in_scrap)
	    Ed_CopyCols(0, 0);	/* Create an empty scrap line */
	return;
    }
    if (to == (-1) || to >= ED_LEN)
	to = ED_LEN - 1;
    len = to - from + 1;
    if (put_in_scrap)
	Ed_CopyCols(from, to);
    Ed_Allocate(STRING, stmp, ED_LEN - len + 1, 52);
    strncpy(stmp, ED_TEXT, (unsigned int)from);
    strncpy(stmp + from, ED_TEXT + to + 1, (unsigned int)(ED_LEN - to - 1));
    Ed_ReplaceLine(Ed_BufNow, stmp, ED_LEN - len);
    Ed_FileSize -= (long)len;
    Ed_ChgCol(from);
    Ed_FileChanged = TRUE;
    Ed_IsCompiled = FALSE;
}

/********************************
*    Main editing functions	*
********************************/

/********************************
* Called from:	Ed_AddScrap	*
*		Ed_LoadFile	*
********************************/

static short NEAR 
Ed_AddLine(BUFPTR * now, STRING txt)
{
    char stmp1[256];
    STRING stmp2;
    short len = 0;
    if (*now == NULL)
	Ed_AllocBuff(now, NULL, NULL);
    if (txt != NULL && txt[0])
    {
	len = (short)strlen(Gen_ExpandTabs(txt, stmp1, (int)Ed_Win->tabsize));
	Ed_Allocate(STRING, stmp2, len, 53);
	if (stmp1[len - 1] == '\n')
	    len--;		/* strip newline */
	strncpy(stmp2, stmp1, (unsigned int)len);
	Ed_ReplaceLine(*now, stmp2, len);
    } else
	Ed_ReplaceLine(*now, NULL, 0);
    Ed_AllocBuff(&(*now)->next, *now, (*now)->next);
    *now = (*now)->next;
    return len;
}

static void NEAR 
Ed_InsertCh(char ch)
{
    STRING stmp;
    if (ED_LEN < ED_MAXLINELEN)
    {
	Ed_Allocate(STRING, stmp, 2 + (int)ED_LEN, 54);
	strncpy(stmp, ED_TEXT, (unsigned int)Ed_ColNow);
	stmp[Ed_ColNow] = ch;
	strncpy(stmp + Ed_ColNow + 1, ED_TEXT + Ed_ColNow, (unsigned int)(ED_LEN - Ed_ColNow));
	Ed_ReplaceLine(Ed_BufNow, stmp, ED_LEN + 1);
	Ed_FileSize++;
	Ed_Right();
	Ed_FileChanged = TRUE;
	Ed_IsCompiled = FALSE;
    } else
	beep();
}

static void NEAR 
Ed_InsertStr(STRING str)
{
    STRING stmp;
    short len, sublen;
    sublen = str ? strlen(str) : 0;
    len = (short)(ED_LEN + sublen);
    if (len <= ED_MAXLINELEN)
    {
	Ed_Allocate(STRING, stmp, len + 1, 55);
	strncpy(stmp, ED_TEXT, (unsigned int)Ed_ColNow);
	if (str)
	    strcpy(stmp + Ed_ColNow, str);
	strncat(stmp, ED_TEXT + Ed_ColNow, (unsigned int)(ED_LEN - Ed_ColNow));
	Ed_ReplaceLine(Ed_BufNow, stmp, len);
	Ed_FileSize += (long)(sublen);
	Ed_FileChanged = TRUE;
	Ed_IsCompiled = FALSE;
    } else
	beep();
}


static BUFPTR NEAR 
Ed_RemoveLine(BUFPTR b)
/* NB - to delete current line, call
    Ed_BufNow = Ed_RemoveLine(Ed_BufNow);
***/
{
    BUFPTR p = b->prev, n = b->next;
    if (p != NULL)
	p->next = n;
    if (n != NULL)
	n->prev = p;
    Mem_Free(b->text);
    Mem_Free(b);
    return n ? n : p;
}

void 
Ed_DeleteLine()
{
    if (Ed_BufNow)
    {
	Ed_FileSize -= ED_LEN;
	Ed_BufNow = Ed_RemoveLine(Ed_BufNow);
	if (Ed_LineNow == Ed_LastLine)
	    Ed_LineNow--;
	else
	    Ed_FileSize -= 2l;	/* CR/LF */
	Ed_LastLine--;
	Ed_FileChanged = Ed_MustRefresh = TRUE;
	Ed_IsCompiled = FALSE;
	Ed_ReCursor();
    }
}

static void NEAR 
Ed_JoinLines()
{
    STRING stmp;
    if (Ed_LineNow > 0)
    {
	short len = (short)ED_LEN;
	Ed_Up();
	len += (short)ED_LEN;
	if (len > ED_MAXLINELEN)
	{
	    beep();
	    Ed_Down();
	    return;
	}
	Ed_Allocate(STRING, stmp, len + 1, 56);
	if (ED_TEXT != NULL)
	    strcpy(stmp, ED_TEXT);
	else
	    *stmp = '\0';
	if (ED_NEXTTEXT != NULL)
	    strcat(stmp, ED_NEXTTEXT);
	Ed_ChgCol(ED_LEN);
	Ed_ReplaceLine(Ed_BufNow, stmp, len);
	Ed_FileSize -= 2l;
	(void)Ed_RemoveLine(ED_NEXT);
	Ed_LastLine--;
	Ed_RefreshAll = TRUE;
	Ed_ReCursor();
    }
}

static void NEAR 
Ed_BackSpace(uchar keybd_status)
{
    if (Ed_ColNow > 0)
    {
	if (keybd_status & 3)
	{			/* Shift keys? */
	    short i, tabstop, tmp = Ed_ColNow % (int)(Ed_Win->tabsize);
	    tabstop = tmp ? Ed_ColNow - tmp : Ed_ColNow - (int)(Ed_Win->tabsize);
	    if (tabstop >= 0)
	    {
		for (i = Ed_ColNow - 1; i >= tabstop; i--)
		    if (ED_TEXT[i] != ' ')
			break;
		if (i < Ed_ColNow - 1)
		    Ed_CutCols(i + 1, Ed_ColNow - 1, FALSE);
	    }
	} else
	    Ed_CutCols(Ed_ColNow - 1, Ed_ColNow - 1, FALSE);
    } else
	Ed_JoinLines();
    Ed_FileChanged = TRUE;
    Ed_IsCompiled = FALSE;
}

static void NEAR 
Ed_InsertCR()
{
    short tlen;
    STRING ctmp, txt, nxttxt;

    /* Insert new buffer record */

    Ed_AllocBuff(&ED_NEXT, Ed_BufNow, ED_NEXT);
    Ed_LastLine++;

    ctmp = ED_TEXT;
    tlen = max(0, ED_LEN - Ed_ColNow);	/* length of new line */
    Ed_Allocate(STRING, txt, Ed_ColNow + 1, 57);
    Ed_Allocate(STRING, nxttxt, tlen + 1, 58);

    strncpy(txt, ctmp, (unsigned int)Ed_ColNow);
    strncpy(nxttxt, ctmp + Ed_ColNow, (unsigned int)tlen);
    Ed_ReplaceLine(Ed_BufNow, txt, Ed_ColNow);
    (void)Ed_Point(Ed_LineNow + 1, TRUE);
    Ed_ReplaceLine(Ed_BufNow, nxttxt, tlen);
    Ed_FileSize += 2l;
    Ed_ChgCol(0);
    Ed_FileChanged = TRUE;
    Ed_IsCompiled = FALSE;
}


/********************************
* Column/block mark operations	*
********************************/

void 
Ed_Mark()
{
    if (Ed_MarkType == ED_NOMARK)
    {
	Ed_MarkType = (ed_op == _Ed_MarkBlock) ? ED_BLOKMARK : ED_COLMARK;
	Ed_MarkLine = Ed_LineNow;
	Ed_MarkCol = Ed_ColNow;
    } else
	Ed_MarkType = ED_NOMARK;
    Ed_RefreshAll = TRUE;
    Ed_WinRefresh();
    Ed_ShowMessage(Ed_MarkType == ED_BLOKMARK ?
		   "Block mark set" : "Column mark set");
}

void 
Ed_BlockOp()
{
    short line;

    if (Ed_MarkType == ED_NOMARK)
    {
	if (Ed_ColNow < ED_LEN)
	    if (ed_op != _Ed_Copy)
		Ed_CutCols(Ed_ColNow, Ed_ColNow, FALSE);
	    else
	    {
		Ed_ShowMessage("No mark set");
		beep();
	    }
    } else
    {
	Ed_ShowMessage(ed_op != _Ed_Copy ?
		       "Cut text to scrap" :
		       "Copied text to scrap");
	Ed_FreeAll(&Ed_ScrapBuf);
	Ed_SetUpMarkCoords();
	Ed_QuietMode = TRUE;	/* Prevent WinRefresh from corrupting mark
				 * coordinates */
	line = Ed_MarkLBegin;
	while (line <= Ed_MarkLEnd)
	{
	    (void)Ed_Point(line, FALSE);
	    if (ed_op != _Ed_Copy)
		Ed_CutCols(Ed_MarkStart(line), Ed_MarkStop(line), TRUE);
	    else
		Ed_CopyCols(Ed_MarkStart(line), Ed_MarkStop(line));
	    line++;
	}
	if (ed_op != _Ed_Copy && Ed_MarkType == ED_BLOKMARK && Ed_MarkLBegin < Ed_MarkLEnd)
	{
	    line = Ed_MarkLBegin;
	    while (line < Ed_MarkLEnd)
	    {
		(void)Ed_Point(line++, FALSE);
		if (ED_LEN == 0)
		{
		    Ed_JoinLines();
		    Ed_MarkLEnd--;
		}
	    }
	    (void)Ed_Point(Ed_MarkLEnd, FALSE);
	    Ed_JoinLines();
	}
	Ed_MarkType = ED_NOMARK;
	Ed_QuietMode = FALSE;
	Ed_RefreshAll = TRUE;
	Ed_FixCursor(Ed_MarkLBegin, Ed_PrefCol = Ed_MarkCBegin);
    }
    if (ed_op != _Ed_Copy)
    {
	Ed_FileChanged = TRUE;
	Ed_IsCompiled = FALSE;
    }
}

void 
Ed_BlockPaste()
{
    Ed_Rewind(&Ed_ScrapBuf);
    if (Ed_ScrapBuf != NULL)
    {
	short startline = Ed_LineNow + 1;
	BUFPTR scrapstart = Ed_ScrapBuf;
	Ed_QuietMode = TRUE;
	Ed_InsertCR();
	Ed_AllocBuff(&Ed_BufNow, Ed_BufNow->prev, Ed_BufNow);
	while (Ed_ScrapBuf != NULL && Ed_ScrapBuf->next != NULL)
	{
	    Ed_FileSize += (long)(2l + Ed_AddLine(&Ed_BufNow, Ed_ScrapBuf->text));
	    Ed_LastLine++;
	    Ed_LineNow++;
	    Ed_ScrapBuf = Ed_ScrapBuf->next;
	}
	Ed_BufNow = Ed_RemoveLine(Ed_BufNow);
	Ed_JoinLines();
	(void)Ed_Point(startline, FALSE);
	Ed_JoinLines();
	Ed_ScrapBuf = scrapstart;	/* Can't rewind as currently NULL */
	Ed_QuietMode = FALSE;
	Ed_RefreshAll = TRUE;
	Ed_ShowMessage("Block pasted");
	Ed_WinRefresh();
    } else
    {
	Ed_ShowMessage("No scrap to paste!");
	beep();
    }
}

void 
Ed_ColumnPaste()
{
    Ed_Rewind(&Ed_ScrapBuf);
    if (Ed_ScrapBuf != NULL)
    {
	BUFPTR scrapstart = Ed_ScrapBuf;
	short pastecol = Ed_ColNow;
	char spaceline[256];
	Ed_SavePosition();
	Ed_QuietMode = TRUE;
	while (Ed_ScrapBuf != NULL && Ed_ScrapBuf->next != NULL /* Skip extra one at end */ )
	{
	    register short i = pastecol - ED_LEN;
	    if (i > 0)
	    {
		spaceline[i] = NUL;
		while (i > 0)
		    spaceline[--i] = ' ';
		Ed_ColNow = ED_LEN;
		Ed_InsertStr(spaceline);
	    }
	    Ed_ColNow = pastecol;
	    Ed_InsertStr(Ed_ScrapBuf->text);
	    if (Ed_LineNow == Ed_LastLine)
	    {
		Ed_AllocBuff(&Ed_BufNow->next, Ed_BufNow, NULL);
		Ed_FileSize += 2;
		Ed_LastLine++;
	    }
	    (void)Ed_Point(Ed_LineNow + 1, FALSE);
	    Ed_ScrapBuf = Ed_ScrapBuf->next;
	}
	Ed_RestorePosition();
	Ed_ScrapBuf = scrapstart;	/* Restore scrap pointer */
	Ed_QuietMode = FALSE;
	Ed_RefreshAll = TRUE;
	Ed_ShowMessage("Columns pasted");
	Ed_WinRefresh();
    } else
    {
	Ed_ShowMessage("No scrap to paste!");
	beep();
    }
}

/*********************************/
/* Language-Dependent Operations */
/*********************************/

static void NEAR 
Ed_GetCurrentWord(short *start, short *len)
{
    char c;
    short end;
    *start = Ed_ColNow;
    *len = 0;
    while (c = ED_TEXT[*start], c = (char)toupper(c), *start >= 0 && (c == '_' || (c >= 'A' && c <= 'Z')))
    {
	*start -= 1;
	*len += 1;
    }
    *start += 1;
    end = *start + *len;
    while (c = ED_TEXT[end], c = (char)toupper(c), end < ED_LEN && (c == '_' || (c >= 'A' && c <= 'Z')))
    {
	end++;
	*len += 1;
    }
}

void 
Ed_LangHelp()
{
    short start, len;
    char index[40];
    Ed_GetCurrentWord(&start, &len);
    strncpy(index, ED_TEXT + start, (unsigned)len);
    index[len] = '\0';
    Gen_BuildName("PEWPATH", "LANGHELP", Hlp_FileName);
    Hlp_SHelp(index);
    Gen_BuildName("PEWPATH", "PEW_HELP", Hlp_FileName);
}

void 
Ed_Template()
{
    short start, len;
    char index[40];
    Ed_GetCurrentWord(&start, &len);
    if (len)
    {
	char buff[512], tmpl_name[80];
	Gen_BuildName("PEWPATH", "TEMPLATE", tmpl_name);
	strncpy(index, ED_TEXT + start, (unsigned)len);
	index[len] = '\0';
	if (Gen_SGetFileEntry(tmpl_name, index, buff, 512, FALSE) == SUCCESS)
	{
	    char new_line[80], *s1, *s2;
	    short startline = Ed_LineNow + 1, notdone = (int)TRUE;
	    s1 = buff;
	    Ed_QuietMode = TRUE;
	    Ed_CutCols(start, start + len - 1, FALSE);	/* remove keyword */
	    Ed_InsertCR();
	    Ed_AllocBuff(&Ed_BufNow, Ed_BufNow->prev, Ed_BufNow);
	    while (notdone)
	    {
		s2 = new_line;
		while ((notdone = *s1) != 0 && ((*s2++ = *s1) != '\n'))
		    s1++;
		if (notdone)
		    notdone = *++s1;	/* skip \n */
		*--s2 = '\0';	/* strip \n and terminate */
		Ed_FileSize += (long)(2l + Ed_AddLine(&Ed_BufNow, new_line));
		Ed_LastLine++;
		Ed_LineNow++;
	    }
	    Ed_BufNow = Ed_RemoveLine(Ed_BufNow);
	    Ed_JoinLines();
	    (void)Ed_Point(startline, FALSE);
	    Ed_JoinLines();
	    Ed_QuietMode = FALSE;
	    Ed_RefreshAll = TRUE;
	    Ed_ShowMessage("Template expanded");
	    Ed_WinRefresh();
	} else
	    beep();
    } else
	beep();
}

/************************
* File Operations	*
*************************/


void 
Ed_LoadFile(STRING name, BOOLEAN mustexist)
{
    FILE *fp;
    char buff[256];
    if ((fp = fopen(name, "rt")) != NULL)
    {
	if (Ed_InitBuff(name) != KB_ESC)
	{
	    while (!feof(fp))
	    {
		if (fgets(buff, 256, fp) != NULL)
		{
		    Ed_LastLine++;
		    Ed_FileSize += (long)(2l + Ed_AddLine(&Ed_BufNow, buff));
		    Ed_LineNow++;
		}
		if (Ed_LineNow % 10 == 0)
		    Ed_ShowPos(FALSE);
	    }
	    if (strchr(buff, '\n') == NULL)
	    {
		Ed_BufNow = Ed_BufNow->prev;
		Mem_Free(Ed_BufNow->next);
		Ed_BufNow->next = NULL;
		Ed_FileSize -= 2l;	/* Correct for last line */
		Ed_LineNow--;
		Ed_LastLine--;
	    }
	    Ed_FixCursor(0, 0);
	    if (ED_TEXT != NULL)
		strcpy(Ed_UndoBuff, ED_TEXT);
	}
	fclose(fp);
	Ed_ShowMessage("File loaded");
    } else if (mustexist)
	Error(TRUE, ERR_NOOPENFL, name);
    else
	(void)Ed_InitBuff(name);/* Clear and initialise new file */
    Ed_ShowPos(TRUE);
    Ed_GetBaseFileName();
}

void 
Ed_WriteFile(STRING name)
{
    FILE *fp;
    char backupname[80], drive[MAXDRIVE], directory[MAXDIR], basename[MAXFILE],
        extension[MAXEXT];
    short line = 0, oldline = Ed_LineNow;
    if (!Ed_FileChanged)
	return;
    if (Ed_BackUp)
    {
	if (access(name, 0) == 0)
	{
	    (void)fnsplit(name, drive, directory, basename, extension);
	    fnmerge(backupname, drive, directory, basename, ".BAK");
	    (void)rename(name, backupname);
	}
    }
    if ((fp = fopen(name, "wt")) != NULL)
    {
	char tmp[260];
	while (line <= Ed_LastLine)
	{
	    short i;
	    (void)Ed_Point(line++, FALSE);
	    if (Ed_FillTabs)
		i = (int)strlen(Gen_CompressSpaces(ED_TEXT, tmp, (int)(Ed_Win->tabsize)));
	    else
		i = (int)strlen(strcpy(tmp, ED_TEXT));
	    if (line <= Ed_LastLine)
		tmp[i++] = '\n';
	    tmp[i] = 0;
	    fputs(tmp, fp);
	}
	fclose(fp);
/*		if (Ed_FileChanged) Ed_IsCompiled = FALSE;*/
	Ed_FileChanged = FALSE;
	Ed_ShowMessage("File saved");
    } else
	Error(TRUE, ERR_NOOPENFL, name);
    (void)Ed_Point(oldline, TRUE);
}


/* Merge with Ed_LoadFile????? */


static void NEAR 
Ed_LoadList(BUFPTR * b, STRING fname, BOOLEAN mustexist)
{
    FILE *fp;
    char buff[256];
    if ((fp = fopen(fname, "rt")) != NULL)
    {
	Ed_FreeAll(b);
	while (!feof(fp))
	{
	    (void)fgets(buff, 256, fp);
	    if (!feof(fp))
	    {
		(void)Ed_AddLine(b, buff);
	    }
	}
	*b = (*b)->prev;
	Mem_Free((*b)->next);
	(*b)->next = NULL;
	fclose(fp);
    } else if (mustexist)
	Error(TRUE, ERR_NOOPENFL, fname);
}


void 
Ed_LoadScrap(STRING name)
{
    Ed_ShowMessage("Loading scrap");
    Ed_LoadList(&Ed_ScrapBuf, name, TRUE);
}

void 
Ed_WriteScrap(STRING name)
{
    FILE *fp;
    STRING line;
    if (Ed_ScrapBuf == NULL)
	return;
    if ((fp = fopen(name, "wt")) != NULL)
    {
	Ed_Rewind(&Ed_ScrapBuf);
	while ((line = Ed_ScrapBuf->text) != NULL)
	{
	    Ed_ScrapBuf = Ed_ScrapBuf->next;
	    fputs(line, fp);
	    fputs("\n", fp);
	}
	fclose(fp);
	Ed_ShowMessage("Written scrap");
    } else
	Error(TRUE, ERR_NOOPENFL, name);
}


#define MASKCHECK	0x12348765L

void 
Ed_SaveConfig()
{
    FILE *cfp;
    short i;
    long Ed_MaskChk = MASKCHECK;
    if ((cfp = fopen("PEW.CFG", "wb")) == NULL)
	return;
    for (i = 0; i < (int)_Ed_NumKeyDefs; i++)
	fwrite(&Ed_Key[i], sizeof(Ed_Key[0]), 1, cfp);
    fwrite(&Ed_BackUp, sizeof(int), 1, cfp);
    fwrite(&Ed_FillTabs, sizeof(int), 1, cfp);
    fwrite(&Ed_WrapLines, sizeof(int), 1, cfp);
    fwrite(&Ed_TabStop, sizeof(ushort), 1, cfp);
    fwrite(&Ed_ConfigSave, sizeof(int), 1, cfp);
    fwrite(LeftSearch, sizeof(char), ED_SEARCHSETLEN, cfp);
    fwrite(RightSearch, sizeof(char), ED_SEARCHSETLEN, cfp);
    fwrite(Ed_SearchText, ED_SEARCHTEXTLEN, 1, cfp);
    fwrite(Ed_ReplaceText, ED_SEARCHTEXTLEN, 1, cfp);
    fwrite(&Ed_CaseSensitive, sizeof(int), 1, cfp);
    fwrite(&Ed_IdentsOnly, sizeof(int), 1, cfp);
    fwrite(&Ed_MaskChk, sizeof(long), 1, cfp);
    fwrite(&Ed_IDCnt, sizeof(int), 1, cfp);
    fwrite(Ed_FileName, MAXFILENAMELEN, 1, cfp);
    fwrite(Ed_LoadName, MAXFILENAMELEN, 1, cfp);
    fwrite(&Ed_LineNow, sizeof(short), 1, cfp);
    fwrite(&Ed_ColNow, sizeof(short), 1, cfp);
    fclose(cfp);
}

static BOOLEAN NEAR 
Ed_LoadConfig(void)
{
    FILE *cfp;
    short i;
    long Ed_MaskChk = MASKCHECK;
    if ((cfp = fopen("PEW.CFG", "rb")) == NULL)
	return FALSE;
    for (i = 0; i < (int)_Ed_NumKeyDefs; i++)
	(void)fread(&Ed_Key[i], sizeof(Ed_Key[0]), 1, cfp);
    (void)fread(&Ed_BackUp, sizeof(int), 1, cfp);
    (void)fread(&Ed_FillTabs, sizeof(int), 1, cfp);
    (void)fread(&Ed_WrapLines, sizeof(int), 1, cfp);
    (void)fread(&Ed_TabStop, sizeof(ushort), 1, cfp);
    if (Ed_TabStop <= 0 || Ed_TabStop > 80)
	Ed_TabStop = 8;
    Ed_Win->tabsize = (ushort) Ed_TabStop;
    (void)fread(&Ed_ConfigSave, sizeof(int), 1, cfp);
    (void)fread(Ed_LeftSearch, sizeof(char), ED_SEARCHSETLEN, cfp);
    (void)fread(Ed_RightSearch, sizeof(char), ED_SEARCHSETLEN, cfp);
    (void)fread(Ed_SearchText, ED_SEARCHTEXTLEN, 1, cfp);
    (void)fread(Ed_ReplaceText, ED_SEARCHTEXTLEN, 1, cfp);
    (void)fread(&Ed_CaseSensitive, sizeof(int), 1, cfp);
    (void)fread(&Ed_IdentsOnly, sizeof(int), 1, cfp);
    (void)fread(&Ed_MaskChk, sizeof(long), 1, cfp);
    if (Ed_MaskChk != MASKCHECK)
    {
	(void)Win_Error(NULL, "", "Error in configuration file - delete it.", FALSE, TRUE);
	Fatal(NULL);
    }
    if (*Ed_FileName == NUL)
    {
	(void)fread(&Ed_IDCnt, sizeof(int), 1, cfp);
	(void)fread(Ed_FileName, MAXFILENAMELEN, 1, cfp);
	(void)fread(Ed_LoadName, MAXFILENAMELEN, 1, cfp);
	(void)fread(&Ed_LineNow, sizeof(short), 1, cfp);
	(void)fread(&Ed_ColNow, sizeof(short), 1, cfp);
    }
    fclose(cfp);
    return TRUE;
}

/************************
*  Invoke the compiler	*
************************/


void 
Ed_CompileUsing(char *name, short value)
{
    extern BOOLEAN run_compiler(char *, short);
    extern short ec_errline;
    extern short ec_errcol;
    WINPTR Old_Win = Win_Current;
    WINPTR Comp_Win;
    Ed_SavePosition();
    Comp_Win = Win_Make(WIN_BORDER | WIN_CURSOROFF | WIN_NONDESTRUCT, 26, 13, 28, 6, ED_BORDER_COLOR, ED_INVRS_COLOR);
    Win_Activate(Comp_Win);
    Win_PutTitle("< Compiler >", SCR_TOP, SCR_CENTRE);
    Win_FastPutS(1, 2, "Compiling:");
    Win_FastPutS(1, 13, Ed_BaseFileName);
    Win_FastPutS(3, 2, "Lines Compiled :");
    (void)Ed_Point(0, FALSE);
    Ed_ColNow = 0;
    if (run_compiler(name, value))
    {
	extern BOOLEAN analyzing;
	Ed_IsCompiled = TRUE;
	if (!analyzing)
	{
	    Win_FastPutS(5, 2, "Press Any Key");
	    (void)Kb_RawGetC();
	}
    } else
    {
	Ed_IsCompiled = FALSE;
	Ed_OldCol = ec_errcol;
	Ed_OldLine = ec_errline - 1;
    }
    Win_Free(Comp_Win);
    if (Old_Win)
	Win_Activate(Old_Win);
    Ed_RestorePosition();
}

void 
Ed_Compile()
{
    Ed_CompileUsing(NULL, 0);	/* Menu version */
}

/*************************
* Invoke the interpreter *
*************************/

void 
Ed_ExecUntil(ulong until)
{
    if (!Ed_IsCompiled)
	Ed_Compile();
    if (Ed_IsCompiled)
    {
	extern void execute_spec(ulong);
	Ed_SavePosition();
	Ed_MarkType = ED_NOMARK;
	execute_spec(until);
	Win_Activate(Ed_Win);
	Ed_RestorePosition();
	Scr_CursorOn();
    }
}

void 
Ed_Execute()			/* Menu version */
{
    Ed_ExecUntil((ulong) 0l);
}

/****************************************
* Execute the command specified by the	*
* control characters c1 and c2.		*
*****************************************/

static ushort NEAR 
Ed_Command(ushort c, uchar keybd_status)
{
    ushort i;
    ushort rtn = c;
    BOOLEAN done = TRUE;
    enum editorcommand cmd;
    if ((char)rtn)
	switch ((char)rtn)
	{
	case KB_TAB:
	    i = Ed_Win->tabsize - (ushort) Ed_ColNow % Ed_Win->tabsize;
	    while (i--)
		Ed_InsertCh(' ');
	    break;
	case KB_CRGRTN:
	    Ed_InsertCR();
	    break;
	case KB_BACKSPC:
	    Ed_BackSpace(keybd_status);
	    break;
	default:
	    if (rtn >= ' ' && rtn <= '~')
		Ed_InsertCh((char)rtn);
	    else
		done = FALSE;
	    break;
	}
    else
	switch (rtn)
	{
	case KB_UP:
	    Ed_Up();
	    break;
	case KB_DOWN:
	    Ed_Down();
	    break;
	case KB_LEFT:
	    Ed_Left();
	    break;
	case KB_RIGHT:
	    Ed_Right();
	    break;
	case KB_HOME:
	    Ed_Home();
	    break;
	case KB_END:
	    Ed_End();
	    break;
	case KB_PGUP:
	    Ed_PageUp();
	    break;
	case KB_PGDN:
	    Ed_PageDown();
	    break;
	case KB_DEL:
	    if (Ed_ColNow >= ED_LEN && Ed_LineNow < Ed_LastLine
		&& Ed_MarkType == ED_NOMARK)
	    {
		Ed_PrefCol = Ed_ColNow;
		Ed_Down();
		Ed_JoinLines();
	    } else
	    {
		ed_op = _Ed_DelBlock;
		Ed_BlockOp();
	    }
	    break;
	case KB_F1:
	case KB_ALT_H:
	    (void)Mnu_Process(&Help_Menu, Msg_Win, 0);
	    Ed_RefreshAll = TRUE;
	    Ed_ReCursor();
	default:
	    done = FALSE;
	    break;
	}
    if (!done)
	if (rtn == Ed_Key[(int)_Ed_Quit])
	    rtn = Ed_Quit();
	else
	    for (cmd = _Ed_LeftWord; cmd < _Ed_NumKeyDefs; cmd++)
		if (rtn == Ed_Key[(int)cmd])
		{
		    (*(Ed_Func[(int)(ed_op = cmd)])) ();
		    break;
		}
    return rtn;
}


/**************************
* Set up the Edit Windows *
**************************/

static void NEAR 
Ed_Initialise()
{
    short i;
    ushort keypress;
    uchar foreground = 9;
    WINPTR tmpwin;
    Msg_Win = Win_Make(WIN_NOBORDER, 0, 24, 80, 1, 0, ED_INSIDE_COLOR);
    Pew_InitMainMenu();
    Win_1LineBorderSet();
    Ed_Win = Win_Make(WIN_WRAP | WIN_CLIP | WIN_BORDER, 1, 2, 78, 21, ED_BORDER_COLOR, ED_INSIDE_COLOR);
    Pos_Win = Win_Make(WIN_NOBORDER | WIN_CURSOROFF, 3, 1, 74, 1, 0, ED_BORDER_COLOR);
    Win_Activate(Ed_Win);
    if (Ed_LoadConfig() == FALSE)
    {
	Ed_AssignKeys();
	strcpy(Ed_LoadName, DEFAULTNAME);
	if (*Ed_FileName == NUL)
	    strcpy(Ed_FileName, DEFAULTNAME);
    }
    Ed_SavePosition();
    Ed_LoadFile(Ed_FileName, FALSE);
    Ed_RestorePosition();
    (void)Ed_Point(Ed_LineNow, TRUE);
    for (i = 0; i < (int)_Ed_NumKeyDefs; i++)
	Kb_GetKeyName(Ed_KeyNames[i], Ed_Key[i]);
    Ed_AssignFunctions();
    Ed_ReCursor();
    Win_2LineBorderSet();
    tmpwin = Win_Make(WIN_NONDESTRUCT | WIN_BORDER | WIN_CURSOROFF, 23, 6, 32, 12, SCR_NORML_ATTR, SCR_NORML_ATTR);
    Win_Activate(tmpwin);
    Win_SetAttribute(Scr_ColorAttr(LIGHTRED, BLACK));
    Win_FastPutS(1, 5, "The Estelle PEW  v2.1");
    Win_FastPutS(3, 7, "by Graham Wheeler");
    Win_SetAttribute(SCR_NORML_ATTR);
    Win_SetAttribute(Scr_ColorAttr(LIGHTBLUE, BLACK));
    Win_FastPutS(5, 1, "Department of Computer Science");
    Win_FastPutS(6, 13, "U.C.T.");
    Win_FastPutS(7, 4, "(c) 1989-1992");
    while (Kb_RawLookC(&keypress) == FALSE)
    {
	Win_SetAttribute(Scr_ColorAttr(foreground, BLACK));
	Win_FastPutS(9, 7, "Press F1 for Help");
	Win_FastPutS(10, 5, "Press any key to begin");
	delay(250);
	foreground++;
	if (foreground > 15)
	    foreground = 9;
    }
    if (keypress != KB_F1)
	(void)Kb_RawGetC();
    Win_Free(tmpwin);
    Win_Activate(Ed_Win);
    Gen_BuildName("PEWPATH", "PEW_HELP", Hlp_FileName);
}

/************************************************************************

	The main program simply sets up the screen, then repeatedly
	reads key presses and acts on them by calling the 'command'
	keystroke interpreter. If the character read is a null, then
	a special key has been pressed, and a second character must
	be read as well.

*************************************************************************/

ushort 
Ed_Quit()
{
    if (Ed_ConfigSave)
	Ed_SaveConfig();
    if (Ed_CleanUp() == KB_ESC)
	return KB_ESC;
    Win_Free(Ed_Win);
    Win_Free(Pos_Win);
    Win_Free(Msg_Win);
    Win_Free(Menu_Win);
    return (ushort) 0;
}

void 
Ed_Abort(short sig)
{
    extern void FAR Pew_EndSystem(short);
    if (Ed_Quit() == KB_ESC)
	return;
    Pew_EndSystem(sig);
}

static void NEAR 
Ed_WinResize(short lines, short cols)
{
    if (lines >= 0 && lines < 22 && cols >= 0 && cols <= 78)
    {
	Ed_SavePosition();
	Win_Free(Ed_Win);
	Win_1LineBorderSet();
	Ed_Win = Win_Make(WIN_WRAP | WIN_CLIP | WIN_BORDER, 1, 2, cols, lines, ED_BORDER_COLOR, ED_INSIDE_COLOR);
	Win_Activate(Ed_Win);
	Ed_RefreshAll = TRUE;
	Ed_ScrImage.firstln = Ed_ScrImage.lastln = Ed_ScrImage.lastlnrow = 0;
	Ed_RestorePosition();
	Ed_ShowPos(TRUE);
    }
}

void 
Ed_ToggleSize()
{
    if (Ed_Win->h == 21)
	Ed_WinResize(5, 78);
    else
	Ed_WinResize(21, 78);
}

void 
Fatal(char *msg)
{
    Scr_EndSystem();
    if (msg)
    {
	puts("PEW Fatal Error : ");
	puts(msg);
    }
    if (Ed_FileChanged)
    {
	puts("Saving editor buffer to RECOVER.EST\n");
	Ed_BackUp = FALSE;
	Ed_WriteFile("RECOVER.EST");
    }
    exit(-1);
}

void 
Ed_Main()
{
    ushort c;
    Ed_Initialise();
    Scr_InstallFatal(Fatal);
    (void)signal(SIGINT, Ed_Abort);
    while (TRUE)
    {
	Ed_ShowPos(FALSE);
	c = Kb_GetCh();
	if (c == Ed_Key[(int)_Ed_Quit])
	{
	    if (Ed_Quit() != KB_ESC)
		break;
	} else
	    c = Ed_Command(c, (uchar) * ((uchar far *) 0x417));
    }
}




/**********************************/
/* Set Up Default Key Assignments */
/**********************************/

static void NEAR 
Ed_AssignKeys()
{
    Ed_Key[(int)_Ed_Quit] = KB_ALT_Z;
    Ed_Key[(int)_Ed_LeftWord] = KB_CTRL_LEFT;
    Ed_Key[(int)_Ed_RightWord] = KB_CTRL_RIGHT;
    Ed_Key[(int)_Ed_LeftMatch] = KB_CTRL_PGUP;
    Ed_Key[(int)_Ed_RightMatch] = KB_CTRL_PGDN;
    Ed_Key[(int)_Ed_FSearch] = KB_ALT_S;
    Ed_Key[(int)_Ed_BSearch] = KB_CTRL_S;
    Ed_Key[(int)_Ed_Translate] = KB_ALT_T;
    Ed_Key[(int)_Ed_MarkBlock] = KB_ALT_M;
    Ed_Key[(int)_Ed_MarkColumn] = KB_ALT_C;
    Ed_Key[(int)_Ed_MarkPlace] = KB_CTRL_P;
    Ed_Key[(int)_Ed_GotoMark] = KB_CTRL_G;
    Ed_Key[(int)_Ed_File] = KB_CTRL_F;
    Ed_Key[(int)_Ed_Undo] = KB_ALT_U;
    Ed_Key[(int)_Ed_Compile] = KB_CTRL_C;
    Ed_Key[(int)_Ed_Execute] = KB_CTRL_X;
    Ed_Key[(int)_Ed_Options] = KB_CTRL_O;
    Ed_Key[(int)_Ed_Analyze] = KB_CTRL_A;
    Ed_Key[(int)_Ed_Copy] = KB_ALT_MINUS;
    Ed_Key[(int)_Ed_BlokPaste] = KB_INS;
    Ed_Key[(int)_Ed_ColPaste] = KB_ALT_P;
    Ed_Key[(int)_Ed_Edit] = KB_CTRL_E;
    Ed_Key[(int)_Ed_TopFile] = KB_CTRL_HOME;
    Ed_Key[(int)_Ed_EndFile] = KB_CTRL_END;
    Ed_Key[(int)_Ed_RecMacro] = KB_ALT_R;
    Ed_Key[(int)_Ed_LangHelp] = KB_ALT_F1;
    Ed_Key[(int)_Ed_Template] = KB_CTRL_F1;
    Ed_Key[(int)_Ed_DelLine] = KB_ALT_D;
}

static void NEAR 
Ed_AssignFunctions()
{
    Ed_Func[(int)_Ed_Quit] = NULL;	/* handled explicitly */
    Ed_Func[(int)_Ed_LeftWord] = Ed_LeftWord;
    Ed_Func[(int)_Ed_RightWord] = Ed_RightWord;
    Ed_Func[(int)_Ed_LeftMatch] = Ed_LeftSearch;
    Ed_Func[(int)_Ed_RightMatch] = Ed_RightSearch;
    Ed_Func[(int)_Ed_FSearch] = Ed_SearchForward;
    Ed_Func[(int)_Ed_BSearch] = Ed_SearchBackward;
    Ed_Func[(int)_Ed_Translate] = Ed_Translate;
    Ed_Func[(int)_Ed_MarkBlock] = Ed_Mark;
    Ed_Func[(int)_Ed_MarkColumn] = Ed_Mark;
    Ed_Func[(int)_Ed_MarkPlace] = Ed_MarkPlace;
    Ed_Func[(int)_Ed_GotoMark] = Ed_GotoMark;
    Ed_Func[(int)_Ed_File] = Pew_MainMenu;
    Ed_Func[(int)_Ed_Undo] = Ed_Undo;
    Ed_Func[(int)_Ed_Compile] = Pew_MainMenu;
    Ed_Func[(int)_Ed_Execute] = Pew_MainMenu;
    Ed_Func[(int)_Ed_Options] = Pew_MainMenu;
    Ed_Func[(int)_Ed_Analyze] = Pew_MainMenu;
    Ed_Func[(int)_Ed_Copy] = Ed_BlockOp;
    Ed_Func[(int)_Ed_BlokPaste] = Ed_BlockPaste;
    Ed_Func[(int)_Ed_ColPaste] = Ed_ColumnPaste;
    Ed_Func[(int)_Ed_Edit] = Pew_MainMenu;
    Ed_Func[(int)_Ed_TopFile] = Ed_TopFile;
    Ed_Func[(int)_Ed_EndFile] = Ed_EndFile;
    Ed_Func[(int)_Ed_RecMacro] = (edHndl) Kb_RecordOn;
    Ed_Func[(int)_Ed_LangHelp] = Ed_LangHelp;
    Ed_Func[(int)_Ed_Template] = Ed_Template;
    Ed_Func[(int)_Ed_DelLine] = Ed_DeleteLine;
}


char 
Ed_NextChar(short *lineno)	/* Compiler interface */
{
    char ch;
    while (Ed_ColNow > ED_LEN && Ed_LineNow < Ed_LastLine)
    {
	(void)Ed_Point(Ed_LineNow + 1, FALSE);
	Ed_ColNow = 0;
    }
    if (Ed_LineNow <= Ed_LastLine)
	if (Ed_ColNow == ED_LEN)
	    ch = '\n';
	else
	    ch = ED_TEXT[Ed_ColNow];
    else
	ch = 0;
    Ed_ColNow++;
    *lineno = Ed_LineNow + 1;
    return ch;
}
