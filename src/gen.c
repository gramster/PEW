/*********************************************************************
The GWLIBL library, its source code, and header files, are all
copyright 1987, 1988, 1989, 1990 by Graham Wheeler. All rights reserved.
**********************************************************************/

/********************************
*   gen.c - General Functions	*
********************************/

#include <conio.h>
#include "misc.h"
#include "help.h"
#include "screen.h"
#include "keybd.h"
#include "errors.h"
#include "gen.h"
#include <errno.h>
#include <io.h>
#include <stdarg.h>

#define GEN_BORDER_ATTR	Scr_ColorAttr(LIGHTCYAN,BLUE)
#define GEN_INSIDE_ATTR Scr_ColorAttr(RED,LIGHTGRAY)

short Debugging = FALSE, Debug2File = FALSE;

void 
Gen_Debug(STRING fmt,...)
{
    static FILE *fp = NULL;
    char msgbuf[80];
    va_list args;
    if (Debugging)
    {
	va_start(args, fmt);
	vsprintf(msgbuf, fmt, args);
	if (fp == NULL)
	    if (Debug2File)
		fp = fopen("DEBUG", "wt");
	    else
		fp = stdprn;
	fputs(msgbuf, fp);
	fputs("\n", fp);
	fflush(fp);
    }
}

/************************************************************************/
/* Gen_HasDiskChanged - returns a value which indicates whether the	*/
/*	floppy disk has changed since the last disk operation, and	*/
/*	resets this condition if it occurs. This is not supported by	*/
/*	all drivers!							*/
/* Returns:								*/
/*	0 : signal not active						*/
/*	1: invalid drive						*/
/*	6 : signal active						*/
/*	0x80 : drive not ready						*/
/************************************************************************/

short FAR 
Gen_HasDiskChanged(short disk)
{
    short rtn;
    _DL = disk;			/* Disk number */
    _AH = 22;			/* Change line service */
    geninterrupt(0x13);
    rtn = _AH;
    if (rtn == 6)
    {
	_AH = 4;
	_AL = 1;
	_DL = disk;
	_DH = _CH = _CL = 0;
	geninterrupt(0x13);	/* Verify sector 0 to clear */
    }
    return rtn;
}

/************************************************************************
* Gen_ShowMessage							*
* Gen_ClearMessage - These two routines are useful for displaying	*
* messages on the screen. Gen_ShowMessage checks if the message window	*
* is currently open, and if not opens a default message window. The user*
* can of course create their own window and assign Gen_MessageWindow to	*
* it to override the default. Gen_ShowMessage then clears the window	*
* and prints the message. Gen_ClearMessage frees the window pointer.	*
* These routines leave the message window as the active window.		*
************************************************************************/

static WINPTR Gen_PrevWindow;
WINPTR Gen_MessageWindow = NULL;

void 
Gen_ShowMessage(STRING message)
{
    if (Win_Current != Gen_MessageWindow)
	Gen_PrevWindow = Win_Current;
    if (Gen_MessageWindow == NULL)
    {
	Win_1LineBorderSet();
	Gen_MessageWindow = Win_Make(WIN_TILED | WIN_NONDESTRUCT | WIN_BORDER,
			    9, 10, 60, 1, GEN_BORDER_ATTR, GEN_INSIDE_ATTR);

    }
    if (Gen_MessageWindow)
    {
	Win_Activate(Gen_MessageWindow);
	Win_Clear();
	Win_FastPutS(0, 1, message);
    }
}

void 
Gen_ClearMessage(void)
{
    if (Gen_MessageWindow)
	Win_Free(Gen_MessageWindow);
    if (Gen_PrevWindow)
	Win_Activate(Gen_PrevWindow);
    Gen_MessageWindow = Gen_PrevWindow = NULL;
}

/************************************************************************
* Gen_BuildName - build a name in 'dest' based on thebase name 'file'	*
*		   and the path in the environment variable 'env_var'.	*
************************************************************************/

void FAR 
Gen_BuildName(STRING env_var, STRING basename, STRING dest)
{
    *dest = '\0';
    /* If there is one, get the value of the environemnt variable */
    if (env_var)
    {
	short i;
	STRING tmp;
	if ((tmp = getenv(env_var)) != 0)
	    strcpy(dest, tmp);
	/* Add a backslash if needed */
	i = strlen(dest);
	if (i != 0)
	    if (dest[i - 1] != '\\')
		strcat(dest, "\\");
    }
    /* Append the file name */
    strcat(dest, basename);
}

/*****************************************
* The next function is a general purpose
* routine for reading messages from a
* number of different types of files, given
* an index. The name of the file, and location
* and size of the read buffer must also be
* specified.
*
* If the filename is specified without a
* suffix, the routine will attempt to open
* a file with the suffix ".idx". This is a
* direct index file, and should contain a
* number of longs, each specifying the file
* offset of the corresponding entry in a data
* file with suffix ".itx". This is the fastest
* method, but requires some kind of processing
* to set up these files correctly. Each entry
* in the ".itx" file should consist of a short
* length entry followed by the actual entry text.
*
* If there is no ".itx" file, or the filename
* included a suffix, an attempt to open the
* specified file is made. If successful, the
* first character is read. If this corresponds
* to INDEXCHAR, the file is assumed to be a
* sorted indexed file and a binary search is
* performed. If not, a straight linear search
* is done, and the nth line of the file read.
*
* In the former case, it is assumed the file
* has the layout:
*
*	@i1
*	<Text for index i1>
*	@i2
*	<text for index i2>
*	...
*	@in
*	<text for index in>
*
* where the indexes are arbitrary unique +ve integers
* or single alphabetic strings in ascending order,
* and the text may be spread over more than one line.
* In this case, the nl_to_spc flag will cause newlines
* to be converted to spaces.
*
* RETURN VALUES:
*	NOENTRY	- no entry found for index
*	NOFILE  - the data file could not be opened
*	BUFFOVRFLOW - the text exceeded the buffer size
*	BADXREF - a #see was found with no index
*	SUCCESS
*
***************************************************/

#define INDEXCHAR	'@'

static short 
_Gen_GetFileEntry(char *filename, short Index, char *Sndex,
		  char *buffer, short bufsize, BOOLEAN nl_to_spc)
{
    FILE *fp, *ifp;
    short rtn;
    char tempname[80], *oldbuff;

    oldbuff = buffer;
SEARCH_AGAIN:
    buffer = oldbuff;
    fp = ifp = NULL;
    rtn = SUCCESS;
    if (Sndex == NULL && Index < 0)
    {
	rtn = NOENTRY;
	goto DONE;
    }
    strcpy(tempname, filename);
    if (strchr(tempname, '.') == NULL)
    {
	char *suffix = tempname + strlen(tempname);
	strcpy(suffix, ".idx");
	if ((ifp = fopen(tempname, "rb")) != NULL)
	{
	    strcpy(suffix, ".itx");
	    if ((fp = fopen(tempname, "rt")) != NULL)
	    {
		long pos;
		short len;
		fseek(ifp, sizeof(long) * Index, SEEK_SET);
		fread(&pos, sizeof(long), 1, ifp);
		fseek(fp, pos, SEEK_SET);
		fread(&len, sizeof(short), 1, fp);
		if (len >= bufsize)
		    rtn = BUFFOVRFLOW;
		else
		{
		    fread(buffer, sizeof(char), len, fp);
		    buffer += len;	/* skip to end */
		}
	    } else
		rtn = NOFILE;
	    goto DONE;
	} else
	    *suffix = '\0';
    }
    if ((fp = fopen(tempname, "rt")) == NULL)
    {
	rtn = NOFILE;
	goto DONE;
    }
    fgets(buffer, bufsize, fp);
    if (*buffer != INDEXCHAR)	/* straight sequential file */
    {
	while (--Index && !feof(fp))
	    fgets(buffer, bufsize, fp);
	rtn = (feof(fp)) ? NOENTRY : SUCCESS;
	buffer += strlen(buffer);	/* skip to end */
    } else
    {
	short top, bottom = 0, mid, tmp, maxchklen;
	top = filelength(fileno(fp));
	if (Sndex)
	{
	    Index = 0;
	    maxchklen = strlen(Sndex);
	    strupr(Sndex);
	}
	while (bottom <= top)
	{
	    mid = bottom + (top - bottom) / 2;
	    fseek(fp, mid, SEEK_SET);

	    do
	    {
		fgets(buffer, bufsize, fp);
	    } while (*buffer != INDEXCHAR && !feof(fp));

	    if (feof(fp))
		top = mid - 1;	/* reached EOF */
	    else
	    {
		if (Sndex)
		{
		    /* Do a case-insensitive compare */
		    short blen = 0;
		    while (blen < maxchklen)
		    {
			char c = buffer[blen + 1];
			tmp = toupper(c) - Sndex[blen];
			if (tmp)
			    break;
			blen++;
		    }
		    /* Even if we match, must still compare lengths... */
		    if (blen >= maxchklen && buffer[blen + 1] >= 32)
			tmp = 1;
		}
		/* numeric index - convert to integer */
		else
		    tmp = atoi(buffer + 1);
		if (tmp == Index)
		    break;
		else if (tmp > Index)
		    top = mid - 1;
		else
		    bottom = mid + 1;
	    }
	}
	if (tmp != Index)
	    rtn = NOENTRY;
	else
	    do
	    {			/* read the entry */
		short len;
		fgets(buffer, bufsize, fp);
		if (*buffer != INDEXCHAR)	/* not next entry */
		{
		    len = strlen(buffer);
		    if (buffer[len - 1] == '\n' && nl_to_spc)
			buffer[len - 1] = ' ';
		    buffer += len;
		    bufsize -= len;
		} else
		    break;
	    } while (!feof(fp));
    }
DONE:
    if (fp)
	fclose(fp);
    if (ifp)
	fclose(ifp);
    *buffer = 0;		/* terminate entry */
    if (strncmp(oldbuff, "#SEE ", 5) == 0)	/* got a see reference */
    {
	char *tmp = oldbuff + 5;
	while (*tmp && *tmp == ' ')
	    tmp++;
	if (*tmp)
	{
	    if (Sndex)
	    {
		char *tmp2 = tmp + strlen(tmp);
		while (*--tmp2 == ' ');	/* skip trailing space */
		*++tmp2 = 0;
		strcpy(Sndex, tmp);
	    } else
		Index = atoi(tmp);
	    goto SEARCH_AGAIN;
	} else
	    rtn = BADXREF;
    }
    return rtn;
}

short 
Gen_IGetFileEntry(char *filename, short index, char *buffer,
		  short bufsize, BOOLEAN nl_to_spc)
{
    return _Gen_GetFileEntry(filename, index, NULL, buffer, bufsize, nl_to_spc);
}

short 
Gen_SGetFileEntry(char *filename, char *index, char *buffer,
		  short bufsize, BOOLEAN nl_to_spc)
{
    return _Gen_GetFileEntry(filename, 0, index, buffer, bufsize, nl_to_spc);
}

/*************************/

void 
Gen_EditString(ushort c, char *start, char **end, char **now,
	       BOOLEAN * insmode, short maxlen)
{
    char *tptr;

    switch (c)
    {
    case KB_ESC:		/* Escape - clear line */
	*now = *end = start;
	*start = 0;
	break;
    case KB_BACKSPC:		/* Backspace */
	if ((tptr = *now - 1) >= start)
	{
	    tptr = *now - 1;
	    while (tptr < *end)
	    {
		*tptr = *(tptr + 1);
		tptr++;
	    }
	    (*now)--;
	    (*end)--;
	}
	break;
    case KB_DEL:
	if (*now < *end)	/* Delete under cursor */
	{
	    tptr = *now;
	    while (tptr < *end)
	    {
		*tptr = *(tptr + 1);
		tptr++;
	    }
	    (*end)--;
	    if (*now > *end)
		*now = *end;
	}
	break;
    case KB_INS:		/* Toggle insert mode */
	*insmode = (BOOLEAN) (1 - (int)*insmode);
	break;
    case KB_RIGHT:
	if (*now < *end)
	    (*now)++;
	break;
    case KB_LEFT:
	if (*now > start)
	    (*now)--;
	break;
    case KB_HOME:
	*now = start;
	break;
    case KB_END:
	*now = *end;
	break;
    default:
	if (*insmode || (*now >= *end))
	    /* If inserting, shift line to make space */
	{
	    if ((*end - start) < maxlen)
	    {
		tptr = ++(*end);
		while (tptr > *now)
		{
		    *tptr = *(tptr - 1);
		    tptr--;
		}
	    } else
	    {
		beep();
		break;
	    }
	}
	/* Put the character in at the appropriate position */
	*((*now)++) = (char)c;
	*(*end) = 0;
	break;
    }
    if (*insmode)
	Win_SetFlags(WIN_BLOKCURSOR);
    else
	Win_ClearFlags(WIN_BLOKCURSOR);

}


STRING 
Gen_ExpandTabs(STRING buff, STRING dest, short tsiz)
{
    short posout = 0;
    if (buff == NULL)
    {
	*dest = 0;
	return dest;
    }
    while (*buff)
    {
	if (*buff == KB_TAB)
	{
	    short spcs = tsiz - posout % tsiz;
	    while (spcs--)
		dest[posout++] = ' ';
	} else
	    dest[posout++] = *buff;
	buff++;
    }
    dest[posout] = 0;
    return dest;
}

/* Gen_CompressSpaces - translate spaces into tabs where possible,
	and remove trailing blanks from a line.
*/

STRING 
Gen_CompressSpaces(STRING buff, STRING dest, short tsiz)
{
    short posin = 0, posout = 0;
    if (buff == NULL)
    {
	*dest = 0;
	return dest;
    }
    while (buff[posin])
    {
	if (buff[posin] != ' ')
	    dest[posout++] = buff[posin++];
	else
	{
	    short nxttab, pnow = posin;
	    while (buff[++posin] == ' ');
	    if (tsiz)
	    {
		nxttab = tsiz * (pnow / tsiz + 1);
		while (nxttab < posin)
		{
		    dest[posout++] = '\t';
		    pnow = nxttab;
		    nxttab += tsiz;
		}
		while (pnow++ < posin)
		    dest[posout++] = ' ';
	    }
	}
    }
    dest[posout--] = 0;
    while (dest[posout] == ' ' && posout >= 0)	/* Strip trailing blanks */
	dest[posout--] = 0;
    return dest;
}

/************************************************************************/

static char Gen_MShift[128];

static short NEAR 
Gen_Matches(STRING t, STRING pat, STRING endpat, short CaseSensitive)
{
    short rtn = TRUE;
    if (CaseSensitive)
    {
	while (--endpat >= pat)
	    if (*endpat != *--t)
	    {
		rtn = FALSE;
		break;
	    }
    } else
    {
	while (--endpat >= pat)
	{
	    if (*endpat != *--t)
		if (*endpat >= 'A' && *endpat <= 'Z')
		    if ((*endpat + 'a' - 'A') != *t)
		    {
			rtn = FALSE;
			break;
		    }
	}
    }
    return rtn;
}


#define IDENTSET "_ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"

short 
Gen_IsMatch(STRING txt, STRING pat, short end, BOOLEAN CaseSensitive,
	    BOOLEAN IdentsOnly)
{
    STRING p, endpat;
    short i, inc, txtlim, plen, ptmp;
    txtlim = (txt == NULL) ? 0 : strlen(txt);
    ptmp = plen = (pat == NULL) ? 0 : strlen(pat);
    endpat = pat + plen - 1;
    for (i = 0; i < 128; i++)
	Gen_MShift[i] = plen;
    for (p = pat; *p; p++)
    {
	Gen_MShift[*p & 0x7F] = --ptmp;
	if (!CaseSensitive)
	    Gen_MShift[(*p & 0x7F) - 'A' + 'a'] = ptmp;
    }
    while (TRUE)
    {
	while (end < txtlim && (inc = Gen_MShift[txt[end] & 0x7F]) > 0)
	    end += inc;
	if (end < txtlim)
	{
	    if (Gen_Matches(&txt[end], pat, endpat, CaseSensitive))
		if (IdentsOnly)
		{
		    short matched = TRUE;
		    if ((end + 1) < txtlim)
			if (strchr(IDENTSET, txt[end + 1]) != NULL)
			    matched = FALSE;
		    if (end >= plen)
			if (strchr(IDENTSET, txt[end - plen]) != NULL)
			    matched = FALSE;
		    if (matched)
			return end;
		} else
		    return end;
	    end++;
	} else
	    return (-1);
    }
}

/*********************************************************************/

void 
Gen_Suspend(STRING progname)
{
    short result = ENOMEM;
    ushort _wfar *image = (ushort _wfar *) NULL;

    if ((Scr_SaveScreen(image)) == NULL)
	Error(TRUE, ERR_NOSAVESCR);
    else
    {
	Scr_ClearAll();
	printf("Type 'exit' to return to %s\n", progname);
	Scr_CursorOn();
	result = system("");
	if (result == ENOENT)
	    Error(TRUE, ERR_NOSHELL);
	else if (result == ENOMEM)
	    Error(TRUE, ERR_NOMEM);
	else if (result)
	    Error(TRUE, ERR_BADSHELL, result);
	sleep(1);
	Scr_RestoreScreen(image);
    }
}

/*******************************************************************
*
* The next routine prints a prompt in a window (opening
* the window if necessary), followed by a response. If the
* response starts with a parenthesis, it is assumed to be
* a list of viable 1-char uppercase responses, otherwise it is
* taken to be a default response in a response buffer.
* In the first case, the user is prompted until a valid
* character response is entered. In the second case, the
* user may freely edit the response, until a carriage return
* is entered. The last character entered is returned. In each
* case, a help index is provided; if the user presses F1 or
* Alt-H, Hlp_IHelp is called with this index.
* If a window was opened, it is closed upon returning.
*
********************************************************************/

ushort 
Gen_GetResponse(WINPTR w, STRING prompt, STRING response,
		BOOLEAN showresponse, short resplen, enum helpindex help)
    {
	WINPTR OldW = Win_Current, TmpW = w;
	char *cptr, *eptr;
	ushort c;
	short rcol, ask_mode;
	BOOLEAN ins_mode = TRUE;

	    rcol = strlen(prompt) + 2;	/* reserve space char on each side */
	    ask_mode = (*response != '(');	/* 0 => single character mode */
	if  (w == NULL)
	{
	    Win_1LineBorderSet();
	    TmpW = Win_Make(WIN_TILED | WIN_NONDESTRUCT | WIN_BORDER,
		       (77 - rcol - resplen) / 2, 10, rcol + resplen + 3, 1,
			    GEN_BORDER_ATTR, GEN_INSIDE_ATTR);
	}
	if  (TmpW)
	{
	    Win_Activate(TmpW);
	    if (ask_mode)
		Win_SetFlags(WIN_BLOKCURSOR);
	    cptr = eptr = response;	/* current, end pointers */
	    while (*eptr)
		eptr++;
	    for (;;)
	    {
		Win_Clear();
		Win_FastPutS(0, 1, prompt);
		if (showresponse)
		    Win_FastPutS(0, rcol, response);
		Win_PutCursor(0, rcol + (cptr - response));
		c = Kb_GetCh();
		if (ask_mode && c == KB_CRGRTN)
		    break;
		if ((!ask_mode || (eptr == response)) && c == KB_ESC)
		    break;
		if (c == KB_F1 || c == KB_ALT_H)
		    Hlp_IHelp(help);
		else if (ask_mode)
		    Gen_EditString(c, response, &eptr, &cptr, &ins_mode, resplen);
		else if ((char)c)
		{
		    if (c >= 'a' && c <= 'z')
			c -= 'a' - 'A';
		    if (strchr(response + 1, c) != NULL)
			if (c != ')')
			    break;
		}
	    }
	    Win_Clear();
	    if (w == NULL)
		Win_Free(TmpW);
	}
	if (OldW)
	    Win_Activate(OldW);
	return c;
    }

/**********************************************************************
 Low-level disk writes and formatiing
***********************************************************************/

static void 
format_track(short drive, short head, short track)
{
    uchar buf[9 * 4];
    char msg[60];
    uchar far *b = (uchar far *) buf;
    short i = 0, sector = 1;
    sprintf(msg, "Formatting: Head %2d   Track %2d\n", head, track);
    Gen_ShowMessage(msg);
    while (i < 36)
    {
	buf[i++] = (uchar) track;
	buf[i++] = (uchar) head;
	buf[i++] = (uchar) sector++;
	buf[i++] = (uchar) 2;	/* 512 byte sectors */
    }
    _ES = FP_SEG(b);
    _BX = FP_OFF(b);
    _CH = (uchar) track;
    _DL = (uchar) drive;	/* drive B=1		 */
    _DH = 1;			/* double sided 	 */
    _AL = 9;			/* format 9 sectors 	 */
    _AH = 5;			/* Format service	 */
    geninterrupt(19);
}

void 
Gen_WriteSector(short drive, short head, short track, short sector, uchar * buf)
{
    uchar far *b = (uchar far *) buf;
    _ES = FP_SEG(b);
    _BX = FP_OFF(b);
    _CH = (uchar) track;
    _DH = (uchar) head;
    _CL = (uchar) sector;
    _DL = (uchar) drive;	/* drive B=1	 */
    _AL = 1;			/* write 1 sector 	 */
    _AH = 3;			/* Write Sector service	 */
    geninterrupt(19);
}


/* Sector buffer - initialised for boot sector */
#if 0
uchar GF360buf[512] = {0xEB, 0x34, 0x90,
    /* DOS version - MSDOS3.3 */
    0x4d, 0x53, 0x44, 0x4f, 0x53, 0x33, 0x2e, 0x33, 0x0,
    2, 2, 1, 0, 2, 0x70, 0,
    /* For 720k - a0, 05, f9, 03; 360k - d0, 02, fd, 02 */
    0xD0, 0x02, 0xFD, 0x02,
    0, 9, 0, 2, 0
};
#else
uchar GF360buf[512] = {0xEB, 0x3A, 0xF7, 0x00, 0xF0, 0xEB, 0x35, 0x20,
    0x33, 0x2e, 0x31, 0x00, 0x02, 0x02, 0x01, 0x00, 0x02, 0x70, 0x00, 0xd0,
    0x02, 0xFD, 0x02, 0x00, 0x09, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,

    0x00, 0x00, 0x00, 0xcf, 0x02, 0x25, 0x02, 0x09, 0x2a, 0xff, 0x50, 0xf6,
    0x19, 0x04, 0x0d, 0x0a, 0x42, 0x4F, 0x4F, 0x54, 0x20, 0x76, 0x33, 0x2e,
    0x31, 0x30, 0x20, 0x00, 0x33, 0xC0, 0x8e, 0xd0, 0xbc, 0x00, 0x7c, 0x8e,
    0xd8, 0x88, 0x16, 0x1e, 0x7c, 0xbe, 0x3b, 0x7c, 0xe8, 0x54, 0x01, 0xb8,
    0x23, 0x7c, 0xa3, 0x78, 0x00, 0x8c, 0x0e, 0x7a, 0x00, 0xbe, 0x0b, 0x7c,
    0x8a, 0x44, 0x05, 0x98, 0xf7, 0x64, 0x0b, 0x03, 0x44, 0x03, 0x8b, 0xe8,
    0xe8, 0x45, 0x01, 0xb8, 0x50, 0x00, 0x8e, 0xc0, 0x33, 0xdb, 0x8a, 0x16,
    0x1e, 0x7c, 0xb8, 0x01, 0x02, 0xcd, 0x13, 0x72, 0x4e, 0xfc, 0xb9, 0x0b,
    0x00, 0x33, 0xff, 0xbe, 0xcd, 0x7d, 0xf3, 0xa6, 0x75, 0x41, 0xbf, 0x20,
    0x00, 0xb9, 0x0b, 0x00, 0xf3, 0xa6, 0x75, 0x37, 0xbe, 0x0b, 0x7c, 0xb8,

    0x20, 0x00, 0xf7, 0x64, 0x06, 0x8b, 0x1c, 0x03, 0xc3, 0x48, 0xf7, 0xf3,
    0x03, 0xc5, 0xe8, 0x07, 0x01, 0xa1, 0x1c, 0x05, 0x05, 0xff, 0x01, 0xd0,
    0xec, 0x8a, 0xc4, 0xbd, 0x70, 0x00, 0x89, 0x2e, 0x21, 0x7c, 0x8e, 0xc5,
    0x33, 0xdb, 0x8a, 0x16, 0x1e, 0x7c, 0xe8, 0x7e, 0x00, 0x72, 0x10, 0xff,
    0x2e, 0x1f, 0x7c, 0xbe, 0xf6, 0x7c, 0xe8, 0xd2, 0x00, 0xb4, 0x00, 0xcd,
    0x16, 0xcd, 0x19, 0xbe, 0xe0, 0x7c, 0xe8, 0xc6, 0x00, 0xfb, 0xeb, 0xfe,
    0x0a, 0x0d, 0x44, 0x69, 0x73, 0x6b, 0x20, 0x42, 0x6f, 0x6f, 0x74, 0x20,
    0x66, 0x61, 0x69, 0x6c, 0x75, 0x72, 0x65, 0x0a, 0x0d, 0x00, 0x0a, 0x0d,
    0x4e, 0x6f, 0x6e, 0x2d, 0x53, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x20, 0x64,
    0x69, 0x73, 0x6b, 0x20, 0x6f, 0x72, 0x20, 0x64, 0x69, 0x73, 0x6b, 0x20,

    0x65, 0x72, 0x72, 0x6f, 0x72, 0x2e, 0x0a, 0x0d, 0x52, 0x65, 0x70, 0x6c,
    0x61, 0x63, 0x65, 0x20, 0x61, 0x6e, 0x64, 0x20, 0x73, 0x74, 0x72, 0x69,
    0x6b, 0x65, 0x20, 0x61, 0x6e, 0x79, 0x20, 0x6b, 0x65, 0x79, 0x20, 0x77,
    0x68, 0x65, 0x6e, 0x20, 0x72, 0x65, 0x61, 0x64, 0x79, 0x0a, 0x0d, 0x00,
    0x00, 0x00, 0x00, 0xbe, 0x0b, 0x7c, 0xa2, 0x40, 0x7d, 0x88, 0x2e, 0x41,
    0x7d, 0x8a, 0xc1, 0xd0, 0xc0, 0xd0, 0xc0, 0x24, 0x03, 0xa2, 0x42, 0x7d,
    0xa0, 0x40, 0x7d, 0x8a, 0x64, 0x0d, 0x80, 0xe1, 0x3f, 0x2a, 0xe1, 0xfe,
    0xc4, 0x3a, 0xc4, 0x76, 0x02, 0x8a, 0xc4, 0x8b, 0xf8, 0x8a, 0x26, 0x42,
    0x7d, 0xd0, 0xcc, 0xd0, 0xcc, 0x0a, 0xcc, 0xb4, 0x02, 0xcd, 0x13, 0x72,
    0x25, 0x8b, 0xc7, 0x32, 0xe4, 0x52, 0xf7, 0x24, 0x5a, 0x03, 0xd8, 0xfe,

    0xc6, 0x3a, 0x74, 0x0f, 0x72, 0x0a, 0x32, 0xf6, 0xff, 0x06, 0x41, 0x7d,
    0x8a, 0x2e, 0x41, 0x7d, 0xb1, 0x01, 0x8b, 0xc7, 0x28, 0x06, 0x40, 0x7d,
    0x77, 0xb6, 0xc3, 0xfc, 0x2e, 0xac, 0x0a, 0xc0, 0x74, 0xf8, 0xb4, 0x0e,
    0xcd, 0x10, 0xeb, 0xf4, 0x03, 0x44, 0x11, 0x33, 0xd2, 0xf7, 0x74, 0x0d,
    0x42, 0x52, 0x33, 0xd2, 0xf7, 0x74, 0x0f, 0x8a, 0xf2, 0x8a, 0xe8, 0xd0,
    0xcc, 0xd0, 0xcc, 0x8a, 0xcc, 0x58, 0x0a, 0xc8, 0xc3, 0x49, 0x4f, 0x20,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x53, 0x59, 0x53, 0x4d, 0x53, 0x44, 0x4f,
    0x53, 0x20, 0x20, 0x20, 0x53, 0x59, 0x53, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x70, 0x73, 0x55, 0xaa,
};
#endif

/* Note that 720k drives use 3 sector FATs, 360k drives use 2 */

void 
Gen_Format360(short drive)
{
    short head, track, i;
    char msg[60];
    sprintf(msg, "Preparing disk in drive %c - please wait", 'A' + drive);
    Gen_ShowMessage(msg);
    sleep(2);
    for (track = 0; track < 40; track++)
	for (head = 0; head < 2; head++)
	    format_track(drive, head, track);

    Gen_WriteSector(drive, 0, 0, 1, GF360buf);	/* Write boot sector */

    /* Write first sectors of FATs */
    GF360buf[0] = 0xFd;		/* f9 for 720k */
    GF360buf[1] = 0xff;
    GF360buf[2] = 0xff;
    for (i = 3; i < 512; i++)
	GF360buf[i] = 0;
    Gen_WriteSector(drive, 0, 0, 2, GF360buf);
    Gen_WriteSector(drive, 0, 0, 4, GF360buf);

    /* Write other FAT sectors and root directory */
    for (i = 0; i < 3; i++)
	GF360buf[i] = 0;
    Gen_WriteSector(drive, 0, 0, 3, GF360buf);
    Gen_WriteSector(drive, 0, 0, 5, GF360buf);
    for (i = 1; i < 512; i++)
	GF360buf[i] = 0xF6;
    Gen_WriteSector(drive, 0, 0, 6, GF360buf);
    Gen_WriteSector(drive, 0, 0, 7, GF360buf);
    Gen_WriteSector(drive, 0, 0, 8, GF360buf);
    Gen_WriteSector(drive, 0, 0, 9, GF360buf);
    Gen_WriteSector(drive, 1, 0, 1, GF360buf);
    Gen_WriteSector(drive, 1, 0, 2, GF360buf);
    Gen_ClearMessage();
}

/******************************************************
  Password Encryption
*******************************************************/

static short 
mod95(short val)
{
    /* This is not quite the same as C's % operator... */
    while (val >= 9500)
	val -= 9500;
    while (val >= 950)
	val -= 950;
    while (val >= 95)
	val -= 95;
    while (val < 0)
	val += 95;
    return (val);
}

void 
Gen_Encrypt(STRING in, STRING out)
{
    /*
     * The encryption routine has been adapted from the MicroEMACS encryption
     * function written by Dana Hoggatt, known as DLH-POLY-86-B CIPHER.
     */
    register short cc;		/* Current character 	 */
    long key = 0;		/* 29-bit cipher key	 */
    short salt = 0;		/* Spice for key	 */
    if (in)
    {
	short len = strlen(in);
	while (len--)
	{
	    cc = *in;
	    if ((cc >= ' ') && (cc <= '~'))	/* Printable? */
	    {
		key &= 0x1FFFFFFFL;	/* strip off overflow */
		if (key & 0x10000000L)
		    key ^= 0x0040A001L;	/* feedback */
		cc = mod95((int)(key % 95) - (cc - ' ')) + ' ';
		if (++salt >= 20857)
		    salt = 0;
		key <<= 1;
		key += cc + *in + salt;
	    }
	    in++;
	    *out++ = cc;
	}
	*out = '\0';
    }
}
