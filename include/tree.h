/*************************/
/* Behaviour Graph Nodes */
/*************************/

#ifndef INCLUDED_TREE
#define INCLUDED TREE

#define MAX_SEQ		64	/* Max number of unique sequences */
#define MAX_DELAYS      8	/* Maximum number of allowed delay
				 * transitions in whole system */
#define MAX_TRIGGERS    4	/* Maximum number of allowed trigger
				 * transitions per delay transition */
#define MAX_LOSSES      8	/* Maximum number of allowed loss transitions
				 * in whole system */
#define MAXTRIGLISTS	(MAX_DELAYS+MAX_LOSSES)

#ifdef UNIX
#ifdef PRUNE
#define MAXNODES	((ushort)1024)
#else
#define MAXNODES	((ushort)50000)
#endif
#else
#ifdef PRUNE
#define MAXNODES	128
#else
#define MAXNODES	3200
#endif
#endif

#define SEQ_LEN		64

#define RIGHTMOST	1
#define DONE_SEQ	2
#define DEFUNCT		4	/* Trimmed due to cycle */
#define MARKED		8	/* Used for recursion */
#define LOSSBRANCH	16	/* Loss branch?		 */
#define LOSSTRIGGER	32
#define DELAYBRANCH	64

/******************/
/* Node sequences */
/******************/

struct seqStruct
{
    char seqLen;
    short crc;
    ushort *seqArray;
};

typedef struct seqStruct seqNode;

/*********/
/* Nodes */
/*********/

struct treeStruct
{
    short
        refcnt,			/* Reference count			 */
       *child,			/* Pointer to block of child indices	 */
#ifdef PRUNE
       *parent,			/* Array of parent indices		 */
        index, numhist, equiv;
    float *prob,		/* Array of branch probabilities	 */
       *Delay;			/* Array of branch delays		 */
    unsigned char
        hist[MAXHISTS / 8],	/* Set of histories			 */
        nparent;		/* Number of parents			 */
#else
        parent;			/* Index of parent in treeStruct table	 */
#endif

    unsigned char
        flags,			/* Node info				 */
        seq,			/* Index into seq structure table	 */
        seqlen,			/* sum of sequence length		 */
        depth,			/* Depth of this node in tree		 */
        delayTrans,		/* Index of delay transition, if any	 */
        nchild;			/* Number of children			 */
};

typedef struct treeStruct treeNode;

void freeTree(void);
void dumpSeq(void);
void dumpTree(FILE * fp, short n);
short addSeq(ushort * seq, short seqlen);
void trimGraph(void);
void checkTree(void);
void noteDelayTrans(short n, short c, short d);
short delay_branch(BOOLEAN * end_it, short choices);
BOOLEAN output_branch(BOOLEAN * end_it);
void addToPath(ushort idx, BOOLEAN * end_it);
void closePath(void);
ushort getTarget(void);
void initTreeRun(void);

#ifndef GLOBAL
#define GLOBAL extern
#endif

GLOBAL seqNode seqTable[MAX_SEQ];
GLOBAL unsigned char depthNow;
GLOBAL unsigned short tr_seq[SEQ_LEN];	/* Linear sequence of transitions	 */
GLOBAL ushort targetNode, treeTop,	/* Next free entry; 0 = NULL; 1 =
					 * root */
    freeNodes, minNodes, maxNodes, pathpos;	/* Position in current linear
						 * seq	 */
GLOBAL short pathroot, pathnow, seqTop,	/* Table indices of next free entries */
    compareDepth;		/* Number of tree levels to recurse when
				 * comparing exec tree nodes */
GLOBAL FILE *treefp;

GLOBAL treeNode *treeTable;


#endif				/* INCLUDED_TREE */
