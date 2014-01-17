#include <stdarg.h>
#include "misc.h"
#include "help.h"
#include "screen.h"
#include "keybd.h"
#include "gen.h"
#include "menu.h"
#include "errors.h"

/* Macros for accessing menu fields, given menu pointer 'menu' */

#define MNU_OPTS	(*(menu->Opts))
#define MNU_COL 	(menu->optx)
#define MNU_NOPTS	(menu->nopts)
#define MNU_OPTNOW	(menu->optnow)
#define MNU_Y		(menu->y)
#define MNU_X		(menu->x)
#define MNU_H		(menu->h)
#define MNU_W		(menu->w)
#define MNU_TITLE	(menu->title)

/* Macros for accessing option fields, given option index 'i' and
	menu pointer 'menu' */

#define OPT_NAME(i)	(MNU_OPTS[i].Item)
#define OPT_TYPE(i)	(MNU_OPTS[i].type)
#define OPT_HELP(i)	(MNU_OPTS[i].help)
#define OPT_BTYPE(i)	(7 & OPT_TYPE(i))	/* Basic type */
#define OPT_PVALS(i)	(*(MNU_OPTS[i].p_vals))
#define OPT_PVAL(i,j)	(OPT_PVALS(i)[j])
#define OPT_CHOICE(i)	MNU_OPTS[i].choice
#define OPT_CHOICES(i)	(MNU_OPTS[i].maxchoices)
#define OPT_VALUE(i)    (MNU_OPTS[i].value)
#define OPT_AUXFN(i)	(*(MNU_OPTS[i].aux))
#define OPT_SUBMENU(i)	(MNU_OPTS[i].submenu)

static void NEAR 
Mnu_ShowLine(MENUPTR menu, int line, char *str)
{
    char tmpstr[16];
    int maxlen, thislen;
    if (str)
    {
	maxlen = min(15, MNU_W - MNU_COL - 1);
	thislen = strlen(str);
	if (thislen > maxlen)
	    thislen = maxlen;
	strncpy(tmpstr, str, maxlen);
	tmpstr[maxlen] = 0;
	Win_FastPutS(line, MNU_COL, tmpstr);
    }
}

static ushort NEAR 
Mnu_ValOption(MENUOPT * Opt, WINPTR responseWin, int size)
{
    char response[62];
    ushort ch;
    int tmp;
    BOOLEAN valid = TRUE;
    int isstring = FALSE;

    if ((Opt->type & MN_STRING) == MN_STRING)
    {
	isstring = TRUE;
	strcpy(response, Opt->value);
    } else
    {
	itoa(*((int *)Opt->value), response, (Opt->type & MN_HEX) ? 16 : 10);
    }
    for (;;)
    {
	ch = Gen_GetResponse(responseWin, Opt->Item, response, TRUE, 60, Opt->help);
	if (strlen(response) <= size)
	    break;
	beep();
    }
    if (ch != KB_ESC)
    {
	if (!isstring)
	{
	    char *fptr = response;
	    while (*fptr == ' ')
		fptr++;
	    if (*fptr < '0' || *fptr > '9')
	    {
		valid = FALSE;
		if ((Opt->type & MN_HEX) == MN_HEX)
		    if ((*fptr >= 'a' && *fptr <= 'f') ||
			(*fptr >= 'A' && *fptr <= 'F'))
			valid = TRUE;
	    }
	    if (!valid)
		Error(TRUE, ERR_BADINT);
	    else if ((Opt->type & MN_HEX) == MN_HEX)
		sscanf(response, "%x", &tmp);
	    else
		tmp = atoi(response);
	}
	if (valid == TRUE && Opt->aux != NULL)
	{
	    void *arg;
	    if (isstring)
		arg = (void *)response;
	    else
		arg = (void *)tmp;
	    valid = (BOOLEAN) ((*(Opt->aux)) (arg));
	}
	if (valid)
	    if (isstring)
		strcpy(Opt->value, response);
	    else
		*((int *)Opt->value) = tmp;
    }
    return ch;
}



static void NEAR 
Mnu_ShowOption(MENUPTR menu, int o)
{
    if (OPT_TYPE(o) & MN_SKIP)
	Win_SetAttribute(menu->ba);
    Win_ClearLine(o);
    if (OPT_BTYPE(o) == 0 && OPT_NAME(o) == NULL)	/* No basic type */
    {
	char tmp[80];
	short i;
	for (i = 0; i < 80; i++)
	    tmp[i] = 'Ä';
	tmp[MNU_W] = 0;
	Win_FastPutS(o, 0, tmp);
    } else
    {
	int col = 1;
	if (menu->textalign == SCR_LEFT)
	    col = 1;
	else
	{
	    int len = strlen(OPT_NAME(o));
	    col = MNU_COL - 1 - len;
	    if (menu->textalign == SCR_CENTRE)
		col /= 2;
	    if (col < 1)
		col = 1;
	}
	Win_FastPutS(o, col, OPT_NAME(o));
    }
    if (OPT_BTYPE(o) == MN_TOGGLE)
	Mnu_ShowLine(menu, o, OPT_PVAL(o, OPT_CHOICE(o)));
    else if (OPT_BTYPE(o) == MN_VALUE)
	if ((OPT_TYPE(o) & MN_STRING) == MN_STRING)
	    Mnu_ShowLine(menu, o, OPT_VALUE(o));
	else
	{
	    char tmp[16];
	    if ((OPT_TYPE(o) & MN_HEX) == MN_HEX)
		sprintf(tmp, "%x", *((int *)(OPT_VALUE(o))));
	    else
		itoa(*((int *)(OPT_VALUE(o))), tmp, 10);
	    Mnu_ShowLine(menu, o, tmp);
	}
    if (OPT_TYPE(o) & MN_SKIP)
	Win_SetAttribute(menu->ia);
}

static ushort NEAR 
Mnu_DoOption(MENUOPT * Opt, ushort ch, WINPTR responseWin, int size)
{
    if (Opt->type & MN_SPECIAL)
    {
	if (Opt->aux)
	    ch = (ushort) (*(Opt->aux)) ((void *)NULL);
    } else
	switch (7 & Opt->type)
	{
	case MN_SUBMENU:
	    ch = Mnu_Process(Opt->submenu, responseWin, 0);
	    if (ch == KB_CRGRTN && Opt->aux)
		(void)(*(Opt->aux)) ((void *)NULL);
	    break;
	case MN_COMMAND:
	    if (Opt->aux)
		ch = (ushort) ((*(Opt->aux)) ((void *)NULL));
	    break;
	case MN_VALUE:
	    ch = Mnu_ValOption(Opt, responseWin, size);
	    break;
	case MN_TOGGLE:
	    Opt->choice++;
	    if (Opt->choice >= Opt->maxchoices)
		Opt->choice = 0;
	    if (Opt->value != NULL)
		*((int *)Opt->value) = Opt->choice;
	    break;
	}
    return ch;
}


ushort 
Mnu_Process(MENUPTR menu, WINPTR responseWin, int timeout)
{
    WINPTR Mnu_Win, Old_Win = Win_Current;
    int i, Exit = FALSE;
    ushort ch = 0, wflags;
    char c;

    /* If all fields are SKIP type, will loop infinitely!!! */
    while (OPT_TYPE(MNU_OPTNOW) & MN_SKIP)
    {
	MNU_OPTNOW++;
	if (MNU_OPTNOW >= MNU_NOPTS)
	    MNU_OPTNOW = 0;
    }
    if (menu->border == 1)
	Win_1LineBorderSet();
    else
	Win_2LineBorderSet();
    wflags = WIN_CURSOROFF | WIN_BORDER | WIN_TILED | WIN_NONDESTRUCT | WIN_ZOOM;
    if (menu->border)
	wflags |= WIN_BORDER;
    Mnu_Win = Win_Make(wflags, MNU_X, MNU_Y, MNU_W, MNU_H, menu->ba, menu->ia);
    Mnu_Win->ia = menu->ha;
    Win_Activate(Mnu_Win);
    Win_PutTitle(MNU_TITLE, SCR_TOP, menu->titlealign);
    for (i = 0; i < MNU_NOPTS; i++)
    {
	if (OPT_BTYPE(i) == MN_TOGGLE)
	    if (OPT_VALUE(i) != NULL)
		if (OPT_CHOICE(i) != *((int *)OPT_VALUE(i)))
		    *((int *)OPT_VALUE(i)) = OPT_CHOICE(i);
	Mnu_ShowOption(menu, i);
    }
    while (!Exit)
    {
	i = MNU_OPTNOW;
	Win_PutAttribute(menu->ha, 1, i, MNU_W - 2, 1);
	if ((ch = Kb_GetChWithTimeout(timeout)) == 0)
	    break;		/* Timed out */
	switch (ch)
	{
	case KB_LEFT:
	case KB_RIGHT:
	case KB_ESC:
	    Exit = TRUE;
	    break;

	case KB_UP:
	    MNU_OPTNOW--;
	    if (MNU_OPTNOW < 0)
		MNU_OPTNOW = MNU_NOPTS - 1;
	    while (OPT_TYPE(MNU_OPTNOW) & MN_SKIP)
	    {
		MNU_OPTNOW--;
		if (MNU_OPTNOW < 0)
		    MNU_OPTNOW = MNU_NOPTS - 1;
	    }
	    break;
	case KB_DOWN:
	    MNU_OPTNOW++;
	    if (MNU_OPTNOW >= MNU_NOPTS)
		MNU_OPTNOW = 0;
	    while (OPT_TYPE(MNU_OPTNOW) & MN_SKIP)
	    {
		MNU_OPTNOW++;
		if (MNU_OPTNOW >= MNU_NOPTS)
		    MNU_OPTNOW = 0;
	    }
	    break;

	case KB_CRGRTN:
	    if (OPT_TYPE(i) & MN_FIXED)
		Hlp_IHelp(OPT_HELP(i));
	    else
		ch = Mnu_DoOption(&MNU_OPTS[i], ch, responseWin, menu->w - menu->optx);	/* Is the return value
											 * used? */
	    Exit = (OPT_TYPE(i) & MN_EXIT);
	    break;
	case KB_ALT_H:
	case KB_F1:
	    Hlp_IHelp(OPT_HELP(i));
	    break;
	default:
	    c = (char)(ch & 0xFF);
	    if (c > 'Z')
		c -= ('z' - 'Z');
	    if (c != 0)
	    {
		int oldopt = i;
		do
		{
		    i++;
		    if (i >= MNU_NOPTS)
			i = 0;
		    if (OPT_NAME(i)[0] == c && ((OPT_TYPE(i) & MN_SKIP) == 0))
		    {
			ch = KB_CRGRTN;
			Win_PutAttribute(menu->ia, 1, oldopt, MNU_W - 2, 1);
			Win_PutAttribute(menu->ha, 1, i, MNU_W - 2, 1);
			break;
		    }
		} while (i != oldopt);
		MNU_OPTNOW = i;
		i = oldopt;
	    } else
		Exit = TRUE;
	    break;
	}
	Mnu_ShowOption(menu, i);
	Mnu_ShowOption(menu, MNU_OPTNOW);
	Win_PutAttribute(menu->ia, 1, i, MNU_W - 2, 1);
    }
    Win_Free(Mnu_Win);
    if (Old_Win)
	Win_Activate(Old_Win);
    return ch;
}
