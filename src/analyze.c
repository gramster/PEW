#include "misc.h"
#include "help.h"
#include "errors.h"
#include "keybd.h"
#include "screen.h"
#include "menu.h"
#include "ed.h"
#include "gen.h"
#undef HUGE
#include <math.h>
#include "symbol.h"
#include "interp.h"
extern FILE *logfp;
extern ulong timelimit;

#undef GLOBAL
#define GLOBAL
#include "analyze.h"

#define MAXDEPENDANTS	8
#define MAXDEPLENGTH	72

char Dependants[MAXDEPENDANTS][MAXDEPLENGTH + 1] = {0};

/* Recalculate cptr and eptr for given row, col, and ensure 	*/
/* col is normalised if moved on to a shorter row.		*/

static void NEAR 
fixup(STRING * eptr, STRING * cptr, short row, short *col)
{
    *eptr = Dependants[row];
    while (**eptr)
	(*eptr)++;
    *cptr = &Dependants[row][*col];
    if (*cptr > *eptr)
    {
	*cptr = *eptr;		/* New row has insufficient cols */
	*col = *cptr - Dependants[row];
    }
}

BOOLEAN 
DepEdit()
{
    WINPTR OldWin = Win_Current, DepEdWin;
    DepEdWin = Win_Make(WIN_TILED | WIN_NONDESTRUCT | WIN_BORDER,
			(80 - MAXDEPLENGTH) / 2, (24 - MAXDEPENDANTS) / 2,
			MAXDEPLENGTH, MAXDEPENDANTS,
	     Scr_ColorAttr(LIGHTCYAN, BLUE), Scr_ColorAttr(RED, LIGHTGRAY));
    if (DepEdWin)
    {
	short row, col = 0;
	BOOLEAN ins_mode = TRUE;
	char *eptr, *cptr;
	ushort c = 0;
	Win_Activate(DepEdWin);
	Win_Clear();
	for (row = 0; row < MAXDEPENDANTS; row++)
	    Win_FastPutS(row, 0, Dependants[row]);
	fixup(&eptr, &cptr, row = 0, &col);
	Win_BlockCursor();
	while (c != KB_CRGRTN)
	{
	    Win_PutCursor(row, col);
	    c = Kb_GetCh();
	    switch (c)
	    {
	    case KB_UP:
		if (row > 0)
		{
		    row--;
		    fixup(&eptr, &cptr, row, &col);
		} else
		    beep();
		break;
	    case KB_DOWN:
		if (row < (MAXDEPENDANTS - 1))
		{
		    row++;
		    fixup(&eptr, &cptr, row, &col);
		} else
		    beep();
		break;
	    case KB_F1:
	    case KB_ALT_H:
		Hlp_IHelp(HLP_ALZEDDEP);
		break;
	    case KB_CRGRTN:
		break;
	    default:
		if (c == KB_LEFT && col)
		    col--;
		else if (c == KB_RIGHT && Dependants[row][col])
		    col++;
		Gen_EditString(c, Dependants[row],
			       &eptr, &cptr, &ins_mode, MAXDEPLENGTH + 1);
		Win_ClearLine(row);
		Win_FastPutS(row, 0, Dependants[row]);
		col = cptr - Dependants[row];
		Win_PutCursor(row, col);
		break;
	    }
	}
	Win_Free(DepEdWin);
    }
    Win_Activate(OldWin);
    return TRUE;
}

/********************************************/
/* Stuff used for the ANALYSE menu commands */
/********************************************/

typedef short tally[MAX_FIRE_ENTRIES];
static tally *Fire_Table, *Instance_Table;
static short exec_num;

void 
analyze_init()
{
    short tablesize;
    if (buildBG)
	return;
    tablesize = (Alz_End - Alz_Start) / Alz_Step + 1;
    Fire_Table = (tally *) Mem_Calloc(tablesize, sizeof(tally), 20);
    Instance_Table = (tally *) Mem_Calloc(tablesize, sizeof(tally), 21);
    exec_num = 0;
}

void 
gather_fire_entries()
{
    short i;
    if (buildBG || Fire_Table == NULL || Instance_Table == NULL)
	return;
    for (i = 0; i < MAX_FIRE_ENTRIES; i++)
    {
	Fire_Table[exec_num][i] = fire_entries[i].fire_count;
	Instance_Table[exec_num][i] = fire_entries[i].instance_count;
    }
    exec_num++;
}

static char analyze_token[32];
static long token_value;
static BOOLEAN parsing_error;
static short lex_row, indep_row, indep_val;
static char lookahead;

static char NEAR 
analyze_lex(short *col)		/* Return next lexical item */
{
    char *str, rtn;
    str = &Dependants[lex_row][*col];
    while (*str == ' ')
	str++;
    switch (*str)
    {
    case '+':
    case '-':
    case '/':
    case '*':
    case '(':
    case ')':
    case '=':
	rtn = *str++;
	break;
    default:
	rtn = 'n';		/* Default is number */
	if (*str == '\0')
	    rtn = 'E';		/* end of line */
	else if (isdigit(*str))
	{
	    token_value = 0l;
	    while (isdigit(*str))
		token_value = token_value * 10l + (long)(*str++ - '0');
	} else if (isalpha(*str))
	{
	    short tokenpos = 0;
	    while (isalnum(*str) || *str == '_')
	    {
		analyze_token[tokenpos++] = (char)tolower(*str);
		str++;
	    }
	    analyze_token[tokenpos] = '\0';
	    if (strcmp(analyze_token, "time") == 0)
		token_value = timelimit;
	    else if (strcmp(analyze_token, Alz_Independent) == 0)
		token_value = (long)indep_val;
	    else
		rtn = 'i';	/* ident */
	} else
	{
	    parsing_error = TRUE;
	    rtn = 'e';		/* error */
	}
	break;
    }
    *col = str - Dependants[lex_row];
    while (*str == ' ')
	str++;
    lookahead = *str;
    return rtn;
}

static double NEAR analyze_expr(short *col);

static double NEAR 
analyze_factor(short *col)
{
    extern short findID();	/* in compiler.c */
    double val = 0.;
    char c = analyze_lex(col);
    if (c == '(')
    {
	val = analyze_expr(col);
	if (analyze_lex(col) != ')')
	    parsing_error = TRUE;
    } else if (c == '-')
    {
	val = -analyze_factor(col);
    } else if (c == 'n')
	val = (double)token_value;
    else if (c == 'i')
    {
	short i, IDindex = findID(analyze_token);
	parsing_error = TRUE;
	for (i = 0; i < next_free_fire_entry; i++)
	    if (fire_entries[i].trans_ident == IDindex)
	    {
		val = (double)(Fire_Table[indep_row][i]);
		parsing_error = FALSE;
		break;
	    }
    } else
	parsing_error = TRUE;
    return val;
}

static double NEAR 
analyze_term(short *col)
{
    double val1, val2;
    val1 = analyze_factor(col);
    if (lookahead == '*' || lookahead == '/')
    {
	char c = analyze_lex(col);	/* skip operator */
	val2 = analyze_term(col);
	if (c == '*')
	    val1 *= val2;
	else if (val2 < 0.001 && val2 > -0.001)
	    parsing_error = TRUE;
	else
	    val1 /= val2;
    }
    return val1;
}

static double NEAR 
analyze_expr(short *col)
{
    double val1, val2;
    val1 = analyze_term(col);
    if (lookahead == '+' || lookahead == '-')
    {
	char c = analyze_lex(col);	/* skip operator */
	val2 = analyze_expr(col);
	if (c == '+')
	    val1 += val2;
	else
	    val1 -= val2;
    }
    return val1;
}

static double NEAR 
analyze_parse(short col)
{
    /* Parse and evaluate expressions. Parsing errors cause an abort. */
    double val = 0.;
    if (analyze_lex(&col) != '=')
	parsing_error = TRUE;
    else
	val = analyze_expr(&col);
    if (analyze_lex(&col) != 'E')
	parsing_error = TRUE;
    return val;
}

static char *line = "=========================================================================\n";

extern short ID_MapTable[];
extern char ID_NameStore[];

void 
analyze_complete()
{
    short i, tablesize = (Alz_End - Alz_Start) / Alz_Step + 1;
    if (buildBG)
	return;
    fprintf(logfp, "ANALYSIS RESULTS\n======== =======\n\n");
    fprintf(logfp, "Execution time: %ld units\n\n", timelimit);
    fprintf(logfp, "%-9s ", Alz_Independent);
    for (i = 0; i < next_free_fire_entry; i++)
	fprintf(logfp, "%-9.9s ", ID_NameStore + ID_MapTable[fire_entries[i].trans_ident]);
    fprintf(logfp, "\n%s", line);
    indep_val = Alz_Start;
    for (indep_row = 0; indep_row < tablesize; indep_row++)
    {
	fprintf(logfp, "%-9d ", indep_val);
	indep_val += Alz_Step;
	for (i = 0; i < next_free_fire_entry; i++)
	{
	    if (Instance_Table[indep_row][i])
		Fire_Table[indep_row][i] /= Instance_Table[indep_row][i];
	    else
		Fire_Table[indep_row][i] = 0;
	    fprintf(logfp, "%-9d ", Fire_Table[indep_row][i]);
	}
	fprintf(logfp, "\n");
    }
    fprintf(logfp, line);
    fprintf(logfp, "\n\n%-9s ", Alz_Independent);
    for (lex_row = 0; lex_row < MAXDEPENDANTS; lex_row++)
    {
	short col = 0;
	if (analyze_lex(&col) == 'i')
	    fprintf(logfp, "%-9.9s ", analyze_token);
    }
    fprintf(logfp, "\n%s", line);
    indep_val = Alz_Start;
    for (indep_row = 0; indep_row < tablesize; indep_row++)
    {
	fprintf(logfp, "%-9d ", indep_val);
	for (lex_row = 0; lex_row < MAXDEPENDANTS; lex_row++)
	{
	    short col = 0;
	    if (analyze_lex(&col) == 'i')
	    {
		double val;
		parsing_error = FALSE;
		val = analyze_parse(col);
		if (!parsing_error)
		    fprintf(logfp, "%-9.3g ", val);
		else
		    fprintf(logfp, "######### ");
	    }
	}
	fprintf(logfp, "\n");
	indep_val += Alz_Step;
    }
    fprintf(logfp, line);
    Mem_Free(Fire_Table);
    Mem_Free(Instance_Table);
    fclose(logfp);
    logfp = NULL;
    analyzing = FALSE;
}

void 
analyze_update(transition_table tptr)
{
    short j;
    /* Search for an entry and update */
    if (buildBG)
	return;
    for (j = 0; j < next_free_fire_entry; j++)
    {
	if (fire_entries[j].trans_ident == tptr->transname)
	{
	    fire_entries[j].instance_count++;
	    fire_entries[j].fire_count += tptr->fire_count;
	    break;
	}
    }
    /* If no entry found, make one */
    if (j == next_free_fire_entry)
    {
	fire_entries[j].trans_ident = tptr->transname;
	fire_entries[j].instance_count = 1;
	fire_entries[j].fire_count = tptr->fire_count;
	if (++next_free_fire_entry >= MAX_FIRE_ENTRIES)
	    interp_error(ERR_FIREENTRIES, MAX_FIRE_ENTRIES);
    }
}
