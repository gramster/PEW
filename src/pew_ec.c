/********************************************************

	This file contains the conditionally
	compiled code for the Estelle compiler.
	The INTEGRATE macro must be defined for
	the integrated system; ie, a standalone
	compiler is the default.

	These routines are only separated out for
	compilation speed and size if both versions
	are to be made. They would normally all be
	local to compiler.c.

********************************************************/

#include "misc.h"

#if !defined(UNIX) && defined(INTEGRATE)
#include <conio.h>
#include "screen.h"
#include "menu.h"
#include "ed.h"
#include "keybd.h"
#endif

#include <stdarg.h>

#include "symbol.h"
#include "compiler.h"
#include "help.h"
#include "gen.h"

#include <time.h>
#include <setjmp.h>

#define MAX_CODE_SIZE	30000

short ec_errcol;

static jmp_buf abort_info;

#ifndef INTEGRATE
time_t start_time;
BOOLEAN dump_stab = FALSE, dump_ids = FALSE;

#endif

/********************/
/* Error Processing */
/********************/

static void NEAR 
    process_err_and_warn(char *message, enum errornumber kind, BOOLEAN warn)
    {
#ifdef INTEGRATE
	extern BOOLEAN analyzing;
#ifdef UNIX
#ifdef XWINDOWS
	extern void X_CompileError();
	    X_CompileError(message, (int)warn, abort_info);
#else
	    fprintf(stderr, "%s: %s\n", warn ? "Warning" : "Error", message);
#endif				/* XWINDOWS */
#else
	ushort rtn = Win_Error(NULL, "", message, warn, (BOOLEAN) (warn ? !analyzing : TRUE));
	    Ed_ShowMessage(message);
	if  (rtn == KB_ESC)
	        longjmp(abort_info, -1);
	else if (rtn == KB_F1 || rtn == KB_ALT_H)
	{
	    Gen_BuildName("PEWPATH", "ERROR.HLP", Hlp_FileName);
	    Hlp_IHelp(kind);
	    Gen_BuildName("PEWPATH", "PEW_HELP", Hlp_FileName);
	}
#endif				/* UNIX */
#else
	    fprintf(stderr, message);
#endif				/* INTEGRATE */
	if  (!warn)
	{
#ifndef INTEGRATE
	    show_stats(start_time, out_address, TRUE);
#endif
	    longjmp(abort_info, -1);
	}
	    (void)kind;		/* avoid compiler warning */
    }

static void NEAR 
    lookup_message(enum errornumber kind, char *message)
    {
	char FilName[64];
	    Gen_BuildName("PEWPATH", "ERROR.TBL", FilName);
	    (void)Gen_IGetFileEntry(FilName, (int)kind, message, 80, FALSE);
    }

    void compiler_error(short code, enum errornumber kind)
    {
	extern short ec_errline;
	char msg[80], err[80];
	    lookup_message(kind, msg);
	    sprintf(err, msg, ec_errline, code);
	    process_err_and_warn(err, kind, FALSE);
    }

    void undef_ident_err(short code, char *name)
{
    extern short ec_errline;
    char msg[80], err[80];
    lookup_message(ERR_UNDEFINED, msg);
    sprintf(err, msg, ec_errline, code, name);
    process_err_and_warn(err, ERR_UNDEFINED, FALSE);
}


void 
    warn(enum errornumber kind, short ident)
    {
	char msg[80], err[80], identname[20];
	extern short ID_MapTable[];
	extern char ID_NameStore[];
	    lookup_message(kind, msg);
	    strncpy(identname, ID_NameStore + ID_MapTable[ident], 19);
	    identname[19] = 0;
	    sprintf(err, msg, identname);
	    process_err_and_warn(err, kind, TRUE);
    }

/************************************/
/* Code Generation part of compiler */
/************************************/

#define NUMDELTAS	111

    static short Op_StackDeltas[NUMDELTAS] = {
	/* Number of items PUSHed onto stack	 */
	/* -ve for POPs. Some are not constant;	 */
	/* these are indicated with -0 and must	 */
	/* be explicitly handled in the parser.	 */
	 /* 00  Add              */ -1,
	 /* 01  Address          */ 0,	/* not used */
	 /* 02  AddSetElement    */ -1,
	 /* 03  AddSetRange      */ -2,
	 /* 04  And              */ -1,
	 /* 05  Assign           */ -0,
	 /* 06  Attach           */ -5,
	 /* 07  Case             */ 0,
	 /* 08  Connect          */ -0,
	 /* 09  Constant         */ 1,
	 /* 10  Copy             */ 1,
	 /* 11  Detach           */ -2,
	 /* 12  Difference       */ -SETSIZE,
	 /* 13  Disconnect       */ -0,
	 /* 14  Div              */ -1,
	 /* 15  Do               */ -1,
	 /* 16  EndProc          */ -0,
	 /* 17  EndTrans         */ 0,
	 /* 18  Equal            */ -1,
	 /* 19  Field            */ 0,
	 /* 20  For              */ -1,
	 /* 21  Goto             */ 0,
	 /* 22  Greater          */ -1,
	 /* 23  In               */ -SETSIZE,
	 /* 24  Intersection     */ -SETSIZE,
	 /* 25  Index            */ -1,
	 /* 26  IndexedJump      */ -1,
	 /* 27  Init             */ -2,
	 /* 28  Less             */ -1,
	 /* 29  Minus            */ 0,
	 /* 30  ModuleAssign     */ -3,
	 /* 31  Modulo           */ -1,
	 /* 32  Multiply         */ -1,
	 /* 33  NewLine          */ 0,
	 /* 34  Next             */ -2,
	 /* 35  NextInstance     */ 0,	/* not used	 */
	 /* 36  Not              */ 0,
	 /* 37  NotEqual         */ -1,
	 /* 38  NotGreater       */ -1,
	 /* 39  NotLess          */ -1,
	 /* 40  Or               */ -1,
	 /* 41  Output           */ -0,
	 /* 42  Pop              */ -0,
	 /* 43  ProcCall         */ -0,
	 /* 44  Procedure        */ -0,
	 /* 45  Range            */ 0,
	 /* 46  Release          */ -1,
	 /* 47  SetAssign        */ -SETSIZE - 1,
	 /* 48  SetInclusion     */ -SETSIZE - SETSIZE + 1,
	 /* 49  SetInclusionToo  */ -SETSIZE - SETSIZE + 1,
	 /* 50  Module  		*/ -0,
	 /* 51  StringCompare    */ -0,
	 /* 52  Trans            */ -0,
	 /* 53  Union            */ -SETSIZE,
	 /* 54  Value            */ -0,
	 /* 55  Variable         */ 1,
	 /* 56  VarParam         */ 1,
	 /* 57  Abs              */ 0,
	 /* 58  Dispose          */ -1,
	 /* 59  New              */ 1,
	 /* 60  Odd              */ 0,
	 /* 61  ReadInt          */ -0,
	 /* 62  Sqr              */ 0,
	 /* 63  Subtract         */ -1,
	 /* 64  WriteInt         */ -1,
	 /* 65  DefAddr          */ 0,
	 /* 66  DefArg           */ 0,
	 /* 67  GlobalCall       */ 0,	/* Generated by linker; ignore		 */
	 /* 68  GlobalValue      */ 0,	/* "     "   "	  "		 */
	 /* 69  GlobalVar        */ 0,	/* "     "   "	  "		 */
	 /* 70  LocalValue       */ 0,	/* "     "   "	  "		 */
	 /* 71  LocalVar         */ 0,	/* "     "   "	  "		 */
	 /* 72  SimpleAssign     */ 0,	/* "     "   "	  "		 */
	 /* 73  SimpleValue      */ 0,	/* "     "   "	  "		 */
	 /* 74  StringEqual      */ 0,	/* "     "   "	  "		 */
	 /* 75  StringLess       */ 0,	/* "     "   "	  "		 */
	 /* 76  StringGreater    */ 0,	/* "     "   "	  "		 */
	 /* 77  StringNotEqual   */ 0,	/* "     "   "	  "		 */
	 /* 78  StringNotLess    */ 0,	/* "     "   "	  "		 */
	 /* 79  StringNotGreater,*/ 0,	/* "     "   "	  "		 */
	 /* 80  EnterBlock	*/ 0,
	 /* 81  ExitBlock	*/ 0,	/* Not used */
	 /* 82  EndClause	*/ -1,
	 /* 83  Delay		*/ -2,
	 /* 84  When		*/ 0,
	 /* 85  ReturnVar	*/ 1,
	 /* 86  Random		*/ 0,
	 /* 87  ReadCh           */ -0,
	 /* 88  WriteCh          */ -1,
	 /* 89  WriteStr		*/ -0,
	 /* 90  SetEqual		*/ -SETSIZE - SETSIZE + 1,
	 /* 91  ReadStr		*/ -0,
	 /* 92  DefIps		*/ 0,
	 /* 93  Scope		*/ 0,
	 /* 94  ExpVar		*/ 1,
	 /* 95  With		*/ 3,
	 /* 96  WithField	*/ 1,
	 /* 97  EndWith		*/ 0,
	 /* 98  EndWrite		*/ 0,
	 /* 99  Domain		*/ -0,
	 /* 100 ExistMod		*/ 0,
	 /* 101 EndDomain	*/ -0,
	 /* 102 NumChildren	*/ 1,
	 /* 103 Otherwise	*/ 1,
	 /* 104 FireCount	*/ 1,
	 /* 105 GlobalTime	*/ 1,
	 /* 106 QLength		*/ -1,
	 /* 107 SetOutput	*/ 0,
	 /* 108 Terminate	*/ -1,
	 /* 109 Erandom		*/ -2,
	 /* 110 Prandom		*/ -2
    };


    void emit(ecode_op op,...)
{
    va_list args;
    short numwrds, tmp;
    va_start(args, op);
    //op = va_arg(args, ecode_op);
    numwrds = 1 + (int)Op_NArgs[(int)op];
    if ((out_address + numwrds) * 2 >= MAX_CODE_SIZE)
	compiler_error(0, ERR_CODEFULL);
    if (emitcode)
    {
	emitbuff[out_address++] = (ushort) op;
	while (--numwrds)
	{
	    ushort tmp = va_arg(args, ushort);
	    emitbuff[out_address++] = tmp;
	}
	tmp = Op_StackDeltas[(int)op];
	if (tmp > 0)
	    push(tmp);
	else
	    pop(-tmp);
    } else
	out_address += numwrds;
    va_end(args);
}

void 
emitarray(ushort * array)
{
    short a = 0;
    ecode_op op = (ecode_op) array[0];
    short numargs = (int)Op_NArgs[(int)op];
    if (emitcode)
    {
	while (a <= numargs)
	    emitbuff[out_address++] = array[a++];
    } else
	out_address += 1 + numargs;
}

/*************************
	Lexical Analysis
************************/

void 
nextchar(void)
{
#ifndef INTEGRATE
    if (feof(sourcefile))
	ch = ETX;
    else
	do
	{
	    ch = fgetc(sourcefile);
	} while (!feof(sourcefile) && (ch < 32 || ch > 126) && ch != '\n' && ch != '\t');
#else
    /* Read char from editor buffer */
    extern char Ed_NextChar();
    ch = Ed_NextChar(&lineno);
    if (ch == 0)
	ch = ETX;
#endif
}

void 
endline(void)
{
    nextchar();
#ifndef INTEGRATE

#ifdef UNIX
    fprintf(stderr, "%d\r", ++lineno);
#else
    cprintf("%d\r", ++lineno);
#endif

#else				/* Integrated */

#ifndef UNIX
    if ((lineno % 10) == 0)
    {
	Win_PutCursor(3, 19);
	Win_Printf("%d", lineno);
    }
#else
    lineno++;
#endif

#endif
}


void 
nextsymbol(void)
{
    short tmpline;
#ifdef INTEGRATE

#ifndef UNIX
    extern short Ed_LineNow;
    extern short Ed_ColNow;
    tmpline = Ed_LineNow + 1;
    ec_errcol = Ed_ColNow;
#else
    tmpline = lineno;
    ec_errcol = X_QueryPosition();	/* Actually the offset into the file  */
#endif

#else				/* Not integrated */
    tmpline = lineno;
#endif
    if (ec_errline != tmpline)
	emit(OP_NEWLINE, tmpline);
    ec_errline = tmpline;
    _nextsymbol();
}

/***************************
	Compiler interface
****************************/

#if defined(INTEGRATE) || defined(EC_EI)
void 
pew_compile(char *name, short value)
#else
void 
pew_compile(int argc, char *argv[])
#endif
{
#ifndef INTEGRATE
    char source[16];
#ifndef EC_EI
    short filearg = 1, got_file = 0;
#endif
    extern void dump_idents();
    (void)time(&start_time);
#ifndef EC_EI
    if (argc < 2 || argc > 4)
	compiler_error(0, ERR_ECUSEAGE);
    while (filearg < argc)
    {
	if (argv[filearg][0] != '-')
	{
	    if (got_file)
		compiler_error(0, ERR_ECUSEAGE);
	    got_file = 1;
	    strcpy(source, argv[filearg]);
	} else if (argv[filearg][1] == 'I')
	    dump_ids = TRUE;
	else if (argv[filearg][1] == 'S')
	    dump_stab = TRUE;
	else
	    compiler_error(0, ERR_ECUSEAGE);
	filearg++;
    }
    if (!got_file)
	compiler_error(0, ERR_ECUSEAGE);
#else
    strcpy(source, name);
#endif
    strcat(source, ".est");
    if ((sourcefile = fopen(source, "rt")) == NULL)
	compiler_error(0, ERR_NOSOURCE);
#endif
    out_address = 0;
#ifdef INTEGRATE
    compile(name, value);
#else
    compile(NULL, 0);
    fclose(sourcefile);
    show_stats(start_time, out_address, FALSE);
    if (dump_ids)
	dump_idents();
#endif
}


#if defined(INTEGRATE) || defined(EC_EI)
BOOLEAN 
run_compiler(char *name, short value)
#else
int main(int argc, char *argv[])
#endif
{
    static BOOLEAN firsttime = TRUE;
    BOOLEAN rtn;
    char *targetname;
    rtn = TRUE;
    targetname = 0;
#if defined(INTEGRATE) && !defined(UNIX)
    Ed_ShowMessage("");
#endif
    if (setjmp(abort_info) == 0)
    {
	if (firsttime)
	{
#ifdef UNIX
	    if ((locspace = malloc(LOC_SPACE_SIZE)) == NULL)
		compiler_error(0, ERR_ECMEMFAIL);
#else
	    locspace = MK_FP(0xb800, 4096);
#endif
	    if ((emitbuff = (ushort *) malloc(MAX_CODE_SIZE)) == NULL)
		compiler_error(0, ERR_ECCODFAIL);
	    firsttime = FALSE;
	}
	locspacenow = locspace;
	emitcode = TRUE;
#ifdef INTEGRATE
	pew_compile(name, value);
	targetname = "DEBUG";
#else
#ifdef EC_EI
	pew_compile(name, value);
	targetname = name;
#else
	pew_compile(argc, argv);
	targetname = argv[1];
#endif
#endif
    } else
	rtn = FALSE;
#if defined(INTEGRATE) && !defined(UNIX)
    Win_PutCursor(3, 19);
    Win_Printf("%d", lineno);
#endif
    if (rtn)
    {
	short codebytes;
#if defined(INTEGRATE) && !defined(UNIX)
	Win_FastPutS(1, 2, "Linking... ");
#else
	printf("Linking:  ");
	fflush(stdout);
#endif
	emitcode = FALSE;
#if defined(WRITECODEFILE) || !defined(INTEGRATE)
	codebytes = sizeof(short) * Link(targetname, out_address);
#else
	codebytes = sizeof(short) * Link(out_address);
	(void)targetname;
#endif
#ifdef INTEGRATE
#ifndef UNIX
	Win_FastPutS(3, 2, "                     ");
	Win_PutCursor(3, 2);
	Win_Printf("Code Bytes:   %4d", codebytes);
#endif
#else
	printf("Code Bytes:  %4d\n", codebytes);
#ifndef EC_EI
	Sym_Save(argv[1]);
	free(locspace);
#endif
#endif
    }
#if defined(INTEGRATE) || defined(EC_EI)
    return rtn;
#else
    return (int)rtn;
#endif
}
