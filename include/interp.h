/************/
/* interp.h */
/************/

#ifndef INCLUDED_INTERP
#define INCLUDED_INTERP

#include <time.h>
#include <setjmp.h>

#include "misc.h"
#include "screen.h"
#include "ecode.h"
#include "errors.h"
#include "symbol.h"

#define Set_Bit(var,bitval)	var |= bitval
#define Clear_Bit(var,bitval)	var &= ~bitval
#define Toggle_Bit(var,bitval)	var ^= bitval
#define Bit_Set(var,bitval)	((var)&(bitval))
#define Bit_Clear(var,bitval)	(Bit_Set(var,bitval) == 0)


/************************/
/* Special States	*/
/************************/

#define PREINITIAL_STATE	(-2)
#define IMPLICIT_STATE		(-1)

/*******************/
/* Execution modes */
/*******************/

#define STEP_MODE       ((unsigned)2)
#define TRANS_MODE	((unsigned)3)
#define UNIT_MODE	((unsigned)4)
#define AUTO_MODE	((unsigned)8)
#define MODE_MASK	((unsigned)(15|FAST))
#define FAST		((unsigned)32)
#define TRACE		((unsigned)64)
#define CONVERGE	((unsigned)128)

#define TRON		Bit_Set(db_mode,TRACE)
#define IS_STEP  	((db_mode & MODE_MASK) == STEP_MODE)
#define IS_AUTO  	((db_mode & MODE_MASK) == AUTO_MODE)
#define IS_TRANS 	((db_mode & MODE_MASK) == TRANS_MODE)
#define IS_UNIT  	((db_mode & MODE_MASK) == UNIT_MODE)
#define IS_FAST		((db_mode & FAST) || (db_mode & CONVERGE))


/*****************/
/* System limits */
/*****************/

#define MAX_SRC_LNS	5000
#define STACKMAX	1024	/* Dynamically expanded if necessary */
#define SET_SIZE         4	/* 4 * 16 bits */

#define NUM_TGROUPS	16

/* The next two are also defined in compiler.h and symbol.h respectively */

#define MAXTRANS	64	/* Maximum number of state transitions per
				 * process */
#define MAXLEVEL	16	/* Max scope levels */

#define HEAP_SIZE	1024
#define MAXBREAKPOINTS	32

/* Macros to access word/bit in set */

#define SET_BITS	16
#define SET_WORD(e)	((e) / SET_BITS)
#define SET_MASK(e)	(1 << ((e) % SET_BITS))

/*****************************************/
/* Module and transition data structures */
/*****************************************/

typedef struct module_entry MODULE;
typedef struct module_table_entry *module_table;
typedef struct IP_entry IP;
typedef IP *IP_table;
typedef struct trans_entry TRANS;
typedef struct interaction_entry INTERACTION;

struct module_table_entry
{
    MODULE *mod;
    short varident;		/* bound identifier */
};

/* Transition status flags */

#define HAS_WHEN	1	/* Transition has a WHEN clause		 */
#define HAS_PROV	2	/* Transition has a PROVIDED clause	 */
#define HAS_DELAY	4	/* Transition has a DELAY clause	 */
#define HAS_FROM	8	/* Transition has a FROM clause		 */
#define WHEN_OK		16	/* Trans's WHEN clause is satisfied	 */
#define PROV_OK		32	/* Trans's PROVIDED clause is satisfied	 */
#define DELAY_OK	64	/* Trans's DELAY clause is satisfied	 */
#define FROM_OK		128	/* Trans's FROM clause is satisfied	 */
#define FIREABLE	256	/* Transition is completely enabled	 */
#define WAITING		512	/* Transition is enabled but delayed	 */
#define SELECTED	1024	/* Transition has been selected		 */
#define EXECUTING	2048	/* Transition is busy executing		 */
#define COMPLETED	4096	/* Transition is done executing		 */
#define	NOT_FIRST	8192	/* Transition has been processed before	 */

#define DISABLE_ALL(t)	t->flags &= (HAS_WHEN|HAS_PROV|HAS_DELAY|HAS_FROM|NOT_FIRST)
#define IS_FIREABLE(t)	Bit_Set(t->flags,FIREABLE)

struct trans_entry
{
    ushort flags;
    ushort oldflags;
    short transname, fromident, toident;	/* symbolic stuff */
    short addr;			/* Offset of TRANS instruction in code */
    short metaidx;		/* Index into global meta table */
    ulong timestamp;
    short priority;
    short startline;
    ushort breakpoint;
    ulong first_tm;
    ulong last_tm;
    short enbl_count;
    short fire_count;
    short prov_cnt;
    short when_cnt;
    short sumdelay;
    short trigger;
    short col;
    short ip;			/* offset of ip of when clause */
    short out_ip;		/* Offset of an ip refered to in OUTPUT stmt;
				 * used for channel blocking  */
    short *meta_trans_tbl;	/* Transition flow counters */
    short *meta_delay_tbl;	/* Transition mean delay counters */
    short *meta_min_tbl;	/* Transition min delay counters */
    short *meta_max_tbl;	/* Transition max delay counters */
    ulong lasttime;		/* Delay from last transition	 */
    ulong selfdelay;		/* Cycle time			 */
    ulong nextdelay;		/* Delay till next transition	 */
    float forward_delay;
};

typedef struct trans_entry *transition_table;

/* Values for 'checked' flag vector */

#define EVAL_BIT	1
#define EXEC_BIT	2
#define DUMP_BIT	4
#define MODEL_BIT	8

/* We keep track of interactions dequeued/lost through IPs, with
   entries for each (IP, interaction type) pair. We assume that no
   process has more than IP_INT_MAX such entries
*/

#define IP_INT_MAX	32

struct IP_Int_Entry
{
    short ip;
    short ident;
    short successful;
    short lost;
};

/* When gathering the meta-transition table entries into cumulative
	sums for service classes, we want to have dynamic two-dimensional
	non-regular arrays. To do this, a linked list mechanism is used
	for the rows. The columns simply hold transition numbers, so they
	can best be represented with a bit vector. Assuming no more than
	64 transitions in a process, 8 bytes will suffice */

typedef struct EventEntryStruct
{
    BOOLEAN reception;		/* 1=>WHEN; 0=>OUTPUT	 	 */
    short msgident;		/* Message ID; 0 if none    	 */
    struct EventEntryStruct *next;	/* Next on list			 */
    uchar transvect[MAXTRANS / 8];	/* Transition vector		 */
}   EventEntry;

struct module_entry
{
    char bodyname[40];
    MODULE *parent;
    short index;		/* Offset into parent's table	 */
    short columnpos;		/* Where to print output interactions in
				 * trace */
    short bodident;
    short headident;
    processclass class;
    ushort numtrans;
    short first_trans;
    ushort nummodvars;
    ushort numIPs;
    ushort numextIPs;
#ifdef ECODE_DEBUGGING
    short vars;
    short params;
    short exvars;
#endif
    short *stack;
    short stacksize;
    short heapbot;
    short state;
    short first_state;		/* Used for cycle checking */
    short state_ident;
    short cycles;		/* How many times we've passed through
				 * first_state */
    short s;
    short b;
    short p;
    ushort checked;
    ulong init_time;
    transition_table trans_tbl;
    BOOLEAN trans_initialised;	/* Ready for display */
    module_table modvar_tbl;
    IP_table IP_tbl;
    struct queue_header *commonq;
    short maxlevel;
    short scopetable[MAXLEVEL];
    short tgroup;
    short last_trans[2];	/* Last executed transition in tgroup */
    ulong last_time[2];		/* Time of last executed transition */
    struct EventEntryStruct *classify;	/* The transition/message
					 * classification table */
    unsigned char other[MAXTRANS / 8];	/* Transitions having no event
					 * classification */
    struct IP_Int_Entry IP_Int_Tbl[IP_INT_MAX];
};

struct interaction_entry
{
    INTERACTION *next;		/* Next interaction in Queue	 */
    IP *destIP,			/* Destination IP		 */
       *entryIP;		/* Used for attach/detach moves */
    short ident;		/* Interaction ID		 */
    ushort len;			/* length of arguments		 */
    ushort tnum;		/* index of OUTPUT transition	 */
    short priority;
    ulong time;			/* OUTPUT time			 */
    short *argptr;		/* Pointer to argument block	 */
};

struct queue_header		/* Needed for common queues in particular */
{
    INTERACTION *first;
};

/* Queue disciplines for IPs */

enum q_discipline
{
    COMMONQ, INDIVIDUALQ
};

struct IP_entry
{
    char name[20];		/* Symbolic name */
    short ident;
    IP *outIP;			/* for connect or parent attach */
    IP *inIP;			/* for child attach only */
    struct queue_header *queue;
    MODULE *owner;
    enum q_discipline discipline;
    ushort breakpoint;
    ushort total_ints;
    ushort current_qlength;
    ulong total_length;
    ulong total_time;
    ushort max_time;
    ushort max_length;
    BOOLEAN connectpoint;	/* true=>ip has been connected; ie, its outIP
				 * is connected, not attached */
};


#define MODULE_SIZE	        sizeof(MODULE)
#define TRANS_SIZE	        sizeof(TRANS)
#define IP_SIZE                 sizeof(IP)
#define INTERACTION_SIZE        sizeof(INTERACTION)

#define CHILD(m,i)	(((m)->modvar_tbl)[i]).mod
#define Ip(m,i)		((m)->IP_tbl+(i))

/****************/
/* Breakpoints	*/
/****************/

/* Class - these correspond to windows, so don't change! */

enum brkpt_class
{
    BRK_LINE, BRK_FREE, BRK_TRANS, BRK_IP
};

/* Flags */

#define BRK_ACTIVATE	       	1
#define BRK_STOP		2
#define BRK_SUSPENDED		4

#define BRK_DUMPTRANS		8
#define BRK_DUMPIPS		16
#define BRK_DUMPBOTH		(8|16)

#define BRK_FORBODTYPE		32	/* Default 0 = This process only */
#define BRK_FORPEERS		64
#define BRK_FORALLPROCS		(32|64)
#define FOR_ALL(f)		((f&BRK_FORALLPROCS)==BRK_FORALLPROCS)

#define BRK_ONEXECUTE		128	/* Default 0 = On enable	 */

#define BRK_RESET		256
#define BRK_CHKALL		512	/* Combined breakpoint for same type */

struct breakpoint
{
    enum brkpt_class class;
    ushort pass_count;
    ushort enable_count;
    ushort flags;
    ushort where;
    ushort reactivate;
    short modbodID;
    MODULE *process;
};

/************************************/
/* TRANS operation argument offsets */
/************************************/

#define TR_TRANSNAME		1
#define TR_FROMSTATES           2
#define TR_FROMIDENT		6
#define TR_TOSTATE		7
#define TR_TOIDENT		8
#define TR_PRIORITY		9
#define TR_PROV			10
#define TR_WHEN			11
#define TR_DELAY        	12
#define TR_NEXT			13
#define TR_WHENLEN              14
#define TR_VARS			15
#define TR_TMPS			16
#define TR_BLOCK		17


/*********************/
/* Common operations */
/*********************/

#define TOS			stack[s]
#define PUSH(v)			stack[++s] = v
#define DROP()			stack[s--]
#define POP(lval)		lval = DROP()

#define CHAIN(l,v)		v = b; while (l--) v=stack[v]
#define PARAM()			(short)code[++p]

/******************/
/* Window related */
/******************/

/* window heights */

#define SOURCE_LNS	5
#define INIT_TRANS_LNS	8
#define IO_LNS		12
#define INIT_CHILD_LNS	INIT_TRANS_LNS
#define INIT_IP_LNS	2

/* Window widths */

#define SOURCE_COLS	78
#define TRANS_COLS	54
#define CHILD_COLS	22
#define IP_COLS		78

/* window indices for switchable windows */

enum exec_win
{
    SOURCE_WIN, CHILD_WIN, TRANS_WIN, IP_WIN
};

#define NUM_SWITCH_WINS	4

#define WIN_MAKE(f,l,t,w,h,ba,a)	Win_Make(WIN_CLIP|WIN_TILED|f,l,t,w,h,ba,a)

/********************/
/* Global variables */
/********************/

#ifndef GLOBAL
#define GLOBAL extern
#endif

GLOBAL FILE *sourcefl, *logfp, *trace, *ctrace, *etrace;

GLOBAL short current_fire_count, columnpos, groupStart, groupEnd, critical_section;

GLOBAL MODULE *root, *modnow;

GLOBAL ecode_op *code;

GLOBAL ulong timestamp, timelimit, last_globaltime, globaltime;

GLOBAL short s, b, p;

GLOBAL ushort run_sourceline, total_enabled, min_delay_time, iteration, nextfreebrkpt, first_line[NUM_SWITCH_WINS], total_lines[NUM_SWITCH_WINS], highlight_line[NUM_SWITCH_WINS], actual_line[NUM_SWITCH_WINS], scr_lines[NUM_SWITCH_WINS], globalTRG, buildBG, cycles;	/* Set for controlled
																																		 * execution		 */

GLOBAL short output_to;		/* Controls output: 0 - none 1 - to logfile
				 * only 2 - to screen only 3 - to both Set by
				 * $o<n> compiler directive */

GLOBAL enum exec_win active_win;

GLOBAL struct breakpoint breakpts[MAXBREAKPOINTS];

GLOBAL short *stack;

GLOBAL WINPTR pew_wins[NUM_SWITCH_WINS], IO_win;

GLOBAL ushort db_mode,		/* Debug mode   */
    tgroups[NUM_TGROUPS],	/* Number of transition groups */
    numgroups, animate_delay;

GLOBAL BOOLEAN ignore_interrupts, ecode_debugging, fullspeed, blocking, pathabort, nondeterministic, autoquit, refreshallwindows;

GLOBAL jmp_buf quit_info;

short getTrigger(short index);
void interp_error(enum errornumber code,...);
void ActivateScope(MODULE * m);
void activate_module(MODULE * m);
void schedule(void);
void exec_op(ecode_op op);
void 
_Module(MODULE * mod, short index, short addr, ushort exvarlen,
	ushort paramlen, short *params, short modvarident);
void dump_stats(MODULE * mod, short level);
void ReleaseProcess(MODULE * process_ptr);
void ReleaseProcesses(void);
void resync_display(BOOLEAN do_lazy_update);
void get_user_cmd(MODULE * running_process);
struct IP_entry *upperattach(IP * ip);
struct IP_entry *downattach(IP * ip);

void check_breakpoints(enum brkpt_class class, short index);
void dumpRecursively(MODULE * mod, BOOLEAN dumpTrans, BOOLEAN dumpTransStats, BOOLEAN dumpIP);
BOOLEAN open_browser_windows(ushort child_size, ushort trans_size, ushort IP_size);

#endif				/* INCLUDED_INTERP */
