/************************
* help.c - Help System	*
************************/

#include "misc.h"
#include "help.h"
#include "screen.h"
#include "keybd.h"
#include "gen.h"


#define HWINHGHT	12	/* Help window height	 */
#define HWINWDTH	60	/* Help window width	 */
#define HWINTOP         (23-HWINHGHT)/2	/* Top edge position	 */
#define HWINLEFT	(78-HWINWDTH)/2	/* Left edge position	 */
#define HWINLINES	(HWINHGHT-4)	/* Actual usable lines	 */
#define HWINCOLS	(HWINWDTH-2)	/* Actual usable cols	 */
#define HLPMAXPGS	10	/* Max screens of help	 */
#define HLP_PARAMARK	'~'	/* Paragraph mark	 */
#define HLP_TABMARK	'^'	/* Indented paragraph	 */
#define HLPBUFSIZE	3072	/* Help buffer size	 */
#define HLP_BORDER_ATTR	Scr_ColorAttr(LIGHTCYAN,BLUE)
#define HLP_INSIDE_ATTR Scr_ColorAttr(RED,LIGHTGRAY)
#define HLP_INVRS_ATTR	Scr_ColorAttr(BLACK,RED)

static WINPTR Help_Win;
static STRING HlpLines[HLPMAXPGS][HWINLINES];	/* Offsets of each line into
						 * help text string */
static short HlpLnLens[HLPMAXPGS][HWINLINES];	/* length of each line */
char Hlp_FileName[64] = {0};


static short NEAR 
_Hlp_Paginate(STRING txt)
{
    STRING txtnow = txt;
    short linenow = 0;
    short colnow = 0;
    short coltmp;
    short pages = 0;
    BOOLEAN helpend = FALSE, keepspace;
    while (!helpend)
    {
	for (linenow = 0; linenow < HWINLINES; linenow++)
	{
	    keepspace = FALSE;
	    if (helpend)
	    {
		HlpLines[pages][linenow] = NULL;
		HlpLnLens[pages][linenow] = 0;
		continue;
	    }
	    HlpLines[pages][linenow] = txtnow;
	    colnow = strlen(txtnow);
	    if (colnow > HWINCOLS)
		colnow = HWINCOLS;
	    while (txtnow[colnow] != ' ' && txtnow[colnow] != '\0')
		colnow--;
	    if (txtnow[colnow] == '\0')
		helpend = TRUE;
	    coltmp = 0;
	    while (coltmp < colnow)
	    {
		if (txtnow[coltmp] == HLP_PARAMARK || txtnow[coltmp] == HLP_TABMARK)
		{
		    helpend = FALSE;
		    if (txtnow[coltmp] == HLP_TABMARK)
			keepspace = TRUE;
		    break;
		}
		coltmp++;
	    }
	    txtnow += (colnow = coltmp);
	    if (!helpend)
		do
		{
		    ++txtnow;
		}
		while (!keepspace && *txtnow == ' ');
	    HlpLnLens[pages][linenow] = colnow;
	}
	pages++;
	if (pages >= HLPMAXPGS)
	{
	    (void)Win_Error(NULL, "_Hlp_Paginate", "Help text too large", FALSE, FALSE);
	    pages = 0;
	    break;
	}
    }
    return pages;
}



static ushort NEAR 
_Hlp_ShowPage(short page)
{
    char c;
    short i;
    for (i = 0; i < HWINLINES; i++)
    {
	if (HlpLines[page][i] == NULL)
	    continue;
	c = HlpLines[page][i][HlpLnLens[page][i]];
	HlpLines[page][i][HlpLnLens[page][i]] = 0;
	Win_FastPutS(i + 1, 1, HlpLines[page][i]);
	HlpLines[page][i][HlpLnLens[page][i]] = c;
    }
    return Kb_GetCh();
}


void 
Hlp_ShowHelp(STRING txt)
{
    WINPTR Old_Win = Win_Current;
    short numpgs, pgnow = 0;
    STRING prompt;
    short promptpos;
    short promptlen;

    if (txt == NULL)
	txt = "No help available for this item.";
    if ((numpgs = _Hlp_Paginate(txt)) == 0)
	return;
    Win_1LineBorderSet();
    Help_Win = Win_Make(WIN_CURSOROFF | WIN_TILED | WIN_NONDESTRUCT | WIN_BORDER,
			HWINLEFT, HWINTOP, HWINWDTH, HWINHGHT,
			HLP_BORDER_ATTR, HLP_INSIDE_ATTR);
    Win_Activate(Help_Win);
    Win_PutTitle("< Help >", SCR_TOP, SCR_CENTRE);

    while (TRUE)
    {
	Win_Clear();
	if (pgnow == 0 && numpgs == 1)
	    prompt = " Press any key to resume ";
	else if (pgnow == 0)
	    prompt = " PgDn for next screen ";
	else if (pgnow < (numpgs - 1))
	    prompt = " PgDn for next; PgUp for previous ";
	else
	    prompt = " PgUp for previous screen ";
	promptlen = strlen(prompt);
	promptpos = (HWINCOLS - promptlen) / 2;

	Win_SetAttribute(HLP_INVRS_ATTR);
	Win_FastPutS(HWINHGHT - 2, promptpos, prompt);
	Win_SetAttribute(HLP_INSIDE_ATTR);
	switch (_Hlp_ShowPage(pgnow))
	{
	case KB_PGUP:
	    pgnow--;
	    if (pgnow < 0)
	    {
		pgnow = 0;
		beep();
	    }
	    continue;
	case KB_PGDN:
	    pgnow++;
	    if (pgnow >= numpgs)
	    {
		pgnow--;
		beep();
	    }
	    continue;
	default:
	    break;
	}
	break;
    }
    Win_Free(Help_Win);
    if (Old_Win)
	Win_Activate(Old_Win);
}


static void 
    _Hlp_Help(enum helpindex ih, STRING sh)
    {
	char helpbuff[HLPBUFSIZE];
	short rtn;
	if  (sh != NULL)
	        rtn = Gen_SGetFileEntry(Hlp_FileName, sh, helpbuff, HLPBUFSIZE, TRUE);
	else
	        rtn = Gen_IGetFileEntry(Hlp_FileName, ih, helpbuff, HLPBUFSIZE, TRUE);
	switch (rtn)
	{
	    case NOFILE:strcpy(helpbuff, "No help file: ");
	    strcat(helpbuff, Hlp_FileName);
	    Hlp_ShowHelp(helpbuff);
	    break;
	    case NOENTRY:Hlp_ShowHelp("No help entry");
	    break;
	    case BUFFOVRFLOW:Hlp_ShowHelp("Help text too large for help buffer");
	    break;
	    case SUCCESS:Hlp_ShowHelp(helpbuff);
	    break;
	}
    }

void 
    Hlp_IHelp(enum helpindex hlpindx)
    {
	_Hlp_Help(hlpindx, NULL);
    }

    void Hlp_SHelp(STRING hlpindx)
{
    _Hlp_Help(0, hlpindx);
}
