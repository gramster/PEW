/************************************************/
/*						*/
/*	Interp.c - ECode interpreter		*/
/*						*/
/* written by G.Wheeler, May/June 1989		*/
/*						*/
/************************************************/

#include <math.h>
#include "misc.h"
#ifdef UNIX
extern short myrandom();
#else
#include <conio.h>		/* for getch */
#define myrandom	random
#endif
#include "screen.h"
#include "symbol.h"
#undef GLOBAL
#define GLOBAL
#include "interp.h"
#undef GLOBAL

#include "keybd.h"
#include <assert.h>
#include <stdarg.h>

#include "tree.h"

#ifndef FIFO_Q
#define FIFO_Q		0
#define RANDOM_Q	1
#define PRIORITY_Q	2
#endif

/* Additional prototypes */

extern void writeshort(FILE *, short);
extern void analyze_update(transition_table);
extern void Get_ModvarName(MODULE * mod, char *buff);

#if 0
FILE *sourcefl, *logfp, *trace, *ctrace, *etrace;

short current_fire_count, columnpos, groupStart, groupEnd, critical_section;

MODULE *root, *modnow;

ecode_op *code;

ulong timestamp, timelimit, last_globaltime, globaltime;

short s, b, p;

ushort run_sourceline, total_enabled, min_delay_time, iteration, nextfreebrkpt, first_line[NUM_SWITCH_WINS], total_lines[NUM_SWITCH_WINS], highlight_line[NUM_SWITCH_WINS], actual_line[NUM_SWITCH_WINS], scr_lines[NUM_SWITCH_WINS], globalTRG, buildBG, cycles;	/* Set for controlled
																																	 * execution		 */

short output_to;		/* Controls output: 0 - none 1 - to logfile
				 * only 2 - to screen only 3 - to both Set by
				 * $o<n> compiler directive */

enum exec_win active_win;

struct breakpoint breakpts[MAXBREAKPOINTS];

short *stack;

WINPTR pew_wins[NUM_SWITCH_WINS], IO_win;

ushort db_mode,			/* Debug mode   */
    tgroups[NUM_TGROUPS],	/* Number of transition groups */
    numgroups, animate_delay;

BOOLEAN ignore_interrupts, ecode_debugging, fullspeed, blocking, pathabort, nondeterministic, autoquit, refreshallwindows;

jmp_buf quit_info;
#endif

typedef struct
{
    short index;
    short trigger;
}   delayNode;

static delayNode delayPoints[MAX_DELAYS] = {{0, 0}};
static int delayTop;
static char outputChar[4] = {' ', '*', '+', '-'};

/* The next array is built during all the executions. It stores all the
	possible triggers associated with each delay transition. The array
	row entries contain the delay transition index, the number of triggers,
	and then the trigers themselves.
*/

short delayTriggers[MAX_DELAYS][MAX_TRIGGERS + 2] = {{0, 0}};
short delayKillers[MAX_DELAYS][MAX_TRIGGERS + 2] = {{0, 0}};
static char delayIsEnbl[32] = {0};
short triggerTop = 0, killerTop = 0;

static short lossTransitions[MAX_LOSSES];
static short lossTransTop = 0, outputLoss = 0;

char execSeq[256];		/* Sequence of transitions that have fired
				 * this run */
short execSeqTop = 0;
char transType[256];		/* Each entry corresponds to a transition,
				 * with 1 set if a delay and 2 set if a
				 * trigger */
short delayTransitions = 0;
short triggerTransitions = 0;
short killerTransitions = 0;

static void 
addLoss(short t)
{
    short i;
    for (i = 0; i < lossTransTop; i++)
	if (lossTransitions[i] == t)
	    return;
    lossTransitions[lossTransTop++] = t;
}

static void 
addDelay(short idx, short trigger)
{
    short i;
    assert(delayTop < MAX_DELAYS);
    /*
     * Note that this transition is a delay transition, and idx is a trigger
     * transition
     */
    if ((transType[idx] & 1) == 0)
    {
	transType[idx] |= 1;
	delayTransitions++;
    }
    if (trigger >= 0)
	if ((transType[trigger] & 2) == 0)
	{
	    transType[trigger] |= 2;
	    triggerTransitions++;
	}
    /*
     * Add the info to the set of delay transitions currently being examined
     * by the scheduler
     */
    delayPoints[delayTop].index = idx;
    delayPoints[delayTop].trigger = trigger;
    delayTop++;
    /*
     * Take note of the trigger. We use the delayTriggers array, which
     * consists of a number of rows. The first element in the row is the
     * delay transition, the second is the number of different triggers, and
     * the rest are the triggers themselves
     */
    for (i = 0; i < triggerTop; i++)
	if (delayTriggers[i][0] == idx)
	{
	    short j, nt = delayTriggers[i][1] + 2;
	    for (j = 2; j < nt; j++)
		if (delayTriggers[i][j] == trigger)
		    return;
	    /* Found the transition, but not the trigger */
	    delayTriggers[i][nt] = trigger;
	    delayTriggers[i][1]++;
	    return;
	}
    /* Didn't find the transition */
    delayTriggers[triggerTop][0] = idx;
    delayTriggers[triggerTop][1] = 1;
    delayTriggers[triggerTop][2] = trigger;
    triggerTop++;
}

static void 
addKiller(short idx, short killer)
{
    short i;
    /* Note that this transition is a killer transition */
    if ((transType[killer] & 4) == 0)
    {
	transType[idx] |= 4;	/* killer */
	killerTransitions++;
    }
    for (i = 0; i < killerTop; i++)
	if (delayKillers[i][0] == idx)
	{
	    short j, nt = delayKillers[i][1] + 2;
	    for (j = 2; j < nt; j++)
		if (delayKillers[i][j] == killer)
		    return;
	    /* Found the transition, but not the killer */
	    delayKillers[i][nt] = killer;
	    delayKillers[i][1]++;
	    return;
	}
    /* Didn't find the transition */
    delayKillers[killerTop][0] = idx;
    delayKillers[killerTop][1] = 1;
    delayKillers[killerTop][2] = killer;
    killerTop++;
}

short 
getTrigger(short index)
{
    return delayPoints[index].trigger;
}

void 
dumpTriggers(void)
{
    short i;
    FILE *fp;
    fp = fopen("trig.dat", "wt");
    fprintf(fp, "%d\n", triggerTop);
    fprintf(treefp, "\n****************************************\nTRIGGERS\n========\n");

    for (i = 0; i < triggerTop; i++)
    {
	short j, nt = delayTriggers[i][1];
	fprintf(fp, "%d=E%d(", (int)delayTriggers[i][0] + 1, (int)nt);
	fprintf(treefp, "\nTransition %-2d: ", delayTriggers[i][0] + 1);
	nt += 2;
	for (j = 2; j < nt; j++)
	{
	    fprintf(treefp, "%-2d ", delayTriggers[i][j] + 1);
	    fprintf(fp, "%d ", (int)delayTriggers[i][j] + 1);
	}
	fprintf(fp, ")\n");
    }
    fprintf(treefp, "\n****************************************\n");
    fclose(fp);
}

void 
dumpKillers()
{
    short i;
    FILE *fp;
    fp = fopen("kill.bin", "wb");
    writeshort(fp, killerTop);
    fprintf(treefp, "\n****************************************\nKILLERS\n========\n");

    for (i = 0; i < killerTop; i++)
    {
	short j, nt = delayKillers[i][1];
	writeshort(fp, nt);
	writeshort(fp, delayKillers[i][0]);
	fprintf(treefp, "\nTransition %-2d: ", delayKillers[i][0] + 1);
	nt += 2;
	for (j = 2; j < nt; j++)
	{
	    fprintf(treefp, "%-2d ", delayKillers[i][j] + 1);
	    writeshort(fp, delayKillers[i][j]);
	}
    }
    fprintf(treefp, "\n****************************************\n");
    fclose(fp);
}

void 
dumpParams()
{
    FILE *fp = fopen("param.dat", "wt");
    short i;
    for (i = 0; i < triggerTop; i++)
	fprintf(fp, "D%d\t\n", (int)delayTriggers[i][0] + 1);
    for (i = 0; i < lossTransTop; i++)
	fprintf(fp, "L%d\t\n", (int)lossTransitions[i] + 1);
    fclose(fp);
}

/*********************/
/* Global meta table */
/*********************/

short **GMT = NULL, GlobalTrans, GMTrows;
float **GMTdel;

/************************/
/* Forward declarations */
/************************/

static void NEAR resyncSourceline(short addr);
static short NEAR exec_block(short address);
static short NEAR dequeue_interaction(short ip_offset, BOOLEAN discard);
typedef void NEAR(*LocNearFn) (IP *, ushort);

#define resync_sourceline(a)	if (!buildBG) resyncSourceline(a)


short InInit = 0;		/* Flag to indicate we are in initialisation
				 * section so we shouldn't keep meta-table
				 * statistics */

TRANS *ExecutingTrans;		/* Transition being executed */
TRANS *LastExecutingTrans;

/* Trace file parameters */

#define TRACE_COL_WIDTH		13

char trace_title[] = "\n[ %-7ld]======================================================================\n";
BOOLEAN titledue = FALSE;	/* Flag set when time changes. We will only
				 * print the title when we output a message
				 * and this flag is set */

/***** Scope Handling ******************************/

static void 
SetScope(short level, short offset)
{
    if (offset >= 0)
	block[level].lastobject = (pointer) (locspace + offset);
    else
	block[level].lastobject = NULL;
}

void 
ActivateScope(MODULE * mod)
{
    short i = mod->maxlevel;
    while (i >= 0)
    {
	if (mod->scopetable[i] >= 0)
	    block[i].lastobject = (pointer) (locspace + mod->scopetable[i]);
	else
	    block[i].lastobject = NULL;
	i--;
    }
    blocklevel = mod->maxlevel;
}


/*********************************************************************
	We want to check if there is an interaction at the head of an
	IP queue which is not dealt with by a When clause. To do
	this, we use the array of structs defined below. We assume that
	no process has more than MAXTRANS transitions. Each struct has
	the associated transition's When ip ident, the associated queue's
	head interaction ident, and the associated IP. During clause
	evaluation for a process, all this data gets entered. At the
	end, if any (head interaction,IP) pairs do not have corresponding
	(when ip interaction,IP) pairs, we know there is no WHEN clause
	for the (head interaction, IP) pair. We report the problem, and
	discard the head interaction from the IP. The `chktrans_num'
	variable is used as the index into the table.

	If illegal interactions were found, they may have prevented
	transitions that would otherwise have been enabled from firing.
	Thus, we do not want to advance time until we have run the
	scheduler again. The `must_not_delay' flag is used for this.
*********************************************************************/

struct Interaction_Check
{
    short when_int_ident;
    short head_int_ident;
    ushort when_IP_offset;
    IP *destIP, *thisIP;
};

static struct Interaction_Check Intr_Chks[MAXTRANS];

static short chktrans_num;

static BOOLEAN must_not_delay = FALSE;

static void NEAR 
Note_When_Info(short w_ID, short h_ID, IP * destIP, IP * whenIP, ushort offset)
{
    if (chktrans_num >= MAXTRANS)
	interp_error(ERR_2MANYWHEN);
    Intr_Chks[chktrans_num].when_int_ident = w_ID;
    Intr_Chks[chktrans_num].head_int_ident = h_ID;
    Intr_Chks[chktrans_num].destIP = destIP;
    Intr_Chks[chktrans_num].thisIP = whenIP;
    Intr_Chks[chktrans_num].when_IP_offset = offset;
    chktrans_num++;
}

#define HNDL(f)	static void NEAR f(void)

HNDL(CheckForBadInteractions)
{
    if (chktrans_num)
    {
	short i, j, id;
	BOOLEAN ok;
	IP *destIP;
	for (i = 0; i < chktrans_num; i++)
	{
	    if ((id = Intr_Chks[i].head_int_ident) != 0)
	    {
		ok = FALSE;
		destIP = Intr_Chks[i].destIP;
		for (j = 0; j < chktrans_num; j++)
		{
		    if (Intr_Chks[j].when_int_ident == id)
			if (Intr_Chks[j].thisIP == destIP)
			{
			    /* Entry is OK; clear it */
			    Intr_Chks[i].head_int_ident = 0;
			    ok = TRUE;
			    break;
			}
		}
		if (!ok)
		{
		    /*
		     * We don't wan't time to progress until all bad
		     * interactions have been removed...
		     */
		    must_not_delay = TRUE;
		    (void)dequeue_interaction(Intr_Chks[i].when_IP_offset, TRUE);
		    /*
		     * To ensure we don't try to dequeue again, clear all
		     * other entries with the same id. If there are more bad
		     * interactions, we'll find them next time.
		     */
		    for (j = 0; j < chktrans_num; j++)
			if (Intr_Chks[j].head_int_ident == id)
			    Intr_Chks[j].head_int_ident = 0;
		}
	    }
	}
    }
}

/********************************/
/* Interpreter Error Handler	*/
/* virtually a clone of	Error()	*/
/********************************/

void 
    interp_error(enum errornumber code,...)
    {
	ushort rtn;
	char MsgBuf[80], ErrBuf[80], Line[12];
	va_list argptr;
	    va_start(argptr, code);
	if  (ErrorGetMessage(code, MsgBuf, 80))
	        vsprintf(ErrBuf, MsgBuf, argptr);
	else
	        sprintf(ErrBuf, "Code: %d", (int)code);
	    resync_sourceline(p);
	    sprintf(Line, "Line %d : ", (int)run_sourceline);
	    rtn = Win_Error(NULL, Line, ErrBuf, FALSE, TRUE);
	    va_end(argptr);
	if  (rtn == KB_F1 || rtn == KB_ALT_H)
	        ErrorHelp(code);
	    longjmp(quit_info, -1);
    }

/***************************/
/* Dynamic stack expansion */
/***************************/

#define CHUNKSIZE	64

    static void NEAR expand_stack(MODULE * mod, short newsize)
{
    short i, *newstack;
    if (modnow->heapbot == 0)	/* Can expand as no dynamic allocation has
				 * occured yet... */
    {
	/* To prevent trivial expansions, round up to a chunk */

	if ((newsize - mod->stacksize) < CHUNKSIZE)
	    newsize = mod->stacksize + CHUNKSIZE;

	/* Allocate new stack and copy contents of old... */

	newstack = (short *)Mem_Calloc(newsize, sizeof(short), 1);
	if (newstack == NULL)
	    interp_error(ERR_FAILEXPAND);
	i = mod->stacksize;
	while (i--)
	    newstack[i] = mod->stack[i];
	Mem_Free(mod->stack);
	stack = mod->stack = newstack;
	mod->stacksize = newsize;
    } else
	interp_error(ERR_FAILEXPAND);
}

/********************************/
/* Multiple PUSH/POP operations */
/********************************/

static void NEAR 
nPush(short num,...)
{
    short val;
    va_list args;
    va_start(args, num);
    while (num--)
    {
	val = va_arg(args, short);
	PUSH(val);
    }
    va_end(args);
}

static void NEAR 
nPop(short num,...)
{
    short *loc;
    va_list args;
    va_start(args, num);
    while (num--)
    {
	loc = va_arg(args, short *);
	if (loc != NULL)
	    POP(*loc);
	else
	    (void)DROP();
    }
    va_end(args);
}

/********************************/
/* Multiple parameter loading:	*/
/* load a lot of short variables*/
/* (passed by reference) with	*/
/* parameters from the code	*/
/********************************/

static void NEAR 
get_params(short num, short *addr,...)
{
    short *param;
    va_list args;
    va_start(args, addr);
    while (num--)
    {
	param = va_arg(args, short *);
	*param = (short)code[*addr += 1];
    }
    va_end(args);
}

/************************************************/
/* Find the next op of a specified type from a	*/
/* given address, and return its address.	*/
/************************************************/

static short NEAR 
skipto(ecode_op op, short fromaddr)
{
    while (code[fromaddr] != op)
	fromaddr += Op_NArgs[(short)code[fromaddr]] + 1;
    return fromaddr;
}

/************************************************/
/* Resynchronise with the source code by 	*/
/* by searching for the nearest NEWLINE op.	*/
/************************************************/

static void NEAR 
resyncSourceline(short addr)
{
    addr = skipto(OP_NEWLINE, addr);
    run_sourceline = (ushort) ((short)code[addr + 1] - 1);
}

/********************************************************/
/* Given an IP, find its uppermost and lowermost	*/
/* attachment points and return them. These routines are*/
/* global as they are used by the process browser to	*/
/* perform the View CEP command.			*/
/********************************************************/

IP *
upperattach(IP * ip)
{
    if (ip != NULL)
	while (ip->outIP != NULL && ip->connectpoint == FALSE)
	    ip = ip->outIP;
    return ip;
}

IP *
downattach(IP * ip)
{
    if (ip != NULL)
	while (ip->inIP != NULL)
	    ip = ip->inIP;
    return ip;
}


static void NEAR 
enqueue_interaction(MODULE * mod, short ip_offset,
	   INTERACTION * intr, short priority, short propdelay, short Qtype)
{
    IP *ip_ent = Ip(mod, ip_offset);
    ip_ent = upperattach(ip_ent)->outIP;
    if (ip_ent == NULL)
	interp_error(ERR_NOCONNECT);
    intr->entryIP = ip_ent;
    ip_ent = downattach(ip_ent);
    intr->priority = priority;
    intr->destIP = ip_ent;
    intr->time = globaltime + propdelay;
    intr->next = NULL;
    if (ip_ent->queue->first == NULL)
	ip_ent->queue->first = intr;
    else
	switch (Qtype)
	{
	case FIFO_Q:
	    {			/* append onto queue */
		INTERACTION *tmp = ip_ent->queue->first;
		while (tmp->next != NULL)
		    tmp = tmp->next;
		tmp->next = intr;
	    }
	    break;
	case RANDOM_Q:
	    {
		int pos = 1 + myrandom(ip_ent->current_qlength - 1);
		INTERACTION *tmp = ip_ent->queue->first;
		while (pos--)
		    tmp = tmp->next;
		intr->next = tmp->next;
		tmp->next = intr;
	    }
	    break;
	case PRIORITY_Q:
	    {
		INTERACTION *tmp = ip_ent->queue->first;
		while (tmp)
		{
		    if (tmp->next == NULL || tmp->next->priority > priority)
		    {
			intr->next = tmp->next;
			tmp->next = intr;
			break;
		    }
		    tmp = tmp->next;
		}
	    }
	    break;
	}
    /* Update stats */
    ip_ent->current_qlength++;
    ip_ent->total_length++;
    if (ip_ent->current_qlength > ip_ent->max_length)
	ip_ent->max_length++;
}

static short NEAR 
copy_interaction(short ip_offset)
{
    INTERACTION *tmp;
    short len, rtn;
    short *args;
    IP *ip_ent = Ip(modnow, ip_offset);
    if ((tmp = ip_ent->queue->first) == NULL)
	interp_error(ERR_IPEMPTY);
    rtn = len = (short)tmp->len;
    args = tmp->argptr;
    if ((s + len) > modnow->stacksize)
	expand_stack(modnow, s + len);
    while (len--)
	PUSH(*args++);
    return rtn;
}

static short NEAR 
dequeue_interaction(short ip_offset, BOOLEAN discard)
{
    IP *ip = Ip(modnow, ip_offset);
    INTERACTION *tmpq;
    short rtn = 0;
    long tm;
    tmpq = ip->queue->first;
    if (tmpq)
    {
	if (!discard)
	    rtn = copy_interaction(ip_offset);
	Mem_Free(tmpq->argptr);
	ip->queue->first = tmpq->next;
	ip->total_ints++;
	ip->current_qlength--;
	tm = (long)globaltime - (long)tmpq->time;
	ip->total_time += (ulong) tm;
	if (tm > ip->max_time)
	    ip->max_time = (ushort) tm;
	if (discard || TRON)
	{
	    char int_name[40], modvar_name[22];
	    Get_ModvarName(modnow, modvar_name);
	    get_symbolic_name(tmpq->ident, 0, int_name, NULL, 40);
	    if (discard)
		fprintf(logfp, "Cannot deal with interaction!\n");
	    fprintf(logfp, "####### interaction '%s' %s by %s from IP %s\n",
		    int_name, discard ? "discarded" : "dequeued", modvar_name, ip->name);
	}
	Mem_Free(tmpq);
    }
    return rtn;
}

static void NEAR 
    prepend_list(struct queue_header *queue, INTERACTION * list)
    {
	INTERACTION *tmp;
	if  (list)
	{
	    tmp = list;
	    while (tmp->next)
		tmp = tmp->next;
	    tmp->next = queue->first;
	    queue->first = list;
	}
    }

static void NEAR 
move_interactions(IP * old_dest, IP * new_dest)
{
    INTERACTION *first, *prev, *newlist = NULL, *tmp;
    prev = first = old_dest->queue->first;
    while (first)
    {
	if (first->destIP == old_dest)
	{
	    /* unlink from old queue */
	    if (prev == first)
		old_dest->queue->first = first->next;
	    else
		prev->next = first->next;
	    /* rename destination */
	    first->destIP = new_dest;
	    /* add to new temporary queue */
	    if (newlist == NULL)
		newlist = first;
	    else
	    {
		tmp = newlist;
		while (tmp->next)
		    tmp = tmp->next;
		tmp->next = first;
	    }
	    first->next = NULL;
	}
	prev = first;
	first = first->next;
    }
    prepend_list(new_dest->queue, newlist);
}

/************************/
/* Stack space checking	*/
/************************/

HNDL(_EnterBlock)
{
    short stackspc = PARAM();
    if ((s + 3 + stackspc) > modnow->stacksize)
	expand_stack(modnow, s + 3 + stackspc);
}

/**********************************************/
/* Exponential/Poisson Distribution Utilities */
/**********************************************/

#ifdef UNIX
extern double drand48();
#else
#define drand48()	( ((double)rand()) / ((double)RAND_MAX) )
#endif

static short NEAR 
Dis_Exponential(short Min, short Max, short mean)
{
    short rtn;
    double Mean = (double)mean;
    do
    {
	double tmp = drand48() + .0001;
	rtn = (short)(-Mean * log(tmp));
    } while (rtn < Min || rtn > Max);
    return rtn;
}

static short NEAR 
Dis_Poisson(short Min, short Max, short mean)
{
    double b = exp(-mean), tr;
    short rtn;
    do
    {
	rtn = -1;
	tr = 1.;
	do
	{
	    tr *= drand48();
	    rtn++;
	} while (tr > b);
    } while (rtn < Min || rtn > Max);
    return rtn;
}

static short NEAR 
Dis_Geometric(short Min, short Max, short mean)
{
    short rtn;
    float expekt = log(1.001 - 1. / ((float)mean));
    do
    {
	rtn = (short)(0.5 + (log(.001 + drand48()) / expekt));
    } while (rtn < Min || rtn > Max);
    return rtn;
}

/***************/
/* Expressions */
/***************/

HNDL(_Minus)
{
    TOS = -TOS;
}
HNDL(_Not)
{
    TOS = !TOS;
}

HNDL(_Add)
{
    s--;
    TOS += stack[s + 1];
}
HNDL(_And)
{
    s--;
    TOS &= stack[s + 1];
}
HNDL(_Div)
{
    if (TOS == 0)
	interp_error(ERR_ZERODIV);
    s--;
    TOS /= stack[s + 1];
}

HNDL(_Modulo)
{
    s--;
    TOS %= stack[s + 1];
}
HNDL(_Multiply)
{
    s--;
    TOS *= stack[s + 1];
}
HNDL(_Or)
{
    s--;
    TOS |= stack[s + 1];
}
HNDL(_Subtract)
{
    s--;
    TOS -= stack[s + 1];
}

HNDL(_Equal)
{
    s--;
    TOS = (TOS == stack[s + 1]);
}
HNDL(_Greater)
{
    s--;
    TOS = (TOS > stack[s + 1]);
}
HNDL(_Less)
{
    s--;
    TOS = (TOS < stack[s + 1]);
}
HNDL(_Notequal)
{
    s--;
    TOS = (TOS != stack[s + 1]);
}
HNDL(_Notgreater)
{
    s--;
    TOS = (TOS <= stack[s + 1]);
}
HNDL(_Notless)
{
    s--;
    TOS = (TOS >= stack[s + 1]);
}

HNDL(_Range)
{
    short Min, Max;		/* capitalised to distinguish from macros */
    get_params(2, &p, &Min, &Max);
    if (TOS < Min || TOS > Max)
	interp_error(ERR_OUTRANGE, TOS, Min, Max);
}

/************************************/
/* Variable addressing & assignment */
/************************************/

HNDL(_Assign)
{
    short length, addr, val, *deststk;
    s -= (length = PARAM());	/* pop value */
    POP(addr);			/* pop address */
    val = s + 2;		/* get start of value */
    if (addr >= 0)
	deststk = stack;
    else
    {
	addr = -addr;
	deststk = CHILD(modnow, DROP())->stack;
    }
    while (length--)
	deststk[addr++] = stack[val++];
}


HNDL(_With)
{
    /* Create a new activation record */
    nPush(3, b, b, 0);		/* Push WITH activation record */
    b = s - 2;
}

HNDL(_WithField)
{
    /*
     * Get the record record from the appropriate activation record, and do a
     * field op
     */
    short fieldoffset = PARAM(), withlevel, addr, recaddr;
    withlevel = PARAM();
    CHAIN(withlevel, addr);
    recaddr = stack[addr - 1];
    if (recaddr < 0)
	recaddr -= fieldoffset;
    else
	recaddr += fieldoffset;
    PUSH(recaddr);
}

HNDL(_EndWith)
{
    /*
     * Pop the specified number of activation records, plus the last address
     */
    short numlevels = PARAM();
    while (numlevels--)
    {
	s = b + 2;
	nPop(3, NULL, &b, NULL);/* pop activation record */
    }
    s--;			/* Pop last address */
}

HNDL(_Field)
{
    if ((TOS) < 0)
	TOS -= PARAM();		/* exported var */
    else
	TOS += PARAM();
}

HNDL(_Index)
{
    short lower, upper, eltsize, ix;
    BOOLEAN exported;
    POP(ix);
    get_params(3, &p, &lower, &upper, &eltsize);
    exported = (ix < 0) ? ((ix = -ix), TRUE) : FALSE;
    if (ix < lower || ix > upper)
	interp_error(ERR_OUTRANGE, ix, lower, upper);
    else
	TOS += (ix - lower) * eltsize;
    if (exported)
	TOS = -TOS;		/* Re-negate address if exported */
}

HNDL(_Value)
{
    short length = PARAM(), addr, *sourcestk;
    POP(addr);
    if (addr >= 0)
	sourcestk = stack;
    else
    {
	addr = -addr;
	sourcestk = CHILD(modnow, DROP())->stack;
    }
    while (length--)
	PUSH(sourcestk[addr++]);
}

HNDL(_Variable)
{
    short x, level = PARAM(), displ;
    displ = PARAM();
    CHAIN(level, x);		/* Follow activation chain */
    PUSH(x + displ);		/* get address of variable */
}

HNDL(_ExpVar)			/* Exported variable */
{
    short x, level = PARAM(), displ, module;
    displ = PARAM();
    POP(module);
    activate_module(CHILD(modnow, module));
    CHAIN(level, x);		/* Follow activation chain */
    activate_module(modnow->parent);
    PUSH(module);
    PUSH(-(x + displ));		/* push -ve address of child's variable */
}

HNDL(_Varparam)
{
    short x, level = PARAM(), displ;
    displ = PARAM();
    CHAIN(level, x);		/* Follow activation chain */
    PUSH(stack[x + displ]);	/* Get address of bound variable */
}

HNDL(_ReturnVar)
{
    PUSH(PARAM() + b);
}

/*************************************/
/* Procedure/ function call handling */
/*************************************/

HNDL(_EndProc)
{
    short params, rtn_len, rtn_val_top;
    get_params(2, &p, &params, &rtn_len);
    s = b + 2;
    rtn_val_top = b - 1;
    nPop(3, &p, &b, NULL);	/* pop activation record */
    s -= params + rtn_len;	/* pop parameters and return value */
    while (rtn_len--)
	PUSH(stack[rtn_val_top - rtn_len]);
    resync_sourceline(p + 1);
}

HNDL(_Proccall)
{
    short x, level, displ, rtn_len;
    get_params(3, &p, &level, &displ, &rtn_len);
    CHAIN(level, x);
    s += rtn_len;
    nPush(3, x, b, p);		/* Push activation record */
    b = s - 2;
    p += displ - 4;
    resync_sourceline(p + 1);
}

HNDL(_Procedure)
{
    short tmps, vars, p_delta;
    vars = PARAM();
    tmps = PARAM();
    if ((s + vars + tmps) > modnow->stacksize)
	expand_stack(modnow, s + vars + tmps);
    s += vars;
    p_delta = PARAM() - 4;
    p += p_delta;
    resync_sourceline(p + 1);
}

/*************************/
/* Structured statements */
/*************************/

HNDL(_Case)
{
    /*
     * We only get here if the case index value has no corresponding
     * statement - we must generate a run-time error
     */
    interp_error(ERR_CASEVALUE);
}

HNDL(_Do)
{
    short offset = PARAM();
    if (!DROP())
    {
	p += offset - 1 - Op_NArgs[(short)OP_DO];
	resync_sourceline(p + 1);
    }
}

HNDL(_For)
{
    short start, end, address, displ, mode;
    get_params(2, &p, &displ, &mode);
    nPop(3, &end, &start, &address);
    stack[address] = start;
    if ((mode * start) > (mode * end))
    {
	p += displ - 1 - Op_NArgs[(short)OP_FOR];	/* jump out of loop */
	resync_sourceline(p + 1);
    } else
	nPush(2, address, end);
}

HNDL(_Next)
{
    short displ, mode, addr, end;
    get_params(2, &p, &displ, &mode);
    nPop(2, &end, &addr);
    if ((mode > 0 && stack[addr] < end) || (mode < 0 && stack[addr] > end))
    {
	stack[addr] += mode;	/* update loop var and loop back */
	nPush(2, addr, end);
	p += displ - 1 - Op_NArgs[(short)OP_NEXT];
	resync_sourceline(p + 1);
    }
}


HNDL(_Goto)
{
    short offset = PARAM();
    p += offset - 1 - Op_NArgs[(short)OP_GOTO];
    resync_sourceline(p + 1);
}

HNDL(_Indexedjump)
/* Should be followed by a number of GOTO statements */
{
    p += DROP() * 2;		/* As 2 = 1+Op_NArgs[OP_GOTO] */
    resync_sourceline(p + 1);
}

/****************/
/* Set Handling */
/****************/

HNDL(_Addsetelement)
{
    short elt, bitmask;
    elt = SET_WORD(TOS);
    bitmask = SET_MASK(TOS);
    stack[--s - elt] |= bitmask;
}

HNDL(_Addsetrange)
{
    short stop, start;
    nPop(2, &stop, &start);
    while (start <= stop)
    {
	stack[s - SET_WORD(start)] |= SET_MASK(start);
	start++;
    }
}

HNDL(_Difference)
{
    register short setelt;
    for (setelt = 0; setelt < SET_SIZE; setelt++)
    {
	stack[s - setelt - SET_SIZE] ^= (stack[s - setelt - SET_SIZE] & stack[s - setelt]);
    }
    s -= SET_SIZE;
}

HNDL(_In)
{
    s -= SET_SIZE;		/* pop set constant */
    if (stack[s + SET_SIZE - SET_WORD(TOS)] & SET_MASK(TOS))
	TOS = 1;
    else
	TOS = 0;		/* overwrite set element number */
}

HNDL(_Intersection)
{
    register short setelt;
    for (setelt = 0; setelt < SET_SIZE; setelt++)
    {
	stack[s - setelt - SET_SIZE] &= stack[s - setelt];
    }
    s -= SET_SIZE;
}

HNDL(_Setassign)
{
    register short destaddr = stack[s - SET_SIZE], source = s - SET_SIZE,
       *deststk;
    if (destaddr >= 0)
	deststk = stack;
    else
    {
	destaddr = -destaddr;
	deststk = CHILD(modnow, DROP())->stack;
    }
    while (source != s)
	deststk[destaddr++] = stack[++source];
    s -= SET_SIZE + 1;		/* Pop set constant and dest address */
}

static void NEAR 
SetInclusion(BOOLEAN is_subset)
/*
	is_subset should be TRUE to test if lower is a subset of higher,
	or FALSE to test if higher is a subset of lower. */
{
    register short setelt, offset = is_subset ? SET_SIZE : 0;
    for (setelt = 0; setelt < SET_SIZE; setelt++)
    {
	if ((stack[s - setelt] & stack[s - setelt - SET_SIZE])
	    != stack[s - setelt - offset])
	    break;
    }
    s -= SET_SIZE << 1;
    PUSH(setelt < SET_SIZE ? 0 : 1);
}

HNDL(_Setinclusion)
{
    SetInclusion(TRUE);
}

HNDL(_Setinclusiontoo)
{
    SetInclusion(FALSE);
}

HNDL(_Setequal)
{
    register short setelt;
    for (setelt = 0; setelt < SET_SIZE; setelt++)
    {
	if (stack[s - setelt] != stack[s - setelt - SET_SIZE])
	    break;
    }
    s -= SET_SIZE << 1;
    PUSH(setelt < SET_SIZE ? 0 : 1);
}

HNDL(_Union)
{
    register short setelt;
    for (setelt = 0; setelt < SET_SIZE; setelt++)
    {
	stack[s - setelt - SET_SIZE] |= stack[s - setelt];
    }
    s -= SET_SIZE;
}

/*******************/
/* String handling */
/*******************/

static void NEAR 
StringCompare(BOOLEAN ls_val, BOOLEAN eq_val, BOOLEAN gt_val)
{
    short len = PARAM(), pos = 1;
    s -= len * 2;		/* Pop strings */
    while (pos <= len && stack[s + pos] == stack[s + pos + len])
	pos++;
    if (pos > len)
	PUSH((ushort) eq_val);
    else
    {
	ushort tmp = (ushort) (stack[s + pos] < stack[s + pos + len] ? ls_val : gt_val);
	PUSH(tmp);
    }
}

HNDL(_Stringequal)
{
    StringCompare(FALSE, TRUE, FALSE);
}
HNDL(_Stringless)
{
    StringCompare(TRUE, FALSE, FALSE);
}
HNDL(_Stringgreater)
{
    StringCompare(FALSE, FALSE, TRUE);
}
HNDL(_Stringnotequal)
{
    StringCompare(TRUE, FALSE, TRUE);
}
HNDL(_Stringnotless)
{
    StringCompare(FALSE, TRUE, TRUE);
}
HNDL(_Stringnotgreater)
{
    StringCompare(TRUE, TRUE, FALSE);
}

/*************/
/* Debugging */
/*************/

HNDL(_Scope)
{
    short level, offset;
    get_params(2, &p, &level, &offset);
    SetScope(level, offset);
}


/***********************/
/* Standard procedures */
/***********************/

#define BLK_LEN		0
#define BLK_NEXT	1
#define BLK_DATA	2


HNDL(_New)
{
    short nwords = PARAM(), *hblock, bloc_loc;
    BOOLEAN done = FALSE;
    if (modnow->heapbot == 0)	/* Must create a heap */
    {
	expand_stack(modnow, modnow->stacksize + HEAP_SIZE);
	modnow->heapbot = modnow->stacksize - BLK_DATA;
	hblock = stack + modnow->heapbot;
	hblock[BLK_NEXT] = 0;
	hblock[BLK_LEN] = 0;
    }
    bloc_loc = modnow->heapbot;
    hblock = stack + bloc_loc;
    while (hblock[BLK_NEXT])
    {
	if ((hblock[BLK_NEXT] - bloc_loc - hblock[BLK_LEN] - (BLK_DATA << 1)) >= nwords)
	{
	    short next_bloc = hblock[BLK_NEXT];
	    hblock[BLK_NEXT] = bloc_loc + hblock[BLK_LEN] + BLK_DATA;
	    PUSH(hblock[BLK_NEXT] + BLK_DATA);
	    hblock += hblock[BLK_LEN] + BLK_DATA;
	    hblock[BLK_NEXT] = next_bloc;
	    hblock[BLK_LEN] = nwords;
	    done = TRUE;
	    break;
	}
	bloc_loc = hblock[BLK_NEXT];
	hblock = stack + bloc_loc;
    }
    if (!done)
    {
	bloc_loc = modnow->heapbot - (BLK_DATA + nwords);
	if (bloc_loc <= s)
	    interp_error(ERR_NEWFAIL);
	hblock = stack + bloc_loc;
	hblock[BLK_NEXT] = modnow->heapbot;
	hblock[BLK_LEN] = nwords;
	modnow->heapbot = bloc_loc;
	PUSH(bloc_loc + BLK_DATA);
    }
}

HNDL(_Dispose)
{
    short ptr_addr = DROP(), *hblock;
    BOOLEAN done = FALSE;
    if (modnow->heapbot == 0)
	interp_error(ERR_BADDISPOSE);
    ptr_addr = stack[ptr_addr] - BLK_DATA;
    hblock = stack + modnow->heapbot;
    if (ptr_addr == modnow->heapbot)
	modnow->heapbot = hblock[BLK_NEXT];
    else
    {
	while (hblock[BLK_NEXT])
	{
	    if (hblock[BLK_NEXT] == ptr_addr)
	    {
		hblock[BLK_NEXT] = stack[ptr_addr + BLK_NEXT];
		done = TRUE;
		break;
	    }
	    hblock = stack + hblock[BLK_NEXT];
	}
	if (!done)
	    interp_error(ERR_BADDISPOSE);
    }
}


static short NEAR 
GetCh(void)
{
    short c;
    Win_PutChar((uchar) (c = getch()));
    if (c == (short)'\n')
	Win_PutChar((uchar) '\r');
    else if (c == (short)'\r')
	Win_PutChar((uchar) '\n');
    return c;
}

HNDL(_ReadInt)
{
    short i, addr;
#ifdef UNIX
    scanf("%hd", &i);
#else
    BOOLEAN done = FALSE;
    short j, sign;
    WINPTR oldwin = Win_Current;

    Win_Activate(IO_win);
    Win_SetAttribute(SCR_NORML_ATTR | SCR_BOLD_CHAR);
    while (!done)
    {
	i = 0;
	sign = 1;
	while (j = getch(), (j < '0' || j > '9') && (j != '-') && (j != '+'))
	    Win_PutChar((uchar) j);
	Win_PutChar((uchar) j);
	while (j == '-' || j == '+' || j == ' ')
	{
	    if (j == '-')
		sign = -sign;
	    Win_PutChar((uchar) (j = getch()));
	}
	while (j >= '0' && j <= '9')
	{
	    if (i * 10 < i)
		break;		/* overflow */
	    i = i * 10 + j - '0';
	    Win_PutChar((uchar) (j = getch()));
	    done = TRUE;
	}
	i *= sign;
    }
#endif
    POP(addr);
    if (addr >= 0)
	stack[addr] = i;
    else
	CHILD(modnow, DROP())->stack[-addr] = i;	/* Exported var */
    Win_PutChar('\n');
#ifndef UNIX
    if (oldwin)
	Win_Activate(oldwin);
#endif
}

HNDL(_ReadStr)
{
    /*
     * I actually don't think the compiler will allow reading and writing of
     * exported variables. Anyway, ther's no real harm done in implementing
     * this side.
     */
    short length = PARAM(), address = DROP(), *stk;
    WINPTR oldwin = Win_Current;
    Win_Activate(IO_win);
    Win_SetAttribute(SCR_NORML_ATTR | SCR_BOLD_CHAR);
    if (address >= 0)
	stk = stack;
    else
    {
	address = -address;
	stk = CHILD(modnow, DROP())->stack;
    }
    do
    {
	short c = GetCh();
	if (c < 32 || c > 127)
	    break;
	stk[address++] = c;
    } while ((--length) != 0);
    while (length--)
	stk[address++] = 0;
    if (oldwin)
	Win_Activate(oldwin);
}

HNDL(_WriteInt)
{
    short val = DROP();
    if (output_to == 1 || output_to == 3)
	fprintf(logfp, "%d", (int)val);
    if (output_to == 2 || output_to == 3)
    {
	WINPTR oldwin = Win_Current;
	Win_Activate(IO_win);
	Win_SetAttribute(SCR_NORML_ATTR);
	Win_Printf("%d", val);
	if (oldwin)
	    Win_Activate(oldwin);
    }
}

HNDL(_ReadCh)
{
    short addr = DROP();
    WINPTR oldwin = Win_Current;
    Win_Activate(IO_win);
    Win_SetAttribute(SCR_NORML_ATTR | SCR_BOLD_CHAR);
    if (addr >= 0)
	stack[addr] = GetCh();
    else
	CHILD(modnow, DROP())->stack[-addr] = GetCh();
    if (oldwin)
	Win_Activate(oldwin);
}


HNDL(_WriteCh)
{
    short c;
    POP(c);
    if (output_to > 0 && output_to < 4)
    {
	WINPTR oldwin = Win_Current;
	if (output_to != 1)
	{
	    Win_Activate(IO_win);
	    Win_SetAttribute(SCR_NORML_ATTR);
	}
	if (c == (short)'\n' || c == (short)'\r')
	{
	    if (output_to != 1)
	    {
		Win_PutChar('\n');
		Win_PutChar('\r');
	    }
	    if (output_to != 2)
		fputc('\n', logfp);
	} else
	{
	    if (output_to != 1)
		Win_PutChar((uchar) c);
	    if (output_to != 2)
		fputc(c, logfp);
	}
	if (oldwin && output_to != 1)
	    Win_Activate(oldwin);
    }
}

HNDL(_WriteStr)
{
    short len = PARAM(), i = 0;
    char buff[128];
    while (i < len)
    {
	buff[i] = (char)stack[s - len + i + 1];
	i++;
    }
    buff[i] = 0;
    if (output_to > 1 && output_to < 4)
    {
	WINPTR oldwin = Win_Current;
	Win_Activate(IO_win);
	Win_SetAttribute(SCR_NORML_ATTR);
	Win_PutString(buff);
	if (oldwin)
	    Win_Activate(oldwin);
    }
    if (output_to == 1 || output_to == 3)
	fputs(buff, logfp);
    s -= len;			/* pop */
}

HNDL(_EndWrite)
{
    if (output_to == 2 || output_to == 3)
    {
	WINPTR oldwin = Win_Current;
	Win_Activate(IO_win);
	if (!buildBG)
	    (void)sleep((unsigned)1);
	if (oldwin)
	    Win_Activate(oldwin);
    }
}

/**********************/
/* Standard functions */
/**********************/

HNDL(_Abs)
{
    if (TOS < 0)
	TOS = -TOS;
}
HNDL(_Odd)
{
    TOS %= 2;
}
HNDL(_Sqr)
{
    TOS *= TOS;
}

/*************************/
/* Nonstandard Functions */
/*************************/

HNDL(_Random)
{
    TOS = buildBG ? (TOS / 2) : myrandom(TOS);
}
HNDL(_GlobalTime)
{
    PUSH((ushort) globaltime);
}
HNDL(_FireCount)
{
    PUSH(current_fire_count);
}

HNDL(_Erandom)
{
    short Min, Max, Mean;
    nPop(3, &Max, &Mean, &Min);
    if (!buildBG)
	PUSH(Dis_Exponential(Min, Max, Mean));
    else
	PUSH(Mean);
}

HNDL(_Prandom)
{
    short Min, Max, Mean;
    nPop(3, &Max, &Mean, &Min);
    if (!buildBG)
	PUSH(Dis_Poisson(Min, Max, Mean));
    else
	PUSH(Mean);
}

HNDL(_Grandom)
{
    short Min, Max, Mean;
    nPop(3, &Max, &Mean, &Min);
    if (!buildBG)
	PUSH(Dis_Geometric(Min, Max, Mean));
    else
	PUSH(Mean);
}

HNDL(_QLength)
{
    /*
     * stack has -> ext ip index ext ip ident ...
     */
    short parentIP, parentIPident;
    nPop(2, &parentIP, &parentIPident);
    PUSH(Ip(modnow, parentIP)->current_qlength);
}

/********************/
/* Channel handling */
/********************/

static void 
get_IP_name(MODULE * m, short idx, IP * iptr)
{
    MODULE *oldm = modnow;
    activate_module(m);
    get_symbolic_name(iptr->ident, idx, iptr->name, NULL, 20);
    activate_module(oldm);
}

HNDL(_Attach)
{
    /*
     * stack has ->	child ip index child ip ident child index ext ip
     * index ext ip ident ...
     */
    short childIP, child, parentIP, childIPident, parentIPident;
    IP *parentIP_ptr, *childIP_ptr;
    nPop(5, &childIP, &childIPident, &child, &parentIP, &parentIPident);
    parentIP_ptr = Ip(modnow, parentIP);
    childIP_ptr = Ip(CHILD(modnow, child), childIP);
    if (parentIP_ptr->inIP || childIP_ptr->outIP)
	interp_error(ERR_BADATTACH);
    parentIP_ptr->inIP = childIP_ptr;
    childIP_ptr->outIP = parentIP_ptr;
    parentIP_ptr->ident = parentIPident;
    childIP_ptr->ident = childIPident;
    /* Get symbolic names */
    get_IP_name(modnow, parentIP, parentIP_ptr);
    get_IP_name(CHILD(modnow, child), childIP, childIP_ptr);

    /*
     * prepend queue from extIP onto bottom queue, renaming destinations...
     */
    move_interactions(parentIP_ptr, downattach(childIP_ptr));
}

static void NEAR 
BreakAttachment(IP * ip_ptr, ushort mode)
{
    IP *childIP_ptr, *parentIP_ptr;
    if (mode != 0)
    {
	childIP_ptr = ip_ptr;
	parentIP_ptr = ip_ptr->outIP;
    } else
    {
	parentIP_ptr = ip_ptr;
	childIP_ptr = ip_ptr->inIP;
    }
    parentIP_ptr->inIP = childIP_ptr->outIP = NULL;
    move_interactions(downattach(childIP_ptr), parentIP_ptr);
}

static void NEAR 
Separate_IPs(LocNearFn f)
{
    ushort mode = PARAM(), ip = 0;
    MODULE *m = modnow;
    if (mode != 2)
	POP(ip);
    if (mode != 0)
	m = CHILD(m, DROP());
    if (mode != 2)
    {
	(void)DROP();		/* drop ip ident */
	(*f) (Ip(m, ip), mode);
    } else
	for (ip = 0; ip < m->numextIPs; ip++)
	    (*f) (Ip(m, ip), mode);
}

HNDL(_Detach)
{
    Separate_IPs(BreakAttachment);
}

HNDL(_DefIPs)
{
    (void)PARAM();
    (void)PARAM();
}

HNDL(_Connect)
/* stack has:
		->connectipstuff
		  connectipstuff
		  ...
  where connectipstuff is:
	  	->ip index
		  ip ident
		  [ip module index if not internal]
*/
{
    short internal1, internal2, ip1, ip2, ipid1, ipid2;
    MODULE *m1, *m2;
    IP *ip_ptr1, *ip_ptr2;

    m1 = m2 = modnow;
    get_params(2, &p, &internal1, &internal2);
    POP(ip1);
    POP(ipid1);
    if (!internal1)
	m1 = CHILD(m1, DROP());
    POP(ip2);
    POP(ipid2);
    if (!internal2)
	m2 = CHILD(m2, DROP());
    ip_ptr1 = Ip(m1, ip1);
    ip_ptr2 = Ip(m2, ip2);

    if (ip_ptr1->outIP || ip_ptr2->outIP)
	interp_error(ERR_BADCONNECT);

    ip_ptr1->outIP = ip_ptr2;
    ip_ptr2->outIP = ip_ptr1;
    ip_ptr1->ident = ipid1;
    ip_ptr2->ident = ipid2;
    /* Get symbolic names */
    get_IP_name(m1, ip1, ip_ptr1);
    get_IP_name(m2, ip2, ip_ptr2);
    ip_ptr1->connectpoint = ip_ptr2->connectpoint = TRUE;
}

static void NEAR 
break_connect(IP * ip, ushort mode)
{
    if (ip && ip->connectpoint && ip->outIP)
    {
	IP *ip2 = ip->outIP;
	ip->outIP = ip2->outIP = NULL;
	ip->connectpoint = ip2->connectpoint = FALSE;
    }
    /* Just prevents the compiler warning about mode not being used */
    mode--;
}

HNDL(_Disconnect)
{
    Separate_IPs(break_connect);
}

HNDL(_Output)
{
    /*
     * Stack should be: s->	arg ... arg ip_offset ip_ident
     */
    short i, arg_len, reliability, priority, propdelay, Qtype;
    short *ip_args, ip_offset;
    INTERACTION *intr;
    BOOLEAN lose_it = FALSE;
    intr = (INTERACTION *) Mem_Calloc(1, INTERACTION_SIZE, 2);
    if (intr == NULL)
	interp_error(ERR_INTRALLOCFAIL);
    get_params(7, &p, &intr->ident, &intr->len, &arg_len, &reliability,
	       &priority, &propdelay, &Qtype);
    if (arg_len)
    {
	intr->argptr = ip_args = (short *)Mem_Calloc(arg_len, sizeof(short), 3);
	if (ip_args == NULL)
	    interp_error(ERR_INTRALLOCFAIL);
	while (arg_len--)
	    POP(ip_args[arg_len]);
    } else
	intr->argptr = NULL;
    intr->tnum = GlobalTrans;
    nPop(2, &ip_offset, NULL);
    if (!buildBG)
	check_breakpoints(BRK_IP, ip_offset);
    /*
     * If we are doing path expansion, we may have to fork. We certainly
     * don't need to do trace info, etc.
     */
    if (buildBG)
    {
	if (reliability != 100)
	{
	    lose_it = output_branch(&pathabort);
	    addLoss(ExecutingTrans->metaidx);
	}
    } else
    {
	lose_it = (BOOLEAN) (myrandom((short)100) >= reliability);
	/* Add entry to module's table */
	/*
	 * First check if an (IP,interaction) entry exists with intr->ident
	 * and ip_offset
	 */
	i = 0;
	if (intr->ident)
	    while (i < IP_INT_MAX)
	    {
		if (modnow->IP_Int_Tbl[i].ident == 0)
		{
		    /* Make a new entry */
		    modnow->IP_Int_Tbl[i].ident = intr->ident;
		    modnow->IP_Int_Tbl[i].ip = ip_offset;
		}
		if (modnow->IP_Int_Tbl[i].ident == intr->ident &&
		    modnow->IP_Int_Tbl[i].ip == ip_offset)
		{
		    if (lose_it)
			modnow->IP_Int_Tbl[i].lost++;
		    else
			modnow->IP_Int_Tbl[i].successful++;
		    break;
		}
		i++;
	    }
	if (i >= IP_INT_MAX) /* Warning!! */ ;

	if (TRON)
	{
	    char int_name[40], modvar_name[22];
	    Get_ModvarName(modnow, modvar_name);
	    get_symbolic_name(intr->ident, 0, int_name, NULL, 40);
	    fprintf(logfp, "####### interaction '%s' queued %s by %s\n", int_name,
		    lose_it ? "and lost" : "", modvar_name);
	}
	if (trace)
	{
	    char int_name[TRACE_COL_WIDTH];
	    if (titledue)
	    {
		fprintf(trace, trace_title, globaltime);
		titledue = FALSE;
	    }
	    fprintf(trace, "%*c", modnow->columnpos, ' ');
	    get_symbolic_name(intr->ident, 0, int_name, NULL, TRACE_COL_WIDTH);
	    fprintf(trace, "%s\n", int_name);
	}
    }
    if (ExecutingTrans->out_ip == -1)
    {
	ExecutingTrans->out_ip = ip_offset;
    }
    if (!lose_it)
    {
	enqueue_interaction(modnow, ip_offset, intr,
			    priority, propdelay, Qtype);
	outputLoss = (reliability == 100) ? 1 : 2;
	return;
    } else
	outputLoss = 3;
    Mem_Free(intr->argptr);
    Mem_Free(intr);
}

HNDL(_When)
/*
	Expects on stack:     -> IP#

	Leaves result on stack:  0 :  When fails
				>0 :  When succeeds, value is IP#+1

	The `len' parameter is not used.
*/
{
    short len, ident, offset;
    INTERACTION *head;
    IP *ip_ptr;
    get_params(2, &p, &ident, &len);
    POP(offset);
    ip_ptr = Ip(modnow, offset);
    head = ip_ptr->queue->first;
    if (head != NULL && head->ident == ident && head->destIP == ip_ptr
	&& head->time <= globaltime)
	PUSH(offset + 1);
    else
	PUSH((ushort) FALSE);
    if (head != NULL && !fullspeed)
	Note_When_Info(ident, head->ident, head->destIP, ip_ptr, offset);
}

/*************************/
/* Transitions & Modules */
/*************************/

HNDL(_Moduleassign)
{
    /*
     * Ideally for both this and INIT, if the dest is not NULL, check if
     * other modvars refer to it, and if not, release it properly. This would
     * actually contradict the Estelle standard, so another alternative is to
     * put all such `lost' processes on a linked list for freeing upon exit
     * from the PEW. For now, we'll leave things as they stand...
     */
    short source, destident, dest;
    nPop(3, &source, &destident, &dest);
    CHILD(modnow, dest) = CHILD(modnow, source);
    modnow->modvar_tbl[dest].varident = destident;
}


static void NEAR 
ClearEqualProcesses(MODULE * parent, ushort child)
{
    ushort c = parent->nummodvars;
    MODULE *tmp_ch = CHILD(parent, child);
    while (c--)
	if (CHILD(parent, c) == tmp_ch)
	    if (c != child)
		CHILD(parent, c) = NULL;
}

void 
ReleaseProcess(MODULE * process_ptr)
{
    /*
     * Detach / disconnect all extIPs of the module and its descendants and
     * free everything
     */
    extern void analyze_update();
    ushort child;
    EventEntry *e;
    if (process_ptr)
    {
	ushort ip;
	short i;
	transition_table tptr = process_ptr->trans_tbl;
	child = process_ptr->nummodvars;
	/* Recursively release children */
	while (child--)
	{
	    ReleaseProcess(CHILD(process_ptr, child));
	    ClearEqualProcesses(process_ptr, child);
	}
	Mem_Free(process_ptr->stack);
	i = process_ptr->numtrans;
	while (i--)
	{
	    if (tptr->transname != -1)	/* named transition? */
		analyze_update(tptr);	/* Update stats */
	    if (tptr->meta_trans_tbl)
		Mem_Free(tptr->meta_trans_tbl);
	    if (tptr->meta_delay_tbl)
		Mem_Free(tptr->meta_delay_tbl);
	    if (tptr->meta_min_tbl)
		Mem_Free(tptr->meta_min_tbl);
	    if (tptr->meta_max_tbl)
		Mem_Free(tptr->meta_max_tbl);
	    tptr++;
	}
	Mem_Free(process_ptr->trans_tbl);
	Mem_Free(process_ptr->modvar_tbl);
	ip = process_ptr->numIPs;
	while (ip--)
	{
	    INTERACTION *intr, *prev;
	    IP *ip_ptr;
	    ip_ptr = Ip(process_ptr, ip);
	    intr = prev = ip_ptr->queue->first;
	    while (intr)
	    {
		intr = intr->next;
		Mem_Free(prev->argptr);
		Mem_Free(prev);
		prev = intr;
	    }
	    if (ip_ptr->discipline == INDIVIDUALQ)
		Mem_Free(ip_ptr->queue);
	}
	Mem_Free(process_ptr->IP_tbl);
	Mem_Free(process_ptr->commonq);
	e = process_ptr->classify;
	while (e)
	{
	    EventEntry *olde = e;
	    e = e->next;
	    Mem_Free(olde);
	}
	Mem_Free(process_ptr);
    }
}

void 
ReleaseProcesses(void)
{
    ReleaseProcess(root);
    root = NULL;
}

static void NEAR 
_Kill(BOOLEAN simpledetach)
/* Must delete all breakpoints that refer to this process only */
{
    ushort child, ip;
    MODULE *child_ptr;
    POP(child);
    child_ptr = CHILD(modnow, child);
    fprintf(logfp, "\n\nProcess released!! \n\n");
    dump_stats(child_ptr, 0);
    for (ip = 0; ip < child_ptr->numextIPs; ip++)
    {
	IP *ip_ptr = Ip(child_ptr, ip);
	if (ip_ptr->connectpoint)
	    break_connect(ip_ptr, 0);
	else if (ip_ptr->outIP)
	{
	    /* detach ... */
	    if (!simpledetach)
		move_interactions(downattach(ip_ptr), ip_ptr->outIP);
	    ip_ptr->outIP->inIP = NULL;
	}
    }
    ReleaseProcess(child_ptr);
    ClearEqualProcesses(modnow, child);	/* For modvar assignments */
    refreshallwindows = TRUE;
}

HNDL(_Release)
{
    _Kill(FALSE);
}

HNDL(_Terminate)
{
    _Kill(TRUE);
}

HNDL(_Delay)
{
    short Min, Max, DelayTime, Type;
    /* timestamp is the time at which this transition became enabled */
    ulong enabled_time;
    nPop(2, &Max, &Min);
    Type = PARAM();
    (void)PARAM();		/* Expect is not used! */
    if (timestamp == 0l)
    {				/* Not enabled */
	PUSH(0);
	DelayTime = Min;
	enabled_time = 0l;
    } else
    {
	enabled_time = globaltime - timestamp;
	if (enabled_time < (ulong) Min)
	{
	    /* Enabled, but not delayed enough yet */
	    PUSH(0);
	    DelayTime = Min;
	} else
	{
	    short FiringOdds = 1;	/* Forces a PUSH(1) */
	    if (Max == -1)	/* No upper bound */
		Max = FiringOdds = 32767;
	    else if (enabled_time < (ulong) Max)
		FiringOdds = Max - Min + 1;
	    if (Type == 0 || FiringOdds == 1)	/* Uniform distribution or
						 * must fire */
		DelayTime = Min + (buildBG ? FiringOdds / 2 : myrandom(FiringOdds));
	    else if (Type == 1)	/* Poisson distribution */
		DelayTime = buildBG ? -Type : Dis_Poisson(Min, Max, -Type);
	    else if (Type == 2)	/* Exponential distribution */
		DelayTime = buildBG ? Type : Dis_Exponential(Min, Max, Type);
	    else		/* Geometric */
		DelayTime = buildBG ? Type : Dis_Geometric(Min, Max, Type);
	    PUSH((ushort) (enabled_time >= DelayTime));
	}
    }
    /*
     * Keep track of minimum delay, so that we can advance globaltime by this
     * much when no non-delayed clauses can fire.
     */
    DelayTime -= (short)enabled_time;	/* How much to still delay */
    if (DelayTime > 0)
    {
	if (min_delay_time == 0 || min_delay_time > (ushort) DelayTime)
	    min_delay_time = (ushort) DelayTime;
    }
#if 0
    else
    {				/* Backwardly correct the time if possible */
	/*
	 * This is necessary to prevent, for example, an emphasis on longer
	 * delays in the so-called 'uniform distribution'. It is a hack, but
	 * it works!
	 */
	ushort tmp = -DelayTime;/* Get around the signed/unsigned mix */
	tmp /= 2;
	if ((globaltime - tmp) > last_globaltime)
	    globaltime -= tmp;	/* Go back */
	else if (globaltime > last_globaltime)
	    globaltime = last_globaltime + 1;	/* Go back as far as allowed */
    }
#endif
}

HNDL(_Init)
{
    ushort paramlen, exvarlen;
    short displ, index, modvarident, tmp;

    get_params(3, &p, &displ, &paramlen, &exvarlen);
    tmp = s - (short)paramlen;
    index = stack[tmp - 1];	/* modvar index */
    modvarident = stack[tmp];
    CHILD(modnow, index) = (MODULE *) Mem_Calloc(1, sizeof(MODULE), 4);
    if (CHILD(modnow, index) == NULL)
	interp_error(ERR_INITFAIL);
    modnow->modvar_tbl[index].varident = modvarident;
    _Module(CHILD(modnow, index), index, p + displ - Op_NArgs[(short)OP_INIT],
	    exvarlen, paramlen, &stack[tmp + 1], modvarident);
    s = tmp - 2;		/* pop parameters, index, ident !!!! - check! */
    refreshallwindows = TRUE;
}


HNDL(_NumChildren)
{
    PUSH((short)modnow->nummodvars - 1);
}


HNDL(_Otherwise)
{
    short first, last;
    register TRANS *tptr;
    BOOLEAN succeed = TRUE;
    first = PARAM();
    last = PARAM();
    tptr = modnow->trans_tbl + first;
    while (first <= last)
    {
	if (Bit_Set(tptr->flags, HAS_PROV))
	    if (Bit_Set(tptr->flags, PROV_OK))
	    {
		succeed = FALSE;
		break;
	    }
	tptr++;
	first++;
    }
    PUSH((ushort) succeed);
}

/* EXIST, FORALL, FORONE */

HNDL(_Domain)
{
    short numvars = PARAM();
    s += numvars;
    nPush(3, b, b, 0);		/* Push an activation record */
    b = s - 2;
}

HNDL(_ExistMod)
{
    short headident, faildispl;
    headident = PARAM();
    faildispl = PARAM();
    if (CHILD(modnow, stack[b - 1])->headident != headident)
	p += faildispl - Op_NArgs[(short)OP_EXISTMOD] - 1;
}

HNDL(_EndDomain)
{
    short numvars = PARAM(), retval;
    retval = TOS;
    s = b + 2;
    nPop(3, NULL, &b, NULL);	/* pop activation record */
    s -= numvars;
    PUSH(retval);
}

/*************/
/* Unknown?? */
/*************/

HNDL(_SetOutput)
{
    output_to = PARAM();
}

HNDL(_Copy)
{
    short v = TOS;
    PUSH(v);
}

HNDL(_Pop)
{
    short num = PARAM();
    s -= num;
}

/*****************/
/* Optimisations */
/*****************/

HNDL(_Globalcall)
{
    short displ, rtn_len;
    get_params(2, &p, &displ, &rtn_len);
    s += rtn_len;
    nPush(3, stack[b], b, p);	/* Push activation record */
    b = s - 2;
    p += displ - 3;
    resync_sourceline(p + 1);
}


HNDL(_Simplevalue)
{
    short addr = DROP(), *stk;
    if (addr >= 0)
	stk = stack;
    else
    {
	addr = -addr;
	stk = CHILD(modnow, DROP())->stack;
    }
    PUSH(stk[addr]);
}

HNDL(_Globalvar)
{
    PUSH(stack[b] + PARAM());
}

HNDL(_Globalvalue)
{
    _Globalvar();
    _Simplevalue();
}

HNDL(_Localvar)
{
    PUSH(b + PARAM());
}

HNDL(_Localvalue)
{
    _Localvar();
    _Simplevalue();
}

HNDL(_Simpleassign)
{
    short valpos = s--, addr, *deststk;
    POP(addr);
    if (addr >= 0)
	deststk = stack;
    else
    {
	addr = -addr;
	deststk = CHILD(modnow, DROP())->stack;
    }
    deststk[addr] = stack[valpos];
}

/***************************************************************************/

HNDL(_Newline)
{
    p++;
}

static void 
NEAR(*op_func[]) (void)=
{
     /* ADD	       0*/ _Add,
     /* ADDRESS	*/ NULL,
     /* ADDSETELEMENT*/ _Addsetelement,
     /* ADDSETRANGE	*/ _Addsetrange,
     /* AND		*/ _And,
     /* ASSIGN	*/ _Assign,
     /* ATTACH	*/ _Attach,
     /* CASE		*/ _Case,
     /* CONNECT	*/ _Connect,
     /* CONSTANT	*/ NULL,
     /* COPY	      10*/ _Copy,
     /* DETACH	*/ _Detach,
     /* DIFFERENCE	*/ _Difference,
     /* DISCONNECT	*/ _Disconnect,
     /* DIV		*/ _Div,
     /* DO		*/ _Do,
     /* ENDPROC	*/ _EndProc,
     /* ENDTRANS     */ NULL,
     /* EQUAL	*/ _Equal,
     /* FIELD	*/ _Field,
     /* FOR	      20*/ _For,
     /* GOTO		*/ _Goto,
     /* GREATER	*/ _Greater,
     /* IN		*/ _In,
     /* INTERSECTION	*/ _Intersection,
     /* INDEX	*/ _Index,
     /* INDEXEDJUMP	*/ _Indexedjump,
     /* INIT		*/ _Init,
     /* LESS		*/ _Less,
     /* MINUS	*/ _Minus,
     /* MODULEASSIGN30*/ _Moduleassign,
     /* MODULO	*/ _Modulo,
     /* MULTIPLY   	*/ _Multiply,
     /* NEWLINE	*/ _Newline,	/* dummy function for graph building mode */
     /* NEXT		*/ _Next,
     /* NEXTINSTANCE	*/ NULL,
     /* NOT		*/ _Not,
     /* NOTEQUAL	*/ _Notequal,
     /* NOTGREATER	*/ _Notgreater,
     /* NOTLESS	*/ _Notless,
     /* OR	      40*/ _Or,
     /* OUTPUT	*/ _Output,
     /* POP	      	*/ _Pop,
     /* PROCCALL	*/ _Proccall,
     /* PROCEDURE	*/ _Procedure,
     /* RANGE	*/ _Range,
     /* RELEASE	*/ _Release,
     /* SETASSIGN	*/ _Setassign,
     /* SETINCLUSION	*/ _Setinclusion,
     /* SETINCLUSIONTOO*/ _Setinclusiontoo,
     /* MODULE     50*/ NULL,
     /* STRINGCOMPARE*/ NULL,
     /* TRANS	*/ NULL,
     /* UNION	*/ _Union,
     /* VALUE	*/ _Value,
     /* VARIABLE	*/ _Variable,
     /* VARPARAM	*/ _Varparam,
     /* ABS		*/ _Abs,
     /* DISPOSE    	*/ _Dispose,
     /* NEW		*/ _New,
     /* ODD	      60*/ _Odd,
     /* READ		*/ _ReadInt,
     /* SQR		*/ _Sqr,
     /* SUBTRACT	*/ _Subtract,
     /* WRITE	*/ _WriteInt,
     /* DEFADDR	*/ NULL,
     /* DEFARG	*/ NULL,
     /* GLOBALCALL	*/ _Globalcall,
     /* GLOBALVALUE	*/ _Globalvalue,
     /* GLOBALVAR	*/ _Globalvar,
     /* LOCALVALUE 70*/ _Localvalue,
     /* LOCALVAR	*/ _Localvar,
     /* SIMPLEASSIGN	*/ _Simpleassign,
     /* SIMPLEVALUE	*/ _Simplevalue,
     /* STRINGEQUAL	*/ _Stringequal,
     /* STRINGLESS	*/ _Stringless,
     /* STRINGGREATER*/ _Stringgreater,
     /* STRINGNOTEQUAL*/ _Stringnotequal,
     /* STRINGNOTLESS*/ _Stringnotless,
     /* STRINGNOTGREATER*/ _Stringnotgreater,
     /* ENTERBLOCK 80*/ _EnterBlock,
     /* EXITBLOCK	*/ NULL,
     /* ENDCLAUSE	*/ NULL,
     /* DELAY	*/ _Delay,
     /* WHEN		*/ _When,
     /* RETURNVAR	*/ _ReturnVar,
     /* RANDOM	*/ _Random,
     /* READCH	*/ _ReadCh,
     /* WRITECH	*/ _WriteCh,
     /* WRITESTR	*/ _WriteStr,
     /* SETEQUAL   90*/ _Setequal,
     /* READSTR	*/ _ReadStr,
     /* DEFIPS	*/ _DefIPs,
     /* SCOPE	*/ _Scope,
     /* EXPVAR	*/ _ExpVar,
     /* WITH		*/ _With,
     /* WITHFIELD	*/ _WithField,
     /* ENDWITH	*/ _EndWith,
     /* ENDWRITE	*/ _EndWrite,
     /* DOMAIN	*/ _Domain,
     /* EXISTMOD	*/ _ExistMod,
     /* ENDDOMAIN	*/ _EndDomain,
     /* NUMCHILDREN	*/ _NumChildren,
     /* OTHERWISE	*/ _Otherwise,
     /* FIRECOUNT	*/ _FireCount,
     /* GLOBALTIME	*/ _GlobalTime,
     /* QLENGTH	*/ _QLength,
     /* SETOUTPUT	*/ _SetOutput,
     /* TERMINATE	*/ _Terminate,
     /* ERANDOM	*/ _Erandom,
     /* PRANDOM	*/ _Prandom,
     /* GRANDOM	*/ _Grandom
};

/***************************************/
/* The complex high-level instructions */
/***************************************/

static short NEAR 
exec_block(short address)
{
    register ecode_op op;
    p = address;
    resync_sourceline(p);
    while (op = code[p], op != OP_ENDCLAUSE && op != OP_ENDTRANS)
    {
	if (op == OP_CONSTANT)
	{
	    /*
	     * Profiling shows that more than 50% of calls to exec_op are for
	     * constants, so we do them inline.
	     */
	    PUSH(PARAM());
	    ++p;
	} else if (buildBG)
	{
	    /* If building graph we assume validity of instruction for speed */
	    (*op_func[(short)op]) ();
	    ++p;
	} else if (op == OP_NEWLINE)
	{
	    actual_line[(short)SOURCE_WIN] = run_sourceline = (ushort) ((short)code[p + 1] - 1);
	    check_breakpoints(BRK_LINE, (short)run_sourceline);
	    p += 2;
	    if (IS_AUTO)
	    {
		resync_display(FALSE);
		delay(animate_delay);
	    } else if (IS_STEP && !ecode_debugging)
		get_user_cmd(modnow);
#ifdef UNIX
	} else if (op < NUM_OPS && op_func[(int)op] != NULL)
	{
#else
	} else if (op >= (ecode_op) 0 && op < NUM_OPS && op_func[(int)op] != NULL)
	{
#endif
	    if (IS_STEP && ecode_debugging)
		get_user_cmd(modnow);
	    (*op_func[(short)op]) ();
	    ++p;
	} else
	    interp_error(ERR_ILLEGALOP, op, p);
	if (!buildBG && Kb_RawLookC(NULL))
	    get_user_cmd(modnow);
	if (pathabort)
	    return 0;
    }
    return (short)(op == OP_ENDCLAUSE ? DROP() : 0);
}


/**************************************************/

static short NEAR 
exec_clause(short address, short arg_offset, TRANS * te, unsigned OK_flag)
{
    short rtn;
    timestamp = te->timestamp;
    rtn = exec_block(address + (short)code[address + arg_offset]);
    if (rtn)
	Set_Bit(te->flags, OK_flag);
    return rtn;
}


/* Add an entry to the module classification table */

void 
AddClass(MODULE * mod, BOOLEAN is_reception, short trans, short ID)
{
    EventEntry *e = mod->classify;
    /* Search for event */
    if (e)
	while (e)
	{
	    if (e->msgident == ID && e->reception == is_reception)
		break;
	    e = e->next;
	}
    if (e == NULL)
    {
	/* Make a new entry */
	EventEntry *e2 = mod->classify;
	e = (EventEntry *) Mem_Calloc(1, sizeof(EventEntry), 70);
	if (e2 == NULL)		/* Create a new list for the process */
	    mod->classify = e;
	else
	{			/* Add on end of list */
	    while (e2->next)
		e2 = e2->next;
	    e2->next = e;
	}
	e->next = NULL;
	e->msgident = ID;
	e->reception = is_reception;
    }
    /* Mark the transition as being associated with the event */
    e->transvect[trans / 8] |= (1 << (trans % 8));
}

void 
AddWhenClass(MODULE * mod, short trans, short addr)
{
    addr = skipto(OP_WHEN, addr + code[addr + TR_WHEN]);
    AddClass(mod, TRUE, trans, code[addr + 1]);
}

static char *
showFlags(int f)
{
    static char F[5];
    F[0] = (f & HAS_WHEN) ? ((f & WHEN_OK) ? 'W' : 'w') : '-';
    F[1] = (f & HAS_PROV) ? ((f & PROV_OK) ? 'P' : 'p') : '-';
    F[2] = (f & HAS_DELAY) ? ((f & DELAY_OK) ? 'D' : 'd') : '-';
    F[3] = (f & HAS_FROM) ? ((f & FROM_OK) ? 'F' : 'f') : '-';
    F[4] = '\0';
    return F;
}

static char *
diffFlags(int oldf, int newf)
{
    static char F[5];
    int i = 0;
    if ((oldf & (HAS_WHEN | WHEN_OK)) != (newf & (HAS_WHEN | WHEN_OK)))
	F[i++] = (newf & WHEN_OK) ? 'W' : 'w';
    if ((oldf & (HAS_PROV | PROV_OK)) != (newf & (HAS_PROV | PROV_OK)))
	F[i++] = (newf & PROV_OK) ? 'P' : 'p';
    if ((oldf & (HAS_DELAY | DELAY_OK)) != (newf & (HAS_DELAY | DELAY_OK)))
	F[i++] = (newf & DELAY_OK) ? 'D' : 'd';
    if ((oldf & (HAS_FROM | FROM_OK)) != (newf & (HAS_FROM | FROM_OK)))
	F[i++] = (newf & FROM_OK) ? 'F' : 'f';
    F[i] = '\0';
    return F;
}

static char *
unkillFlags(int oldf, int newf)
{
    static char F[5];
    int i = 0;
    if ((oldf & WHEN_OK) && (newf & WHEN_OK))
	F[i++] = 'W';
    if ((oldf & PROV_OK) && (newf & PROV_OK))
	F[i++] = 'P';
    if ((oldf & DELAY_OK) && (newf & DELAY_OK))
	F[i++] = 'D';
    if ((oldf & FROM_OK) && (newf & FROM_OK))
	F[i++] = 'F';
    F[i] = '\0';
    return F;
}

static ushort NEAR 
eval_clauses(short addr, ushort count, BOOLEAN initialisation)
{
    register TRANS *tptr = modnow->trans_tbl;
    BOOLEAN enabled, just_wait;
    ushort trans_now, enbl_cnt = 0;
    chktrans_num = 0;

    for (trans_now = 0; trans_now < count; trans_now++)
    {
	short ip_offset = 0;
	short intarg_len, i, stack_free, stack_needed;
	ushort last_min_delay = min_delay_time;
	DISABLE_ALL(tptr);	/* clear all clause flags */
	if (tptr->addr == 0)
	    tptr->addr = skipto(OP_TRANS, addr);
	addr = tptr->addr;
	stack_free = modnow->stacksize - modnow->heapbot - s;
	stack_needed = (short)code[addr + TR_WHENLEN] + (short)code[addr + TR_VARS]
	    + (short)code[TR_TMPS];
	if (stack_needed > stack_free)
	    expand_stack(modnow, modnow->stacksize + stack_needed - stack_free);

	/* If first time, get static info */

	if (Bit_Clear(tptr->flags, NOT_FIRST))
	{
	    BOOLEAN from_clause = FALSE;
	    short tmpaddr;
	    for (i = 0; i < 4; i++)
		if ((short)code[addr + TR_FROMSTATES + i] != -1)
		    from_clause = TRUE;
	    if (from_clause)
		Set_Bit(tptr->flags, HAS_FROM);
	    tptr->transname = (short)code[addr + TR_TRANSNAME];
	    tptr->priority = (short)code[addr + TR_PRIORITY];
	    tptr->fromident = (short)code[addr + TR_FROMIDENT];
	    tptr->toident = (short)code[addr + TR_TOIDENT];
	    tptr->out_ip = -1;
	    /*
	     * If TO part is SAME, use FROM id; but if this is not unique (ie
	     * +ve), leave toident=0, so that we don't use it as the
	     * state_ident
	     */
	    if (tptr->toident == 0)
	    {
		tptr->toident = tptr->fromident;
		if (tptr->fromident < -1)
		    tptr->toident = 0;
	    }
	    if (tptr->fromident < -1)
		tptr->fromident = -tptr->fromident;
	    if ((short)code[addr + TR_WHEN] != -1)
	    {
		Set_Bit(tptr->flags, HAS_WHEN);
		if (!buildBG && !InInit)
		    AddWhenClass(modnow, trans_now, addr);
	    } else
		Clear_Bit(tptr->flags, HAS_WHEN);
	    if ((short)code[addr + TR_PROV] != -1)
		Set_Bit(tptr->flags, HAS_PROV);
	    else
		Clear_Bit(tptr->flags, HAS_PROV);
	    if ((short)code[addr + TR_DELAY] != -1)
		Set_Bit(tptr->flags, HAS_DELAY);
	    else
		Clear_Bit(tptr->flags, HAS_DELAY);
	    Set_Bit(tptr->flags, NOT_FIRST);
	    tmpaddr = (short)(addr + (short)code[addr + TR_BLOCK]);
	    tmpaddr = skipto(OP_NEWLINE, tmpaddr);
	    tptr->startline = (short)code[tmpaddr + 1] - 2;
	    tptr->breakpoint = (ushort) - 1;
	    if (modnow->numtrans && InInit == 0)
	    {
		short i = modnow->numtrans;
		tptr->meta_trans_tbl = (short *)Mem_Calloc(i, sizeof(short), 5);
		tptr->meta_delay_tbl = (short *)Mem_Calloc(i, sizeof(short), 6);
		tptr->meta_max_tbl = (short *)Mem_Calloc(i, sizeof(short), 6);
		tptr->meta_min_tbl = (short *)Mem_Calloc(i, sizeof(short), 6);
		if (tptr->meta_trans_tbl == NULL || tptr->meta_delay_tbl == NULL ||
		    tptr->meta_min_tbl == NULL || tptr->meta_max_tbl == NULL)
		    interp_error(ERR_METATRANS);
		while (--i)
		    tptr->meta_min_tbl[i] = 0x7FFF;

	    }
	    /* Add output classes */
	    tmpaddr = (short)(addr + (short)code[addr + TR_BLOCK]);
	    if (!buildBG && !InInit)
		while (code[tmpaddr] != OP_ENDTRANS)
		{
		    if (code[tmpaddr] == OP_OUTPUT)
			AddClass(modnow, FALSE, trans_now, code[tmpaddr + 1]);
		    tmpaddr += Op_NArgs[(short)code[tmpaddr]] + 1;
		}
	}
	enabled = TRUE;
	just_wait = FALSE;
	/*
	 * If blocking is on, check that the IP referred to in out_ip has an
	 * empty queue at the other end
	 */
	if (blocking && (tptr->out_ip != -1))
	{
	    IP *tmp = Ip(modnow, tptr->out_ip);
	    tmp = downattach(upperattach(tmp)->outIP);
	    if (tmp->current_qlength != 0)
		enabled = FALSE;
	}
	/* Check From clause */

	if (Bit_Set(tptr->flags, HAS_FROM))
	{
	    short offset = (modnow->state - 1) / 16;
	    short bitpos = (modnow->state - 1) % 16;
	    if (((short)code[addr + TR_FROMSTATES + offset] & (1 << bitpos)) != 0)
		Set_Bit(tptr->flags, FROM_OK);
	    else
		enabled = FALSE;
	}
	/* Check when clause */

	intarg_len = 0;
	if (Bit_Set(tptr->flags, HAS_WHEN))
	{
	    ip_offset = exec_clause(addr, TR_WHEN, tptr, WHEN_OK);
	    if (ip_offset)
		intarg_len = copy_interaction(ip_offset - 1);
	    else
		enabled = FALSE;
	}
	nPush(3, b, b, 0);
	b = s - 2;		/* delay and provided are in the scope of the
				 * when clause, so make AR */
	if (Bit_Set(tptr->flags, HAS_DELAY))
	    if (buildBG || exec_clause(addr, TR_DELAY, tptr, DELAY_OK) == 0)
	    {
		just_wait = enabled;
		enabled = FALSE;
	    }
	if ((fullspeed == FALSE) || enabled || just_wait)
	    if (Bit_Set(tptr->flags, HAS_PROV))
		if (exec_clause(addr, TR_PROV, tptr, PROV_OK) == 0)
		    just_wait = enabled = FALSE;
	nPop(3, NULL, NULL, &b);
	s -= intarg_len;
	if (etrace && (just_wait ||
		       (Bit_Set(tptr->flags, HAS_DELAY) && enabled)))
	{
	    int i = tptr->metaidx;
	    delayIsEnbl[i / 8] |= 1 << (i % 8);
	}
	if (just_wait)
	    Set_Bit(tptr->flags, WAITING);
	else if (enabled)
	{
	    Set_Bit(tptr->flags, FIREABLE);
	    enbl_cnt++;
	    total_enabled++;
	    tptr->enbl_count++;
	    tptr->last_tm = globaltime;
	    tptr->ip = ip_offset - 1;
	    /* The minus is to distinguish from exec type */
	    if (!fullspeed && !buildBG)
		check_breakpoints(BRK_TRANS, -1 - (short)trans_now);
	}
	if (enabled || just_wait)
	{
	    /*
	     * The timestamp is the time at which the transition becomes
	     * enabled, ignoring its delay clauses
	     */
	    if (tptr->timestamp == 0)
	    {
		tptr->timestamp = globaltime;
		/*
		 * The trigger must be in the same module, as WHEN/DELAY are
		 * mutex. We assume no groups are being used
		 */
#if 0
		if (modnow->last_trans[0] == -1)	/* First time */
		    tptr->trigger = tptr->metaidx;
		else
		    tptr->trigger = modnow->trans_tbl->metaidx + modnow->last_trans[0];
#else
		if (ExecutingTrans == NULL)	/* First time */
		    tptr->trigger = -1 /* tptr->metaidx */ ;
		else
		    tptr->trigger = ExecutingTrans->metaidx;
#endif
	    }
	    if (buildBG && just_wait)
		addDelay(tptr->metaidx, tptr->trigger);
	} else
	{
	    /*
	     * Unfireable: clear timestamp and undo any changes to min delay
	     * time
	     */
	    if (tptr->timestamp && buildBG)
		if (Bit_Set(tptr->flags, HAS_DELAY))
		    addKiller(tptr->metaidx, ExecutingTrans ? ExecutingTrans->metaidx : -1);
	    tptr->timestamp = 0;
	    min_delay_time = last_min_delay;
	}
#define CHKFLAGS	(WHEN_OK|PROV_OK|DELAY_OK|FROM_OK|HAS_PROV|HAS_WHEN|HAS_DELAY|HAS_FROM)
	if (tptr->oldflags != (tptr->flags & CHKFLAGS))
	{
	    if (tptr->oldflags && ctrace)
	    {
		char modvar_name[22];
		fprintf(ctrace, "\t%-2d %4s ",
			tptr->metaidx,
			diffFlags(tptr->oldflags, tptr->flags & CHKFLAGS));
		fprintf(ctrace, "%4s (",
			unkillFlags(tptr->oldflags, tptr->flags & CHKFLAGS));
		Get_ModvarName(modnow, modvar_name);
		fprintf(ctrace, "%s:", showFlags(tptr->oldflags));
		fprintf(ctrace, "%s)\t%20s:%-2d\n",
			showFlags(tptr->flags & CHKFLAGS),
			modvar_name, trans_now);
	    }
	    tptr->oldflags = tptr->flags & CHKFLAGS;
	}
	addr += (short)code[addr + TR_NEXT];	/* skip to next TRANS */
	tptr++;
    }
    if (enbl_cnt == 0 && !fullspeed)
	CheckForBadInteractions();
    modnow->trans_initialised = initialisation;
    return enbl_cnt;
}


/********************************************************/

static short NEAR 
random_select(ushort enbl_cnt)
{
    short trans_now = 0, selected = 1;
    if (nondeterministic)
	selected += myrandom(((short)enbl_cnt));
    while (selected)
    {
	if (IS_FIREABLE((modnow->trans_tbl + trans_now)))
	    selected--;
	if (selected == 0)
	    break;
	trans_now++;
    }
    return trans_now;
}

/************************************************************/

static void NEAR 
exec_trans_block(short trans_num)
{
    short tmp = trans_num, addr, whichgrp = 99,	/* Which group does sequence
						 * fall in? */
        pop_len;		/* for popping interaction args */
    long wait_time;
    TRANS *tptr = modnow->trans_tbl + trans_num;
    LastExecutingTrans = ExecutingTrans;
    ExecutingTrans = tptr;
    GlobalTrans = modnow->trans_tbl->metaidx + trans_num;	/* Global index */
    Set_Bit(tptr->flags, SELECTED);
    Set_Bit(tptr->flags, EXECUTING);

    if (tptr->addr == 0)
	fprintf(stderr, "Error tptr->addr=0 in exec_trans_block\n");
    addr = tptr->addr;
    if (!buildBG)
	check_breakpoints(BRK_TRANS, trans_num);
    if (((short)code[addr + TR_WHEN]) != -1)
    {
	/*
	 * Update global sequence matrix using the trans index in the
	 * interaction
	 */
	short tnum = Ip(modnow, tptr->ip)->queue->first->tnum;
	if (GMT && !globalTRG)
	{
	    GMT[tnum][tptr->metaidx]++;
	    GMTdel[tnum][tptr->metaidx] += (float)(globaltime - Ip(modnow, tptr->ip)->queue->first->time);
	}
	/* Allocate space on stack, dequeue message and copy */
	tptr->when_cnt++;
	pop_len = dequeue_interaction(tptr->ip, FALSE);
    } else
	pop_len = 0;

    /*
     * Update global sequence matrix using the last local transition in same
     * group, if not first in sequence
     */

    if (GMT && !buildBG)
    {
	if (!globalTRG)
	{
	    short mi;
	    mi = modnow->trans_tbl->metaidx;	/* Global index of first
						 * trans */
	    whichgrp = (trans_num < modnow->tgroup) ? 0 : 1;
	    if (modnow->last_trans[whichgrp] >= 0)
	    {
		float tmp = (float)(globaltime - modnow->last_time[whichgrp]);
		GMT[mi + modnow->last_trans[whichgrp]][tptr->metaidx]++;
		if (trace)
		    fprintf(trace, "Adding %f to GMTdel[%d][%d]\n", tmp,
			  mi + modnow->last_trans[whichgrp], tptr->metaidx);
		GMTdel[mi + modnow->last_trans[whichgrp]][tptr->metaidx] += tmp;
	    }
	} else
	{
	    if (LastExecutingTrans)
	    {
		float tmp = (float)(globaltime - LastExecutingTrans->last_tm);
		GMT[LastExecutingTrans->metaidx][tptr->metaidx]++;
		if (trace)
		    fprintf(trace, "Adding %f to GMTdel[%d][%d]\n", tmp,
			    LastExecutingTrans->metaidx, tptr->metaidx);
		GMTdel[LastExecutingTrans->metaidx][tptr->metaidx] += tmp;
	    }
	}
    }
    if (((short)code[addr + TR_PROV]) != -1)
	tptr->prov_cnt++;

    nPush(3, b, b, 0);		/* Push activation record */
    b = s - 2;
    s += (short)code[addr + TR_VARS];	/* allocate variable space */
    tmp = (short)code[addr + TR_TMPS];
    if ((s + tmp) > modnow->stacksize)
	expand_stack(modnow, s + tmp);

    current_fire_count = ++tptr->fire_count;
    (void)exec_block(addr + (short)code[addr + TR_BLOCK]);
    if (pathabort)
	return;

    s -= (short)code[addr + TR_VARS];	/* deallocate variable space */
    nPop(3, NULL, NULL, &b);
    s -= pop_len;		/* pop interaction arguments from when clause */

    wait_time = (long)globaltime - (long)tptr->timestamp /* +1l */ ;
    if (wait_time < 0)
	wait_time = 0;		/* This is a hack to fix situation when
				 * globaltime is decreased */
    if (Bit_Set(tptr->flags, HAS_DELAY))
	addKiller(tptr->metaidx, tptr->metaidx);
    tptr->timestamp = 0;	/* clear enabled time */
    if (tptr->first_tm == 0)
	tptr->first_tm = globaltime;

    if ((short)code[addr + TR_TOSTATE] != -1)
    {				/* Not SAME */
	/*
	 * The +1 is necessary as every state must have a value that can be
	 * represented as an element of a state set
	 */
	short newstate = (short)code[addr + TR_TOSTATE] + 1;
	if (newstate != modnow->state)
	{
	    /*
	     * We initially set cycles to -1. If we have a state change for
	     * the first time, we set it to zero. Subsequently each time we
	     * go back to the start state, we increment cycles. Then we can
	     * check for cycle termination in the scheduler by checking for a
	     * value bigger than cycles, or for -1
	     */
	    if (newstate == modnow->first_state)
		modnow->cycles++;
	    else if (modnow->cycles == -1)
		modnow->cycles = 0;
	    modnow->state = newstate;
	}
    }
    if (modnow->state == PREINITIAL_STATE)
	modnow->state = IMPLICIT_STATE;
    if (tptr->toident)
	modnow->state_ident = tptr->toident;

    Clear_Bit(tptr->flags, EXECUTING);
    Set_Bit(tptr->flags, COMPLETED);
    /* NEW STUFF */
    /* Note transition flow statistics */
    if (tptr->lasttime)
	tptr->selfdelay += globaltime - tptr->lasttime;
    tptr->lasttime = globaltime;
    if ((whichgrp != 99) && !buildBG && (modnow->last_trans[whichgrp] != -1))
    {
	TRANS *tp2 = modnow->trans_tbl + modnow->last_trans[whichgrp];
	if (tp2->meta_delay_tbl && tp2->meta_trans_tbl)
	{
	    /* Update moving average */
	    ulong tmp;
	    short tm, cnt;
	    cnt = tp2->meta_trans_tbl[trans_num];
	    tmp = cnt * tp2->meta_delay_tbl[trans_num];
	    tmp += (tm = globaltime - modnow->last_time[whichgrp]);
	    tp2->nextdelay += tm;	/* forward time */
	    tptr->sumdelay += tm;	/* back time */
	    tmp /= ++cnt;
	    tp2->meta_trans_tbl[trans_num] = cnt;
	    tp2->meta_delay_tbl[trans_num] = (short)tmp;
	    if (tm > tp2->meta_max_tbl[trans_num])
		tp2->meta_max_tbl[trans_num] = tm;
	    if (tm < tp2->meta_min_tbl[trans_num])
		tp2->meta_min_tbl[trans_num] = tm;
	}
    }
    if (buildBG || trans_num < modnow->tgroup)
	whichgrp = 0;
    else
	whichgrp = 1;
    modnow->last_trans[whichgrp] = trans_num;
    modnow->last_time[whichgrp] = globaltime;
    /* END OF NEW STUFF */
}


void 
_Module(MODULE * mod, short index, short addr, ushort exvarlen, ushort paramlen, short *params, short modvarident)
{
    processclass class;
    short ipdef_lbl, scope_lbl, init_lbl, trans_lbl, i;
    ushort ip_total, intIPs, extIPs, initcount, transcount, vars, modvars;
    ushort tgroup;
    MODULE *parent = modnow;
    addr = skipto(OP_MODULE, addr);	/* get past EnterBlock */
    get_params(14, &addr, &mod->bodident, &mod->headident, &class, &vars,
	     &modvars, &initcount, &transcount, &intIPs, &extIPs, &init_lbl,
	       &trans_lbl, &ipdef_lbl, &scope_lbl, &tgroup);
    mod->parent = parent;
    mod->index = index;
    mod->classify = NULL;
    mod->tgroup = tgroup;
    for (i = 0; i < MAXTRANS / 8; i++)
	mod->other[i] = (uchar) 0;
    if (trace)
    {
	short i = columnpos;
	if (i)
	{
	    char *m;
	    char modvar_name[TRACE_COL_WIDTH];
	    short len = strlen(trace_title) - 1;
	    get_symbolic_name(modvarident, index, modvar_name, NULL, TRACE_COL_WIDTH - 1);
	    m = modvar_name;
	    while (*m && i < len)
		trace_title[i++] = *m++;
	}
	mod->columnpos = columnpos;
	columnpos += TRACE_COL_WIDTH;
    }
    i = (short)max(vars + paramlen + exvarlen + 16, STACKMAX);
    mod->stack = (short *)Mem_Calloc(i, sizeof(short), 5);
    if (mod->stack == NULL)
	interp_error(ERR_MODSTACK);
    mod->stacksize = i;
    if (modvars)
    {
	mod->modvar_tbl = (struct module_table_entry *)Mem_Calloc((short)modvars, sizeof(struct module_table_entry), 6);
	if (mod->modvar_tbl == NULL)
	    interp_error(ERR_MODVARTBL);
    }
    ip_total = mod->numIPs = intIPs + extIPs;
    mod->numextIPs = extIPs;
    if (ip_total)
    {
	mod->IP_tbl = (IP_table) Mem_Calloc((short)ip_total, IP_SIZE, 7);
	if (mod->IP_tbl == NULL)
	    interp_error(ERR_IPTBL);
	if (ipdef_lbl != -1)
	{			/* has common queues? */
	    struct queue_header *commonq;
	    mod->commonq = commonq = (struct queue_header *)Mem_Calloc(1, sizeof(struct queue_header), 8);
	    if (commonq == NULL)
		interp_error(ERR_COMMONQ);
	    ipdef_lbl += addr - Op_NArgs[(short)OP_MODULE];
	    while (code[ipdef_lbl] == OP_DEFIPS)
	    {
		short start, size;
		start = (short)code[ipdef_lbl + 1];
		size = (short)code[ipdef_lbl + 2];
		ipdef_lbl += 3;
		while (size--)
		    (mod->IP_tbl + start + size)->queue = commonq;
	    }
	} else
	    mod->commonq = NULL;
	while (ip_total--)
	{			/* allocate individual queues and set
				 * discipline */
	    IP *ip_ptr = Ip(mod, ip_total);
	    if (ip_ptr->queue)
		ip_ptr->discipline = COMMONQ;
	    else
	    {
		ip_ptr->queue = (struct queue_header *)Mem_Calloc(1, sizeof(struct queue_header), 9);
		if (ip_ptr->queue == NULL)
		    interp_error(ERR_QUEUE);
		ip_ptr->discipline = INDIVIDUALQ;
	    }
	    ip_ptr->owner = mod;/* Set owner */
	    ip_ptr->breakpoint = (ushort) - 1;
	}
    }
    if (scope_lbl != -1)
    {
	scope_lbl += addr - Op_NArgs[(short)OP_MODULE];
	mod->maxlevel = -1;
	while (code[scope_lbl] == OP_SCOPE)
	{
	    short level, offset;
	    level = (short)code[scope_lbl + 1];
	    offset = (short)code[scope_lbl + 2];
	    scope_lbl += 3;
	    if (level > mod->maxlevel)
		mod->maxlevel = level;
	    mod->scopetable[level] = offset;
	}
    }
    i = paramlen + 1;
    while (i-- > 1)
	mod->stack[exvarlen + i] = *params--;	/* get init params */
    i = paramlen + exvarlen + 1;/* Must start stack at 1, as we can't tell if
				 * address 0 is local or child-exported. */
    mod->s = 3 + (short)vars + i;	/* was 2; see above comment */
    mod->b = i;
    mod->stack[i] = i;		/* set stack[b] to point to itself */
    mod->p = addr;
    /* NEW STUFF */
    mod->last_trans[0] = mod->last_trans[1] = -1;	/* Last executed
							 * transition */
    /* END OF NEW STUFF */
    if (modnow == mod)
	p = addr;		/* Needed before calling activate_module, to
				 * prevent mod->p being set to old value of
				 * p. */
#ifdef ECODE_DEBUGGING
    mod->vars = vars;
    mod->params = paramlen;
    mod->exvars = exvarlen;
#endif

    /*
     * Stack now looks like: top  <--s Vars AR   <--b Params ExVars  0
     */

    activate_module(mod);

    get_symbolic_name(modnow->bodident, 0, modnow->bodyname, NULL, 40);
    modnow->class = class;
    modnow->numtrans = transcount;
    modnow->nummodvars = modvars;
    modnow->init_time = globaltime;
    modnow->cycles = -1;	/* Assume no state changes until one happens */
    while (modvars--)
    {
	CHILD(modnow, modvars) = NULL;
	modnow->modvar_tbl[modvars].varident = 0;
    }
    modnow->state = PREINITIAL_STATE;
    modnow->first_trans = p + trans_lbl - Op_NArgs[(short)OP_MODULE];

    mod->trans_initialised = FALSE;
    if (initcount)
    {
	ushort enabled;
	short first_trans;
	TRANS *tptr;
	InInit++;
	first_trans = p + init_lbl - Op_NArgs[(short)OP_MODULE];
	tptr = modnow->trans_tbl = (transition_table) Mem_Calloc((short)initcount, TRANS_SIZE, 10);
	if (tptr == NULL)
	    interp_error(ERR_TRANSTBL);
	enabled = eval_clauses(first_trans, initcount, FALSE);
	if (enabled)
	{
	    short tnum = random_select(enabled);
	    /*
	     * As initialisation code may create processes, it is critical.
	     * All user commands other than execution ones are disabled.
	     */
	    if (modnow->nummodvars)
		critical_section++;
	    exec_trans_block(tnum);
	    if (modnow->nummodvars)
		critical_section--;
	}
	/* Free the metatrans tabel and the trans table */
	while (initcount--)
	{
	    if (tptr->meta_trans_tbl || tptr->meta_delay_tbl)
		fputs("Assertion failure!\n", stderr);
	    if (tptr->meta_trans_tbl)
		Mem_Free(tptr->meta_trans_tbl);
	    if (tptr->meta_delay_tbl)
		Mem_Free(tptr->meta_delay_tbl);
	    if (tptr->meta_max_tbl)
		Mem_Free(tptr->meta_max_tbl);
	    if (tptr->meta_min_tbl)
		Mem_Free(tptr->meta_min_tbl);
	    tptr++;
	}
	Mem_Free(modnow->trans_tbl);
	InInit--;		/* Increase init flag setting */
    }
    modnow->last_trans[0] = modnow->last_trans[1] = -1;	/* Reset this */
    mod->trans_tbl = transcount ? (transition_table) Mem_Calloc((short)transcount, TRANS_SIZE, 11) : NULL;
    if (mod->trans_tbl == NULL && transcount)
	interp_error(ERR_TRANSTBL);
    mod->first_state = mod->state;	/* Save start state */
    activate_module(parent);	/* return to parent */
    if (trace)
	fprintf(trace, trace_title, globaltime);
}


/********************** SCHEDULING *************************/


static void NEAR 
calculate_enable_status(MODULE * mod /* , short level, short col */ )
{
    MODULE *modtmp = modnow;
    activate_module(mod);
    if (eval_clauses(modnow->first_trans, modnow->numtrans, TRUE) == 0
	|| ctrace)
    {
	register ushort m;
	for (m = 0; m < modnow->nummodvars; m++)
	    if (CHILD(modnow, m))
		Set_Bit(CHILD(modnow, m)->checked, EVAL_BIT);
	for (m = 0; m < modnow->nummodvars; m++)
	{
	    MODULE *child = CHILD(modnow, m);
	    if (child)
		if (Bit_Set(child->checked, EVAL_BIT))
		{
		    calculate_enable_status(child);
		    Clear_Bit(child->checked, EVAL_BIT);
		}
	}
    }
    activate_module(modtmp);
}

/*static short PID=-1;*/

static BOOLEAN 
execute_transitions(short delayTrans)
{
    short selected = -1, max_priority = -1;
    ushort trans_now, enbl_cnt = 0;
    TRANS *tptr = modnow->trans_tbl;
    BOOLEAN done_something = FALSE;
    if (IS_TRANS)
	get_user_cmd(modnow);
    /*
     * If we have a delay target, then if it is in this process, do it; else
     * skip to next process; otherwise proceed as normal
     */
    if (delayTrans >= 0)
    {				/* This should only happen in cycle mode */
	int start;
	if (modnow != root)
	{
	    start = modnow->trans_tbl->metaidx;
	    if (delayTrans >= start && delayTrans < (start + modnow->numtrans))
		selected = delayTrans - start;
	}
    } else
    {
	for (trans_now = 0; trans_now < modnow->numtrans; trans_now++)
	{
	    if (IS_FIREABLE(tptr))
	    {
		enbl_cnt++;
		if (tptr->priority != -1)
		{
		    if (max_priority == -1)
			max_priority = tptr->priority;
		    /* remember, priorities work backwards */
		    if (max_priority >= tptr->priority)
		    {
			max_priority = tptr->priority;
			selected = (short)trans_now;
		    }		/* if */
		}		/* if */
	    }			/* if */
	    tptr++;
	}			/* for */
    }				/* else */
    if (selected == -1 && enbl_cnt)	/* No priorities; select randomly */
	selected = random_select(enbl_cnt);
    if (selected != -1)
    {
	char modvar_name[22];
	/*
	 * Add selected transition into the sequence path tree if necessary
	 */
	if (buildBG)
	{
	    short idx = (modnow->trans_tbl + selected)->metaidx;
	    /*
	     * If we are following an existing path, check integrity, else
	     * add transition to new linear path
	     */
/*			fprintf(treefp,"T%-2d ",idx);*/
	    assert(modnow != root);
	    if (idx >= groupStart && idx <= groupEnd)
		addToPath(idx, &pathabort);
/*			execSeq[execSeqTop++] = idx;*/
	}
	if (ctrace)
	{
	    Get_ModvarName(modnow, modvar_name);
	    fprintf(ctrace, "!%-2d@%8ld\t%s:%d\n",
		    (modnow->trans_tbl + selected)->metaidx,
		    globaltime, modvar_name, (int)selected);
	} else
	    modvar_name[0] = 0;
	outputLoss = 0;
	exec_trans_block(selected);
	if (etrace)
	{
	    if (!modvar_name[0])
		Get_ModvarName(modnow, modvar_name);
	    fprintf(etrace, "%8ld  t%d%c\t%s:%d\n",
		    globaltime,
		    (modnow->trans_tbl + selected)->metaidx,
		    outputChar[outputLoss],
		    modvar_name, (int)selected);
	}
	done_something = TRUE;
	if (pathabort)
	    return TRUE;
    } else
    {				/* look at children */
	MODULE *modtmp = modnow;
	ushort child;
	for (child = 0; child < modtmp->nummodvars; child++)
	    Set_Bit(CHILD(modtmp, child)->checked, EXEC_BIT);
	for (child = 0; child < modtmp->nummodvars; child++)
	{
	    MODULE *ch = CHILD(modtmp, child);
/*			if (buildBG && PID!=-1 && ch->trans_tbl->metaidx!=PID) continue; */
	    if (ch)
	    {
		if (Bit_Set(ch->checked, EXEC_BIT))
		{
		    activate_module(ch);
		    if (execute_transitions(delayTrans))
		    {
			done_something = TRUE;
			/* PID = modnow->trans_tbl->metaidx; */
		    }
		    Clear_Bit(ch->checked, EXEC_BIT);
		}
	    }
	    if (done_something)
	    {
		if (modtmp->class == CLASS_SYSACTIVITY
		    || modtmp->class == CLASS_ACTIVITY
		    || buildBG || ctrace)
		    break;
	    }
	}
	activate_module(modtmp);
    }
    return done_something;
}


/***********************************************/

void 
MakeGMT(short nrows)
{
    short *GMTmem, i = 0;
    float *GMTdmem;
    GMTmem = (short *)Mem_Calloc(nrows * nrows, sizeof(short), 666);
    GMT = (short **)Mem_Calloc(nrows, sizeof(short *), 667);
    GMTrows = nrows;
    for (i = 0; i < nrows; i++)
    {
	GMT[i] = GMTmem;
	GMTmem += nrows;
    }
    GMTdmem = (float *)Mem_Calloc(nrows * nrows, sizeof(float), 666);
    GMTdel = (float **)Mem_Calloc(nrows, sizeof(float *), 667);
    for (i = 0; i < nrows; i++)
    {
	GMTdel[i] = GMTdmem;
	GMTdmem += nrows;
    }
}

/* If we are doing a controlled execution for quick matrix calculation,
   we use a different scheduling method to try to `flatten' the tree a
   bit. We keep firing a single process until it blocks before going on
   to the next. In order to achieve this without too much damage to the
   existing scheduler, we use the metaidx of the first transition of the
   process as a unique PID. We store the current PID, and we do not execute
   any transitions in execute_transitions unless they belong to that PID;
   if the PID is reset, we can execute any transition but set the PID to
   its process. In the scheduler, we reset the PID if execute_transitions
   failed.
*/
void 
schedule()
{
    extern short Ed_ScrLine;
    short i = 0, t = 0, m = root->nummodvars;
    LastExecutingTrans = ExecutingTrans = NULL;
    while (m-- && i < NUM_TGROUPS)
    {
	MODULE *mod = CHILD(root, m);
	TRANS *tp;
	short tc;
	tc = mod->numtrans;
	tp = mod->trans_tbl;
	tgroups[i++] = t;
	if (mod->tgroup)
	    tgroups[i++] = t + mod->tgroup;
	while (tc--)
	{
	    tp->metaidx = t++;
	    tp++;
	}
    }
    if (etrace)
	fprintf(etrace, "%d transitions\n", t);
    numgroups = i;
    execSeqTop = 0;
    /*
     * Allocate a global meta table of size t*t. Each time the a transition
     * gets executed, its entry gets incremented, using the last transitions
     * index and this ones. The indices are stored in the metaidx field.
     */
    MakeGMT(t);
    /* Loop until user quits or we deadlock */

    while (TRUE)
    {
	if (!IS_FAST)
	{
	    short arrowline = (short)run_sourceline - Ed_ScrLine;
	    if (arrowline >= 0 && arrowline < SOURCE_LNS)
		Scr_PutChar(arrowline + 2, 0, 's', pew_wins[(short)SOURCE_WIN]->ba);
	}
	if (TRON)
	    fprintf(logfp, "SCHEDULER AT ITERATION %d   GLOBALTIME %ld\n\n",
		    (int)iteration, globaltime);
	if (etrace)
	    for (i = 0; i < 32; i++)
		delayIsEnbl[i] = (char)0;
	activate_module(root);
	delayTop = min_delay_time = total_enabled = 0;
	ignore_interrupts = TRUE;	/* Clause evaluation is atomic */
	/*
	 * The next flag is set to TRUE if we discovered illegal
	 * interactions. Such interactions may prevent transitions from being
	 * enabled. Only when all such illegal intr's have been removed, and
	 * we still can't do anything, do we change the time.
	 */
	must_not_delay = FALSE;
	calculate_enable_status(root);
	if (TRON)
	    dumpRecursively(root, TRUE, TRUE, TRUE);
	activate_module(root);
	if (!buildBG)
	    ignore_interrupts = FALSE;
#ifdef TREE_DEBUG
	else
	{
	    int i;
	    fprintf(treefp, "*******************\n");
	    for (i = 0; i < delayTop; i++)
		fprintf(treefp, "Can choose delay trans %d triggered by node %d\n",
			delayPoints[i].index, delayPoints[i].trigger);
	    fprintf(treefp, "*******************\n");
	}
#endif
	/* If no enabled transitions, and no illegal interactions... */
	if (total_enabled == 0 && (must_not_delay == FALSE))
	{
	    /* Can we do something in the future? */
	    if (min_delay_time == 0 && (!buildBG || delayTop == 0))
	    {
		fprintf(logfp, "\nNO MORE POSSIBLE TRANSITIONS!\n");
		fprintf(logfp, "SYSTEM HALTED\n");
		(void)Win_Error(NULL, "Scheduler", "No more possible transitions!", TRUE, TRUE);
		if (buildBG)
		    closePath();
		pathabort = TRUE;
	    } else if (!buildBG)
	    {
		if (etrace)
		{
		    int i;
		    fprintf(etrace, "$%8ld +%4d %d\n ",
			    globaltime, min_delay_time,
			    (int)(globaltime + min_delay_time));
		    for (i = 0; i < 256; i++)
			if (delayIsEnbl[i / 8] & (1 << (i % 8)))
			    fprintf(etrace, "t%d\t", i);
		    fprintf(etrace, "\n");
		}
		last_globaltime = globaltime;
		globaltime += min_delay_time;
		if (IS_FAST)
		    resync_display(FALSE);
		if (trace)
		    titledue = TRUE;
	    } else
	    {
		/* Use delay_branch to decide what to do */
		int t;
		t = delay_branch(&pathabort, delayTop);
		if (!pathabort)
		{
		    if (execute_transitions(delayPoints[t].index) == FALSE) /* PID=-1 */ ;
		} else
		    break;
	    }
	} else if (execute_transitions(-1) == FALSE) /* PID = -1 */ ;
	if (pathabort)
	    break;
	if (IS_UNIT)
	    get_user_cmd(modnow);
	iteration++;
	if (timelimit)
	{
	    if (globaltime > timelimit)
	    {
#ifdef UNIX
		longjmp(quit_info, -1);
#else
		db_mode = STEP_MODE | (db_mode & TRACE);
		refreshallwindows = TRUE;
		resync_display(FALSE);
		if (autoquit)
		    longjmp(quit_info, -1);
#endif
	    }
	}
    }
}

/*********************************************************/
