#include <stdio.h>
#include <assert.h>
#include "nrutil.h"
#include "misc.h"

#undef GLOBAL
#define GLOBAL
#include "prune.h"

unsigned _stklen = 20000;

#define popDelay()	(delayStackTop--)

/*************/
/* Variables */
/*************/

short baseTop;
short tr_start, tr_end, comparedepth;
short hist[MAX_DELAYS];
static short numNodes = 1;
float **solmat, **trg, **delaymat;

/**************/
/* Prototypes */
/**************/

static void processTree(void);
static void doLossesRecursively(short n);
static void doDelays(void);
static void doDelaysRecursively(short n, short t);
static void pushDelay(short n, short t);
static short findDelayTrans(short node);
static short buildHistory(short tm, short *hist, short n);
static short findTriggerTime(short tr);
static BOOLEAN transInNode(short tr, short node);
static short posInNode(short n, short tr);
static short lookupHist(short *hist, short choice);
static int compareHist(short n, short *h1, short *h2);
static int inSet(uchar set[], short elt);
static void addToSet(uchar set[], short elt);
static void expandGraph(short n, short t);
static short chooseDelay(short n, short hnum, short *mindelay);
static void printHist(short h);
static short cloneNode(short n, short choice);
static void unlinkNode(short n, short parent, short equiv);
static void relink(short n, short uc, short ec);
static short cloneLossNode(short n);
static short dupNode(short n);
static short stripTransition(short n);
static int fixSelfReference(short n);	/* Empty nodes only */
static void numberTree(short n);
static void stripTransitions(void);
static void buildTRG(void);
static void solveGraph(void);
static void trimSequences(void);

/* Having read in the tree, we recurse through it, setting the delay
and probability values. We trim any paths with zero probability. Note
that we must store a probability value for each branch. Once we have
done this, we call checkTree, although a stricter check is imposed
incorporating probabilities and delays.

In terms of assigning delays and probabilities, losses are straighforward.
Delays are more tricky. We can take note of all possible triggers when we
read in the file. We want to maintain a stack of the last trigger times
which is valid at any point in the recursive process. A simpler first
approach is then use a modified recursion - we mark each node as
we process it

*/

#ifndef UNIX
static void forcefloat(float *p)
{
    float f = *p;
    forcefloat(&f);
}
#endif

char *USAGE[] = {
    "Useage: GP <depth> <start> <end>\n",
    "\nwhere <depth> is the maximum depth reached building graph\n",
    "\t<start> is the index of the first transition (from 1)\n",
    "\t<end> is the index of the last transition (from 1)\n\n",
    "This program reads the behaviour graph in the file TREE.BIN and a set of\n",
    "parameter values in PARAM.DAT, together with additional information in the\n",
    "files KILL.BIN, SEQ.BIN and TRIG.DAT, and parameterises the behaviour graph\n",
    "with the specified values. The resulting graph is then solved as a Markov\n",
    "process, and a transition relation graph is determined from the result.\n",
    "This graph is in turn solved to produce throughput predictions for the\n",
    "specified range of transitions. This range should not cross process\n",
    "boundaries; that is, all the transitions in the range must belong to\n",
    "just one process.\n\nUse the -C option of EI to build a behaviour\n",
    "graph.\n\nThe values in PARAM.DAT are the delays or loss probabilities\n",
    "of the listed transitions\n",
    NULL
};

int 
main(int argc, char **argv)
{
    if (argc != 4)
    {
	int i = 0;
	while (USAGE[i])
	    fprintf(stderr, USAGE[i++]);
	exit(0);
    } else
    {
	comparedepth = atoi(argv[1]) - 1;
	tr_start = atoi(argv[2]) - 1;
	tr_end = atoi(argv[3]) - 1;
    }
    Mem_Init();
    if (readTriggers() == 0)
    {
	puts("Couldn't open trig.bin");
	exit(0);
    }
    if (readKillers() == 0)
    {
	puts("Couldn't open kill.bin");
	exit(0);
    }
    if (readNodes() == 0)
    {
	puts("Couldn't open tree.bin");
	exit(0);
    }
    readSeq();
    if (readParams())
	processTree();
    else
	puts("Couldn't read param.dat");
    Mem_Check(TRUE);
    return 0;
}

static void 
processTree()
{
    puts("Doing losses");
    clearMarks();
    doLossesRecursively(1);
    putchar('\n');
    dumpTree(stdout, 1);

    puts("\n\nDoing delays");
    clearMarks();
    histTop = 0;
    doDelays();
    dumpTree(stdout, 1);

    puts("\n\nChecking tree");
    checkTree();
    puts("At end of checkTree:\n");
    dumpTree(stdout, 1);

    puts("\n\nStripping Transitions");
    treeTable[1].refcnt++;	/* Don't remove this one */
    stripTransitions();
    puts("After stripTransitions\n");
    dumpTree(stdout, 1);

    /* Build and solve the graph throughput matrix; result in solmat */
    solveGraph();
    dumpTree(stdout, 1);
    buildTRG();
}

/****************** HANDLE LOSS PARAMETRISATION ****************/

static void 
doLossesRecursively(short n)
{
    short l, s, c;
    if ((n == 0) || treeTable[n].flags & MARKED)
	return;
    treeTable[n].flags |= MARKED;
    treeTable[n].equiv = n;
    putchar('.');
    fflush(stdout);
    s = treeTable[n].seq;
    l = seqTable[s].seqLen;
    if (l)
    {
	int t = seqTable[s].seqArray[l - 1];	/* last trans in seq */
	if (isLoss[t])
	{
	    float p = 0.;
	    int i;
	    /* Get the probability of the loss */
	    for (i = 0; i < paramTop; i++)
		if (params[i].type == 'L' && params[i].trans == t)
		{
		    p = params[i].prob;
		    break;
		}
	    treeTable[n].prob[0] = 1. - p;
	    treeTable[n].prob[1] = p;
	}
    }
    for (c = 0; c < treeTable[n].nchild; c++)
	doLossesRecursively(treeTable[n].child[c]);
}

/****************** HANDLE DELAY PARAMETRISATION ****************/

static void 
doDelays()
{
    delayStackTop = 0;
    doDelaysRecursively(1, 0);
    printf("Finished doing histories\n");
    equivTbl = (equivNodeData **) Mem_Calloc(linkTop, sizeof(equivNodeData *), 720);
    delayStackTop = 0;
    clearMarks();
    baseTop = linkTop;
    expandGraph(1, 0);
    dumpTree(stdout, 1);
}

static void 
doDelaysRecursively(short n, short t)
{
    if (n == 0)
	return;
    /* Push details on stack */
    printf("Doing node %d\n", n);
    pushDelay(n, t);
    if (treeTable[n].flags & LOSSBRANCH)
    {
	if (treeTable[n].prob[0] > 0.)
	    doDelaysRecursively(treeTable[n].child[0], t);
	if (treeTable[n].prob[1] > 0.)
	    doDelaysRecursively(treeTable[n].child[1], t);
    } else
    {
	{
	    short hnum, choice, mindelay;
	    hnum = buildHistory(t, hist, n);
	    if (inSet(treeTable[n].hist, hnum))
		goto endRecurse;/* Done this before */
	    else
	    {
		addToSet(treeTable[n].hist, hnum);
		treeTable[n].numhist++;
	    }
	    /* Work out delay, which way to recurse */
	    choice = chooseDelay(n, hnum, &mindelay);
	    n = treeTable[n].child[choice];
	    t += mindelay;
	}			/* Free up local vars before recursing */
	doDelaysRecursively(n, t);
    }
endRecurse:
    popDelay();
}

static void 
pushDelay(short n, short t)
{
    assert(delayStackTop < MAXDEPTH);
    delayStack[delayStackTop].node = n;
    delayStack[delayStackTop++].time = t;
}

static short 
findDelayTrans(short node)
{
    short t, seqnum = treeTable[node].seq;
    if (seqTable[seqnum].seqLen)
    {
	t = seqTable[seqnum].seqArray[0];
	if (isDelay[t])
	    return t;
    }
    return -1;
}

static short 
buildHistory(short tm, short *hist, short n)
{
    short i, choice = -1, waitTime = 0, ch, dt;
    for (ch = 0; ch < delayTop; ch++)
	hist[ch] = -1;
    for (ch = 0; ch < treeTable[n].nchild; ch++)
    {
	dt = findDelayTrans(treeTable[n].child[ch]);
	dt = isDelay[dt] - 1;	/* Index into history */
	assert((dt >= 0) && (dt < delayTop));
	hist[dt] = 0;
    }
    for (i = 0; i < delayTop; i++)
    {
	short tt, d;
	d = delayVals[i];
	tt = findTriggerTime(delayTrans[i]);
	if (tt == -1)
	    tt = 0;		/* assert(hist[i]==-1);} */
	else if (hist[i] == 0)
	{
	    hist[i] = tt + d - tm;	/* Time till it can fire */
	    if ((choice == -1) || (hist[i] < waitTime))
	    {
		choice = i;
		waitTime = hist[i];
	    }
	}
    }
    return lookupHist(hist, choice);
}

static short 
findTriggerTime(short tr)
{
    short i = delayStackTop, trig, kill = 0, j, k, lt, lk, tm = -1, nexttm = 0,
        tn = 0, tt = 0, typ;
    if (tr == -1)
	return 0;
    for (j = 0; j < trigTop; j++)
	if (trigTable[j].t == tr)
	    break;
    assert(j < trigTop);
    typ = trigTable[j].typ;
    trig = j;
    lt = trigTable[trig].n;
    if (typ == 'E')
    {
	for (j = 0; j < killTop; j++)
	    if (killTable[j].t == tr)
		break;
	assert(j < killTop);
	kill = j;
	lk = killTable[kill].n;
    } else
	lk = 0;
    while (i--)
    {
	short n = delayStack[i].node;
	for (k = 0; k < lt; k++)
	    if (transInNode(trigTable[trig].trig[k], n))
	    {
		if (tm != -1)
		    nexttm = tm;
		else
		    nexttm = delayStack[i].time;
		tm = delayStack[i].time;
		tn = i;
		tt = trigTable[trig].trig[k];
		if (typ == 'L')
		    return tm;
	    }
	for (k = 0; k < lk; k++)
	    if (transInNode(killTable[kill].trig[k], n))
	    {
		if ((tn != i) || (tm == -1))
		    return tm;
		else
		{
		    /* Which comes first - trigger or killer? */
		    if (posInNode(n, tt) > posInNode(n, killTable[kill].trig[k]))
			return tm;
		    else
			return nexttm;
		}
	    }
    }
    return 0;
}

static BOOLEAN 
transInNode(short tr, short node)
{
    short t, seqnum = treeTable[node].seq;
    for (t = 0; t < seqTable[seqnum].seqLen; t++)
	if (seqTable[seqnum].seqArray[t] == tr)
	    return TRUE;
    return FALSE;
}

static short 
posInNode(short n, short tr)
{
    short t, seqnum = treeTable[n].seq;
    for (t = 0; t < seqTable[seqnum].seqLen; t++)
	if (seqTable[seqnum].seqArray[t] == tr)
	    return t;
    return -1;
}

static short 
lookupHist(short *hist, short choice)
{
    int i;
    for (i = 0; i < histTop; i++)
    {
	if (compareHist(delayTop, hist, History[i]))
	    return i;
    }
    assert(histTop < MAXHISTS);
    printf("Adding new history %d = ( ", histTop + 1);
    for (i = 0; i < delayTop; i++)
    {
	History[histTop][i] = hist[i];
	printf("%d  ", hist[i]);
	History[histTop][delayTop] = choice;
    }
    printf(") [Choice %d]\n", choice + 1);
    i = histTop++;
    return i;
}

static int 
compareHist(short n, short *h1, short *h2)
{
    int i;
    for (i = 0; i < n; i++)
	if (h1[i] != h2[i])
	    return 0;
    return 1;
}

static int 
inSet(uchar set[], short elt)
{
    short bit, ndx;
    ndx = elt / 8;
    bit = 1 << (elt % 8);
    return (set[ndx] & bit);
}

static void 
addToSet(uchar set[], short elt)
{
    short bit, ndx;
    ndx = elt / 8;
    bit = 1 << (elt % 8);
    set[ndx] |= bit;
}

static void 
expandGraph(short n, short t)
{
    printf("Doing node ");
    printNode(stdout, n);
    pushDelay(n, t);
    if (treeTable[n].flags & LOSSBRANCH)
    {
	if (treeTable[n].prob[0] > 0.)
	    expandGraph(treeTable[n].child[0], t);
	if (treeTable[n].prob[1] > 0.)
	    expandGraph(treeTable[n].child[1], t);
    } else
    {
	{
	    short equiv, hnum, choice, mindelay, i;
	    if (treeTable[n].numhist == 1)
	    {
		if (treeTable[n].flags & MARKED)
		{
		    printf("Single history, marked => bottom out\n");
		    goto endRecurse;
		} else
		    treeTable[n].flags |= MARKED;
	    }
	    hnum = buildHistory(t, hist, n);
	    printHist(hnum);
	    /* Work out delay, which way to recurse */
	    choice = chooseDelay(n, hnum, &mindelay);
	    if (treeTable[n].numhist > 1)
	    {
		/*
		 * Multiple history node. We have to check if we have made an
		 * equiv table for it. If not, we make one with the current
		 * history and (primary) node. If one exists, and we have the
		 * same history, we set prob & delay and bottom out. If it is
		 * different, we add it to the equiv list and clone the node
		 * and all its loss children, grandchildren, etc
		 */
		equiv = treeTable[n].equiv;
		if (equivTbl[equiv] == NULL)
		{
		    assert(equiv == n);
		    printf("Created equivTable for node %d\n", equiv);
		    equivTbl[equiv] = (equivNodeData *) Mem_Calloc(treeTable[n].numhist, sizeof(equivNodeData), 9111);
		    equivTbl[equiv][0].n = equiv;
		    equivTbl[equiv][0].h = hnum;
		    treeTable[n].child[choice] = cloneLossNode(treeTable[n].child[choice]);
		} else
		{
		    for (i = 0; i < treeTable[n].numhist; i++)
		    {
			if (equivTbl[equiv][i].n == 0)
			{
			    /* Add a new history */
			    short c = 0, p = delayStack[delayStackTop - 2].node; /* Parent */ ;
			    {
				short i;
				for (i = 0; i < treeTable[p].nchild; i++)
				    if (treeTable[p].child[i] == n)
					c = i;
			    }
			    equivTbl[equiv][i].h = hnum;
			    equivTbl[equiv][i].n = n = cloneNode(n, choice);
			    delayStack[delayStackTop - 1].node = n;
			    treeTable[p].child[c] = n;
			    printf("Added history #%d (H%d), cloned node to %d\n", i + 1, hnum, n);
			    assert(i < treeTable[equivTbl[equiv][0].n].numhist);
			    break;
			} else if (equivTbl[equiv][i].h == hnum)
			{
			    /*
			     * Dup history - must relink any references to
			     * equivTbl[n][i].n and free this node
			     */
/*						assert(n<baseTop);*/
			    printf("Found dup history with node %d\n", equivTbl[equiv][i].n);
			    unlinkNode(n, delayStack[delayStackTop - 2].node, equivTbl[equiv][i].n);
			    goto endRecurse;
			}
		    }
		}
	    }
	    {
		short c, nc;
		nc = treeTable[n].nchild;
		for (c = 0; c < nc; c++)
		    treeTable[n].prob[c] = 0.;
	    }
	    treeTable[n].prob[choice] = 1.;
	    treeTable[n].Delay[choice] = (float)mindelay;
	    n = treeTable[n].child[choice];
	    t += mindelay;
	}			/* Free up local vars before recursing */
	expandGraph(n, t);
    }
endRecurse:
    popDelay();
}

static short 
chooseDelay(short n, short hnum, short *mindelay)
{
    short ch, choice = -1;
    for (ch = 0; ch < treeTable[n].nchild; ch++)
    {
	short dt = findDelayTrans(treeTable[n].child[ch]);
	dt = isDelay[dt] - 1;	/* Index into history */
	if (History[hnum][delayTop] == dt)
	{
	    choice = ch;
	    *mindelay = History[hnum][dt];
	    break;
	}
    }
    assert(choice != -1);
    if (*mindelay < 0)
	*mindelay = 0;
    return choice;
}

static void 
printHist(short h)
{
    short i;
    printf("H%-3d = (", h + 1);
    for (i = 0; i < delayTop; i++)
	printf("%d  ", History[h][i]);
    printf(")");
}

static short 
cloneNode(short n, short choice)
{
    short new = dupNode(n);
    treeTable[new].child[choice] = cloneLossNode(treeTable[n].child[choice]);
    return new;
}

static void 
unlinkNode(short n, short parent, short equiv)
{
    if (n == equiv)
	return;
    printf("Parent is %d\n", parent);
    relink(parent, n, equiv);
}

/* Replace all references to a node by references to an equivalent node,
	and then free the node */

static void 
relink(short n, short uc, short ec)
{
    short c, nc;
    nc = treeTable[n].nchild;
    for (c = 0; c < nc; c++)
    {
	if (treeTable[n].child[c] == uc)
	{
	    printf("Relinking: treeTable[%d].child[%d] = %d\n", n, c, ec);
	    treeTable[n].child[c] = ec;
	}
    }
}

/* Clone a delay node and all its loss children, and return
   the new node index */

static short 
cloneLossNode(short n)
{
    short new, nc, c;
    if (treeTable[n].flags & DELAYBRANCH)
	return n;
    new = dupNode(n);
    printf("Cloned loss node %d as %d\n", n, new);
    nc = treeTable[n].nchild;
    for (c = 0; c < nc; c++)
	treeTable[n].child[c] = cloneLossNode(treeTable[n].child[c]);
    return new;
}


static short 
dupNode(short n)
{
    short i, nc, new = linkTop++;
    treeTable[new].refcnt = 1;
    treeTable[new].child = treeTable[n].child;
    treeTable[new].parent = treeTable[n].parent;
    treeTable[new].index = treeTable[n].index;
    treeTable[new].numhist = treeTable[n].numhist;
    treeTable[new].equiv = treeTable[n].equiv;
    for (i = 0; i < (MAXHISTS / 8); i++)
	treeTable[new].hist[i] = treeTable[n].hist[i];
    treeTable[new].nparent = treeTable[n].nparent;
    treeTable[new].flags = treeTable[n].flags;
    treeTable[new].seq = treeTable[n].seq;
    treeTable[new].seqlen = treeTable[n].seqlen;
    treeTable[new].depth = treeTable[n].depth;
    treeTable[new].delayTrans = treeTable[n].delayTrans;
    nc = treeTable[new].nchild = treeTable[n].nchild;
    treeTable[new].child = (short *)Mem_Calloc(nc, sizeof(short), 803);
    treeTable[new].prob = (float *)Mem_Calloc(nc, sizeof(float), 802);
    treeTable[new].Delay = (float *)Mem_Calloc(nc, sizeof(float), 801);
    for (i = 0; i < nc; i++)
    {
	treeTable[new].child[i] = treeTable[n].child[i];
	treeTable[new].prob[i] = treeTable[n].prob[i];
	treeTable[new].Delay[i] = treeTable[n].Delay[i];
    }
    return new;
}

/****************** STRIP TRANSITIONS ****************/

/* stripTransitions removes all but the specified transitions from the
   graph. Any nodes which become empty are removed. Branches to such a
	node are replaced by branches to its children.
*/

BOOLEAN stripped;

static void 
stripTransitions()
{
    trimSequences();
    do
    {
	short i;
	clearMarks();
	stripped = FALSE;
	for (i = 1; i < linkTop; i++)
	    if (treeTable[i].refcnt)
	    {
		stripTransition(i);
		break;
	    }
    } while (stripped);
}

static void 
trimSequences()
{
    /* Remove unimportant transitions from the sequences */
    short i;
    for (i = 0; i < seqTop; i++)
    {
	short p = 0, p2, l;
	ushort *tbl;
	tbl = seqTable[i].seqArray;
	l = seqTable[i].seqLen;
	while (p < l)
	{
	    if (tbl[p] >= tr_start && tbl[p] <= tr_end)
		p++;
	    /* Remove this transition */
	    else
	    {
		l--;
		for (p2 = p; p2 < l; p2++)
		    tbl[p2] = tbl[p2 + 1];
	    }
	}
	seqTable[i].seqLen = l;
    }
}

static short 
stripTransition(short n)
{
    short c;
    if (n == 0 || treeTable[n].refcnt == 0 || (treeTable[n].flags & MARKED))
	return 0;
    treeTable[n].flags |= MARKED;
    printf("In stripTransition(%d)\n", n);
    for (c = 0; c < treeTable[n].nchild; c++)
    {
	short cn = treeTable[n].child[c], s, l;
	s = treeTable[cn].seq;
	l = seqTable[s].seqLen;
	if (l == 0)
	{
	    short cnch;
	    printf("Can remove node %d\n", cn);
	    if (fixSelfReference(cn))
	    {
		c--;
		continue;
	    }
	    cnch = treeTable[cn].nchild;
	    if (cnch == 1)
	    {
		/* trivial case */
		treeTable[n].child[c] = treeTable[cn].child[0];
		treeTable[n].Delay[c] += treeTable[cn].Delay[0];
		treeTable[cn].refcnt--;
		printf("%s node %d\n", treeTable[cn].refcnt ? "Removed" : "Freed", cn);
		stripped = TRUE;
	    } else
	    {
		short i, j = 0, num, *newch;
		float p, d, *newDl, *newPr;
		num = cnch + treeTable[n].nchild - 1;
		newch = (short *)Mem_Calloc(num, sizeof(short), 385);
		newDl = (float *)Mem_Calloc(num, sizeof(float), 386);
		newPr = (float *)Mem_Calloc(num, sizeof(float), 387);
		for (i = 0; i < c; i++)
		{
		    newch[j] = treeTable[n].child[i];
		    newDl[j] = treeTable[n].Delay[i];
		    newPr[j] = treeTable[n].prob[i];
		    j++;
		}
		d = treeTable[n].Delay[c];
		p = treeTable[n].prob[c];
		for (i = 0; i < cnch; i++)
		{
		    newch[j] = treeTable[cn].child[i];
		    assert(newch[j] == treeTable[cn].child[i]);
		    newDl[j] = d + treeTable[cn].Delay[i];
		    newPr[j] = p * treeTable[cn].prob[i];
		    j++;
		}
		for (i = c + 1; i < treeTable[n].nchild; i++)
		{
		    newch[j] = treeTable[n].child[i];
		    newDl[j] = treeTable[n].Delay[i];
		    newPr[j] = treeTable[n].prob[i];
		    j++;
		}
		Mem_Free(treeTable[n].child);
		Mem_Free(treeTable[n].Delay);
		Mem_Free(treeTable[n].prob);
		treeTable[n].child = newch;
		treeTable[n].Delay = newDl;
		treeTable[n].prob = newPr;
		treeTable[n].nchild += cnch - 1;
		treeTable[cn].refcnt--;
		printf("Multibranch %s node %d\n", treeTable[cn].refcnt ? "Removed" : "Freed", cn);
		stripped = TRUE;
		dumpTree(stdout, 1);
	    }
	    c--;
	    continue;
	}
	stripTransition(cn);	/* recurse */
    }
    return 0;
}

static int 
fixSelfReference(short n)
{				/* Empty nodes only */
    short ch, nch, ch2;
    int rtn = 0;
    nch = treeTable[n].nchild;
    for (ch = 0; ch < nch; ch++)
    {
	if (treeTable[n].child[ch] == n)
	{
	    float p, d;
	    printf("Fixing self-referential node %d\n", n);
	    rtn = 1;
	    p = treeTable[n].prob[ch];
	    d = treeTable[n].Delay[ch];
	    for (ch2 = 0; ch2 < nch; ch2++)
	    {
		if (ch2 == ch)
		    continue;
		treeTable[n].prob[ch2] /= (1. - p);
		treeTable[n].Delay[ch2] += p * d / (1. - p);
	    }
	    for (ch2 = ch; ch2 < (nch - 1); ch2++)
	    {			/* Shift */
		treeTable[n].child[ch2] = treeTable[n].child[ch2 + 1];
		treeTable[n].prob[ch2] = treeTable[n].prob[ch2 + 1];
		treeTable[n].Delay[ch2] = treeTable[n].Delay[ch2 + 1];
	    }
	    treeTable[n].nchild--;
	    dumpTree(stdout, 1);
	}
    }
    return rtn;
}


/****************** SOLVE GRAPH ****************/

static void 
solveGraph()
{
    /*
     * Now we number each node in the tree sequentially; we also build a list
     * of the mappings in the links array.
     */
    short i, j;
    float **gmat, **gmat2;
    clearMarks();
    numNodes = 0;
    i = 1;
    while (i < linkTop)
    {
	if (treeTable[i].refcnt)
	{
	    numberTree(i);
	    break;
	} else
	    i++;
    }
    numNodes--;			/* number of nodes */
    /* Now we build a matrix for the graph */
    gmat = matrix(1, numNodes, 1, numNodes);
    gmat2 = matrix(1, numNodes + 1, 1, numNodes + 1);
    solmat = matrix(1, numNodes + 1, 1, 1);
    for (i = 1; i <= numNodes; i++)
    {
	short c, ch, n = links[i][0];
	ch = treeTable[n].nchild;
	for (j = 1; j <= numNodes; j++)
	    gmat[i][j] = 0.;
	for (c = 0; c < ch; c++)
	{
	    j = treeTable[treeTable[n].child[c]].index;
	    gmat[i][j] += treeTable[n].prob[c];
	    printf("gmat[%d][%d] = %5.3f\n", i, j, gmat[i][j]);
	}
    }
    /* We must now convert this to the form in the thesis; see chap 3 */
    for (i = 1; i <= numNodes; i++)
    {
	gmat2[i][i] = gmat[i][i] - 1.;
	for (j = 1; j <= numNodes; j++)
	{
	    if (j == i)
		continue;
	    gmat2[i][j] = gmat[j][i];
	}
	gmat2[i][j] = 1.;	/* Dummy unknown */
    }
    /*
     * Last row is a sum of probabilities. We add a dummy `unknown', with
     * known value 1, to keep the matrix square.
     */
    for (i = 1; i <= (numNodes + 1); i++)
	gmat2[numNodes + 1][i] = 1.;
    /* Build RHS */
    solmat[numNodes + 1][1] = 2.;
    for (i = 1; i <= numNodes; i++)
	solmat[i][1] = 1.;
    printf("\n\nMatrix\n\n");
    for (i = 1; i <= numNodes; i++)
    {
	for (j = 1; j <= numNodes; j++)
	    printf("%.2f ", gmat2[i][j]);
	printf("\n");
    }
    printf("\n\nSolution Vector\n\n");
    for (i = 1; i <= numNodes; i++)
	printf("%.2f ", solmat[i][1]);
    printf("\n\n");
    /* Now we solve the matrix */
    gaussj(gmat2, numNodes + 1, solmat, 1);
    free_matrix(gmat, 1, numNodes, 1, numNodes);
    free_matrix(gmat2, 1, numNodes + 1, 1, numNodes + 1);
    for (i = 1; i <= numNodes; i++)
    {
	printf("Throughput of node %d is %5.3f\n", links[i][0], solmat[i][1]);
    }
}

static void 
numberTree(short n)
{
    short c, ch;
    if (n == 0)
	return;
    if (treeTable[n].flags & MARKED)
	return;
/*	if (treeTable[n].refcnt==0 || (treeTable[n].flags&MARKED)) return;*/
    treeTable[n].flags |= MARKED;
    links[numNodes][0] = n;
    treeTable[n].index = numNodes++;
    ch = treeTable[n].nchild;
    for (c = 0; c < ch; c++)
	if (treeTable[n].prob[c] > 0.)
	    numberTree(treeTable[n].child[c]);
}

/****************** BUILD TRG ****************/

static void 
buildTRG()
{
    short done, i, j, num = tr_end - tr_start + 1;
    float sum, **trg2, **trg3, **thru, **dmat2;
    trg = matrix(1, num, 1, num);
    delaymat = matrix(1, num, 1, num);
    dmat2 = matrix(1, num, 1, num);
    for (i = 1; i <= num; i++)
	for (j = 1; j <= num; j++)
	{
	    delaymat[i][j] = 0.;
	    dmat2[i][j] = 0.;
	    trg[i][j] = 0.;
	}
    for (i = 1; i <= numNodes; i++)
    {
	/*
	 * Each node has weight solmat[i]. We do the sequence at the node,
	 * and then we add the flow from the last in sequence to the first in
	 * each of the children.
	 */
	short s, l, c, ch, n = links[i][0];
	ushort *tbl;
	s = treeTable[n].seq;
	l = seqTable[s].seqLen;
	tbl = seqTable[s].seqArray;
	for (j = 1; j < l; j++)
	{
	    trg[tbl[j - 1] - tr_start + 1][tbl[j] - tr_start + 1] += solmat[i][1];
	}
	ch = treeTable[n].nchild;
	if (l == 0)
	{
	    printf("Node %d has seqlen zero!\n", n);
/*			assert(l!=0);*/
	}
	for (c = 0; c < ch; c++)
	{
	    short t1, t2, cn = treeTable[n].child[c], seq;
	    float p;
	    seq = treeTable[cn].seq;
	    t1 = tbl[l - 1] - tr_start + 1;
	    t2 = seqTable[seq].seqArray[0] - tr_start + 1;
	    p = solmat[i][1] * treeTable[n].prob[c];
	    trg[t1][t2] += p;
	    delaymat[t1][t2] += p * treeTable[n].Delay[c];
	    /*
	     * Must keep a separate tally of such inter-node delays to
	     * normalise delaymat
	     */
	    dmat2[t1][t2] += p;
	    printf("Delay from t%d to t%d is %f\n", t1 + tr_start, t2 + tr_start, treeTable[n].Delay[c]);
	    printf("Adding %f * %f = %f\n", p, treeTable[n].Delay[c], p * treeTable[n].Delay[c]);
	}
    }

    /* We now have to normalise the TRG */

    for (i = 1; i <= num; i++)
    {
	sum = 0;
	for (j = 1; j <= num; j++)
	    sum += trg[i][j];
	if (sum == 0.)
	{
	    printf("Zero prob of transition t%d\n", tr_start + i - 1);
	    sum = 0.0001;
	}
	printf("sum of probs for t%d is %f\n", i + tr_start, sum);
	for (j = 1; j <= num; j++)
	{
	    trg[i][j] /= sum;
/*			printf("delaymat[t%d][t%d] = %f\n",i+tr_start,j+tr_start,delaymat[i,j]);*/
	    if (dmat2[i][j] > 0.)
		delaymat[i][j] /= dmat2[i][j];
	    else
		delaymat[i][j] = 0.;
/*			printf("After division by %f, delaymat[t%d][t%d] = %f\n",dmat2[i][j],i+tr_start,j+tr_start,delaymat[i,j]);*/
	}
    }
    for (i = 1; i <= num; i++)
    {
	sum = 0;
	for (j = 1; j <= num; j++)
	    sum += delaymat[i][j] * trg[i][j];
	delaymat[i][1] = sum;
	printf("Delay of t%d is %f\n", i + tr_start, sum);
    }
    printf("\n\nTRANSITION RELATION GRAPH\n\n");
    for (i = 1; i <= num; i++)
    {
	for (j = 1; j <= num; j++)
	    printf("%.3f ", trg[i][j]);
	printf("\n");
    }
    printf("\n");
    /* Get ready for gaussj */
    trg2 = matrix(1, num, 1, num);
    /*
     * First we must build a modified trg without the transitions having zero
     * delays
     */
    done = 0;
    for (i = 1; i <= num; i++)
	for (j = 1; j <= num; j++)
	    trg2[i][j] = trg[i][j];
    printf("\n\nTrimming zero delay transitions\n\n");
    while (!done)
    {
	done = 1;
	for (j = 1; j <= num; j++)
	{
	    if (delaymat[j][1] > 0.)
		continue;
	    for (i = 1; i <= num; i++)
	    {
		if (i == j)
		    continue;
		if (trg2[i][j] > 0.)
		{
		    short k;
		    for (k = 1; k <= num; k++)
			trg2[i][k] += (1. / (1. - trg2[j][j])) * trg2[i][j] * trg2[j][k];
		    done = 0;
		    trg2[i][j] = 0.;
		    printf("Cleared trg entry (%d,%d)\n", i + tr_start, j + tr_start);
		}
	    }
	}
    }
    /* Now zap the zero delay rows */
    for (i = 1; i <= num; i++)
	if (delaymat[i][1] == 0.)
	{
	    for (j = 1; j <= num; j++)
		trg2[i][j] = 0.;
	}
    printf("\n\nTRANSITION RELATION GRAPH AFTER REMOVING ZERO DELAYS\n\n");
    for (i = 1; i <= num; i++)
    {
	for (j = 1; j <= num; j++)
	    printf("%.3f ", trg2[i][j]);
	printf("\n");
    }
    printf("\n");
    trg3 = matrix(1, num + 1, 1, num + 1);
    thru = matrix(1, num + 1, 1, 1);
    for (i = 1; i <= num; i++)
    {
	trg3[i][i] = trg2[i][i] - 1.;
	for (j = 1; j <= num; j++)
	{
	    if (j == i)
		continue;
	    /* For now, a quick fix */
	    if (delaymat[j][1] == 0.)
	    {
		printf("Warning: delay of t%d is zero!\n", j + tr_start);
		trg3[i][j] = 0.;
	    } else
		trg3[i][j] = trg2[j][i] / delaymat[j][1] * delaymat[i][1];
	}
	trg3[i][j] = 1.;	/* dummy unknown */
    }
    /* Last row is a sum of probabilities */
    for (i = 1; i <= (num + 1); i++)
	trg3[num + 1][i] = 1.;
    /* Build RHS */
    for (i = 1; i <= num; i++)
	thru[i][1] = 1.;
    thru[num + 1][1] = 2.;

    printf("\n\nTRANSITION RELATION GRAPH AFTER FIXING FOR GAUSS\n\n");
    for (i = 1; i <= num; i++)
    {
	for (j = 1; j <= num; j++)
	    printf("%.3f ", trg3[i][j]);
	printf("\n");
    }
    printf("\n");
    /* Now we solve it */
    gaussj(trg3, num + 1, thru, 1);
    for (i = 1; i <= num; i++)
	printf("t%d has soln %f\n", tr_start + i, thru[i][1]);
    /* Convert to throughput by dividing by delay */
    printf("\n\nIncorporating delays:\n");
    for (i = 1; i <= num; i++)
    {
	if (delaymat[i][1] == 0.)
	    continue;
	thru[i][1] /= delaymat[i][1];
	printf("t%d changes to %f\n", tr_start + i, thru[i][1]);
    }
    /* Now we work out the throughputs of the zero-delay transitions */
    done = 0;
    while (!done)
    {
	done = 1;
	for (i = 1; i <= num; i++)
	{
	    float th = 0.;
	    if (delaymat[i][1] > 0.)
		continue;
	    for (j = 1; j <= num; j++)
		th += trg[j][i] * thru[j][1];
	    if (thru[i][1] != th)
	    {
		thru[i][1] = th;
		done = 0;
	    }
	}
    }

    /* Print results */
    for (i = 1; i <= num; i++)
    {
	if (delaymat[i][1] == 0.)
	    putchar('*');
	printf("Throughput of transition t%d is %5.3f\n", tr_start + i, thru[i][1]);
    }
    free_matrix(trg3, 1, num + 1, 1, num + 1);
    free_matrix(trg2, 1, num, 1, num);
    free_matrix(thru, 1, num + 1, 1, 1);
    free_matrix(dmat2, 1, num, 1, num);
    free_matrix(solmat, 1, numNodes + 1, 1, 1);
    free_matrix(trg, 1, num, 1, num);
    free_matrix(delaymat, 1, num, 1, num);
}

/* We build a history consisting of all the delays in the delayTrans
   array (of which there are delayTop). Then we look up the history,
   adding it if it is new. We then check if the node already has this
   history; if so, we stop recursing. If not, we add the history to the
   node's set and recurse. When this process finally stops, we process
   each node and, assuming each history is equally likely, we calculate
   the branch probabilities and mean delays. */
