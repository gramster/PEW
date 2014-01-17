/* compiler.h */

#ifndef INCLUDED_COMPILER
#define INCLUDED_COMPILER

#include <time.h>
#include "misc.h"
#include "ecode.h"
#include "errors.h"

#define MAXTRANS	64	/* Maximum number of state transitions per
				 * process */

/* Standard (and nonstandard) identifiers */

#define _INTEGER 	1
#define _BOOLEAN 	2
#define _CHAR		3
#define _FALSE		4
#define _TRUE		5
#define _NIL		6
#define _HOURS		7
#define _MINUTES	8
#define _SECONDS	9
#define _MILLISECONDS	10
#define _MICROSECONDS	11
#define _UNPACK		12
#define _PACK		13
#define _NEW		14
#define _DISPOSE	15
#define _ABS		16
#define _SQR		17
#define _ORD		18
#define _CHR		19
#define _SUCC		20
#define _PRED		21
#define _ODD		22
#define _READ		23
#define _WRITE		24
#define _WRITELN	25
#define _RANDOM	        26
#define _ERANDOM	27
#define _PRANDOM	28
#define _GRANDOM	29
#define _QLENGTH	30
#define _FIRECOUNT	31
#define _GLOBALTIME	32
#define _SIZEOF		33


#define ETX		'\032'
#define LASTSTANDARDIDENTIFIER	_SIZEOF

/****************************/
/* Channel Queueing Classes */
/****************************/

#define FIFO_Q		0
#define RANDOM_Q	1
#define PRIORITY_Q	2

/****************************/
/* Type Compatibility Codes */
/****************************/

enum compatibility_class
{
    INCOMPATIBLE = -1, COMPATIBLE, SETCOMPATIBLE, STRINGCOMPATIBLE,
    SUBRANGECOMPATIBLE, RANGECOMPATIBLE, ELTOFCOMPATIBLE, ANYCOMPATIBLE
};

#define GENCOMPATIBLE(x)	((x)>INCOMPATIBLE && (x)<ELTOFCOMPATIBLE)

enum symboltype
{
    SYM_ALL = LASTSTANDARDIDENTIFIER + 1,
    SYM_AND, SYM_ANY, SYM_ACTIVITY, SYM_ARRAY, SYM_ATTACH,
    SYM_ASTERISK, SYM_BECOMES, SYM_BEGIN, SYM_BODY, SYM_BY, SYM_CASE,
    SYM_CHANNEL, SYM_COLON, SYM_COMMA, SYM_COMMON,
    SYM_CONNECT, SYM_CONST, SYM_DEFAULT, SYM_DELAY, SYM_DETACH,
    SYM_DISCONNECT, SYM_DIV, SYM_DO, SYM_DOUBLEDOT, SYM_DOWNTO, SYM_ELSE,
    SYM_END, SYM_ENDTEXT, SYM_EQUAL, SYM_EXIST, SYM_EXPORT, SYM_EXTERNAL,
    SYM_FOR, SYM_FORONE, SYM_FORWARD, SYM_FROM, SYM_FUNCTION, SYM_GOTO,
    SYM_GRAPHIC, SYM_GREATER, SYM_HAT, SYM_IDENTIFIER, SYM_IF, SYM_IN,
    SYM_INDIVIDUAL, SYM_INIT, SYM_INITIALIZE, SYM_INTEGER, SYM_IP,
    SYM_LABEL, SYM_LEFTBRACKET, SYM_LEFTPARENTHESIS, SYM_LESS, SYM_MODULE,
    SYM_MODVAR, SYM_MINUS, SYM_MOD, SYM_NEWLINE, SYM_NOT, SYM_NAME,
    SYM_NOTEQUAL, SYM_NOTGREATER, SYM_NOTLESS, SYM_OF, SYM_OR,
    SYM_PERIOD, SYM_PLUS, SYM_PROCEDURE, SYM_OTHERWISE, SYM_OUTPUT,
    SYM_PACKED, SYM_PRIMITIVE, SYM_PRIORITY, SYM_PROCESS, SYM_PROVIDED,
    SYM_PURE, SYM_QUEUE, SYM_RECORD, SYM_RELEASE, SYM_REPEAT,
    SYM_RIGHTBRACKET, SYM_RIGHTPARENTHESIS, SYM_SAME, SYM_SEMICOLON,
    SYM_SET, SYM_SPECIFICATION, SYM_STATE, SYM_STATESET, SYM_SUCHTHAT,
    SYM_SYSTEMACTIVITY, SYM_SYSTEMPROCESS, SYM_THEN, SYM_TIMESCALE,
    SYM_TO, SYM_TRANS, SYM_TRIPLEDOT, SYM_TYPE, SYM_UNTIL, SYM_VAR,
    SYM_WHEN, SYM_WHILE, SYM_WITH, SYM_UNKNOWN, SYM_TERMINATE,
    SYM_UNDEF
};

#define FIRSTUSERIDENTIFIER	((int)SYM_UNDEF+1)


/*********************/
/* Macro 'functions' */
/*********************/

#define checktypes(t1, t2)	if ((t1) != (t2)) compiler_error(901,ERR_TYPE)
#define header_ident()		gen_ident(MODULETYPE)
#define new_label() 		(++labelno)

/************************/
/* Globals in pew_ec.c	*/
/************************/

void compiler_error(short code, enum errornumber kind);
void undef_ident_err(short code, char *name);
void warn(enum errornumber kind, short ident);
void emit(ecode_op op,...);
void emitarray(ushort * array);
void nextsymbol(void);

/*************************/
/* Globals in compiler.c */
/*************************/

void push(short length);
void pop(short length);
void _nextsymbol(void);
void expect(enum symboltype s);
short expect_ident(void);
void new_block(BOOLEAN estelleblock);
void exitblock(void);
pointer define_nextident(enum class kind);
pointer find(short ident);
void redefine(pointer object);
BOOLEAN isin(pointer typex, enum typeclass class);
BOOLEAN samestringtype(pointer typex1, pointer typex2);
enum compatibility_class compatibletypes(pointer t1, pointer t2);
short typelength(pointer typex);
void compile(char *Anz_Name, short Anz_Value);

/*************************/
/* Globals in pasparse.c */
/*************************/

void ident_list(pointer * last, short *number, enum class kind);
void assign_type(pointer last, short number, pointer type);
void parameter_addressing(short paramlength, pointer lastparam, short rtn_len);
pointer array_specification(pointer * type);
void actual_param_list(pointer lastparam, short *length);
pointer constant(valuepointer * value);
void constant_definition_part(void);
short declaration_part(short modvarcnt, short IPcnt, short extIPcnt, short *numstates);
pointer gen_ident(enum class kind);
void indexed_selector(pointer * type);
void case_constant_list(pointer selectortype, short *number, short label);
pointer ordinal_type(void);
short parameter_list(enum class kind, pointer * lastparam);
void proc_and_func_decl_part(void);
void type_definition_part(void);
pointer variable_access(BOOLEAN is_reference, BOOLEAN is_exported);
short variable_declaration(pointer * lastvar);
short variable_declaration_part(short startlen);

/*************************/
/* Globals in Estparse.c */
/*************************/

void channel_definition(void);
pointer childexternalip(void);
pointer connectip(BOOLEAN * internal);
pointer interaction_reference(void);
pointer IP_reference(void);
void IP_declaration_part(short IPcnt, short firstindex);
void modulebody_definition(void);
void moduleheader_definition(void);
void modvar_declaration_part(short modvarcnt);
pointer module_variable(BOOLEAN include_symbolic_info);
void specification(void);
void stateset_definition_part(void);

/*************************/
/* Globals in Stmt&exp.c */
/*************************/

void compound_statement(void);
void statement(void);
pointer expression(void);

/**********************/
/* Globals in elink.c */
/**********************/

#if !defined(INTEGRATE) || defined(WRITECODEFILE)
short Link(char *targetbasename, short top);
#else
short Link(short top);
#endif

/***********************/
/* Globals in symbol.c */
/***********************/

void dump_idents(void);
void print_stab_level(short level);
void show_stats(time_t start_time, short codesize, BOOLEAN has_error);

/********************************/
/* Global variable declarations */
/********************************/

#ifndef GLOBAL
#define GLOBAL	extern
#endif

GLOBAL BOOLEAN emitcode, fast_write;

GLOBAL short
    tgroup, labelno, argument, lineno, default_reliability, dist_val,	/* Expected value	 */
    distribution_type, chanQtype,	/* Channnel queueing type */
    chanQpri,			/* Priority for priority Q */
    IPQdelay;			/* IP Propogation delay	 */

GLOBAL enum symboltype symbol;

/* setconstanttype	nullset; */

GLOBAL pointer typeuniversal, typeinteger, typechar, typeboolean,
/*			emptyset,*/
    timeunit;

GLOBAL char ch, ec_buffer[256];

GLOBAL ushort *emitbuff;	/* code buffer */
GLOBAL short ec_errline;

GLOBAL ecode_op op;

GLOBAL FILE *sourcefile, *targetfile;

#endif				/* INCLUDED_COMPILER */
