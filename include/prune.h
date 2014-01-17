/* Prune.h - second stage header file */

#define PRUNE
#include "misc.h"

#define MAXPARAMS 	16	/* Must equal MAXTRIGLISTS in tree.h	 */
#define MAXTRANS	128	/* Sizes of isLoss and isDelay arrays	 */
#define MAXDEPTH	1024	/* Size of history stack		 */
#define MAXHISTS	1024	/* Maximum history	 		 */

#include "tree.h"

#ifndef UNIX
#ifndef __TURBOC__
#include <malloc.h>
/*#  define Mem_Calloc(n,s,i)	calloc(n,s)*/
#endif
#endif

typedef struct
{
    char type;
    short trans;
    short Delay;
    float prob;
}   paramData;

typedef struct
{
    short t;			/* delay transition		 */
    short n;			/* number of triggers		 */
    short *trig;		/* array of triggers		 */
    char typ;			/* 'E'- earliest, 'L' - latest	 */
}   trigData;

typedef struct
{
    short node;
    short time;
}   delayStackData;

typedef struct
{
    short h;			/* History	 */
    short n;			/* Clone Node	 */
}   equivNodeData;

#ifndef GLOBAL
#define GLOBAL extern
#endif

GLOBAL paramData params[MAXPARAMS];
GLOBAL trigData trigTable[MAXPARAMS];
GLOBAL trigData killTable[MAXPARAMS];
GLOBAL short links[MAXNODES][2];/* Link reference table 		 */
GLOBAL char isLoss[MAXTRANS];
GLOBAL char isDelay[MAXTRANS];
GLOBAL short History[MAXHISTS][MAX_DELAYS + 1];	/* Different histories	 */
GLOBAL char delayTrans[MAX_DELAYS];
GLOBAL short delayVals[MAX_DELAYS];	/* Delay values		 */
GLOBAL delayStackData delayStack[MAXDEPTH];
GLOBAL equivNodeData **equivTbl;


GLOBAL short paramTop;
GLOBAL short trigTop;
GLOBAL short killTop;
GLOBAL short histTop;
GLOBAL short linkTop;
GLOBAL short delayTop;
GLOBAL short delayStackTop;

int readTriggers(void);
int readKillers(void);
short readNodes(void);
void clearMarks(void);
void readSeq(void);
void dumpTree(FILE * fp, short n);
void checkTree(void);
short readParams(void);
void printNode(FILE * fp, short n);
