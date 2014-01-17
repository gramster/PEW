#include "misc.h"
#include "help.h"
#include "errors.h"
#include "keybd.h"
#include "screen.h"
#include "menu.h"
#include "ed.h"
#include "gen.h"
#include "analyze.h"

extern enum editorcommand ed_op;
extern short GenTrace;

/***********************************************************

	M  M  M  EEEEE  N   N  U   U   SSSS
	MMMMMMM  E      NN  N  U   U  S
	M MMM M  EEE    N N N  U   U   SSSS
	M  M  M  E      N  NN  U   U       S
	M  M  M  EEEEE  N   N   UUU    SSSS

************************************************************/

short MN_LineNum, MN_Seed;

BOOLEAN 
MN_RandomSeed(seed)
{
    srand((unsigned)seed);
    return TRUE;
}

BOOLEAN 
MN_LineValidate(line)
{
    long ln = (long)(line);
    if (ln < 1 || ln > Ed_LastLine)
    {
	Error(TRUE, ERR_BADLINE);
	return FALSE;
    }
    return TRUE;
}

BOOLEAN 
MN_TabValidate(tstop)
{
    if (tstop < 0 || tstop > 128)
    {
	Error(TRUE, ERR_BADTSTOP);
	return FALSE;
    } else
	return TRUE;
}

BOOLEAN 
MN_StepValidate(step)
{
    if (step == 0)
    {
	Error(TRUE, ERR_BADSTEP);
	return FALSE;
    } else
	return TRUE;
}

STRINGARRAY MN_OOToggle = {"Off", "On"};
STRINGARRAY MN_FillToggle = {"Spaces", "Tabs"};


MENUOPTARRAY MN_FileOpts = {
    {"Load", MN_VALUE | MN_STRING | MN_EXIT, HLP_LOADFILE,
    NULL, NULL, 0, 0, Ed_LoadName, NULL},

    {"Load Log", MN_COMMAND | MN_EXIT, HLP_LOADLOG,
    NULL, NULL, 0, 0, NULL, NULL},

    {"Save", MN_COMMAND | MN_EXIT, HLP_SAVEFILE,
    NULL, NULL, 0, 0, NULL, NULL},

    {"Change Name", MN_VALUE | MN_STRING | MN_EXIT, HLP_RENAME,
    NULL, NULL, 0, 0, Ed_FileName, NULL},

    {"New", MN_COMMAND | MN_EXIT, HLP_NEWFILE,
    NULL, NULL, 0, 0, NULL, NULL},

    {"Goto Line", MN_VALUE | MN_EXIT, HLP_GOTOLINE,
    NULL, MN_LineValidate, 0, 0, &MN_LineNum, NULL},

    {"OS Shell", MN_COMMAND | MN_EXIT, HLP_OSSHELL,
    NULL, NULL, 0, 0, NULL, NULL},

    {"Quit", MN_COMMAND | MN_EXIT, HLP_QUIT,
    NULL, NULL, 0, 0, NULL, NULL},

    {"Read Scrap", MN_VALUE | MN_STRING | MN_EXIT, HLP_READSCRAP,
    NULL, NULL, 0, 0, Ed_ScrapFileName, NULL},

    {"Write Scrap", MN_VALUE | MN_STRING | MN_EXIT, HLP_WRITESCRAP,
    NULL, NULL, 0, 0, Ed_ScrapFileName, NULL}
};

MENUOPTARRAY MN_OptOpts = {
    {"Tabsize", MN_VALUE, HLP_TABSIZE,
    NULL, MN_TabValidate, 0, 0, &Ed_TabStop, NULL},

    {"Backup Files", MN_TOGGLE, HLP_BACKUP,
    NULL, NULL, 2, 1, &Ed_BackUp, (STRINGARRAY *) MN_OOToggle},

    {"Fill Character", MN_TOGGLE, HLP_FILLCHAR,
    NULL, NULL, 2, 1, &Ed_FillTabs, (STRINGARRAY *) MN_FillToggle},

    {"Wrap Mode", MN_TOGGLE | MN_EXIT, HLP_WRAPMODE,
    NULL, NULL, 2, 1, &Ed_WrapLines, (STRINGARRAY *) MN_OOToggle},

    {"Left Search", MN_VALUE | MN_STRING, HLP_LEFTSRCH,
    NULL, NULL, 0, 0, LeftSearch, NULL},

    {"Right Search", MN_VALUE | MN_STRING, HLP_RIGHTSRCH,
    NULL, NULL, 0, 0, RightSearch, NULL},

    {"Search Text", MN_VALUE | MN_STRING, HLP_SRCHTEXT,
    NULL, NULL, 0, 0, Ed_SearchText, NULL},

    {"Translate Text", MN_VALUE | MN_STRING, HLP_TRANSTXT,
    NULL, NULL, 0, 0, Ed_ReplaceText, NULL},

    {"Case Sensitivity", MN_TOGGLE, HLP_CASESENS,
    NULL, NULL, 2, 1, &Ed_CaseSensitive, (STRINGARRAY *) MN_OOToggle},

    {"Identifiers Only", MN_TOGGLE, HLP_IDENTONLY,
    NULL, NULL, 2, 0, &Ed_IdentsOnly, (STRINGARRAY *) MN_OOToggle},

    {"Autosave Config", MN_TOGGLE, HLP_CFGAUTO,
    NULL, NULL, 2, 0, &Ed_ConfigSave, (STRINGARRAY *) MN_OOToggle},

    {"Save Config", MN_COMMAND | MN_EXIT, HLP_CFGSAVE,
    NULL, NULL, 0, 0, NULL, NULL},

    {"Random Seed", MN_VALUE | MN_EXIT, HLP_RANDOMSEED,
    NULL, MN_RandomSeed, 0, 0, &MN_Seed, NULL},

    {"Generate Trace", MN_TOGGLE, HLP_GENTRACE,
    NULL, NULL, 2, 0, &GenTrace, (STRINGARRAY *) MN_OOToggle}
};

MENUOPTARRAY MN_AnlyzOpts = {
    {"Independent Var", MN_VALUE | MN_STRING, HLP_ALZINDEP,
    NULL, NULL, 0, 0, Alz_Independent, NULL},
    {"From Value", MN_VALUE, HLP_ALZFROM,
    NULL, NULL, 0, 0, &Alz_Start, NULL},
    {"To Value", MN_VALUE, HLP_ALZTO,
    NULL, NULL, 0, 0, &Alz_End, NULL},
    {"Stepsize", MN_VALUE, HLP_ALZSTEP,
    NULL, MN_StepValidate, 0, 0, &Alz_Step, NULL},
    {"Edit Dependents", MN_COMMAND, HLP_ALZEDDEP,
    NULL, DepEdit, 0, 0, NULL, NULL},
    {"Analyze", MN_COMMAND | MN_EXIT, HLP_ALZGO,
    NULL, NULL, 0, 0, NULL, NULL}
};


MENUOPTARRAY MN_EdHelpOpts = {
    {"Fixed commands", MN_SUBMENU, HLP_FIXDCMDS,
    &FixCmd_Menu, NULL, 0, 0, NULL, NULL},

    {"Movement commands", MN_SUBMENU, HLP_MOVECMDS,
    &MovCmd_Menu, NULL, 0, 0, NULL, NULL},

    {"Block commands", MN_SUBMENU, HLP_BLKCMDS,
    &BlkCmd_Menu, NULL, 0, 0, NULL, NULL},

    {"Search/Translate", MN_SUBMENU, HLP_SRCHTRANS,
    &SrchCmd_Menu, NULL, 0, 0, NULL, NULL},

    {"Miscellaneous", MN_SUBMENU, HLP_MISC,
    &MiscCmd_Menu, NULL, 0, 0, NULL, NULL},

    {"Main Menu", MN_SUBMENU, HLP_MAINMENU,
    &MM_Menu, NULL, 0, 0, NULL, NULL}
};

SUBMENUOPT MN_FixCmdOpts[] = {
    {"Cursor Up", MN_VALUE | MN_STRING | MN_FIXED, HLP_CURSUP,
    NULL, NULL, 0, 0, "\030", NULL},

    {"Cursor Down", MN_VALUE | MN_STRING | MN_FIXED, HLP_CURSDOWN,
    NULL, NULL, 0, 0, "\031", NULL},

    {"Cursor Left", MN_VALUE | MN_STRING | MN_FIXED, HLP_CURSLEFT,
    NULL, NULL, 0, 0, "\033", NULL},

    {"Cursor Right", MN_VALUE | MN_STRING | MN_FIXED, HLP_CURSRGHT,
    NULL, NULL, 0, 0, "\032", NULL},

    {"Home", MN_VALUE | MN_STRING | MN_FIXED, HLP_HOME,
    NULL, NULL, 0, 0, "Home", NULL},

    {"End", MN_VALUE | MN_STRING | MN_FIXED, HLP_END,
    NULL, NULL, 0, 0, "End", NULL},

    {"Page Up", MN_VALUE | MN_STRING | MN_FIXED, HLP_PGUP,
    NULL, NULL, 0, 0, "PgUp", NULL},

    {"Page Down", MN_VALUE | MN_STRING | MN_FIXED, HLP_PGDN,
    NULL, NULL, 0, 0, "PgDn", NULL},

    {"Delete Text", MN_VALUE | MN_STRING | MN_FIXED, HLP_DELETE,
    NULL, NULL, 0, 0, "Del", NULL},

    {"Backspace", MN_VALUE | MN_STRING | MN_FIXED, HLP_BACKSPACE,
    NULL, NULL, 0, 0, "Backspace", NULL},

    {"Help", MN_VALUE | MN_STRING | MN_FIXED, HLP_HELP,
    NULL, NULL, 0, 0, "F1", NULL},

    {"Help", MN_VALUE | MN_STRING | MN_FIXED, HLP_HELP,
    NULL, NULL, 0, 0, "Alt-H", NULL}
};

SUBMENUOPT MN_MovCmdOpts[] = {
    {"Left Word", MN_VALUE | MN_STRING | MN_FIXED, HLP_WORDLEFT,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_LeftWord], NULL},

    {"Right Word", MN_VALUE | MN_STRING | MN_FIXED, HLP_WORDRGHT,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_RightWord], NULL},

    {"Top of File", MN_VALUE | MN_STRING | MN_FIXED, HLP_TOF,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_TopFile], NULL},

    {"End of File", MN_VALUE | MN_STRING | MN_FIXED, HLP_EOF,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_EndFile], NULL},

    {"Mark Place", MN_VALUE | MN_STRING | MN_FIXED, HLP_MARKPLACE,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_MarkPlace], NULL},

    {"Goto Mark", MN_VALUE | MN_STRING | MN_FIXED, HLP_GOTOMARK,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_GotoMark], NULL},

};

SUBMENUOPT MN_BlkCmdOpts[] = {
    {"Set Block Mark", MN_VALUE | MN_STRING | MN_FIXED, HLP_SETBMARK,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_MarkBlock], NULL},

    {"Set Column Mark", MN_VALUE | MN_STRING | MN_FIXED, HLP_SETCMARK,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_MarkColumn], NULL},

    {"Copy Text to Scrap", MN_VALUE | MN_STRING | MN_FIXED, HLP_COPYTEXT,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_Copy], NULL},

    {"Paste Block", MN_VALUE | MN_STRING | MN_FIXED, HLP_BPASTE,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_BlokPaste], NULL},

    {"Paste Columns", MN_VALUE | MN_STRING | MN_FIXED, HLP_CPASTE,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_ColPaste], NULL},

};

SUBMENUOPT MN_SrchCmdOpts[] = {
    {"Search Forward", MN_VALUE | MN_STRING | MN_FIXED, HLP_FSEARCH,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_FSearch], NULL},

    {"Search Backward", MN_VALUE | MN_STRING | MN_FIXED, HLP_BSEARCH,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_BSearch], NULL},

    {"Translate Text", MN_VALUE | MN_STRING | MN_FIXED, HLP_TRANSLATE,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_Translate], NULL},

    {"Match Character Left", MN_VALUE | MN_STRING | MN_FIXED, HLP_LMATCH,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_LeftMatch], NULL},

    {"Match Character Right", MN_VALUE | MN_STRING | MN_FIXED, HLP_RMATCH,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_RightMatch], NULL},

};

SUBMENUOPT MN_MiscCmdOpts[] = {
    {"Quit", MN_VALUE | MN_STRING | MN_FIXED, HLP_QUIT,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_Quit], NULL},

    {"Undo", MN_VALUE | MN_STRING | MN_FIXED, HLP_UNDO,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_Undo], NULL},

    {"Record Macro", MN_VALUE | MN_STRING | MN_FIXED, HLP_RECMACRO,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_RecMacro], NULL},

    {"Language Help", MN_VALUE | MN_STRING | MN_FIXED, HLP_LANGHELP,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_LangHelp], NULL},

    {"Templates", MN_VALUE | MN_STRING | MN_FIXED, HLP_TEMPLATE,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_Template], NULL},

    {"Delete Line", MN_VALUE | MN_STRING | MN_FIXED, HLP_DELLINE,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_DelLine], NULL},

};

SUBMENUOPT MN_MMOpts[] = {
    {"File Menu", MN_VALUE | MN_STRING | MN_FIXED, HLP_FILEMENU,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_File], NULL},

    {"Edit Command", MN_VALUE | MN_STRING | MN_FIXED, HLP_EDITCMD,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_Edit], NULL},

    {"Compile Command", MN_VALUE | MN_STRING | MN_FIXED, HLP_COMPILE,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_Compile], NULL},

    {"eXecute Command", MN_VALUE | MN_STRING | MN_FIXED, HLP_EXECCMD,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_Execute], NULL},

    {"Options Menu", MN_VALUE | MN_STRING | MN_FIXED, HLP_OPTMENU,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_Options], NULL},

    {"Analyze Menu", MN_VALUE | MN_STRING | MN_FIXED, HLP_ANLYZMENU,
    NULL, NULL, 0, 0, Ed_KeyNames[(int)_Ed_Analyze], NULL},

};

MENU File_Menu = {2, 2, 10, 26, 2, SCR_LEFT, SCR_LEFT, MENU_INSIDE_COLOR, MENU_INVRS_COLOR, MENU_BORDER_COLOR, 10, 0, 14, "<File>", (MENUOPTARRAY *) MN_FileOpts};
MENU Option_Menu = {2, 46, 14, 26, 2, SCR_LEFT, SCR_LEFT, MENU_INSIDE_COLOR, MENU_INVRS_COLOR, MENU_BORDER_COLOR, 14, 1, 18, "<Options>", (MENUOPTARRAY *) MN_OptOpts};
MENU Analyze_Menu = {2, 48, 6, 31, 2, SCR_LEFT, SCR_LEFT, MENU_INSIDE_COLOR, MENU_INVRS_COLOR, MENU_BORDER_COLOR, 6, 0, 18, "<Analyze>", (MENUOPTARRAY *) MN_AnlyzOpts};
MENU Help_Menu = {10, 30, 6, 20, 2, SCR_LEFT, SCR_LEFT, MENU_INSIDE_COLOR, MENU_INVRS_COLOR, MENU_BORDER_COLOR, 6, 0, 0, "<Editor Help>", (MENUOPTARRAY *) MN_EdHelpOpts};

SUBMENU FixCmd_Menu = {7, 28, 12, 26, 2, SCR_LEFT, SCR_LEFT, MENU_INSIDE_COLOR, MENU_INVRS_COLOR, MENU_BORDER_COLOR, 12, 0, 16, "<Fixed Commands>", (MENUOPTARRAY *) MN_FixCmdOpts};
SUBMENU MovCmd_Menu = {10, 24, 6, 31, 2, SCR_LEFT, SCR_LEFT, MENU_INSIDE_COLOR, MENU_INVRS_COLOR, MENU_BORDER_COLOR, 6, 0, 19, "<Movement Commands>", (MENUOPTARRAY *) MN_MovCmdOpts};
SUBMENU BlkCmd_Menu = {9, 22, 5, 34, 2, SCR_LEFT, SCR_LEFT, MENU_INSIDE_COLOR, MENU_INVRS_COLOR, MENU_BORDER_COLOR, 5, 0, 22, "<Block Commands>", (MENUOPTARRAY *) MN_BlkCmdOpts};
SUBMENU SrchCmd_Menu = {10, 21, 5, 36, 2, SCR_LEFT, SCR_LEFT, MENU_INSIDE_COLOR, MENU_INVRS_COLOR, MENU_BORDER_COLOR, 5, 0, 24, "<Search Commands>", (MENUOPTARRAY *) MN_SrchCmdOpts};
SUBMENU MiscCmd_Menu = {11, 26, 6, 27, 2, SCR_LEFT, SCR_LEFT, MENU_INSIDE_COLOR, MENU_INVRS_COLOR, MENU_BORDER_COLOR, 6, 0, 15, "<Misc Commands>", (MENUOPTARRAY *) MN_MiscCmdOpts};
SUBMENU MM_Menu = {9, 25, 6, 30, 2, SCR_LEFT, SCR_LEFT, MENU_INSIDE_COLOR, MENU_INVRS_COLOR, MENU_BORDER_COLOR, 6, 0, 18, "<Main Menu Bar>", (MENUOPTARRAY *) MN_MMOpts};

/************************
* Main Menu handling	*
************************/

#define MM_FILE		0
#define MM_EDIT		1
#define MM_COMPILE	2
#define MM_EXECUTE	3
#define MM_OPTIONS	4
#define MM_ANALYZE	5

#define MAINMENU	"   File     Edit     Compile      eXecute      Options    Analyze"

static short Pew_Menu = MM_FILE;


static void NEAR 
MM_Bold(short s, short w)
{
    WINPTR Last_Win = Win_Current;
    Win_Activate(Menu_Win);
    Win_PutAttribute(ED_INVRS_COLOR, s, 0, w, 1);
    if (Last_Win)
	Win_Activate(Last_Win);
}

static void NEAR 
MM_UnBold(short s, short w)
{
    WINPTR Last_Win = Win_Current;
    Win_Activate(Menu_Win);
    Win_PutAttribute(ED_INSIDE_COLOR, s, 0, w, 1);
    if (Last_Win)
	Win_Activate(Last_Win);
}


short MN_start[] = {3, 12, 21, 34, 47, 58};
short MN_len[] = {4, 4, 7, 7, 7, 7};

#define MN_MarkMenu(m)		MM_UnBold(MN_start[m], MN_len[m])
#define MN_UnMarkMenu(m)	MM_Bold(MN_start[m], MN_len[m])
#define DO_HELP(h) 		if (ch==KB_F1||ch==KB_ALT_H) Hlp_IHelp(h)

void 
Pew_InitMainMenu()
{
    Menu_Win = Win_Make(WIN_CURSOROFF, 0, 0, 80, 1, 0, ED_INVRS_COLOR);
    Win_Activate(Menu_Win);
    Win_FastPutS(0, 0, (STRING) MAINMENU);
    Win_PutAttribute(ED_INSIDE_COLOR, MN_start[MM_EDIT], 0, MN_len[MM_EDIT], 1);
    Alz_Step = 1;
}

void 
Pew_MainMenu()
{
    BOOLEAN active = TRUE, oldactive, quitmenu = FALSE;
    ushort ch;

    switch (ed_op)
    {
    case _Ed_Edit:
	Pew_Menu = MM_EDIT;
	break;
    case _Ed_Compile:
	Pew_Menu = MM_COMPILE;
	break;
    case _Ed_Execute:
	Pew_Menu = MM_EXECUTE;
	break;
    case _Ed_File:
	Pew_Menu = MM_FILE;
	break;
    case _Ed_Options:
	Pew_Menu = MM_OPTIONS;
	break;
    case _Ed_Analyze:
	Pew_Menu = MM_ANALYZE;
	break;
    }
    while (TRUE)
    {
	MM_Bold(0, 80);
	MN_MarkMenu(Pew_Menu);
	Scr_CursorOff();
	analyzing = FALSE;
	switch (Pew_Menu)
	{
	case MM_FILE:
	    if (active)
	    {
		MN_LineNum = Ed_LineNow + 1;
		ch = Mnu_Process(&File_Menu, Msg_Win, 0);
		if (ch == KB_CRGRTN)
		{
		    switch (File_Menu.optnow)
		    {
		    case 0:
			Ed_LoadFile(Ed_LoadName, TRUE);
			break;
		    case 1:
			Ed_LoadFile("ESTELLE.LOG", TRUE);
			break;
		    case 2:
			Ed_WriteFile(Ed_FileName);
			break;
		    case 3:
			Ed_GetBaseFileName();
			Ed_ShowPos(TRUE);
			break;
		    case 4:
			(void)Ed_InitBuff(DEFAULTNAME);
			Ed_ShowMessage("New file");
			break;
		    case 5:
			Ed_ChgRow(MN_LineNum - 1);
			break;
		    case 6:
			Gen_Suspend("the Estelle PDW");
			if (Win_Current)
			    Win_Activate(Win_Current);
			break;
		    case 7:
			Ed_Abort(0);
			break;
		    case 8:
			Ed_LoadScrap(Ed_ScrapFileName);
			break;
		    case 9:
			Ed_WriteScrap(Ed_ScrapFileName);
			break;
		    }
		    quitmenu = TRUE;
		}
	    } else
		ch = Kb_GetCh();
	    DO_HELP(HLP_FILEMENU);
	    break;
	case MM_EDIT:
	    if (active)
	    {
		Scr_CursorOn();
		return;
	    } else
		ch = Kb_GetCh();
	    DO_HELP(HLP_EDITCMD);
	    break;
	case MM_COMPILE:
	    if (active)
	    {
		Ed_Compile();
		Pew_Menu = MM_EDIT;
		goto done;	/* return immediately */
	    } else
		ch = Kb_GetCh();
	    DO_HELP(HLP_COMPILE);
	    break;
	case MM_EXECUTE:
	    if (active)
	    {
		Ed_Execute();
		Pew_Menu = MM_EDIT;
		goto done;
	    } else
		ch = Kb_GetCh();
	    DO_HELP(HLP_EXECCMD);
	    break;
	case MM_OPTIONS:
	    if (active)
	    {
		Ed_TabStop = Ed_Win->tabsize;
		Ed_Reversed = FALSE;
		Ed_MadeUpperCase = FALSE;
		ch = Mnu_Process(&Option_Menu, Msg_Win, 0);
		if (ch == KB_CRGRTN)
		{
		    switch (Option_Menu.optnow)
		    {
		    case 3:
			if (Ed_WrapLines)
			{
			    Ed_LeftCol = 0;
			    Win_SetFlags(WIN_WRAP);
			} else
			    Win_ClearFlags(WIN_WRAP);
			Ed_RefreshAll = TRUE;
			Win_Clear();
			Ed_ReCursor();
			break;
		    case 11:
			Ed_SaveConfig();
			break;
		    default:
			break;
		    }
		    quitmenu = TRUE;
		}
		Ed_Win->tabsize = Ed_TabStop;
	    } else
		ch = Kb_GetCh();
	    DO_HELP(HLP_OPTMENU);
	    break;
	case MM_ANALYZE:
	    if (active)
	    {
		ch = Mnu_Process(&Analyze_Menu, Msg_Win, 0);
		if (ch == KB_CRGRTN && Analyze_Menu.optnow == 5)
		{
		    /* Analyze	 */
		    short value;
		    char Alz_UntilStr[12];
		    ulong until;
		    Alz_UntilStr[0] = '0';
		    Alz_UntilStr[1] = 0;
		    if (Gen_GetResponse(NULL, "Timelimit? ", Alz_UntilStr, TRUE, 11, HLP_ALZTIME) != KB_ESC)
		    {
			analyzing = TRUE;
			until = (ulong) atol(Alz_UntilStr);
			analyze_init();
			strlwr(Alz_Independent);
			for (value = Alz_Start; value <= Alz_End; value += Alz_Step)
			{
			    Ed_CompileUsing(Alz_Independent, value);
			    if (!Ed_IsCompiled)
				break;	/* errors */
			    Ed_ExecUntil(until);
			}
			quitmenu = TRUE;
			analyze_complete();
		    }
		}
	    } else
		ch = Kb_GetCh();
	    DO_HELP(HLP_ANLYZMENU);
	    break;
	default:
	    Error(TRUE, ERR_BAD_ARGUMENT, "Ed_MainMenu");
	}
	if (quitmenu)
	    break;
	DO_HELP(HLP_MAINMENU);
	oldactive = active;
	active = TRUE;
	switch (ch)
	{
	case KB_CRGRTN:
	    active = TRUE;
	    break;
	case KB_ESC:
	    active = FALSE;
	    break;
	case KB_LEFT:
	    Pew_Menu--;
	    if (Pew_Menu < 0)
		Pew_Menu = MM_ANALYZE;
	    active = oldactive;
	    break;
	case KB_RIGHT:
	    Pew_Menu++;
	    if (Pew_Menu > MM_ANALYZE)
		Pew_Menu = MM_FILE;
	    active = oldactive;
	    break;

	case 'F':
	case 'f':
	    Pew_Menu = MM_FILE;
	    break;
	case 'E':
	case 'e':
	    Pew_Menu = MM_EDIT;
	    break;
	case 'C':
	case 'c':
	    Pew_Menu = MM_COMPILE;
	    break;
	case 'X':
	case 'x':
	    Pew_Menu = MM_EXECUTE;
	    break;
	case 'O':
	case 'o':
	    Pew_Menu = MM_OPTIONS;
	    break;
	case 'A':
	case 'a':
	    Pew_Menu = MM_ANALYZE;
	    break;
	default:
	    active = oldactive;
	    break;
	}
	oldactive = TRUE;
	if (ch == Ed_Key[(int)_Ed_Quit])
	    Ed_Abort(0);
	else if (ch == Ed_Key[(int)_Ed_File])
	    Pew_Menu = MM_FILE;
	else if (ch == Ed_Key[(int)_Ed_Edit])
	    Pew_Menu = MM_EDIT;
	else if (ch == Ed_Key[(int)_Ed_Compile])
	    Pew_Menu = MM_COMPILE;
	else if (ch == Ed_Key[(int)_Ed_Execute])
	    Pew_Menu = MM_EXECUTE;
	else if (ch == Ed_Key[(int)_Ed_Options])
	    Pew_Menu = MM_OPTIONS;
	else if (ch == Ed_Key[(int)_Ed_Analyze])
	    Pew_Menu = MM_ANALYZE;
	else
	    oldactive = active;
	active = oldactive;
    }
done:
    MM_Bold(0, 80);
    MN_MarkMenu(MM_EDIT);
    Scr_CursorOn();
    Ed_RefreshAll = TRUE;
    Ed_ReCursor();
}
