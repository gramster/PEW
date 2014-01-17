/************************************************/
/*						*/
/*	Browse.c - Execution environment	*/
/*			user interface.		*/
/*						*/
/************************************************/

#ifdef ECODE_DEBUGGING
#define NEED_OP_NAMES
#endif

#include "misc.h"
#include "help.h"

#include "screen.h"
#include "menu.h"
#include "ed.h"
#include "gen.h"
#include "keybd.h"
#include "symbol.h"
/*#undef GLOBAL
#define GLOBAL */
#include "interp.h"

#ifndef UNIX
static void NEAR UpdateBreakpointWindow(short actual, BOOLEAN firsttime);
static void NEAR MenuInfo_to_Brkpt(ushort bpnum);
#endif
BOOLEAN Set_Breakpoint(void *);
extern BOOLEAN MN_LineValidate(void *);
extern void Get_ModvarName(MODULE * mod, char *buff);
extern void Get_TransName(TRANS * tptr, char *buff, BOOLEAN fordisplay);
extern void Get_ModuleName(MODULE * mod, short index, char *buff);

extern WINPTR Msg_Win;
extern BOOLEAN analyzing;
const char *IP_StatLineTitle = " Name           Total Traffic   Mean Length  Max Length  Mean Time  Max Time";

#ifdef UNIX
#define Gen_ShowMessage(m)	fprintf(stderr,"%s\n",m)
#define Gen_ClearMessage()
#endif

const char *Trans_StatLineTitle = " From      To        Enabled Fired First Last    Back    Self    Forward";
typedef void (*void_function) (char[], short, BOOLEAN);


STRINGARRAY BrkClasses = {"Line", NULL, "Trans", "IP"};
STRINGARRAY proc_opts = {"Current", "All"};
STRINGARRAY pane_opts = {"None", "Trans", "IP", "Both"};
STRINGARRAY dump_opts = {"Current", "Type", "Peers", "All"};
STRINGARRAY YesNo = {"No", "Yes"};
STRINGARRAY control_opts = {"Continue", "Stop"};
STRINGARRAY trans_brkpt_types = {"Enablement", "Execution"};

#ifndef UNIX
static ushort first_interaction = 0;	/* First displayed interaction of
					 * currently highlighted IP queue */
#endif

#define BORDER_ATTR	Scr_ColorAttr(WHITE,BLUE)
#define CHILD_ATTR	Scr_ColorAttr(YELLOW,BLUE)
#define TRANS_ATTR	Scr_ColorAttr(YELLOW,BLUE)
#define IP_ATTR		Scr_ColorAttr(YELLOW,BLUE)

/* child_size and trans_size must be equal; two parameters
	are used simply for flexibility */

BOOLEAN 
open_browser_windows(ushort child_size, ushort trans_size, ushort IP_size)
{
    BOOLEAN rtn = TRUE;
    short i;
    if ((pew_wins[(short)CHILD_WIN] = WIN_MAKE(WIN_BORDER | WIN_CURSOROFF, 1, 4 + SOURCE_LNS, CHILD_COLS, child_size, BORDER_ATTR, CHILD_ATTR)) != NULL)
    {
	Win_Activate(pew_wins[(short)CHILD_WIN]);
	Win_PutTitle(" Children ", SCR_TOP, SCR_LEFT);
	scr_lines[(short)CHILD_WIN] = child_size;
    } else
	rtn = FALSE;
    if ((pew_wins[(short)TRANS_WIN] = WIN_MAKE(WIN_BORDER | WIN_CURSOROFF, 25, 4 + SOURCE_LNS, TRANS_COLS, trans_size, BORDER_ATTR, TRANS_ATTR)) != NULL)
    {
	Win_Activate(pew_wins[(short)TRANS_WIN]);
	Win_PutTitle(" Transitions ", SCR_TOP, SCR_LEFT);
	scr_lines[(short)TRANS_WIN] = trans_size;
    } else
	rtn = FALSE;
    if ((pew_wins[(short)IP_WIN] = WIN_MAKE(WIN_BORDER | WIN_CURSOROFF, 1, 6 + SOURCE_LNS + trans_size, IP_COLS, IP_size, BORDER_ATTR, IP_ATTR)) != NULL)
    {
	Win_Activate(pew_wins[(short)IP_WIN]);
	Win_PutTitle(" Interaction Points ", SCR_TOP, SCR_LEFT);
	scr_lines[(short)IP_WIN] = IP_size;
    } else
	rtn = FALSE;
    for (i = (short)SOURCE_WIN; i <= (short)IP_WIN; i++)
    {
	Win_SetInverse(pew_wins[i], Scr_ColorAttr(BLUE, LIGHTGRAY));
	highlight_line[i] = 0;
    }
    return rtn;
}

#ifndef UNIX

static void 
resize_windows(ushort child_size)
{
    if (child_size > 9 || child_size < 1)
    {
	beep();
	return;
    }
    Win_Free(pew_wins[(short)CHILD_WIN]);
    Win_Free(pew_wins[(short)IP_WIN]);
    Win_Free(pew_wins[(short)TRANS_WIN]);
    (void)open_browser_windows(child_size, child_size, 10 - child_size);
    refreshallwindows = TRUE;
    Win_Activate(pew_wins[(short)active_win]);
}

static void NEAR 
shrink_trans_win(void)
{
    resize_windows(scr_lines[(short)TRANS_WIN] - 1);
}

static void NEAR 
expand_trans_win(void)
{
    resize_windows(scr_lines[(short)TRANS_WIN] + 1);
}
#endif

static void NEAR 
set_mode(ushort mode)		/* set debug mode */
{
    if (mode == FAST)
	Set_Bit(db_mode, FAST);
    else if (mode == TRACE)
	Toggle_Bit(db_mode, TRACE);
    else
	db_mode = mode | (db_mode & TRACE);
    actual_line[(short)SOURCE_WIN] = run_sourceline;
}

void 
activate_module(MODULE * m)
{
    if (modnow != m || modnow->p != p)
    {
	/* deactivate current active module */
	modnow->p = p;
	modnow->s = s;
	modnow->b = b;
	modnow = m;
    }
    p = m->p;
    s = m->s;
    b = m->b;
    stack = m->stack;
    if (!buildBG)
	ActivateScope(modnow);
#ifdef ECODE_DEBUGGING
    if (ecode_debugging)
	total_lines[(short)CHILD_WIN] = s + 1;
    else
#endif
	total_lines[(short)CHILD_WIN] = (ushort) modnow->nummodvars;
    total_lines[(short)TRANS_WIN] = (ushort) modnow->numtrans;
#ifdef ECODE_DEBUGGING
    if (ecode_debugging)
	total_lines[(short)IP_WIN] = 9999;
    else
#endif
	total_lines[(short)IP_WIN] = (ushort) modnow->numIPs;
}


/*** Instead of always using locals and doing strcpys, I have decided
	to use globals for the next function...*/

char Brkpt_Class[24], Brkpt_Item[32], Brkpt_Process[32];

static void NEAR 
Get_Brkpt_Info(ushort bpnum, BOOLEAN addtitles)
{
    MODULE *oldmod = modnow;
    enum brkpt_class class = breakpts[bpnum].class;
    ushort where = breakpts[bpnum].where, offset;
    offset = addtitles ? 7 : 0;
    if (addtitles)
	strcpy(Brkpt_Class, "Class: ");
    strcpy(Brkpt_Class + offset, BrkClasses[(short)class]);
    if (class != BRK_FREE)
    {
	activate_module(breakpts[bpnum].process);
	switch (class)
	{
	case BRK_LINE:
	    sprintf(Brkpt_Item, "Line: %-6d", where + 1);
	    break;
	case BRK_TRANS:
	    if (addtitles)
		strcpy(Brkpt_Item, "Trans: ");
	    Get_TransName(modnow->trans_tbl + where,
			  Brkpt_Item + offset, TRUE);
	    if (breakpts[bpnum].flags & BRK_ONEXECUTE)
		strcat(Brkpt_Class, " execution");
	    else
		strcat(Brkpt_Class, " enablement");
	    break;
	case BRK_IP:
	    if (addtitles)
		strcpy(Brkpt_Item, "Ip:    ");
	    strncpy(Brkpt_Item + offset, Ip(modnow, where)->name, 32 - (int)offset);
	    break;
	default:
	    break;
	}
	if (addtitles)
	    strcpy(Brkpt_Process, "Proc:  ");
	if (breakpts[bpnum].class == BRK_LINE)
	    strcpy(Brkpt_Process + offset, "N/A");
	else if (breakpts[bpnum].flags & BRK_CHKALL)
	{
	    strcpy(Brkpt_Process + offset, "All ");
	    strncpy(Brkpt_Process + offset + 4, modnow->bodyname, 28 - (int)offset);
	} else
	    Get_ModvarName(modnow, Brkpt_Process + offset);
	activate_module(oldmod);
    } else
    {
	Brkpt_Class[0] = Brkpt_Item[0] = Brkpt_Process[0] = 0;
    }
}

#ifndef UNIX

static void NEAR 
View_Stats(short window, char const *title, void_function f)
{
    short num = (short)actual_line[window];	/* first_line[window] +
						 * highlight_line[window]; */
    short lines, count;
    WINPTR View_Win, OldWin = Win_Current;
    char number[4];
    number[0] = '1';
    number[1] = '\0';
    if (Gen_GetResponse(NULL, "How many?", number, TRUE, 3, HLP_NUMSTATS) == KB_CRGRTN)
    {
	count = atoi(number);
	if (count > 20)
	    count = 20;
	if ((num + count) >= total_lines[window])
	    count = total_lines[window] - num;
	if ((View_Win = Win_Make(WIN_BORDER | WIN_TILED | WIN_NONDESTRUCT | WIN_CURSOROFF, 1, (22 - count) / 2, 78, count + 2,
	     Scr_ColorAttr(WHITE, RED), Scr_ColorAttr(WHITE, RED))) != NULL)
	{
	    short i;
	    char buff[80];
	    Win_Activate(View_Win);
	    Win_Clear();
	    Win_PutTitle("< Press any key to resume... >", SCR_TOP, SCR_LEFT);
	    Win_PutCursor(0, 1);
	    Win_Printf((STRING) title);
	    Win_PutCursor(1, 1);
	    View_Win = View_Win;
	    for (i = 1; i < (Win_Width(View_Win) - 1); i++)
		Win_PutChar((uchar) 205);
	    for (i = num; i < (num + count); i++)
	    {
		Win_PutCursor(2 + i - num, 1);
		(*f) (buff, i, TRUE);
		Win_Printf(buff);
	    }
	    do
	    {
		ushort ch = Kb_RawGetC();
		if (ch == KB_ALT_H || ch == KB_F1)
		    Hlp_IHelp(window == (short)IP_WIN ? HLP_IPSTATS : HLP_TRANSSTATS);
		else
		    break;
	    } while (TRUE);
	    Win_Free(View_Win);
	    if (OldWin)
		Win_Activate(OldWin);
	}
    }
}

static BOOLEAN NEAR 
confirm_quit(void)
{
    ushort tmp = Gen_GetResponse(NULL, "Quit - are you sure?", "(YN)", TRUE, 4, HLP_QUITEXEC);
    if (tmp == 'Y' || tmp == 'y')
	return TRUE;
    else
	return FALSE;
}

static ushort NEAR 
get_execution_details(void)
{
    char buff[12];
    ushort rtn;
    strcpy(buff, "0");		/* Default */
    rtn = Gen_GetResponse(NULL, "Enter animate delay (milliseconds)", buff, TRUE, 10, HLP_ANIMATE);
    animate_delay = (ushort) atoi(buff);
    strcpy(buff, "0");		/* Default */
    if (rtn != KB_ESC)
	rtn = Gen_GetResponse(NULL, "Enter Execution Timelimit ", buff, TRUE, 10, HLP_TIMELIMIT);
    if (rtn != KB_ESC && animate_delay == 0)
    {
	rtn = Gen_GetResponse(NULL, "Full Speed? ", "(YN)", TRUE, 1, HLP_FULLSPEED);
	if (rtn == 'Y')
	    fullspeed = TRUE;
    }
    if (rtn != KB_ESC)
    {
	set_mode(AUTO_MODE);
	if (animate_delay < 1)
	    set_mode(FAST);
	if (animate_delay > 5000)
	    animate_delay = 5000;
	timelimit = (ulong) atol(buff);
    }
    return rtn;
}

static short NEAR 
regen_win_info(short win_index, BOOLEAN refreshall)
{
    short rtn = 0;
    static ushort last_first_interaction = 0;
    short win_width, win_height;
    ushort old_firstline = first_line[win_index];
    Win_Activate(pew_wins[win_index]);
    win_width = WIN_WIDTH;
    win_height = WIN_HEIGHT;
    if (total_lines[win_index] == 0)
    {
	Win_Clear();
	Win_PutCursor(0, 0);
	Win_PutString("(none)");
    } else
    {
	if (actual_line[win_index] >= total_lines[win_index])
	    actual_line[win_index] = 0;
	if (actual_line[win_index] < first_line[win_index])
	    first_line[win_index] = actual_line[win_index];
	else if (actual_line[win_index] >= (first_line[win_index] + (unsigned)win_height))
	    first_line[win_index] = actual_line[win_index] - (unsigned)win_height + 1;
	Win_PutAttribute(pew_wins[win_index]->ca, 0, (short)highlight_line[win_index], win_width, 1);
	highlight_line[win_index] = actual_line[win_index] - first_line[win_index];
	if (old_firstline != first_line[win_index] || refreshall
	    || (win_index == (short)IP_WIN && last_first_interaction != first_interaction))
	{
	    Win_Clear();
	    rtn = 2;
	} else
	    rtn = 1;
    }
    last_first_interaction = first_interaction;
    return rtn;
}

#endif				/* UNIX */

#ifdef ECODE_DEBUGGING

void 
Get_Stack_Line(short stk_elt, char *buff, BOOLEAN add_newline)
{
    char c = ' ';
    short v_start, x_cnt = modnow->exvars, p_cnt = modnow->params, v_cnt = modnow->vars;
    v_start = x_cnt + p_cnt + 3;/* address of start of variable space */
    if (stk_elt == s)
	c = '>';
    if (stk_elt == b)
	c = 'b';
    if (stk_elt <= x_cnt)
	c = 'x';
    else if (stk_elt <= (x_cnt + p_cnt))
	c = 'p';
    else if (stk_elt > v_start && stk_elt <= (v_start + v_cnt))
	c = 'v';
    sprintf(buff, "%3d:  %c %5d%c", stk_elt, c, stack[stk_elt], add_newline ? '\n' : '\0');
}

#endif

void 
resync_display(BOOLEAN do_lazy_update)
{
#ifndef UNIX
    extern short Ed_ScrLine;
    extern void Ed_FixCursor(), Ed_ShowPos();
    char linebuff[80], tmpbuff[24];
    ushort i;
    short regen_level, j;
    static enum exec_win last_active_win = -1;
    static MODULE *lastactivemodule = NULL;
    WINPTR old_win = Win_Current;
    if (buildBG)
	return;
    if (ignore_interrupts && (do_lazy_update || active_win != TRANS_WIN))
	goto DONE;
    /* If memory is low Ed_ShowPos should display it even in FAST mode */
    if (IS_FAST)
	if (farcoreleft() > 20000)
	{
	    Scr_PutCursor(24, 74);
	    sprintf(linebuff, "%5ld", globaltime);
	    Scr_PutString(linebuff, SCR_NORML_ATTR);
	    Scr_CursorOff();
	    goto DONE;
	} else if (farcoreleft() < 2000)
	{
	    (void)Win_Error(NULL, "", "Memory low - returning to step mode", TRUE, TRUE);
	    set_mode(STEP_MODE);
	}
    if (!do_lazy_update || active_win == SOURCE_WIN || active_win == TRANS_WIN)
    {
	uchar attr = Win_Attribute(pew_wins[(short)SOURCE_WIN]), invattr = Win_InverseAttr(pew_wins[(short)SOURCE_WIN]), battr = Win_BorderAttr(pew_wins[(short)SOURCE_WIN]);
	Win_Activate(pew_wins[(short)SOURCE_WIN]);
	Ed_FixCursor((short)(actual_line[(short)SOURCE_WIN]), (short)0);
	Ed_ShowPos(FALSE);
	if (active_win == SOURCE_WIN)
	    Win_PutAttribute(attr, 0, (short)highlight_line[(short)SOURCE_WIN], SOURCE_COLS, 1);
	for (i = 0; i < SOURCE_LNS; i++)
	{
	    if (run_sourceline != i + (unsigned)Ed_ScrLine)
		Scr_PutChar(2 + (short)i, 0, '\xB3', battr);
	    if (actual_line[(short)SOURCE_WIN] == i + (unsigned)Ed_ScrLine)
	    {
		if (active_win == SOURCE_WIN)
		    Win_PutAttribute(invattr, 0, (short)i, 78, 1);
		highlight_line[(short)SOURCE_WIN] = (ushort) i;
	    }
	}
    }
    if (last_active_win != active_win)
	do_lazy_update = FALSE;
    if (lastactivemodule != modnow)
	refreshallwindows = TRUE;
    lastactivemodule = modnow;
    Scr_PutCursor(24, 0);
    Scr_PutString("                                                                               ", SCR_NORML_ATTR);
    if (modnow == NULL)
	Scr_PutString("Preinitial State", SCR_NORML_ATTR);
    else
    {
	Get_ModvarName(modnow, tmpbuff);
	sprintf(linebuff, "%3s %-20s (", ClassName(modnow->class, TRUE), tmpbuff);
	strcat(linebuff, modnow->bodyname);
	strcat(linebuff, ")");
	linebuff[49] = 0;	/* truncate */
	Scr_PutString(linebuff, SCR_NORML_ATTR);
	Scr_PutCursor(24, 50);
	if (modnow->state == PREINITIAL_STATE)
	    strcpy(tmpbuff, "Preinitial");
	else if (modnow->state == IMPLICIT_STATE)
	    strcpy(tmpbuff, "Implicit");
	else
	    get_symbolic_name(modnow->state_ident, 0, tmpbuff, NULL, 24);
	sprintf(linebuff, "State:%-12s Time:%5ld ", tmpbuff, globaltime);
	Scr_PutString(linebuff, SCR_NORML_ATTR);
	Scr_CursorOff();
    }
    if (!do_lazy_update || active_win == CHILD_WIN
#ifdef ECODE_DEBUGGING
	|| ecode_debugging
#endif
	)
    {
	regen_level = regen_win_info((short)CHILD_WIN, refreshallwindows);
	if (regen_level == 2 || (regen_level == 1 && !do_lazy_update))
	{
#ifdef ECODE_DEBUGGING
	    if (ecode_debugging)
	    {
		short stk_line = 0, last_stk_line, stk_elt;
		stk_elt = first_line[(short)CHILD_WIN];
		Win_Activate(pew_wins[(short)CHILD_WIN]);
		Win_Clear();
		for (last_stk_line = scr_lines[(short)CHILD_WIN]; stk_line < last_stk_line; stk_line++)
		{
		    if (stk_elt != 0)
		    {
			char buff[16];
			Get_Stack_Line(stk_elt, buff, FALSE);
			Win_FastPutS(stk_line, 0, buff);
		    }
		    if (stk_elt++ >= s)
			break;
		}
	    } else
	    {
#endif
		for (i = 0; i < scr_lines[(short)CHILD_WIN]; i++)
		{
		    ushort child = i + first_line[(short)CHILD_WIN];
		    char *cl;
		    if (child >= total_lines[(short)CHILD_WIN])
			break;
		    get_symbolic_name(modnow->modvar_tbl[child].varident, (ushort) child, tmpbuff, NULL, 24);
		    if (CHILD(modnow, child))
		    {
			switch (CHILD(modnow, child)->class)
			{
			case CLASS_UNATTRIB:
			    cl = "U ";
			    break;
			case CLASS_SYSPROCESS:
			    cl = "SP";
			    break;
			case CLASS_SYSACTIVITY:
			    cl = "SA";
			    break;
			case CLASS_PROCESS:
			    cl = "P ";
			    break;
			case CLASS_ACTIVITY:
			    cl = "A ";
			    break;
			}
		    } else
			cl = "    Uninitialised";
		    strcpy(linebuff, cl);
		    strcat(linebuff, "  ");
		    strcat(linebuff, tmpbuff);
		    linebuff[CHILD_COLS] = 0;
		    Win_PutCursor((short)i, 0);
		    Win_PutString(linebuff);
		}
	    }
#ifdef ECODE_DEBUGGING
	}
#endif
	if (regen_level != 0 && active_win == CHILD_WIN)
	    Win_PutAttribute(pew_wins[(short)CHILD_WIN]->ia, 0, (short)highlight_line[(short)CHILD_WIN], CHILD_COLS, 1);
    }
    if (!do_lazy_update || active_win == IP_WIN
#ifdef ECODE_DEBUGGING
	|| ecode_debugging
#endif
	)
    {
#ifdef ECODE_DEBUGGING
	if (ecode_debugging)
	{
	    short address = p;
	    Win_Activate(pew_wins[(short)IP_WIN]);
	    Win_Clear();
	    Win_PutCursor(0, 0);
	    for (i = 0; i < scr_lines[(short)IP_WIN]; i++)
	    {
		short j = (short)Op_NArgs[(short)code[address]];
		extern int out_address;
		if (address >= out_address)
		    Win_Printf("%78c", ' ');
		else
		{
		    Win_Printf("%c%4d : %-16s%c", i == 0 ? '>' : ' ', address,
			     Op_Names[(short)code[address]], j ? '(' : ' ');
		    while (j--)
			Win_Printf("%5d%c", code[++address], j ? ',' : ')');
		    Win_PutChar((uchar) '\n');
		    Win_PutChar((uchar) '\r');
		}
		address++;
	    }
	} else
	{
#endif
	    regen_level = regen_win_info((short)IP_WIN, refreshallwindows);
	    if (regen_level == 2 || (regen_level == 1 && !do_lazy_update))
	    {
		ushort ipnum = first_line[(short)IP_WIN];
		for (i = 0; i < scr_lines[(short)IP_WIN]; i++)
		{
		    ushort len;
		    IP *ip;
		    INTERACTION *intr;
		    if (ipnum >= total_lines[(short)IP_WIN])
			break;
		    ip = Ip(modnow, ipnum);
		    intr = ip->queue->first;
		    len = ip->current_qlength;
		    if (i == highlight_line[(short)active_win])
		    {
			ushort first = first_interaction;
			if (first >= len && first > 0)
			    first = first_interaction = len - 1;
			while (first--)
			{
			    intr = intr->next;
			    len--;
			}
		    }
		    sprintf(linebuff, " %15s : (%3d) > ", ip->name, len);
		    if (i == highlight_line[(short)active_win] && first_interaction)
		    {
			strcat(linebuff, "...,");
			Win_ClearLine((short)i);
		    }
		    if (intr != NULL)
		    {
			while (strlen(linebuff) < IP_COLS)
			{
			    get_symbolic_name(intr->ident, 0, tmpbuff, NULL, 24);
			    strcat(linebuff, tmpbuff);
			    len--;
			    if (len)
				strcat(linebuff, ", ");
			    else
				break;
			    intr = intr->next;
			}
		    } else
			strcat(linebuff, "(empty)");
		    linebuff[IP_COLS] = 0;
		    Win_PutCursor((short)i, 0);
		    Win_PutString(linebuff);
		    ipnum++;
		}
	    }
	    if (regen_level != 0 && active_win == IP_WIN)
		Win_PutAttribute(pew_wins[(short)IP_WIN]->ia, 0, (short)highlight_line[(short)IP_WIN], IP_COLS, 1);
	}
#ifdef ECODE_DEBUGGING
    }
#endif
    if ((!do_lazy_update || active_win == TRANS_WIN) && modnow->trans_initialised)
    {
	regen_level = regen_win_info((short)TRANS_WIN, refreshallwindows);
	if (regen_level == 2 || (regen_level == 1 && !do_lazy_update))
	{
	    ushort trnum = first_line[(short)TRANS_WIN];
	    for (i = 0; i < scr_lines[(short)TRANS_WIN]; i++)
	    {
		if (trnum >= total_lines[(short)TRANS_WIN])
		    break;
		Get_TransStatusLine(trnum, linebuff, TRUE);
		Win_PutCursor((short)i, 0);
		Win_PutString(linebuff);
		trnum++;
	    }
	}
	if (regen_level != 0 && active_win == TRANS_WIN)
	    Win_PutAttribute(pew_wins[(short)TRANS_WIN]->ia, 0, (short)highlight_line[(short)TRANS_WIN], TRANS_COLS, 1);
    }
    if (old_win)
	Win_Activate(old_win);
    Scr_CursorOff();
    last_active_win = active_win;
DONE:
    j = (short)run_sourceline - Ed_ScrLine;
    if (j >= 0 && j < SOURCE_LNS)
	Scr_PutChar(j + 2, 0, '', pew_wins[(short)SOURCE_WIN]->ba);
    refreshallwindows = FALSE;
#endif
}

#ifndef UNIX
static void NEAR 
Toggle_Window(void)
{
    Win_PutAttribute(pew_wins[(short)active_win]->ca, 0, (short)highlight_line[(short)active_win], pew_wins[(short)active_win]->w, 1);
    do
    {
	active_win++;
	if (active_win > IP_WIN)
	    active_win = SOURCE_WIN;
    } while (total_lines[(short)active_win] == 0);
    Win_Activate(pew_wins[(short)active_win]);
    Win_PutAttribute(pew_wins[(short)active_win]->ia, 0, (short)highlight_line[(short)active_win], Win_Current->w, 1);
}

/* The initialisation values must agree with the menu toggle
	initial option value */

ushort brkpt_dumpproc = 1, brkpt_dumppane, brkpt_proc = 0, brkpt_passcnt = 0,
    brkpt_reset = 1, brkpt_activ = 0, brkpt_other = 0, goto_line, trans_brktyp,
    brkpt_stop = 1, brkpt_isactive = 1, brkpt_num = 1;

#define BRKPT_MENU_HGHT		17
#define BRKPT_MENU_WDTH		34
#define BRKPT_CONTEXT_LN	15

BOOLEAN 
Check_OtherBrkpt(void *arg)
{
    short bpnum = (short)arg;
    if (bpnum < 1 || bpnum > nextfreebrkpt)
    {
	Error(TRUE, ERR_BADBRKPT);
	return FALSE;
    }
    return TRUE;
}

MENUOPTARRAY brkpt_optarray = {
    {"Ready", MN_COMMAND | MN_EXIT, HLP_BRKPTREADY,
    NULL, NULL, 0, 0, NULL, NULL},
    {"Pass Count", MN_VALUE, HLP_PASSCNT,
    NULL, NULL, 0, 0, &brkpt_passcnt, NULL},
    {"Process", MN_TOGGLE, HLP_BRKPROCESS,
    NULL, NULL, 2, 0, &brkpt_proc, (STRINGARRAY *) proc_opts},
    {"Transition", MN_TOGGLE, HLP_TRANSBRKTYPES,
    NULL, NULL, 2, 0, &trans_brktyp, (STRINGARRAY *) trans_brkpt_types},
    {"Control", MN_TOGGLE, HLP_BRKCONTROL,
    NULL, NULL, 2, 1, &brkpt_stop, (STRINGARRAY *) control_opts},
    {"Reset", MN_TOGGLE, HLP_BRKRESET,
    NULL, NULL, 2, 1, &brkpt_reset, (STRINGARRAY *) YesNo},
    {"Active", MN_TOGGLE, HLP_BRKSUSPEND,
    NULL, NULL, 2, 1, &brkpt_isactive, (STRINGARRAY *) YesNo},


    {"컴컴컴컴 ACTION 컴컴컴컴컴�", MN_SKIP, (enum helpindex)0, NULL, 0,
	0, 0, NULL, NULL},
	{"Activate Other", MN_TOGGLE, HLP_BRKACTIVATE,
	NULL, Check_OtherBrkpt, 2, 0, &brkpt_activ, (STRINGARRAY *) YesNo},
	{"Breakpoint", MN_VALUE, HLP_BRKOTHER,
	NULL, NULL, 0, 0, &brkpt_other, NULL},
	{NULL, MN_SKIP, (enum helpindex)0, NULL, 0, 0, 0, NULL, NULL},
	    {"Dump Pane", MN_TOGGLE, HLP_DUMPPANE,
	    NULL, NULL, 4, 0, &brkpt_dumppane, (STRINGARRAY *) pane_opts},
	    {"Process", MN_TOGGLE, HLP_DUMPPROCESS,
	    NULL, NULL, 4, 1, &brkpt_dumpproc, (STRINGARRAY *) dump_opts},
	    {NULL, MN_SKIP, (enum helpindex)0, NULL, 0, 0, 0, NULL, NULL},
		{"Breakpoint Number", MN_SKIP | MN_VALUE, (enum helpindex)0,
		    NULL, NULL, 0, 0, &brkpt_num, NULL},
		    {NULL, MN_SKIP, (enum helpindex)0, NULL, 0, 0, 0, NULL,
			NULL},
			{NULL, MN_SKIP, (enum helpindex)0, NULL, 0, 0,
			    0, NULL, NULL}
			    };

			    MENU brkpt_menu = {(22 - BRKPT_MENU_HGHT) / 2, (78 - BRKPT_MENU_WDTH) / 2,
				BRKPT_MENU_HGHT, BRKPT_MENU_WDTH, 2, SCR_LEFT, SCR_LEFT,
				Scr_ColorAttr(WHITE, RED), Scr_ColorAttr(LIGHTRED, BLUE),
				Scr_ColorAttr(BLACK, LIGHTGRAY), BRKPT_MENU_HGHT,
			    0, 20, " Breakpoint Menu ", (MENUOPTARRAY *) brkpt_optarray};

			    static void NEAR BrkptInfo_to_Menu(ushort bpnum)
			    {
				MODULE *oldmod = modnow;
				/*
				 * Must set up: brkpt_passcnt	- Pass count
				 * brkpt_proc	- Processes for enablement
				 * brkpt_isactive	- Active or
				 * suspended? brkpt_activ	- Activate
				 * breakpoint? brkpt_other	- Breakpoint
				 * to Resume brkpt_dumpproc	- Processes
				 * for which to Dump brkpt_dumppane	-
				 * Panes to dump brkpt_stop	- Stop
				 * execution? brkpt_reset	- Reset
				 * breakpoint? trans_brktyp	- Transition
				 * brkpt type
				 */

				    brkpt_passcnt = breakpts[bpnum].pass_count;
				    brkpt_proc = breakpts[bpnum].flags & BRK_CHKALL ? 1 : 0;
				    brkpt_isactive = breakpts[bpnum].flags & BRK_SUSPENDED ? 0 : 1;
				    brkpt_activ = breakpts[bpnum].flags & BRK_ACTIVATE ? 1 : 0;
				    brkpt_other = breakpts[bpnum].reactivate;

				    brkpt_dumpproc = 0;
				if  (breakpts[bpnum].flags & BRK_FORBODTYPE)
				        brkpt_dumpproc = 1;
				else if (breakpts[bpnum].flags & BRK_FORPEERS)
				        brkpt_dumpproc = 2;
				else if (breakpts[bpnum].flags & BRK_FORALLPROCS)
				        brkpt_dumpproc = 3;

				    brkpt_dumppane = 0;
				if  (breakpts[bpnum].flags & BRK_DUMPTRANS)
				        brkpt_dumppane++;
				if  (breakpts[bpnum].flags & BRK_DUMPIPS)
				        brkpt_dumppane += 2;

				    brkpt_stop = breakpts[bpnum].flags & BRK_STOP ? 1 : 0;
				    brkpt_reset = breakpts[bpnum].flags & BRK_RESET ? 1 : 0;
				    trans_brktyp = breakpts[bpnum].flags & BRK_ONEXECUTE ? 1 : 0;
				    brkpt_num = bpnum + 1;
				    active_win = (enum exec_win)breakpts[bpnum].class;
				    actual_line[(short)active_win] = breakpts[bpnum].where;
				    Get_Brkpt_Info(bpnum, TRUE);
				    brkpt_optarray[BRKPT_CONTEXT_LN].Item = Brkpt_Item;
				if  (active_win == SOURCE_WIN)
				{
				    brkpt_menu.nopts = BRKPT_CONTEXT_LN + 1;
				    brkpt_menu.h = BRKPT_MENU_HGHT - 1;
				    brkpt_optarray[2].type |= MN_SKIP;
				} else
				{
				    brkpt_optarray[BRKPT_CONTEXT_LN + 1].Item = Brkpt_Process;
				    brkpt_menu.nopts = BRKPT_CONTEXT_LN + 2;
				    brkpt_menu.h = BRKPT_MENU_HGHT;
				    brkpt_optarray[2].type &= ~MN_SKIP;
				}
				if (active_win != TRANS_WIN)
				    brkpt_optarray[3].type |= MN_SKIP;
				else
				    brkpt_optarray[3].type &= ~MN_SKIP;
				activate_module(breakpts[bpnum].process);	/* What if this has been
										 * released? */
				if (Mnu_Process(&brkpt_menu, Msg_Win, 0) == KB_CRGRTN)
				    MenuInfo_to_Brkpt(bpnum);
				activate_module(oldmod);
				UpdateBreakpointWindow((short)bpnum, TRUE);
			    }

			    static void NEAR MenuInfo_to_Brkpt(ushort bpnum)
			    {
				/*
				 * The breakpoint info is given by:
				 * active_win	- Class brkpt_passcnt	-
				 * Pass count brkpt_proc	- Processes
				 * for enablement brkpt_isactive	-
				 * Active or suspended? brkpt_activ	-
				 * Activate breakpoint? brkpt_other	-
				 * Breakpoint to Resume brkpt_dumpproc	-
				 * Processes for which to Dump brkpt_dumppane
				 * - Panes to dump brkpt_stop	- Stop
				 * execution? brkpt_reset	- Reset
				 * breakpoint? actual_line[(short)active_win]
				 * - Breakpoint item index trans_brktyp
				 *  Transition brkpt type
				 */
				breakpts[bpnum].class = (enum brkpt_class)active_win;
				    breakpts[bpnum].modbodID = modnow->bodident;
				    breakpts[bpnum].pass_count = brkpt_passcnt;
				    breakpts[bpnum].enable_count = 0;
				    breakpts[bpnum].process = modnow;
				    breakpts[bpnum].flags = 0;
				if  (brkpt_activ)
				{
				    breakpts[bpnum].flags |= BRK_ACTIVATE;
				    breakpts[bpnum].reactivate = brkpt_other;
				}
				switch (brkpt_dumppane)
				{
				    case 0:break;
				case 1:
				    breakpts[bpnum].flags |= BRK_DUMPTRANS;
				    break;
				case 2:
				    breakpts[bpnum].flags |= BRK_DUMPIPS;
				    break;
				case 3:
				    breakpts[bpnum].flags |= BRK_DUMPTRANS | BRK_DUMPIPS;
				    break;
				}
				switch (brkpt_dumpproc)
				{
				case 0:
				    break;
				case 1:
				    breakpts[bpnum].flags |= BRK_FORBODTYPE;
				    break;
				case 2:
				    breakpts[bpnum].flags |= BRK_FORPEERS;
				    break;
				case 3:
				    breakpts[bpnum].flags |= BRK_FORALLPROCS;
				    break;
				}
				if (brkpt_reset)
				    breakpts[bpnum].flags |= BRK_RESET;
				if (!brkpt_isactive)
				    breakpts[bpnum].flags |= BRK_SUSPENDED;
				if (brkpt_stop)
				    breakpts[bpnum].flags |= BRK_STOP;
				if (brkpt_proc)
				    breakpts[bpnum].flags |= BRK_CHKALL;
				breakpts[bpnum].where = actual_line[(short)active_win];
				switch (active_win)
				{
				case TRANS_WIN:
				    (modnow->trans_tbl + actual_line[(short)TRANS_WIN])->breakpoint = bpnum;
				    if (trans_brktyp == 1)
					breakpts[bpnum].flags |= BRK_ONEXECUTE;
				    else
					breakpts[bpnum].flags &= (0xFFFF ^ BRK_ONEXECUTE);
				    break;
				case IP_WIN:
				    (modnow->IP_tbl + actual_line[(short)IP_WIN])->breakpoint = bpnum;
				default:
				    break;
				}
			    }

			    BOOLEAN Set_Breakpoint(void *arg)
			    {
				(void)arg;
				if (nextfreebrkpt == MAXBREAKPOINTS)
				    (void)Win_Error(NULL, "", "No more breakpoints allowed!", FALSE, TRUE);
				else
				    MenuInfo_to_Brkpt(nextfreebrkpt++);
				return TRUE;
			    }

			    static ushort NEAR DeleteBreakpoint(short bpnum)
			    {
				BOOLEAN has_refs = FALSE, mustdelete = TRUE;
				ushort first, last, now, other;
				if  (bpnum != -1)
				        first = last = (ushort) bpnum;
				else
				{
				    first = 0;
				    last = (ushort) ((short)nextfreebrkpt - 1);
				}
				for (now = first; now <= last; now++)
				{
				    if (bpnum != -1)	/* Check references */
				    {
					for (other = 0; other < nextfreebrkpt; other++)
					{
					    if (other != (ushort) bpnum)
						if (breakpts[other].flags & BRK_ACTIVATE)
						    if (breakpts[other].reactivate == (ushort) bpnum + 1)
						    {
							has_refs = TRUE;
							break;
						    }
					}
					if (has_refs)
					    if (Gen_GetResponse(NULL, "Breakpoint is referenced! Confirm deletion?", "(YN)", TRUE, 4, HLP_CONFIRMDEL) == 'N')
						mustdelete = FALSE;
				    }
				    if (mustdelete)
				    {
					breakpts[bpnum].class = BRK_FREE;
					--nextfreebrkpt;
					if (bpnum != -1)
					{
					    for (other = 0; other <= nextfreebrkpt; other++)
					    {
						if (other == (ushort) bpnum)
						    continue;
						if (breakpts[other].flags & BRK_ACTIVATE)
						{
						    if (breakpts[other].reactivate == (ushort) bpnum + 1)
							breakpts[other].flags ^= BRK_ACTIVATE;
						    else if (breakpts[other].reactivate == nextfreebrkpt + 1)
							breakpts[other].reactivate = (ushort) bpnum + 1;
						}
					    }
					    breakpts[bpnum].class = breakpts[nextfreebrkpt].class;
					    breakpts[bpnum].pass_count = breakpts[nextfreebrkpt].pass_count;
					    breakpts[bpnum].enable_count = breakpts[nextfreebrkpt].enable_count;
					    breakpts[bpnum].flags = breakpts[nextfreebrkpt].flags;
					    breakpts[bpnum].where = breakpts[nextfreebrkpt].where;
					    breakpts[bpnum].reactivate = breakpts[nextfreebrkpt].reactivate;
					    breakpts[bpnum].modbodID = breakpts[nextfreebrkpt].modbodID;
					    breakpts[bpnum].process = breakpts[nextfreebrkpt].process;
					}
				    }
				}
				if (bpnum > 0)
				    bpnum--;
				else
				    bpnum = 0;
				UpdateBreakpointWindow(bpnum, TRUE);
				return (ushort) bpnum;
			    }


			    static void NEAR Delete_Interaction(ushort which)
			    {
				ushort ipnum = actual_line[(short)IP_WIN];
				if  (ipnum < modnow->numIPs && modnow->numIPs > 0)
				{
				    char prompt[40];
				    ushort ch;
				        sprintf(prompt, "Delete interaction at position %d?", (int)(1 + which));
				        ch = Gen_GetResponse(NULL, prompt, "(YN)", TRUE, 4, HLP_DELINTR);
				    if  (ch == 'y' || ch == 'Y')
				    {
					INTERACTION *intr, *previntr = NULL;
					IP *ip;
					    ip = Ip(modnow, ipnum);
					    intr = ip->queue->first;
					while (which-- && intr)
					{
					    previntr = intr;
					    intr = intr->next;
					}
					if  (previntr == NULL)
					        ip->queue->first = intr->next;
					else
					    previntr->next = intr->next;
					Mem_Free(intr->argptr);
					Mem_Free(intr);
					ip->current_qlength--;
				    }
				} else
				    beep();
			    }

#define SHOW_HEIGHT		10
#define SHOW_WIDTH		78
#define SHOW_ATTR		Scr_ColorAttr(BLUE,LIGHTGRAY)
#define SHOW_BORDER_ATTR	Scr_ColorAttr(WHITE,LIGHTGRAY)
#define SHOW_TITLE_ATTR		Scr_ColorAttr(CYAN,BLACK)
#define SHOW_INVRS_ATTR 	Scr_ColorAttr(WHITE,RED)

			    static void NEAR UpdateBreakpointWindow(short actual, BOOLEAN firsttime)
			    {
				static short first = 0, lasthighlight = 0;
				short lastfirst;
				if  (!firsttime)
				        Win_PutAttribute(SHOW_ATTR, 1, lasthighlight, SHOW_WIDTH - 2, 1);
				    lastfirst = first;
				if  (firsttime)
				{
				    first = actual - SHOW_HEIGHT + 1;
				    if (first < 0)
					first = 0;
				} else if ((actual - first - 1) >= SHOW_HEIGHT)
				        first = SHOW_HEIGHT - actual + 1;
				if (first > actual)
				    first = actual;

				if (firsttime || first != lastfirst)
				{
				    /* Redraw window */
				    char linebuff[SHOW_WIDTH];
				    short ln;
				    Win_Clear();
				    if (nextfreebrkpt == 0)
				    {
					Win_FastPutS(1, 1, "No breakpoints have been set!");
					Win_FastPutS(3, 1, "Press any key...");
					lasthighlight = 1;
					return;
				    }
				    Win_SetAttribute(SHOW_TITLE_ATTR);
				    Win_FastPutS(0, 0, "    Num  Class       Process             Pass Count    Breakpoint Item        ");
				    Win_SetAttribute(SHOW_ATTR);
				    for (ln = 1; ln < SHOW_HEIGHT && (ln + first) <= (short)nextfreebrkpt; ln++)
				    {
					ushort bp = (ushort) (first + ln - 1),
					    tmplen;
					char c1, c2, *tmp;
					if (breakpts[bp].flags & BRK_SUSPENDED)
					{
					    c1 = '(';
					    c2 = ')';
					} else
					{
					    c1 = c2 = ' ';
					}
					Get_Brkpt_Info(bp, FALSE);
					tmp = Brkpt_Item;
					tmplen = 32;
					while (*tmp == ' ')
					{
					    tmp++;
					    tmplen--;
					}
					if (tmplen > 23)
					    tmp[23] = 0;
					Brkpt_Class[10] = Brkpt_Process[19] = 0;
					sprintf(linebuff, "%c%5d%c %-10s  %-19s%6d/%-8d%-23s", c1, bp + 1, c2, Brkpt_Class,
						Brkpt_Process, breakpts[bp].enable_count, breakpts[bp].pass_count, tmp);
					Win_FastPutS(ln, 1, linebuff);
				    }
				}
				Win_PutAttribute(SHOW_INVRS_ATTR, 1, lasthighlight = actual - first + 1, SHOW_WIDTH - 2, 1);
			    }


			    BOOLEAN Show_Breakpoints(void *arg)
			    {
				WINPTR ShowWin, oldwin = Win_Current;
				ushort keypress, highlight = 0, ch;
				BOOLEAN firsttime = TRUE;
				    (void)arg;
				    ShowWin = Win_Make(WIN_BORDER | WIN_TILED | WIN_CURSOROFF | WIN_NONDESTRUCT, 1, (22 - SHOW_HEIGHT) / 2,
						           SHOW_WIDTH, SHOW_HEIGHT, SHOW_BORDER_ATTR, SHOW_ATTR);
				    Win_Activate(ShowWin);
				do
				{
				    UpdateBreakpointWindow((short)highlight, firsttime);
				    firsttime = FALSE;
				    keypress = Kb_RawGetC();
				    if (nextfreebrkpt == 0)
					break;
				    switch (keypress)
				    {
					case KB_UP:if (highlight > 0)
					    highlight--;
					else
					    highlight = nextfreebrkpt - 1;
					break;
					case KB_DOWN:highlight++;
					if (highlight >= nextfreebrkpt)
					    highlight = 0;
					break;
					case KB_ESC:break;
					case KB_CRGRTN:BrkptInfo_to_Menu(highlight);
					break;
					case KB_DEL:ch = Gen_GetResponse(NULL, "Delete this breakpoint?", "(YNA)", TRUE, 5, HLP_BRKPTDEL);
					if (ch == (ushort) 'Y')
					    highlight = DeleteBreakpoint((short)highlight);
					else if (ch == (ushort) 'A')
					    highlight = DeleteBreakpoint(-1);
					break;
					case KB_F1:
					case KB_ALT_H:Hlp_IHelp(HLP_BRKPTBROWSE);
					break;
					default:beep();
					break;
				    }
				} while (keypress != KB_ESC);
				Win_Free(ShowWin);
				if (oldwin)
				    Win_Activate(oldwin);
				return TRUE;
			    }


#define LINE_MENU_HGHT	4
#define LINE_MENU_WDTH	28

			    MENUOPTARRAY line_optarray = {
				{"Set Breakpoint", MN_SUBMENU | MN_EXIT, HLP_SETLNBRKPT,
				&brkpt_menu, NULL, 0, 0, NULL, NULL},

				{"View Breakpoints", MN_COMMAND | MN_EXIT, HLP_VIEWBRKPTS,
				NULL, Show_Breakpoints, 0, 0, NULL, NULL},

				{NULL, MN_SKIP, (enum helpindex)0, NULL,
				    0, 0, 0, NULL, NULL},

				    {"Goto Line", MN_VALUE | MN_EXIT, HLP_GOTOLINE,
				    NULL, MN_LineValidate, 0, 0, &goto_line, NULL}
				};

				MENU line_dbug_menu = {(23 - LINE_MENU_HGHT) / 2, (78 - LINE_MENU_WDTH) / 2,
				    LINE_MENU_HGHT, LINE_MENU_WDTH, 2, SCR_LEFT, SCR_LEFT,
				    Scr_ColorAttr(CYAN, BLACK), Scr_ColorAttr(BLACK, CYAN),
				    Scr_ColorAttr(BLACK, LIGHTGRAY), LINE_MENU_HGHT,
				0, 18, " Line Menu ", (MENUOPTARRAY *) line_optarray};

				static void NEAR line_debug(void)
				{
				    char tmpbuf[40];
				        goto_line = actual_line[(short)SOURCE_WIN] + 1;
				        brkpt_passcnt = 0;
				        sprintf(tmpbuf, "Line               %-5d", goto_line);
				        brkpt_optarray[BRKPT_CONTEXT_LN].Item = tmpbuf;
				        brkpt_menu.nopts = BRKPT_CONTEXT_LN + 1;
				        brkpt_menu.h = BRKPT_MENU_HGHT - 1;
				        brkpt_num = nextfreebrkpt + 1;
				        brkpt_optarray[2].type |= MN_SKIP;
				        brkpt_optarray[3].type |= MN_SKIP;
				    if  (Mnu_Process(&line_dbug_menu, Msg_Win, 0) == KB_CRGRTN)
				    {
					switch (line_dbug_menu.optnow)
					{
					    case 0:(void)Set_Breakpoint(0);
					    break;
					    case 3:actual_line[(short)SOURCE_WIN] = goto_line - 1;
					    default:break;
					}
				    }
				}

#define TRANS_MENU_HGHT		4
#define TRANS_MENU_WDTH		32


				BOOLEAN View_Trans_Stats(void *arg)
				{
				    (void)arg;
				    View_Stats((short)TRANS_WIN, Trans_StatLineTitle,
					 (void_function) Get_TransStatLine);
				    return TRUE;
				}

				MENUOPTARRAY trans_optarray = {
				    {"Set Breakpoint", MN_SUBMENU | MN_EXIT, HLP_SETTRANSBRKPT,
				    &brkpt_menu, NULL, 0, 0, NULL, NULL},

				    {"View Breakpoints", MN_COMMAND | MN_EXIT, HLP_VIEWBRKPTS,
				    NULL, Show_Breakpoints, 0, 0, NULL, NULL},

				    {NULL, MN_SKIP, (enum helpindex)0,
					NULL, 0, 0, 0, NULL, NULL},

					{"View Stats", MN_COMMAND | MN_EXIT, HLP_VIEWTRANSSTATS,
					NULL, View_Trans_Stats, 0, 0, NULL, NULL}
				    };

				    MENU trans_dbug_menu = {(23 - TRANS_MENU_HGHT) / 2, (78 - TRANS_MENU_WDTH) / 2,
					TRANS_MENU_HGHT, TRANS_MENU_WDTH, 2, SCR_LEFT, SCR_LEFT,
					Scr_ColorAttr(CYAN, BLACK), Scr_ColorAttr(BLACK, CYAN),
					Scr_ColorAttr(BLACK, LIGHTGRAY), TRANS_MENU_HGHT,
				    0, 18, " Transition Menu ", (MENUOPTARRAY *) trans_optarray};

				    static void NEAR trans_debug(void)
				    {
					char tmpbuf1[34], tmpbuf2[30];
					TRANS *tptr = modnow->trans_tbl + actual_line[(short)TRANS_WIN];

					    strcpy(tmpbuf1, "Trans: ");
					    Get_TransName(tptr, tmpbuf1 + 7, TRUE);
					    brkpt_optarray[BRKPT_CONTEXT_LN].Item = tmpbuf1;

					    strcpy(tmpbuf2, "Proc:  ");
					    Get_ModvarName(modnow, tmpbuf2 + 7);
					    tmpbuf2[BRKPT_MENU_WDTH - 2] = 0;
					    brkpt_optarray[BRKPT_CONTEXT_LN + 1].Item = tmpbuf2;
					    brkpt_optarray[2].type &= ~MN_SKIP;
					    brkpt_optarray[3].type &= ~MN_SKIP;
					    brkpt_menu.nopts = BRKPT_CONTEXT_LN + 2;
					    brkpt_menu.h = BRKPT_MENU_HGHT;

					    brkpt_num = nextfreebrkpt + 1;
					if  (Mnu_Process(&trans_dbug_menu, Msg_Win, 0) == KB_CRGRTN)
					    if  (trans_dbug_menu.optnow == 0)
						    (void)Set_Breakpoint(0);
				    }

#define IP_MENU_HGHT	5
#define IP_MENU_WDTH	32

				    BOOLEAN View_CEP(void *arg)
				    {
					/*
					 * CHECK THIS FUNCTION and
					 * UPPERATTACH, as it may be that in
					 * upperattach I should only check
					 * 'connectpoint' after at least one
					 * movement along the list.
					 */
					ushort ipnum = first_line[(short)IP_WIN] + highlight_line[(short)IP_WIN];
					IP *ip1, *ip2;
					    (void)arg;
					    ip1 = Ip(modnow, ipnum);
					    ip2 = downattach(ip1);
					if  (ip1 == ip2)
					        ip2 = downattach(upperattach(ip2)->outIP);
					    activate_module(ip2->owner);
					    actual_line[(short)IP_WIN] = (ushort) (ip2 - modnow->IP_tbl);
					    refreshallwindows = TRUE;
					    first_interaction = 0;
					    return TRUE;
				    }

				    BOOLEAN View_IP_Stats(void *arg)
				    {
					(void)arg;
					View_Stats((short)IP_WIN, IP_StatLineTitle, (void_function) Get_IPStatLine);
					return TRUE;	/* Needed as called from
							 * menu */
				    }

				    MENUOPTARRAY IP_optarray = {
					{"Set Breakpoint", MN_SUBMENU | MN_EXIT, HLP_SETIPBRKPT,
					&brkpt_menu, Set_Breakpoint, 0, 0, NULL, NULL},

					{"View Breakpoints", MN_COMMAND | MN_EXIT, HLP_VIEWBRKPTS,
					NULL, Show_Breakpoints, 0, 0, NULL, NULL},

					{NULL, MN_SKIP, (enum helpindex)0,
					    NULL, 0, 0, 0, NULL, NULL},

					    {"View Stats", MN_COMMAND | MN_EXIT, HLP_VIEWIPSTATS,
					    NULL, View_IP_Stats, 0, 0, NULL, NULL},

					    {"View CEP", MN_COMMAND | MN_EXIT, HLP_VIEWCEP,
					    NULL, View_CEP, 0, 0, NULL, NULL},
					};

					MENU IP_dbug_menu = {(23 - IP_MENU_HGHT) / 2, (78 - IP_MENU_WDTH) / 2,
					    IP_MENU_HGHT, IP_MENU_WDTH, 2, SCR_LEFT, SCR_LEFT,
					    Scr_ColorAttr(CYAN, BLACK), Scr_ColorAttr(BLACK, CYAN),
					    Scr_ColorAttr(BLACK, LIGHTGRAY), IP_MENU_HGHT,
					0, 19, " IP Menu ", (MENUOPTARRAY *) IP_optarray};


					static void NEAR IP_debug(void)
					{
					    char tmpbuf1[BRKPT_MENU_WDTH],
					        tmpbuf2[30];
					        strcpy(tmpbuf1, "Ip:    ");
					        strncpy(tmpbuf1 + 7, Ip(modnow, actual_line[(short)IP_WIN])->name, BRKPT_MENU_WDTH - 9);
					        brkpt_optarray[BRKPT_CONTEXT_LN].Item = tmpbuf1;
					        strcpy(tmpbuf2, "Proc:  ");
					        Get_ModvarName(modnow, tmpbuf2 + 7);
					        brkpt_optarray[BRKPT_CONTEXT_LN + 1].Item = tmpbuf2;
					        brkpt_num = nextfreebrkpt + 1;
					        brkpt_optarray[3].type |= MN_SKIP;
					        brkpt_optarray[2].type &= ~MN_SKIP;
					        brkpt_menu.nopts = BRKPT_CONTEXT_LN + 2;
					        brkpt_menu.h = BRKPT_MENU_HGHT;
					        (void)Mnu_Process(&IP_dbug_menu, Msg_Win, 0);
					}

#endif				/* UNIX */

					void get_user_cmd(MODULE * running_process)	/* get a user command
											 * and act on it */
					{
#ifndef UNIX
					    ushort old_actual_line = actual_line[(short)active_win],
					        delta;
					    WINPTR old_win;
#endif
					    extern void resync_display();
#ifdef ECODE_DEBUGGING
					    static FILE *printer = NULL;
					    short stk_elt;
					    if  (buildBG)
						    return;
#ifndef UNIX
					    if  (globaltime > timelimit && analyzing)
						    longjmp(quit_info, -1);
					    if  (ecode_debugging)
					    {
						actual_line[(short)CHILD_WIN] = s;
						total_lines[(short)CHILD_WIN] = s + 1;
						actual_line[(short)IP_WIN] = p;
					    }
#endif
					    if  (ignore_interrupts)
					    {
						ushort tmp;
						if (Kb_RawLookC(&tmp))
						    if (tmp == KB_CTRL_E || tmp == KB_ALT_Z)
						    {
							if (critical_section)
							    Hlp_IHelp(HLP_CRIT_QUIT);
							else
							{
							    (void)Kb_RawGetC();
							    if (confirm_quit())
								longjmp(quit_info, -1);
							}
						    }
						return;
					    }
					    set_mode(STEP_MODE);
					    fullspeed = FALSE;
				    get_another:
					    delta = 0;
					    if (active_win == TRANS_WIN && actual_line[(short)active_win] != old_actual_line)
					    {
						if (total_lines[(short)TRANS_WIN])
						    actual_line[(short)SOURCE_WIN] = (ushort) (modnow->trans_tbl + actual_line[(short)TRANS_WIN])->startline;
					    }
					    resync_display(FALSE);
					    switch (Kb_RawGetC())	/* switch on upper-case
									 * char */
					    {
					    case KB_F10:
					    case KB_ALT_Z:
					    case KB_CTRL_E:
						if (critical_section)
						    Hlp_IHelp(HLP_CRIT_QUIT);
						else if (confirm_quit())
						{
						    Scr_CursorOn();	/* must change screen.c
									 * to do this anyway */
						    longjmp(quit_info, -1);
						}
						goto get_another;
					    case KB_F3:
						set_mode(TRANS_MODE);
						break;
					    case KB_F4:
						set_mode(UNIT_MODE);
						break;
					    case KB_F9:
						if (get_execution_details() == KB_ESC)
						    goto get_another;
						break;
					    case KB_SHIFT_F1:
						shrink_trans_win();
						goto get_another;
					    case KB_SHIFT_F2:
						expand_trans_win();
						goto get_another;
#ifdef ECODE_DEBUGGING
					    case KB_SHIFT_F9:
						stk_elt = s;
						if (printer == NULL)
						    printer = fdopen(4, "w");
						do
						{
						    char buff[16];
						    Get_Stack_Line(stk_elt, buff, TRUE);
						    fputs(buff, printer);
						} while (--stk_elt);
						fflush(printer);
						goto get_another;
					    case KB_SHIFT_F10:
						ecode_debugging = !ecode_debugging;
						if (ecode_debugging)
						{
						    actual_line[(short)CHILD_WIN] = s;
						    total_lines[(short)CHILD_WIN] = s + 1;
						    actual_line[(short)IP_WIN] = p;
						    total_lines[(short)IP_WIN] = 9999;
						} else
						{
						    actual_line[(short)CHILD_WIN] = actual_line[(short)IP_WIN] = 0;
						    total_lines[(short)CHILD_WIN] = modnow->nummodvars;
						    total_lines[(short)IP_WIN] = modnow->numIPs;
						}
						refreshallwindows = TRUE;
						resync_display(FALSE);
						goto get_another;
#endif
					    case KB_DEL:
						if (active_win == IP_WIN && !critical_section)
						{
						    Delete_Interaction(first_interaction);
						    refreshallwindows = TRUE;
						} else
						    beep();
						goto get_another;
					    case KB_F7:
						old_win = Win_Current;
						Win_Activate(IO_win);
						Scr_CursorOff();
						(void)Kb_RawGetC();
						if (old_win)
						    Win_Activate(old_win);
						goto get_another;
					    case KB_F8:
						set_mode(TRACE);
						Scr_PutCursor(23, 46);
						if (TRON)
						    Scr_PutString("Off", SCR_INVRS_ATTR);
						else
						    Scr_PutString("On ", SCR_INVRS_ATTR);
						goto get_another;
					    case KB_F2:
						set_mode(STEP_MODE);
						break;
					    case KB_F6:
						Toggle_Window();
						first_interaction = 0;
						goto get_another;
					    case KB_F5:
						if (modnow != root && !critical_section
#ifdef ECODE_DEBUGGING
						    && !ecode_debugging
#endif
						    )
						    activate_module(modnow->parent);
						else
						    beep();
						if (total_lines[(short)active_win] == 0)
						    Toggle_Window();
						first_interaction = 0;
						goto get_another;
					    case KB_LEFT:
						if (active_win == IP_WIN && first_interaction)
						    first_interaction--;
						else
						    beep();
						goto get_another;
					    case KB_RIGHT:
						if (active_win == IP_WIN)
						{
						    IP *ip = Ip(modnow, actual_line[(short)IP_WIN]);
						    if (ip->current_qlength > first_interaction)
							first_interaction++;
						    else
							beep();
						} else
						    beep();
						goto get_another;
					    case KB_PGUP:
						if (highlight_line[(short)active_win] == 0)
						    delta = scr_lines[(short)active_win] - 1;
						else
					    case KB_HOME:
						    delta = highlight_line[(short)active_win];
						if (actual_line[(short)active_win] < delta)
						    actual_line[(short)active_win] = 0;
						else
						    actual_line[(short)active_win] -= delta;
						first_interaction = 0;
						goto get_another;
					    case KB_CRGRTN:
						switch (active_win)
						{
						case SOURCE_WIN:
						    line_debug();
						    break;
						case CHILD_WIN:
						    if (modnow->nummodvars && !critical_section
#ifdef ECODE_DEBUGGING
							&& !ecode_debugging
#endif
							)
							if (CHILD(modnow, actual_line[(short)CHILD_WIN]))
							{
							    activate_module(CHILD(modnow, actual_line[(short)CHILD_WIN]));
							    if (total_lines[(short)active_win] == 0)
								Toggle_Window();
							    first_interaction = 0;
							    goto get_another;
							} else
							    beep();
						    else
							beep();
						    break;
						case TRANS_WIN:
						    if (total_lines[(short)TRANS_WIN])
							trans_debug();
						    else
							beep();
						    break;
						case IP_WIN:
						    if (total_lines[(short)IP_WIN]
#ifdef ECODE_DEBUGGING
							&& !ecode_debugging
#endif
							)
							IP_debug();
						    else
							beep();
						    break;
						}
						goto get_another;
					    case KB_PGDN:
						if ((highlight_line[(short)active_win] + 1) == scr_lines[(short)active_win])
						    actual_line[(short)active_win] += scr_lines[(short)active_win] - 1;
						else
					    case KB_END:
						    actual_line[(short)active_win] += scr_lines[(short)active_win] - 1 - highlight_line[(short)active_win];
						if (actual_line[(short)active_win] >= total_lines[(short)active_win])
						    actual_line[(short)active_win] = total_lines[(short)active_win] - 1;
						goto get_another;
					    case KB_UP:
						if (actual_line[(short)active_win] > 0)
						    actual_line[(short)active_win]--;
						first_interaction = 0;
						goto get_another;
					    case KB_DOWN:
						if (actual_line[(short)active_win] < (total_lines[(short)active_win] - 1))
						    actual_line[(short)active_win]++;
						first_interaction = 0;
						goto get_another;
					    case KB_F1:
					    case KB_ALT_H:
						Hlp_IHelp(HLP_EXEC_ENVIRONMENT);
						goto get_another;
					    default:
						beep();
						goto get_another;
					    }
					    activate_module(running_process);
					    first_interaction = 0;
#endif
					}


/**********************************/

					short number[10];

					char *hier(short level)
					{
					    static char buff[32], nb[8];
					    short l = 0;
					        buff[0] = 0;
					    if  (level >= 0)
						while (l <= level)
						{
						    sprintf(nb, "%d", (int)number[l++]);
						    strcat(buff, nb);
						    if (l <= level)
							strcat(buff, ".");
						}
					        return buff;
					}

					static void NEAR DumpHeading(MODULE * mod, short level)
					{
					    char modvarname[40], statename[10],
					        message[65];
					        activate_module(mod);
					        Get_ModvarName(mod, modvarname);
					        sprintf(message, "Dumping stats for process %s", modvarname);
					        Gen_ShowMessage(message);
					    if  (mod->state_ident == -1)
						    strcpy(statename, "Implicit");
					    else
						    get_symbolic_name(mod->state_ident, 0, statename, "", 10);
					        fprintf(logfp, "\n\n=====================================================================");
					        fprintf(logfp, "\n\n%s %s %s", ClassName(mod->class, FALSE), hier(level), modvarname);
					        fprintf(logfp, " (%s) [%s] (Init time: %ld)\n\n", mod->bodyname, statename, (long)mod->init_time);
					}

/* The next function has an inconsistency - it uses `mod', but calls
   Get_IPStatLine, which uses `modnow'.
*/

					static void NEAR dumpIPInfo(MODULE * mod)
					{
					    if (mod)
						if (mod->numIPs)
						{
						    ushort i, ipnum;
						    IP *ip;
						        fprintf(logfp, "\n\nINTERACTION POINTS\n\n");
						        fprintf(logfp, IP_StatLineTitle);
						        fprintf(logfp, "\n-------------------------------------------------------------------------------\n");
						    for (ipnum = 0; ipnum < mod->numIPs; ipnum++)
						    {
							char buff[80];
							    Get_IPStatLine(buff, ipnum, FALSE);
							    fprintf(logfp, buff);
						    }
						    fprintf(logfp, "\n\nMESSAGE SENDS - SUCCESSES AND LOSSES\n------- -----   --------- --- ------\n\n");
						    i = 0;
						    while (i < IP_INT_MAX)
						    {
							char intr_name[40];
							short sent, lost,
							    ident = mod->IP_Int_Tbl[i].ident;
							if  (ident == 0)
							        break;
							    ipnum = mod->IP_Int_Tbl[i].ip;
							    sent = mod->IP_Int_Tbl[i].successful;
							    lost = mod->IP_Int_Tbl[i].lost;
							    get_symbolic_name(ident, 0, intr_name, NULL, 40);
							    fprintf(logfp, "IP %-15s Intr: %-15s  Sent: %5d  Lost: %5d  Total: %5d\n",
								        Ip(mod, ipnum)->name, intr_name, sent, lost, sent + lost);
							    i++;
						    }

						        ip = Ip(mod, 0);
						    Gen_ShowMessage("Dumping current queue contents");
						    fprintf(logfp, "\n\nCURRENT QUEUE CONTENTS\n");
						    fprintf(logfp, "-----------------------------\n\n");
						    for (ipnum = 0; ipnum < mod->numIPs; ipnum++)
						    {
							INTERACTION *intr = ip->queue->first;
							fprintf(logfp, "Ip %5s (%5d) : ", ip->name, ip->current_qlength);
							if (intr != NULL)
							{
							    short len = 4;
							    while (len && intr != NULL)
							    {
								char intr_name[40];
								get_symbolic_name(intr->ident, 0, intr_name, NULL, 40);
								fprintf(logfp, "%10s (%ld)", intr_name, intr->time);
								intr = intr->next;
								len--;
							    }
							    if (intr)
								fprintf(logfp, ",...");
							} else
							    fprintf(logfp, "empty");
							fprintf(logfp, "\n");
							ip++;
						    }
						}
					}


					static void NEAR dumpMetaTable(MODULE * mod, char *name, short type)
					{
					    TRANS *tptr = modnow->trans_tbl,
					       *tp2;
					    ushort trnum, ents = 0;
					        fprintf(logfp, "\n\nTransition Sequence %s Table\n\n     ", name);
					    for (trnum = 1; trnum <= mod->numtrans; trnum++)
					    {
						if (tptr->fire_count)
						{
						    fprintf(logfp, "%4d ", trnum);
						    ents++;
						}
						    tptr++;
					    }
					    fprintf(logfp, "\n     ");
					    while (ents--)
						fprintf(logfp, "=====");
					    tptr = modnow->trans_tbl;
					    for (trnum = 0; trnum < mod->numtrans; trnum++, tptr++)
					    {
						short t2, *vals = 0, i;
						if (tptr->fire_count == 0)
						    continue;
						fprintf(logfp, "\n%4d:", trnum + 1);
						switch (type)
						{
						case 0:
						    vals = tptr->meta_trans_tbl;
						    Gen_ShowMessage("Dumping sequence table");
						    break;
						case 1:
						    vals = tptr->meta_delay_tbl;
						    Gen_ShowMessage("Dumping mean delays");
						    break;
						case 2:
						    vals = tptr->meta_min_tbl;
						    Gen_ShowMessage("Dumping minimum delays");
						    break;
						case 3:
						    vals = tptr->meta_max_tbl;
						    Gen_ShowMessage("Dumping maximum delays");
						    break;
						}
						/*
						 * We first want to check the
						 * meta-transition table for
						 * entries that can never
						 * occur. This could be if
						 * the succeeding transition
						 * has a FROM clause that
						 * excludes the current TO
						 * state; or, if the TO
						 * clause is a TO SAME, then
						 * if the two FROM sets are
						 * disjoint.
						 */

						if (vals)
						{	/* Has table; now check
							 * if it has a TO
							 * clause... */
						    short toState = code[tptr->addr + TR_TOSTATE];
						    tp2 = modnow->trans_tbl;
						    if (toState != -1)
						    {	/* has a TO clause; not
							 * TO SAME */
							for (t2 = 0; t2 < mod->numtrans; t2++, tp2++)
							{
							    /*
							     * Check FROM
							     * clauses
							     */
							    short offset = toState / 16;
							    short bitpos = toState % 16;
							    if (((short)code[tp2->addr + TR_FROMSTATES + offset] & (1 << bitpos)) == 0)
								vals[t2] = -1;	/* FROM can't follow TO */
							}
						    } else
						    {	/* no TO clause, or TO
							 * SAME */
							/*
							 * Check for disjoint
							 * from sets
							 */
							for (t2 = 0; t2 < mod->numtrans; t2++, tp2++)
							{
							    BOOLEAN canFollow = FALSE;
							    for (i = 0; i < 4; i++)
								if (code[tptr->addr + TR_FROMSTATES + i] & code[tp2->addr + TR_FROMSTATES + i])
								    canFollow = TRUE;
							    if (canFollow == FALSE)
								vals[t2] = -1;
							}
						    }

						    /*
						     * Also we can clear any
						     * delay table entries
						     * for those that never
						     * occurred
						     */
						    if (type > 0)	/* Delay table? */
							for (t2 = 0; t2 < mod->numtrans; t2++)
							{
							    if (tptr->meta_trans_tbl[t2] == 0)
								vals[t2] = -1;
							}
						}
						tp2 = modnow->trans_tbl;
						for (t2 = 0; t2 < mod->numtrans; t2++, tp2++)
						{
						    if (tp2->fire_count == 0)
							continue;
						    if (vals)
							if (vals[t2] >= 0)
							    fprintf(logfp, "%4d:", vals[t2]);
							else
							    fprintf(logfp, "   *:");
						    else
							fprintf(logfp, "   0:");
						}
						fprintf(logfp, "\n");
					    }
					    fprintf(logfp, "\n\n");
					}

					static void NEAR dumpTransInfo(MODULE * mod, BOOLEAN statenow, BOOLEAN stats)
					{
					    if (mod)
						if (mod->numtrans)
						{
						    ushort trnum;
						    char buff[80];
						        Gen_ShowMessage("Dumping transition info");
						        fprintf(logfp, "        TRANSITIONS\n\n");
						    if  (statenow)
						    {
							fprintf(logfp, "\n\n");
							for (trnum = 0; trnum < mod->numtrans; trnum++)
							{
							    Get_TransStatusLine(trnum, buff, FALSE);
							    fprintf(logfp, buff);
							}
							fprintf(logfp, "\n\n");
						    }
						    if  (stats)
						    {
							EventEntry *e = mod->classify;

							/*
							 * Print the meta
							 * transition tables
							 */

							dumpMetaTable(mod, "Count", 0);
							/*
							 * dumpMetaTable(mod,"
							 * Mean Delay", 1);
							 * dumpMetaTable(mod,"
							 * Min Delay", 2);
							 * dumpMetaTable(mod,"
							 * Max Delay", 3);
							 */

							/*
							 * Print message
							 * classes and
							 * transition lists
							 */

							Gen_ShowMessage("Dumping message classifications");
							if (e)
							{
							    short i;
							    fprintf(logfp, "TRANSITIONS CLASSIFIED BY MESSAGE EVENTS\n");
							    while (e)
							    {
								char intr_name[40];
								get_symbolic_name(e->msgident, 0, intr_name, NULL, 40);
								fprintf(logfp, "\n%c%-40s ", e->reception ? '+' : '-', intr_name);
								for (i = 0; i < MAXTRANS / 8; i++)
								    mod->other[i] |= e->transvect[i];
								for (i = 0; i < MAXTRANS; i++)
								    if (e->transvect[i / 8] & (1 << (i % 8)))
									fprintf(logfp, "%2d ", i + 1);
								e = e->next;
							    }
							    fprintf(logfp, "\n %-40s ", "OTHER");
							    for (i = 0; i < mod->numtrans; i++)
								if ((mod->other[i / 8] & (1 << (i % 8))) == 0)
								    fprintf(logfp, "%2d ", i + 1);
							    fprintf(logfp, "\n\n");
							}
							/*
							 * Print firing
							 * statistics
							 */

							Gen_ShowMessage("Dumping transition firing statistics");
							fprintf(logfp, Trans_StatLineTitle);
							fprintf(logfp, "\n-------------------------------------------------------------------------\n");
							for (trnum = 0; trnum < mod->numtrans; trnum++)
							{
							    Get_TransStatLine(buff, trnum, FALSE);
							    fprintf(logfp, buff);
							}
						    }
						}
					}

					void dumpRecursively(MODULE * mod, BOOLEAN dumpTrans, BOOLEAN dumpTransStats, BOOLEAN dumpIP)
					{
					    ushort modnum;
					        DumpHeading(mod, -1);
					        dumpTransInfo(mod, dumpTrans, dumpTransStats);
					    if  (dumpIP)
						    dumpIPInfo(mod);
					        Gen_ShowMessage("Finished statistics dump for process");
					    for (modnum = 0; modnum < mod->nummodvars; modnum++)
						    dumpRecursively(CHILD(mod, modnum), dumpTrans, dumpTransStats, dumpIP);
					}


					static void NEAR process_breakpoint(ushort brkpt_num)
					{
					    ushort flags = breakpts[brkpt_num].flags;
					    if  (flags & BRK_SUSPENDED)
						    return;
					    else if (breakpts[brkpt_num].enable_count < breakpts[brkpt_num].pass_count)
						    breakpts[brkpt_num].enable_count++;
					    else
					    {
						MODULE *mod = modnow;
						if  (flags & BRK_RESET)
						        breakpts[brkpt_num].enable_count = 0;
						else
						        breakpts[brkpt_num].flags |= BRK_SUSPENDED;

						    Get_Brkpt_Info(brkpt_num, TRUE);

						/* Perform breakpoint action */

						if  (flags & BRK_ACTIVATE)
						{
						    ushort which;
						    if  ((which = breakpts[brkpt_num].reactivate) != 0)
							    breakpts[which - 1].flags &= ~BRK_SUSPENDED;
						    else
							for (which = 0; which < nextfreebrkpt; which++)
							        breakpts[which].flags &= ~BRK_SUSPENDED;
						}
						/*
						 * If on Trans enablement,
						 * modnow may not correspond,
						 * so we activate the
						 * breakpoint process
						 */

						if  (breakpts[brkpt_num].class == BRK_TRANS)
						        activate_module(breakpts[brkpt_num].process);

						if (flags & BRK_DUMPBOTH)	/* Dump something */
						{
						    BOOLEAN dumpIP, dumpTrans;
						    short dumpLevel;
						    dumpIP = (flags & BRK_DUMPIPS) ? TRUE : FALSE;
						    dumpTrans = (flags & BRK_DUMPTRANS) ? TRUE : FALSE;
						    dumpLevel = 0;
						    if (FOR_ALL(flags))
							dumpLevel = 3;
						    else if (flags & BRK_FORBODTYPE)
							dumpLevel = 1;
						    else if (flags & BRK_FORPEERS)
							dumpLevel = 2;
						    fprintf(logfp, "\n\n***********************************\n\n");
						    fprintf(logfp, "Breakpoint %d matured at time %ld!\n\n",
							    (int)brkpt_num + 1, (long)globaltime);
						    fprintf(logfp, "%s\nPass Count: %5d\n", Brkpt_Class, breakpts[brkpt_num].pass_count);
						    fprintf(logfp, "%-24s\n%-32s\n", Brkpt_Process, Brkpt_Item);
						    /*
						     * The root process has
						     * no peers...
						     */
						    if (modnow == root && dumpLevel != 3)
							dumpLevel = 0;
						    if (dumpLevel == 0)
						    {
							DumpHeading(modnow, -1);
							dumpTransInfo(modnow, dumpTrans, dumpTrans);
							if (dumpIP)
							    dumpIPInfo(modnow);
						    } else if (dumpLevel != 3)
						    {
							ushort modnum;
							short whichbodytyp;
							MODULE *parent = modnow->parent;
							whichbodytyp = breakpts[brkpt_num].modbodID;
							for (modnum = 0; modnum < parent->nummodvars; modnum++)
							{
							    MODULE *chld = CHILD(parent, modnum);
							    if (dumpLevel == 1 && chld->bodident != whichbodytyp)
								continue;
							    DumpHeading(chld, -1);
							    dumpTransInfo(chld, dumpTrans, dumpTrans);
							    if (dumpIP)
								dumpIPInfo(chld);
							}
						    } else
							dumpRecursively(root, dumpTrans, dumpTrans, dumpIP);
						}
						if (breakpts[brkpt_num].flags & BRK_STOP)
						{
						    WINPTR oldwin = Win_Current,
						        tmp_win;
						    BOOLEAN old_ignore_interrupts = ignore_interrupts;
						    active_win = (enum exec_win)breakpts[brkpt_num].class;
						    if (active_win == TRANS_WIN)
							actual_line[(short)SOURCE_WIN] = (ushort)
							    (modnow->trans_tbl + breakpts[brkpt_num].where)->startline;
						    else
							actual_line[(short)SOURCE_WIN] = run_sourceline;
						    actual_line[(short)active_win] = breakpts[brkpt_num].where;
						    set_mode(STEP_MODE);
						    refreshallwindows = TRUE;
						    resync_display(FALSE);
						    tmp_win = Win_Make(WIN_BORDER | WIN_NONDESTRUCT | WIN_TILED | WIN_CURSOROFF,
								       20, 8, 40, 7, Scr_ColorAttr(RED, BLACK),
								       Scr_ColorAttr(RED, BLACK));
						    if (tmp_win)
						    {
							Win_Activate(tmp_win);
							Win_Clear();
							Win_PutCursor(1, 1);
							Win_Printf("Breakpoint %d matured!", (int)brkpt_num + 1);
							Win_FastPutS(3, 1, Brkpt_Class);
							Win_PutCursor(4, 1);
							Win_Printf(" Pass Count: %d", (int)breakpts[brkpt_num].pass_count);
							(void)Kb_RawGetC();
							Win_Free(tmp_win);
							if (oldwin)
							    Win_Activate(oldwin);
						    }
						    if (active_win != SOURCE_WIN)
						    {
							/*
							 * We exclude source
							 * win as interp will
							 * call get_user_cmd
							 * on return anyway
							 */
							ignore_interrupts = FALSE;
							get_user_cmd(mod);
							ignore_interrupts = old_ignore_interrupts;
						    }
						}
						activate_module(mod);
					    }
					}


					void check_breakpoints(enum brkpt_class class, short index)
					{
					    ushort i;
					    short idx = index;
					    TRANS *tptr;
					    if  (buildBG)
						    return;
					    if  (ignore_interrupts && class == BRK_LINE)
						    return;
					    if  (index < 0)
						    index = -index - 1;
					        tptr = modnow->trans_tbl + index;
					    for (i = 0; i < nextfreebrkpt; i++)
					    {
						BOOLEAN check = FALSE;
						if  (breakpts[i].class == class && breakpts[i].where == (ushort) index
						     &&  (breakpts[i].modbodID == modnow->bodident || class == BRK_LINE))
						{
						    switch (class)
						    {
							case BRK_TRANS:if (tptr->breakpoint == i || breakpts[i].flags & BRK_CHKALL)
							{
							    if (breakpts[i].flags & BRK_ONEXECUTE)
							    {
								check = (BOOLEAN) (tptr->flags & EXECUTING && idx >= 0);
							    } else
								    check = (BOOLEAN) ((tptr->flags & FIREABLE) && idx < 0);
							}
							break;
						    case BRK_IP:
							if ((modnow->IP_tbl + index)->breakpoint == i || breakpts[i].flags & BRK_CHKALL)
							    check = TRUE;
							break;
						    case BRK_LINE:
							check = TRUE;
						    default:
							break;
						    }
						}
						if (check)
						    process_breakpoint(i);
					    }
					}


/*********************************/



					static void NEAR _dump_stats(MODULE * mod, short level)
					{
					    MODULE *oldmod = modnow;
					    char modvarname[24];
					    if  (mod == NULL || analyzing)
						    return;
					        activate_module(mod);
					        number[level]++;
					        DumpHeading(mod, level);
					    if  (Bit_Clear(mod->checked, DUMP_BIT))
					    {
						short i = 0;
						while (i < mod->index)
						{
						    if (mod->parent->modvar_tbl[i].mod == mod)
						    {
							Get_ModuleName(mod, i, modvarname);
							fprintf(logfp, "Currently bound to same process as %s\n", modvarname);
							break;
						    } else
							    i++;
						}
						return;
					    }
					    dumpTransInfo(mod, TRUE, TRUE);
					    dumpIPInfo(mod);
					    if (mod->nummodvars > 0)	/* Recurse... */
					    {
						ushort m;
						number[++level] = 0;
						for (m = 0; m < mod->nummodvars; m++)
						    if (CHILD(mod, m))
							Set_Bit(CHILD(mod, m)->checked, DUMP_BIT);
						for (m = 0; m < mod->nummodvars; m++)
						{
						    MODULE *child = CHILD(mod, m);
						    if (child)
						    {
							_dump_stats(child, level);
							Clear_Bit(child->checked, DUMP_BIT);
						    }
						}
						level--;
					    }
					    activate_module(oldmod);
					}

					void dump_stats(MODULE * mod, short level)
					{
					    MODULE *modtmp = modnow;
					    short i;
					    if  (buildBG)
						    return;
					    for (i = 0; i <= level; i++)
						    number[i] = 0;
					        _dump_stats(mod, level);
					        activate_module(modtmp);
					}
