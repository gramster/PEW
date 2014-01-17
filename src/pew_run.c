/************************************************/
/* pew_run.c - conditional parts of interpreter */
/************************************************/

#include "misc.h"

#include "screen.h"
#include "gen.h"
#include "menu.h"
#include "ed.h"

#include "help.h"
#include "tree.h"

#ifndef INTEGRATE
#include <fcntl.h>
#ifndef EC_EI
#define NEED_OP_ARGS
#endif
#define ED_INSIDE_COLOR	Scr_ColorAttr(LIGHTGRAY,BLUE)
#define ED_BORDER_COLOR Scr_ColorAttr(WHITE,BLUE)
WINPTR Ed_Win;
WINPTR Msg_Win;
#ifndef UNIX
static WINPTR Pos_Win;
#endif
ulong sourcepos[3000];		/* File offsets for each line */
BOOLEAN analyzing = FALSE;
ulong Ed_FileSize = 0l;
#else
extern WINPTR Ed_Win;
#endif

#ifdef UNIX
#include <signal.h>
#define Gen_ShowMessage(m)	fprintf(stderr,"%s\n",m)
#define Gen_ClearMessage()
#else
#include <io.h>
#endif

#include "symbol.h"
#include "interp.h"
#ifdef INTEGRATE
#include "analyze.h"
#endif

void dumpTriggers(void);
void dumpKillers(void);
void dumpParams(void);
void Get_ModvarName(MODULE * mod, char *buff);

extern FILE *sourcefl;

extern short **GMT, GMTrows;
extern float **GMTdel;



#ifdef INTEGRATE
extern BOOLEAN Ed_WrapLines;
extern void Ed_ToggleSize(void);
extern short Ed_LastLine;
static BOOLEAN old_Ed_WrapLines;
#endif

short GenTrace = 0;
static short ClauseTrace = 0, ExecTrace = 0;
static char specname[12];

#ifndef INTEGRATE

short Ed_LastLine;

#ifdef UNIX
extern int filelength(int fd);
#endif

static void NEAR 
solorun_init(int argc, char *argv[])
{
    FILE *codefile;
    char infile[16], logfile[16];
    short nbytes, filearg = 0;
    char *codenow;
    char buff[128];
    void interp_abort();

#ifndef UNIX
    locspace = MK_FP(0xb800, 4096);
#else
    signal(SIGINT, interp_abort);
    srand48((long)(time(NULL)));
    if ((locspace = malloc(LOC_SPACE_SIZE)) == NULL)
    {
	fprintf(stderr, "Could not allocate symbol table memory\n");
	exit(0);
    }
#endif
    Gen_BuildName("PEWPATH", "PEW_HELP", Hlp_FileName);
    Ed_LastLine = 1;
    blocking = FALSE;
    if (argc < 2 || argc > 7)
    {
usage:
	fprintf(stderr, "Usage: %s <filename> [<log>][-c][-e][-B][-G][-I][-R][-(T|X)<time>]\n", argv[0]);
	fprintf(stderr, "where: <filename>   is the base name of the code file\n");
	fprintf(stderr, "       <log>        is the base name of the log file\n");
	fprintf(stderr, "       <time>       is the time to execute for\n");
	fprintf(stderr, "\nThe -c option creates a clause trace\n");
	fprintf(stderr, "\nThe -e option creates an execution trace for modelling\n");
	fprintf(stderr, "\nThe -B option is used for channel blocking\n");
	fprintf(stderr, "\nThe -G option causes a global TRG to be output\n");
	fprintf(stderr, "\nThe -I option turns interaction tracing on\n");
	fprintf(stderr, "\nThe -R option makes scheduling deterministic\n");
	fprintf(stderr, "\nThe -T option executes for the specified time\n");
	fprintf(stderr, "\nThe -X option executes for the specified time and exits\n");
	fprintf(stderr, "\n\nPress any key...\n");
	(void)getchar();
	fprintf(stderr, "Alternatively: %s <file> [-B] -C <depth> <maxnodes> [<start> <end>]\n", argv[0]);
	fprintf(stderr, "where: <depth>      is the depth of tree node comparison to use\n");
	fprintf(stderr, "\nThe -C option causes a controlled execution to be performed,\n");
	fprintf(stderr, "resulting in a behaviour graph.\n");
	fprintf(stderr, "\t<start> and <end> are the group start and end indices (from 1).\n");
	fprintf(stderr, "\t<maxnodes> is the maximum number of nodes to be explored.\n");
	fprintf(stderr, "\nThe -B option is used for channel blocking as usual\n");
	exit(0);
    }
    if (Sym_Load(argv[1]) == 0)
    {
	fprintf(stderr, "%s: cannot load symbol table file %s.sym; rerun compiler\n", argv[0], argv[1]);
	exit(-1);
    }
    strcpy(specname, argv[1]);
    strcpy(infile, specname);
    strcat(infile, ".est");
    buildBG = globalTRG = 0;
    autoquit = FALSE;
    if (argc > 2)
    {
	short i;
	for (i = 2; i < argc; i++)
	{
	    if (argv[i][0] == '-')
	    {
		switch (argv[i][1])
		{
		case 'c':
		    ClauseTrace = 1;
		    break;
		case 'e':
		    ExecTrace = 1;
		    break;
		case 'B':
		    blocking = TRUE;
		    break;
		case 'G':
		    globalTRG = 1;
		    break;
		case 'I':
		    GenTrace = 1;
		    break;
		case 'R':
		    nondeterministic = FALSE;
		    break;
		case 'X':
		    autoquit = TRUE;
		case 'T':
		    timelimit = (short)atoi(argv[i] + 2);
		    break;
		case 'C':
		    buildBG = 1;
		    if ((argc - i) < 1)
			goto usage;
		    fullspeed = 1;
		    compareDepth = atoi(argv[++i]);
		    maxNodes = atoi(argv[++i]);
		    if ((argc - i) > 1)
		    {
			groupStart = atoi(argv[++i]) - 1;
			groupEnd = atoi(argv[++i]) - 1;
		    } else
		    {
			groupStart = 0;
			groupEnd = 1000;	/* all */
		    }
		    break;
		default:
		    fprintf(stderr, "Illegal option (%s)!\n", argv[i]);
		}
	    } else
		filearg = i;
	}
    }
    if ((timelimit || GenTrace || ClauseTrace || ExecTrace) && buildBG)
    {
	fprintf(stderr, "Cannot have both timelimit/trace and control options!\n");
	exit(0);
    }
    if (ClauseTrace)
	fullspeed = 0;
    if (filearg)
	strcpy(logfile, argv[filearg]);
    else
    {
	strcpy(logfile, specname);
	strcat(logfile, ".log");
    }
    if ((sourcefl = fopen(infile, "rt")) == NULL)
    {
	fprintf(stderr, "%s: cannot open source file (%s)\n", argv[0], infile);
	exit(-3);
    }
    if ((logfp = fopen(logfile, "wt")) == NULL)
    {
	fprintf(stderr, "%s: cannot open log file (%s)\n", argv[0], logfile);
	exit(-3);
    }
    strcpy(infile, specname);
    strcat(infile, ".cod");
#ifdef UNIX
    if ((codefile = fopen(infile, "r")) == NULL)
    {
#else
    if ((codefile = fopen(infile, "rb")) == NULL)
    {
#endif
	fprintf(stderr, "%s: cannot open %s for input\n", argv[0], infile);
	exit(-1);
    }
    if ((code = (ecode_op *) Mem_Calloc((short)filelength(fileno(codefile)), sizeof(char), 0)) == NULL)
    {
	fprintf(stderr, "%s: cannot allocate code space\n", argv[0]);
	exit(-2);
    }
    codenow = (char *)code;
    while (!feof(codefile))
    {
	nbytes = fread((void *)codenow, 1, 512, codefile);
	if (nbytes > 0)
	    codenow += nbytes;
	else
	    break;
    }
    fclose(codefile);
    while (!feof(sourcefl) && Ed_LastLine < MAX_SRC_LNS)
    {
	sourcepos[Ed_LastLine++] = (unsigned short)ftell(sourcefl);
	fgets(buff, 128, sourcefl);
    }
    Ed_FileSize = sourcepos[Ed_LastLine--] + strlen(buff);
    total_lines[(short)SOURCE_WIN] = (ushort) (Ed_LastLine);
    Scr_InitSystem();
    if (!feof(sourcefl))
	interp_error(ERR_BIGSOURCE);
#ifndef UNIX
    Msg_Win = Win_Make(WIN_NOBORDER, 0, 24, 80, 1, 0, ED_INSIDE_COLOR);
    Pos_Win = Win_Make(WIN_NOBORDER | WIN_CURSOROFF, 3, 1, 74, 1, 0, ED_BORDER_COLOR);
    Win_Activate(Pos_Win);
    Win_Clear();
    Win_FastPutS(0, 0, " Line:     /      Col: 1    Size:        Mem:                              ");
    Win_FastPutS(0, 61, argv[1]);
    Scr_PutCursor(0, 0);
    Scr_PutS(" EI: Estelle Interpreter by Graham Wheeler  v2.0                      (c) 1991  ");
#endif
}
#endif

static void NEAR 
init_brkpts(void)
{
    short i;
#ifndef UNIX
    extern ushort brkpt_other;
    brkpt_other = 1;
#endif
    for (i = 0; i < MAXBREAKPOINTS; i++)
	breakpts[i].class = BRK_FREE;
    nextfreebrkpt = 0;
}

static BOOLEAN NEAR 
interp_init_screen(void)
{
    BOOLEAN rtn = TRUE;
    short i;
    randomize();
    Win_1LineBorderSet();
#ifndef INTEGRATE
    Ed_Win = Win_Make(WIN_WRAP | WIN_CLIP | WIN_BORDER, 1, 2, 78, 5, ED_BORDER_COLOR, ED_INSIDE_COLOR);
#else
    Ed_ToggleSize();
    total_lines[(short)SOURCE_WIN] = (ushort) (Ed_LastLine + 1);
#endif
    pew_wins[(short)SOURCE_WIN] = Ed_Win;
    if ((IO_win = WIN_MAKE(WIN_FLASH | WIN_BORDER | WIN_SCROLL | WIN_WRAP,
			   20, (22 - IO_LNS) / 2, 40, IO_LNS, SCR_NORML_ATTR, SCR_NORML_ATTR)) != NULL)
    {
	Win_Activate(IO_win);
	Win_PutTitle("I/O", SCR_TOP, SCR_CENTRE);
    } else
	rtn = FALSE;
    if (!open_browser_windows(INIT_CHILD_LNS, INIT_TRANS_LNS, INIT_IP_LNS))
	rtn = FALSE;
    for (i = (short)SOURCE_WIN; i <= (short)IP_WIN; i++)
	actual_line[i] = first_line[i] = 0;
    scr_lines[(short)SOURCE_WIN] = SOURCE_LNS;
    active_win = SOURCE_WIN;
    Win_Activate(pew_wins[(short)SOURCE_WIN]);
    Scr_PutCursor(22, 0);
    Scr_PutString("  F1-Help         F2-Step         F3-Transition    F4-Iteration    F5-Up Level  ", SCR_INVRS_ATTR);
    Scr_PutCursor(23, 0);
    Scr_PutString("  F6-Next Window  F7-View I/O     F8-Turn Log On   F9-Execute      F10-Quit     ", SCR_INVRS_ATTR);
    refreshallwindows = FALSE;
    return rtn;
}

static MODULE *
interp_initialise(void)
{
#ifdef INTEGRATE
    extern short next_free_fire_entry;
#endif
    pathabort = FALSE;
    db_mode = STEP_MODE;
    if (timelimit)
	db_mode |= FAST;
    if (buildBG)
    {
	db_mode = CONVERGE;
	nondeterministic = FALSE;
	ignore_interrupts = TRUE;
	output_to = 1;
    } else
    {
#ifdef INTEGRATE
	nondeterministic = TRUE;
#endif
	ignore_interrupts = FALSE;
#ifdef UNIX
	output_to = 1;
#else
	output_to = 3;
#endif
    }
    last_globaltime = 0l;
    iteration = globaltime = 1l;
    p = columnpos = 0;
#ifdef INTEGRATE
    next_free_fire_entry = 0;
    old_Ed_WrapLines = Ed_WrapLines;
    Ed_WrapLines = FALSE;
#endif
    ecode_debugging = FALSE;
    trace = GenTrace ? fopen("TRACE", "wt") : NULL;
    ctrace = ClauseTrace ? fopen("CTRACE", "wt") : NULL;
    etrace = ExecTrace ? fopen("ETRACE", "wt") : NULL;
    return modnow = root = (MODULE *) Mem_Calloc(1, MODULE_SIZE, 30);
}

/****************************** SHUTDOWN ***************************/


void 
dump_GMT(void)
{
    FILE *fp = fopen("MAT", "wb");
    short i, j, c, *bounds, *b, *b2;
    if (GMT == NULL)
	return;
    fprintf(logfp, "\n\nGlobal Transition Sequence Count Table\n\n     ");
    fprintf(logfp, "\n\nClassification Key\n==================\n\n");
    c = root->nummodvars;
    b = bounds = (short *)Mem_Calloc(c + 1, sizeof(short), 888);
    fwrite(&c, 1, sizeof(short), fp);
    while (c--)
    {
	char name[40];
	short num, start;
	MODULE *m = CHILD(root, c);
	TRANS *tptr;
	Get_ModvarName(m, name);
	fprintf(logfp, "%s has %d transitions starting at %d\n", name,
		num = m->numtrans, start = m->trans_tbl->metaidx + 1);
	*b++ = start;
	fwrite(name, 40, sizeof(char), fp);
	fwrite(&num, 1, sizeof(short), fp);
	fwrite(&start, 1, sizeof(short), fp);
	/* Write the delays */
	tptr = m->trans_tbl;
	while (num--)
	{
	    fwrite(&tptr->forward_delay, 1, sizeof(float), fp);
	    tptr++;
	}
    }

    /* Print column indices */

    fprintf(logfp, "\n     ");
    j = *b = 0;
    b = bounds;
    for (i = 1; i <= GMTrows; i++)
    {
	if (i == *b)
	{
	    b++;
	    if (i > 1)
		fprintf(logfp, "|");
	}
	fprintf(logfp, "%4d ", i);
    }

    /* Underline column indices */

    fprintf(logfp, "\n     ");
    i = GMTrows * 5 + root->nummodvars - 1;
    while (i--)
	fprintf(logfp, "=");

    /* Print rows */

    b = bounds;
    for (i = 0; i < GMTrows; i++)
    {
	if ((i + 1) == *b)
	{
	    b++;
	    if (i > 1)
		fprintf(logfp, "\n");
	}
	fprintf(logfp, "\n%4d:", i + 1);
	b2 = bounds;
	for (j = 0; j < GMTrows; j++)
	{
	    if ((j + 1) == *b2)
	    {
		b2++;
		if (j > 1)
		    fprintf(logfp, "|");
	    }
	    fprintf(logfp, "%4d:", GMT[i][j]);
	}
    }
    fprintf(logfp, "\n\n\n");

    fwrite(&GMTrows, 1, sizeof(short), fp);
    fwrite(*GMT, 1, sizeof(short) * GMTrows * GMTrows, fp);
    fclose(fp);

    /* PRINT THE DELAY MATRIX */

    /* Print column indices */

    fprintf(logfp, "\n\n     ");
    j = *b = 0;
    b = bounds;
    for (i = 1; i <= GMTrows; i++)
    {
	if (i == *b)
	{
	    b++;
	    if (i > 1)
		fprintf(logfp, "|");
	}
	fprintf(logfp, "%7d ", i);
    }

    /* Underline column indices */

    fprintf(logfp, "\n     ");
    i = GMTrows * 8 + root->nummodvars - 1;
    while (i--)
	fprintf(logfp, "=");

    /* Print rows */

    b = bounds;
    for (i = 0; i < GMTrows; i++)
    {
	if ((i + 1) == *b)
	{
	    b++;
	    if (i > 1)
		fprintf(logfp, "\n");
	}
	fprintf(logfp, "\n%7d:", i + 1);
	b2 = bounds;
	for (j = 0; j < GMTrows; j++)
	{
	    if ((j + 1) == *b2)
	    {
		b2++;
		if (j > 1)
		    fprintf(logfp, "|");
	    }
	    if (GMT[i][j])
		fprintf(logfp, "%7.3f:", GMTdel[i][j] / ((float)GMT[i][j]));
	    else
		fprintf(logfp, "      *:");
	}
    }
    fprintf(logfp, "\n\n\n");
    Mem_Free(bounds);
}

static void NEAR 
FreeGMT(void)
{
    Mem_Free(*GMT);
    Mem_Free(GMT);
    Mem_Free(*GMTdel);
    Mem_Free(GMTdel);
    GMT = NULL;
    GMTdel = NULL;
}

#ifndef UNIX
void 
Close_Windows(void)
{
    enum exec_win i;
    for (i = CHILD_WIN; i <= IP_WIN; i++)
	if (pew_wins[(short)i])
	    Win_Free(pew_wins[(short)i]);
    if (IO_win)
	Win_Free(IO_win);
}
#endif

void 
interp_shutdown(void)
{
    void freeTree();
    extern void Ed_ToggleSize();
    extern void gather_fire_entries();
    root->checked |= DUMP_BIT;

    if (buildBG == 0)
#ifdef INTEGRATE
	if (!analyzing)
	{
#endif
	    activate_module(root);
	    Gen_ShowMessage("Dumping statistics to log file");
	    dump_stats(root, 0);
	    fprintf(logfp, "\n\n\nEXIT AT GLOBAL TIME %ld\n\n", globaltime);

	    /* Dump Global meta trans table */
	    dump_GMT();
#if 0
	    Gen_ShowMessage("Dumping SNAP model to PEW.SNP");
	    dump_model(root);
#endif
	    Gen_ShowMessage("Releasing all processes");
	    ReleaseProcesses();
	    Gen_ClearMessage();
#ifdef INTEGRATE
	} else
	    ReleaseProcesses();
#endif
    if (GMT)
	FreeGMT();

#ifndef INTEGRATE
    Mem_Free(code);
    fclose(sourcefl);
    fclose(logfp);
    Scr_EndSystem();
#else
    Close_Windows();
    Ed_WrapLines = old_Ed_WrapLines;
    Win_Activate(Ed_Win);
    Ed_ToggleSize();
    if (analyzing)
	gather_fire_entries();
    else
    {
	fclose(logfp);
	logfp = NULL;
    }
#endif
    if (trace)
	fclose(trace);
    trace = NULL;
    if (ctrace)
	fclose(ctrace);
    if (etrace)
	fclose(etrace);
    ctrace = etrace = NULL;
#ifndef INTEGRATE
    if (treefp)
	fclose(treefp);
    freeTree();
    Mem_Free(treeTable);
#endif
}


void 
interp_abort(void)
{				/* called on a SIGINT */
    printf("Aborting!\n");
    activate_module(root);
    dump_stats(root, 0);
    fclose(logfp);
    exit(0);
}

/****************** MAIN ***************************/

static void NEAR 
exec_spec(void)
{
    critical_section = 0;
    while (TRUE)
    {
	if (code[p] == OP_MODULE)
	{
	    _Module(root, 0, p, 0, 0, NULL, 0);
	    schedule();
	    break;
	} else if (code[p] == OP_NEWLINE)
	{
	    run_sourceline = (ushort) ((short)code[p + 1] - 1);
	    p += 2;
	} else
	    p += 1 + (short)Op_NArgs[(short)code[p]];
    }
}

void 
execute_spec(ulong until)
{
#ifdef INTEGRATE
    extern ushort *emitbuff;
    code = (ecode_op *) emitbuff;
    if (logfp == NULL || !analyzing)
	if ((logfp = fopen("ESTELLE.LOG", "wt")) == NULL)
	    interp_error(ERR_NOLOGOPEN);
    strcpy(specname, "NONAME");
#else
    char buf[60];
    short runs, maxDepth;
    runs = maxDepth = 0;
    minNodes = freeNodes = MAXNODES;
    pathroot = 1;
#endif
    timelimit = until;
    init_brkpts();
    if (interp_init_screen())
    {
	if (!buildBG)
	{
	    if (interp_initialise() != NULL)
	    {
		short retval = setjmp(quit_info);
		if (retval == 0)
		    exec_spec();
	    }
	} else
	{
#ifndef INTEGRATE
	    while (1)
	    {			/* Still got stuff to explore? */
		targetNode = getTarget();
		if (targetNode == 0)
		    break;	/* No more paths */
		pathnow = pathroot;
		pathpos = depthNow = 0;
		sprintf(buf, "Run %-5d Depth %-3d  FreeNodes %3d  Sequences %d",
			(int)++runs, (int)treeTable[targetNode].depth, freeNodes, seqTop);
		if (treeTable[targetNode].depth > maxDepth)
		    maxDepth = treeTable[targetNode].depth;
		Gen_ShowMessage(buf);
#ifdef TREE_DEBUG
		fprintf(treefp, "\n\n\nRun %d   Target  %d ==============\n\n", runs, targetNode);
#endif
		if (interp_initialise() != NULL)
		{
		    short retval;
		    initTreeRun();
		    retval = setjmp(quit_info);
		    if (retval == 0)
			exec_spec();
		}
#ifdef TREE_DEBUG
		dumpTree(treefp, 1);
#endif
		if (GMT)
		    FreeGMT();
		ReleaseProcesses();
	    }
	    fprintf(treefp, "\n\nSequences:\n\n");
	    dumpSeq();
	    Gen_ShowMessage("Calling check tree");
	    fprintf(treefp, "Runs %d  Nodes used: %d  Max depth: %d\n", runs, MAXNODES - minNodes, (int)maxDepth);
	    dumpTree(treefp, 1);
	    fprintf(treefp, "\n\nCalling checkTree\n\n");
	    (void)checkTree();
	    dumpTree(treefp, 1);
	    dumpTriggers();
	    dumpKillers();
	    dumpParams();
/*			fprintf(treefp,"\n\nCalling trimGraph\n\n");
			trimGraph();
			dumpTree(treefp,1);
*/
#endif
	}
    }
    interp_shutdown();
}

#ifndef INTEGRATE
int 
main(int argc, char **argv)
{
    Mem_Init();
    treefp = fopen("tree", "wt");
    freeTree();
    solorun_init(argc, argv);
    execute_spec(timelimit);
    Mem_Check(TRUE);
    return 0;
}
#endif

/*************** Support routines for standalone interpreter *******/

#ifndef INTEGRATE

/* Replacements for ed.c routines */

short Ed_LineNow;
short Ed_ScrLine = -1;		/* The screen line at which the cursor is at;
				 * used by interpreter */

/**************** SOURCE CODE INTERFACE ******************/
#ifndef UNIX

void 
Source_Position(int line)
{
    fseek(sourcefl, (long)sourcepos[line], SEEK_SET);
}

void 
Source_Line(char *buff)
{
    short i;
    fgets(buff, 128, sourcefl);
    i = strlen(buff) - 1;
    do
    {
	buff[i--] = 0;
    } while (buff[i] == ' ');
    buff[78] = 0;
}

void 
Ed_FixCursor(short line, short col /* col==0 in this case */ )
{
    char buf[128];
    ushort lines = Ed_Win->h;
    ushort old_scrline = Ed_ScrLine;
    (void)col;
    Ed_LineNow = line;
    if (Ed_LineNow < Ed_ScrLine)
	Ed_ScrLine = Ed_LineNow;
    else if (Ed_LineNow >= (Ed_ScrLine + lines))
	Ed_ScrLine = Ed_LineNow - lines + 1;
    if (Ed_ScrLine != old_scrline)
    {
	/* Redraw edit window from Ed_ScrLine */
	Win_Clear();
	while (lines--)
	{
	    Source_Position(Ed_ScrLine + lines);
	    Source_Line(buf);
	    Win_PutCursor(lines, 0);
	    Win_PutString(buf);
	    Win_PutAttribute(Ed_Win->ia, 0, Ed_LineNow - Ed_ScrLine, 78, 1);
	}
    }
}

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

void 
Ed_ShowPos(BOOLEAN showall /* == FALSE in this case */ )
{
    WINPTR Last_Win = Win_Current;
    static ulong oldlnow, oldlast, oldsz;
    char tmp[10];
    Win_Activate(Pos_Win);
    Ed_DisplayPosValue(7, &oldlnow, (ulong) Ed_LineNow + 1, "    ", showall);
    Ed_DisplayPosValue(12, &oldlast, (ulong) Ed_LastLine + 1, "    ", showall);
    Ed_DisplayPosValue(34, &oldsz, (ulong) Ed_FileSize, "     ", showall);
    Ed_DisplayPosValue(46, NULL, farcoreleft(), "      ", showall);

    if (Last_Win)
	Win_Activate(Last_Win);
}

BOOLEAN 
MN_LineValidate(int line)
{
    long ln = (long)(line);
    if (ln < 1 || ln > Ed_LastLine)
    {
	Error(TRUE, ERR_BADLINE);
	return FALSE;
    }
    return TRUE;
}

void 
    compiler_error(short code, enum errornumber kind)
    {
	/* shouldn't happen */
	(void)code;
	Error(TRUE, kind);
    }
#endif

/* from analyze.c */

    void analyze_update(transition_table tptr)
{
    (void)tptr;			/* Nothing to update */
}

#endif
