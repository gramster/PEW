/* ed.h */

#ifndef INCLUDED_ED

#define INCLUDED_ED

#define DEFAULTNAME	"NONAME.EST"
#ifndef GLOBAL
#define GLOBAL extern
#endif

GLOBAL char Ed_FileName[];
GLOBAL char Ed_LoadName[];
GLOBAL char Ed_ScrapFileName[];

GLOBAL WINPTR Ed_Win;
GLOBAL WINPTR Msg_Win;
GLOBAL WINPTR Menu_Win;

/* Reassignable editor commands */

enum editorcommand
{
    _Ed_DelBlock = -1,
    _Ed_Quit, _Ed_LeftWord, _Ed_RightWord, _Ed_LeftMatch, _Ed_RightMatch,
    _Ed_FSearch, _Ed_BSearch, _Ed_Translate, _Ed_MarkBlock, _Ed_MarkColumn,
    _Ed_File, _Ed_Options, _Ed_Analyze, _Ed_Undo, _Ed_Compile, _Ed_Execute,
    _Ed_Copy, _Ed_BlokPaste, _Ed_ColPaste, _Ed_Edit, _Ed_TopFile,
    _Ed_EndFile, _Ed_RecMacro, _Ed_MarkPlace, _Ed_GotoMark, _Ed_ToglDebug,
    _Ed_NextError, _Ed_PrevError, _Ed_LangHelp, _Ed_Template, _Ed_DelLine,
    _Ed_ToggleSize,
    _Ed_NumKeyDefs
};

GLOBAL ushort Ed_Key[_Ed_NumKeyDefs];	/* key assignments */
GLOBAL char Ed_KeyNames[_Ed_NumKeyDefs][16];	/* Printable names */

#define ED_SEARCHTEXTLEN	256
#define ED_SEARCHSETLEN		16
#define ED_PLACEMARKS		10

/********************************
* Editor Linked List Structure	*
********************************/

#define ED_MAXLINELEN	255

typedef struct bufline
{
    struct bufline
    far *next,			/* Forward pointer		 */
        far * prev;		/* Backward pointer		 */
    STRING far text;		/* Actual text of line		 */
    int ID;			/* Unique line identifier	 */
    unsigned char len;		/* Length 0-255	excluding NULL	 */
}
    BUFLINE;

typedef BUFLINE far *BUFPTR;

#define BUFELTSZ	sizeof(BUFLINE)

/****************
* Menus, etc	*
*****************/

#define ED_INSIDE_COLOR	Scr_ColorAttr(LIGHTGRAY,BLUE)
#define ED_BORDER_COLOR Scr_ColorAttr(WHITE,BLUE)
#define ED_INVRS_COLOR	Scr_ColorAttr(YELLOW,LIGHTGRAY)

#define MENU_INSIDE_COLOR	Scr_ColorAttr(BLUE,LIGHTGRAY)
#define MENU_BORDER_COLOR	Scr_ColorAttr(BLACK,LIGHTGRAY)
#define MENU_INVRS_COLOR	Scr_ColorAttr(WHITE,BLUE)

GLOBAL MENU File_Menu, Option_Menu, Debug_Menu, Help_Menu;

GLOBAL SUBMENU FixCmd_Menu, MovCmd_Menu, BlkCmd_Menu, SrchCmd_Menu, MiscCmd_Menu, MM_Menu;

GLOBAL short Ed_LineNow;
GLOBAL short Ed_LastLine;
GLOBAL short Ed_ColNow;
GLOBAL short Ed_LeftCol;
GLOBAL short Ed_PrefCol;

GLOBAL BOOLEAN Ed_RefreshAll;

/****************/

GLOBAL BOOLEAN Ed_IsCompiled;

/****************
* Option Flags	*
****************/

GLOBAL BOOLEAN Ed_BackUp;
GLOBAL BOOLEAN Ed_FillTabs;
GLOBAL BOOLEAN Ed_WrapLines;
GLOBAL ushort Ed_TabStop;
GLOBAL BOOLEAN Ed_ConfigSave;

/************************
* Search and Translate	*
************************/

GLOBAL char LeftSearch[16];
GLOBAL char RightSearch[16];
GLOBAL char Ed_SearchText[256];
GLOBAL char Ed_ReplaceText[256];
GLOBAL char Ed_RevSearchText[256];
GLOBAL BOOLEAN Ed_Reversed;
GLOBAL BOOLEAN Ed_MadeUpperCase;
GLOBAL BOOLEAN Ed_CaseSensitive;
GLOBAL BOOLEAN Ed_IdentsOnly;

/************************
*    Key assignments	*
************************/

GLOBAL char Ed_KeyNames[_Ed_NumKeyDefs][16];

/************************
*   Macro 'functions'	*
************************/

#define Ed_ChgRow(r)	Ed_FixCursor(r,Ed_PrefCol)
#define Ed_ReCursor()	Ed_FixCursor(Ed_LineNow, Ed_PrefCol)

/****************
*   Prototypes	*
****************/

ushort Ed_Quit(void);
void Ed_TopFile(void);
void Ed_EndFile(void);
void Ed_LeftWord(void);
void Ed_RightWord(void);
void Ed_LeftSearch(void);
void Ed_RightSearch(void);
void Ed_SearchForward(void);
void Ed_SearchBackward(void);
void Ed_Undo(void);
void Ed_Mark(void);
void Ed_MarkPlace(void);
void Ed_GotoMark(void);
void Ed_BlockOp(void);
void Ed_BlockPaste(void);
void Ed_ColumnPaste(void);
void Ed_Main(void);
void Ed_Abort(short sig);
void Ed_GetBaseFileName(void);
void Ed_ShowPos(BOOLEAN showall);
void Ed_ShowMessage(STRING msg);
ushort Ed_InitBuff(STRING filename);
void Ed_FixCursor(short r, short c);
void Ed_LoadFile(STRING name, BOOLEAN mustexist);
void Ed_WriteFile(STRING name);
void Ed_LoadScrap(STRING name);
void Ed_WriteScrap(STRING name);
void Ed_SaveConfig(void);
void Ed_Compile(void);
void Ed_CompileUsing(char *name, short value);
void Ed_Execute(void);
void Ed_ExecUntil(ulong until);

char Ed_NextChar(short *lineno);/* Compiler interface */

#endif				/* INCLUDED_ED */
