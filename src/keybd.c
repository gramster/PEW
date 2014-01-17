#ifdef DEMO
#include <time.h>
#endif
#include "misc.h"
#include "help.h"
#include "keybd.h"
#include "screen.h"
#include "errors.h"

#define MACROLEN	128

BOOLEAN demo_mode = FALSE;

short Kb_Record = FALSE, Kb_PlayBack = FALSE, Kb_MacNum = 0;

static short Kb_MacPos = 0;

static ushort Kb_Macros[10][MACROLEN];
static ushort Kb_MacLen[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

#ifdef KBDEBUG
#define Kb_Debug(m,r)  Kb_BugOut(m,r)
#else
#define Kb_Debug(m,r)
#endif

#ifdef KB_DEBUG
void 
Kb_BugOut(char *msg, ushort rtn)
{
    FILE *fp;
    fp = fopen("DEBUG", "at");
    fprintf(fp, "Rtn=%04X (%s)\n", rtn, msg);
    fflush(fp);
    fclose(fp);
}
#endif

void 
Kb_RecordOn(short keynum)
{
    WINPTR OldWin = Win_Current, Kb_RecWin = NULL;
    ushort key;
    short MacNum = keynum;
    Win_1LineBorderSet();
    if (Kb_Record || Kb_PlayBack)
	Error(TRUE, ERR_RECMACRO);
    else
	Kb_RecWin = Win_Make(WIN_CURSOROFF | WIN_NONDESTRUCT | WIN_BORDER,
			     10, 10, 58, 1, SCR_NORML_ATTR, SCR_INVRS_ATTR);
    if (Kb_RecWin)
    {
	Win_Activate(Kb_RecWin);
	Win_PutTitle("< Record Macro >", SCR_TOP, SCR_CENTRE);
	Win_FastPutS(0, 1, "Press the key to which you wish to assign the macro...");
	while (MacNum < 0 || MacNum > 9)
	{
	    key = Kb_RawGetC();
	    if (key >= '0' && key <= '9')
		MacNum = key - '0';
	    else if (key >= KB_ALT_1 && key <= KB_ALT_0)
		MacNum = ((key - KB_ALT_1) / 0x100 + 1) % 10;
	    else if (key == KB_F1 || key == KB_ALT_H)
		Hlp_ShowHelp(
			     "You can assign keystroke macros to the keys Alt-0 to Alt-9. "
		"Press the numeric key to which you wish to assign a macro. "
			     "You cannot play macros back while recording."
		    );
	    else if (key == KB_ESC)
	    {
		Win_Clear();
		Win_FastPutS(0, 1, "Cancelled...");
		sleep(1);
		goto abort;
	    } else
	    {
		Error(TRUE, ERR_MACROKEY);
		MacNum = 99;
	    }
	}
	if (MacNum < 10)
	{
	    if (Kb_MacLen[MacNum])
	    {
		short confirmed = FALSE;
		Win_Clear();
		Win_FastPutS(0, 1, "Overwrite existing macro y/n?");
		beep();
		while (!confirmed)
		    switch (Kb_RawGetC())
		    {
		    case 'Y':
		    case 'y':
			Kb_Record = TRUE;
			confirmed = TRUE;
			break;
		    case KB_ALT_H:
		    case KB_F1:
			Hlp_ShowHelp(
			"A macro is assigned to this key. You may overwrite "
				"it with a new one, or cancel this command."
			    );
			break;

		    default:
			Kb_Record = FALSE;
			confirmed = TRUE;
			break;
		    }
	    } else
		Kb_Record = TRUE;
	    if (Kb_Record)
	    {
		Kb_MacPos = 0;
		Kb_MacLen[MacNum] = 0;
		Kb_MacNum = MacNum;
		Kb_Debug("Start Record", Kb_MacNum);
		Win_Clear();
		Win_FastPutS(0, 1, "Press the Alt key again to terminate recording");
		sleep(1);
	    }
	}
abort:
	Win_Free(Kb_RecWin);
    }
    if (OldWin)
	Win_Activate(OldWin);
}


#ifdef DEMO

FILE *demoscriptfp = NULL;
time_t lastcalltime = (time_t) 0;
ushort demo_ch;
short demowait;
BOOLEAN waiting = FALSE, nointerrupt = FALSE;
WINPTR tutor = NULL;

demo_init(char *scriptfilename)
{
    if ((demoscriptfp = fopen(scriptfilename, "rt")) == NULL)
	return 0;
    demowait = 0;
    return 1;
}

static BOOLEAN NEAR 
_Demo_RawLookC(ushort * rtn)
{
    static ushort scancode = 0;
    if (nointerrupt)
	return FALSE;
    while (TRUE)
    {
	if (demowait > (time(NULL) - lastcalltime))
	{
	    demo_mode = FALSE;
	    if (Kb_RawLookC(NULL))
	    {
		/*
		 * Key pressed, so skip wait. If the key was ESC then quit
		 * demo mode.
		 */
		demowait = 0;
		if (Kb_RawGetC() == KB_ESC)
		{
		    if (tutor)
			Win_Free(tutor);
		    tutor = NULL;
		    return FALSE;
		}
	    }
	    demo_mode = TRUE;
	    if (demowait)
		return FALSE;
	}
	if (waiting)
	{
	    *rtn = scancode;
	    if (*rtn & 0x007F)
		*rtn &= 0x007F;
	    return TRUE;
	}
	if (feof(demoscriptfp))
	{
	    demo_ch = scancode = KB_ALT_Z;
	    waiting = TRUE;
	} else
	    demo_ch = (ushort) fgetc(demoscriptfp);
	if (demo_ch == '%')	/* Comment */
	{
	    char junk[80];
	    fgets(junk, 79, demoscriptfp);
	} else if (isxdigit(demo_ch))
	{
	    while (isxdigit(demo_ch))
	    {
		ushort cval;
		if (demo_ch > 'Z')
		    cval = demo_ch - 'a' + 10;
		else if (demo_ch >= 'A')
		    cval = demo_ch - 'A' + 10;
		else
		    cval = demo_ch - '0';
		scancode = scancode * 16 + cval;
		demo_ch = (ushort) fgetc(demoscriptfp);
	    }
	    waiting = TRUE;
	} else if (demo_ch == 'P')
	{
	    /* Get a pause value */
	    fscanf(demoscriptfp, "%d", &demowait);
	    (void)time(&lastcalltime);
	} else if (demo_ch == 'p')	/* No keys until GetRawC is called */
	{
	    nointerrupt = TRUE;
	    return FALSE;
	} else if (demo_ch == 'S')
	{
	    /* Display info window */
	    short x, y, w, h, r = 0, t;
	    char buff[80];
	    WINPTR oldwin = Win_Current;

	    if (tutor == NULL)
	    {
		fscanf(demoscriptfp, "%d %d %d %d", &x, &y, &w, &h);
		tutor = Win_Make(WIN_BORDER | WIN_CURSOROFF | WIN_TILED | WIN_NONDESTRUCT,
				 x, y, w, h, Scr_ColorAttr(GREEN, BLACK), Scr_ColorAttr(GREEN, BLACK));
		if (tutor == NULL)
		    (void)Win_Error(NULL, "", "Bad tutorial window size", FALSE, FALSE);
	    }
	    Win_Activate(tutor);
	    Win_Clear();
	    while (TRUE)
	    {
		fgets(buff, 79, demoscriptfp);
		if (buff[0] == '@')
		    break;
		buff[strlen(buff) - 1] = 0;
		Win_FastPutS(r++, 1, buff);
	    }
	    Win_Activate(oldwin);
	    t = atoi(buff + 1);
	    demo_mode = FALSE;
	    while (t-- && Kb_RawLookC(NULL) == FALSE)
		sleep(1);
	    if (Kb_RawLookC(NULL))
		(void)Kb_RawGetC();
	    demo_mode = TRUE;
	} else if (demo_ch == 'X')	/* Close window */
	{
	    Win_Free(tutor);
	    tutor = NULL;
	}
    }
}

static ushort NEAR 
_Demo_RawGetC(void)
{
    ushort rtn;
    nointerrupt = FALSE;
    while (!_Demo_RawLookC(&rtn));
    waiting = FALSE;
    return rtn;
}

#endif

ushort 
Kb_RawGetC(void)
{
    ushort rtn;
#ifdef DEMO
    if (demo_mode)
	return _Demo_RawGetC();
#endif
    _AH = 0;
    geninterrupt(0x16);
    rtn = _AX;
    if (rtn & 0x007F)
	return (rtn & 0x007F);
    else
	return rtn;
}

#pragma inline

BOOLEAN 
Kb_RawLookC(ushort * rtn)
{
    BOOLEAN ch_avail;
    ushort tmp = 0;
#ifdef DEMO
    if (demo_mode)
	return _Demo_RawLookC(rtn);
#endif
    _AH = 1;			/* status */
    geninterrupt(0x16);
    asm jz noch
        tmp = _AX;
    ch_avail = TRUE;
    asm jmp done
        noch:ch_avail = FALSE;
done:
    if (rtn)
    {
	*rtn = tmp;
	if (tmp & 0x007F)
	    *rtn &= 0x007F;
    }
    return ch_avail;
}

ushort 
Kb_GetChWithTimeout(short timeout)
{
    short GotChar = FALSE;
    short MacNum;
    ushort rtn;
    ulong tick = get_tick();
    while (!GotChar)
    {
	if (Kb_PlayBack)
	{
	    rtn = Kb_Macros[Kb_MacNum][Kb_MacPos++];
	    if (Kb_MacPos >= Kb_MacLen[Kb_MacNum])
		Kb_PlayBack = FALSE;
	    GotChar = TRUE;
	} else
	{
	    if (timeout)
		while (Kb_RawLookC(NULL) == FALSE)
		{
		    short waited = get_tick() - tick;
		    waited *= 10;
		    waited /= 182;
		    if (waited > timeout)
		    {
			rtn = 0;
			goto timed_out;
		    }
		}
	    rtn = Kb_RawGetC();
	    if (rtn >= KB_ALT_1 && rtn <= KB_ALT_0)
		MacNum = ((rtn - KB_ALT_1) / 0x100 + 1) % 10;	/* index */
	    else
	    {
		GotChar = TRUE;
		if (Kb_Record)
		    if (Kb_MacPos >= MACROLEN)
		    {
			Error(TRUE, ERR_MACROLEN, (int)MACROLEN);
			Kb_Record = FALSE;
			Kb_MacLen[Kb_MacNum] = MACROLEN;
		    } else
			Kb_Macros[Kb_MacNum][Kb_MacPos++] = rtn;
	    }
	    if (!GotChar)
	    {
		if (Kb_Record)
		    if (Kb_MacNum == MacNum)
		    {
			Kb_Record = FALSE;
			Kb_MacLen[MacNum] = Kb_MacPos;
		    } else
			Error(TRUE, ERR_PLAYMACRO);
		else if (Kb_MacLen[MacNum])	/* Only play back if macro is
						 * at least one keystroke	 */
		{
		    Kb_PlayBack = TRUE;
		    Kb_MacNum = MacNum;
		    Kb_MacPos = 0;
		}
	    }
	}
    }
timed_out:
    return rtn;
}


ushort 
Kb_GetCh(void)
{
    return Kb_GetChWithTimeout(0);
}

void 
Kb_Flush(void)
{
    Kb_PlayBack = FALSE;
    while (Kb_RawLookC(NULL))
	(void)Kb_RawGetC();
}

BOOLEAN 
Kb_LookC(ushort * rtn)
{
    BOOLEAN char_avail;
    if (Kb_PlayBack)
    {
	*rtn = Kb_Macros[Kb_MacNum][Kb_MacPos];
	char_avail = TRUE;
    } else
	char_avail = Kb_RawLookC(rtn);
    return char_avail;
}

struct KeyScanInfo
{
    unsigned short start;	/* scancode of first in sequence */
    unsigned short is_shifted;	/* Shift key?			 */
    unsigned short is_alt;	/* Alt key?			 */
    unsigned short is_ctrl;	/* Ctrl Key?			 */
    unsigned short numkeys;	/* Number of keys in sequence	 */
    unsigned short namelen;	/* characters in name		 */
    char *keyseq;		/* Actual key names		 */
}   Key_Info[11] =
{
    {
	0x3B00, 0, 0, 0, 10, 2, "F1F2F3F4F5F6F7F8F9F0" /* Fnkey row		 */ 
    },
    {
	0x5400, 1, 0, 0, 10, 2, "F1F2F3F4F5F6F7F8F9F0" /* Shifted function keys */ 
    },
    {
	0x5E00, 0, 0, 1, 10, 2, "F1F2F3F4F5F6F7F8F9F0" /* Ctrl function keys	 */ 
    },
    {
	0X6800, 0, 1, 0, 10, 2, "F1F2F3F4F5F6F7F8F9F0" /* Alt keys, fnkey row	 */ 
    },
    {
	0x7800, 0, 1, 0, 12, 1, "1234567890-=" /* Alt keys, numeric row */ 
    },
    {
	0x1000, 0, 1, 0, 10, 1, "QWERTYUIOP" /* Alt keys, qwerty row */ 
    },
    {
	0x1E00, 0, 1, 0, 9, 1, "ASDFGHJKL" /* Alt keys, asd row	 */ 
    },
    {
	0x2C00, 0, 1, 0, 7, 1, "ZXCVBNM" /* Alt keys, zxc row	 */ 
    },
    {
	0x5200, 0, 0, 0, 1, 3, "Ins" /* Insert key		 */ 
    },
    {
	0x7300, 0, 0, 1, 5, 4, "\033   \032   End PgDnHome" /* Ctrl cursor keys	 */ 
    },
    {
	0x8400, 0, 0, 1, 1, 4, "PgUp" /* Ctrl PgUp		 */ 
    }
};

void 
Kb_GetKeyName(STRING name, unsigned short scancode)
{
    short i, found;
    if (scancode < 32)
    {
	strcpy(name, "<Ctrl>");
	name[6] = scancode + '@';
	name[7] = 0;
    } else
    {
	found = FALSE;
	for (i = 0; i < 11; i++)
	{
	    if (scancode >= Key_Info[i].start &&
	     (scancode - Key_Info[i].start) < (Key_Info[i].numkeys * 0x100))
	    {
		found = TRUE;
		break;
	    }
	}
	if (!found)
	    strcpy(name, "Illegal");
	else
	{
	    name[0] = 0;	/* Clear name */
	    if (Key_Info[i].is_shifted)
		strcpy(name, "<Shft>");
	    if (Key_Info[i].is_alt)
		strcat(name, "<Alt>");
	    if (Key_Info[i].is_ctrl)
		strcat(name, "<Ctrl>");
	    strncat(name, Key_Info[i].keyseq +
	       Key_Info[i].namelen * (scancode - Key_Info[i].start) / 0x100,
		    Key_Info[i].namelen);
	}
    }
}
