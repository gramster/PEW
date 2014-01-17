/***********************/
/* Tree/graph routines */
/***********************/

#include <assert.h>
#include "misc.h"
#include "screen.h"
#undef GLOBAL
#define GLOBAL
#include "tree.h"

seqNode seqTable[MAX_SEQ];
unsigned char depthNow;
unsigned short tr_seq[SEQ_LEN];	/* Linear sequence of transitions	 */
ushort targetNode, treeTop,	/* Next free entry; 0 = NULL; 1 = root */
    freeNodes, minNodes, maxNodes, pathpos;	/* Position in current linear
						 * seq	 */
short pathroot, pathnow, seqTop,/* Table indices of next free entries */
    compareDepth;		/* Number of tree levels to recurse when
				 * comparing exec tree nodes */
FILE *treefp;

treeNode *treeTable;

FILE *binTreefp;

static int depth, consecutive_rights;

/******************/
/* LOCAL ROUTINES */
/******************/

static void freeNode(short n);

/* portable I/O */

void 
writeshort(FILE * fp, short n)
{
    char msb, lsb;
    msb = n / 256;
    lsb = n & 0xFF;
    fwrite(&msb, 1, sizeof(char), fp);
    fwrite(&lsb, 1, sizeof(char), fp);
}

static void 
clearNode(ushort n)
{
#ifndef INTEGRATE
    if (treeTable[n].refcnt)
	freeNode((short)n);
    treeTable[n].seq = 0;
    treeTable[n].nchild = treeTable[n].flags = treeTable[n].parent = 0;
#endif
}

static void 
clearMarks(void)
{
    ushort i;
    for (i = 1; i < treeTop; i++)
	treeTable[i].flags &= (0xFF ^ MARKED);
}

static void 
writeNode(short n)
{
#ifndef INTEGRATE
    char c;
    writeshort(binTreefp, n);
    fwrite(&treeTable[n].flags, 1, sizeof(char), binTreefp);
    fwrite(&treeTable[n].depth, 1, sizeof(char), binTreefp);
    fwrite(&treeTable[n].nchild, 1, sizeof(char), binTreefp);
#if 0
    fwrite(&treeTable[n].triglist, 1, sizeof(char), binTreefp);
#endif
    fwrite(&treeTable[n].seq, 1, sizeof(char), binTreefp);
    for (c = 0; c < treeTable[n].nchild; c++)
	writeshort(binTreefp, treeTable[n].child[(int)c]);
#endif
}

static void 
printNode(FILE * fp, short n)
{
#ifndef INTEGRATE
    int i;
    short lim, si;
    if (n == 0)
	return;
    if (treeTable[n].refcnt == 0)
	return;
    fprintf(fp, "%3d>%c P%-3d [", n, (treeTable[n].flags & LOSSBRANCH) ? 'L' : 'D',
	    treeTable[n].parent);
    writeNode(n);
    for (i = 0; i < treeTable[n].nchild; i++)
    {
	if (i)
	    fprintf(fp, ",");
	fprintf(fp, "%3d", treeTable[n].child[i]);
    }
    fprintf(fp, "] { ");
    fprintf(fp, "S%-2d ", treeTable[n].seq + 1);
    fprintf(fp, "} ( ");
    si = treeTable[n].seq;
    lim = seqTable[si].seqLen;
    if (lim)
    {
	short j;
	ushort *tbl = seqTable[si].seqArray;
	for (j = 0; j < lim; j++)
	{
	    fprintf(fp, "%-2d ", tbl[j] + 1);
	}
    }
#if 0
    if (treeTable[n].flags & LOSSBRANCH)
	fprintf(fp, ")\n");
    else
    {
	int l;
	char *trigtbl;
	fprintf(fp, ") <%d: ", (int)treeTable[n].triglist);
	trigtbl = trigLists[treeTable[n].triglist].trig;
	l = trigLists[treeTable[n].triglist].len;
	for (i = 0; i < l; i++)
	    fprintf(fp, "%-2d ", trigtbl[i] + 1);
	fprintf(fp, ">\n");
    }
#else
    fprintf(fp, ")\n");
#endif

#endif
}

/***************************************************/
/* Initialisation, allocation and freeing of nodes */
/***************************************************/

/* Initialise the graph stuff */

void 
freeTree()
{
#ifndef INTEGRATE
    ushort i;
#if 0
    trigListTop = 0;
#endif
    if (treeTable == NULL)
	treeTable = (treeNode *) Mem_Calloc(MAXNODES, sizeof(treeNode), 876);
    else
    {
	for (i = 1; i < treeTop; i++)
	    clearNode(i);
	for (i = 0; i < seqTop; i++)
	{
	    if (seqTable[i].seqArray)
	    {
		Mem_Free(seqTable[i].seqArray);
		seqTable[i].seqArray = NULL;
		seqTable[i].seqLen = 0;
	    }
	}
    }
    /* Seq entry 0 is reserved for the empty sequence */
    seqTable[0].crc = 1;
    seqTop = 1;
    treeTable[1].depth = 1;
    treeTable[1].refcnt = 1;
    treeTop = 2;
#endif
}


static short 
allocNode(short parent, unsigned char depth, BOOLEAN is_loss)
{
    ushort i;
    /*
     * printf("In
     * allocNode(%d,%d,%s)\n",(int)parent,(int)depth,is_loss?"TRUE":"FALSE");
     */
#ifndef INTEGRATE
    for (i = 1; i < treeTop; i++)
	if (treeTable[i].refcnt == 0)
	    break;
    if (i == treeTop)
    {
	if (treeTop == ((ushort) (MAXNODES - 1)) || treeTop > maxNodes)
	{			/* Out of nodes! */
	    extern void interp_shutdown(void);
	    extern void ReleaseProcesses();
	    Win_Error(NULL, "allocNode", "Out of nodes!", TRUE, FALSE);
	    dumpTree(treefp, 1);
	    interp_shutdown();
	    Mem_Check(TRUE);
	    exit(0);
	} else
	    treeTop++;
    }
    clearNode(i);
    treeTable[i].parent = parent;
    treeTable[i].depth = depth;
    treeTable[i].refcnt = 1;
    if (is_loss)
	treeTable[i].flags |= LOSSTRIGGER;
    /* We convert maxNodes into its proper form later ... */
    if (--freeNodes < minNodes)
	minNodes = freeNodes;
#endif
    return i;
}

/***************************************************************************/

/************/
/* Node I/O */
/************/


static void 
dumpTreeRecursively(FILE * fp, short n)
{
#ifndef INTEGRATE
    short i;
    if (n == 0)
	return;
    if (treeTable[n].flags & MARKED)
	return;
    treeTable[n].flags |= MARKED;
    printNode(fp, n);
    for (i = 0; i < treeTable[n].nchild; i++)
	dumpTreeRecursively(fp, treeTable[n].child[i]);
#endif
}

void 
dumpTree(FILE * fp, short n)
{
    clearMarks();
    binTreefp = fopen("tree.bin", "wb");
    dumpTreeRecursively(fp, n);
    fclose(binTreefp);
}

static void 
dumpSubtree(FILE * fp, short depth, short n)
{
#ifndef INTEGRATE
    short i;
    if (n == 0 || depth == 0)
	return;
    printNode(fp, n);
    for (i = 0; i < treeTable[n].nchild; i++)
	dumpSubtree(fp, depth - 1, treeTable[n].child[i]);
#endif
}

void 
dumpSeq()
{
    short i;
    FILE *seqBin = fopen("seq.bin", "wb");
    writeshort(seqBin, seqTop);
    for (i = 0; i < seqTop; i++)
    {
	ushort *tbl = seqTable[i].seqArray;
	short j, l = seqTable[i].seqLen;
	writeshort(seqBin, l);
	fprintf(treefp, "%-2d} ", i + 1);
	for (j = 0; j < l; j++)
	{
	    fprintf(treefp, "%2d ", tbl[j] + 1);
	    writeshort(seqBin, tbl[j]);
	}
	fprintf(treefp, "\n");
    }
    fclose(seqBin);
}

/***************************************************************************/

/*****************/
/* Tree Building */
/*****************/

static void 
freeNode(short n)
{
    short c, *tbl = treeTable[n].child, ch = treeTable[n].nchild;
    for (c = 0; c < ch; c++)
	if ((--treeTable[tbl[c]].refcnt) == 0)
	    freeNode(tbl[c]);
    freeNodes++;
    if (treeTable[n].child)
	Mem_Free(treeTable[n].child);
    treeTable[n].child = NULL;
#if 0
    treeTable[n].triglist = 0;
#endif
}

static short 
markDefunct(ushort n, ushort equiv)
{
    ushort i, folded = 0;
#ifndef INTEGRATE
    if (n == 0)
	return 0;
    /* Fix up the cross references */
    for (i = 1; i < treeTop; i++)
    {
	short j;
	if (treeTable[i].refcnt == 0)
	    continue;
	for (j = 0; j < treeTable[i].nchild; j++)
	    if (treeTable[i].child[j] == n)
	    {
		treeTable[i].child[j] = equiv;
		treeTable[equiv].refcnt++;
		treeTable[n].refcnt--;
		folded = 1;
	    }
    }
    assert(treeTable[n].refcnt == 0);
    freeNode((short)n);
#endif
    return folded;
}

static short 
compareNodes(short depth, short n1, short n2)
{
    register treeNode *t1, *t2;
    short same = 1;
#ifndef INTEGRATE
    if (n1 == 0 || n2 == 0)
	same = 0;
    /* Both non-null */
    else
    {
	t1 = &treeTable[n1];
	t2 = &treeTable[n2];
	if (t1->seqlen != t2->seqlen)
	    same = 0;
	else
	{
	    if (t1->seq != t2->seq)
		same = 0;
	}
    }
    if (same)
    {				/* compare children */
	short c = t1->nchild;
	if (c != t2->nchild)
	    same = 0;
	else if (depth)
	{
	    int i;
	    for (i = 0; i < c && same; i++)
		same *= compareNodes(depth - 1, t1->child[i], t2->child[i]);
	}
    }
#endif
    return same;
}

/* Add a sequence to the sequence table, or return the index of a
   matching existing sequence */

short 
addSeq(ushort * seq, short seqlen)
{
    short crc = 1, i;
#ifndef INTEGRATE
    if (seqlen == 0)
	return 0;		/* Reserved entry */
    /* Calculate the CRC of the new node */
    for (i = 0; i < seqlen; i++)
	crc *= (1 + seq[i]);
    /* Now compare with the existing nodes */
    for (i = 0; i < seqTop; i++)
    {
	if (crc == seqTable[i].crc)
	{
	    if (seqlen == seqTable[i].seqLen)
	    {
		short j;
		for (j = 0; j < seqlen; j++)
		    if (seq[j] != seqTable[i].seqArray[j])
			break;
		if (j == seqlen)/* Match! */
		    return i;
	    }
	}
    }
    /* If we get here, we didn't find a match. As the e */
    seqTable[seqTop].seqArray = (ushort *) Mem_Calloc(seqlen, sizeof(ushort), 444);
    for (i = 0; i < seqlen; i++)
	seqTable[seqTop].seqArray[i] = seq[i];
    seqTable[seqTop].crc = crc;
    seqTable[seqTop].seqLen = seqlen;
    i = seqTop++;
    if (seqTop > 255) /* ERROR!! */ ;
#endif
    return i;
}

static short 
checkNode(short n)
{
    ushort n2, bestnode = 0, folded = 0;
#ifndef INTEGRATE
    uchar bestdepth = 0;
    if (n == 0)
	goto abortCheck;
    for (n2 = 0; n2 < treeTable[n].nchild; n2++)
	if (treeTable[n].child[n2] == 0)
	    goto abortCheck;
    for (n2 = 1; n2 < treeTop; n2++)
    {
	short c, ready = 1;
	if (treeTable[n2].refcnt == 0 || n2 == n)
	    continue;
	for (c = 0; c < treeTable[n2].nchild; c++)
	    if (treeTable[n2].child[c] == 0)
	    {
		ready = 0;
		break;
	    }
	if (!ready)
	    continue;
	if (compareNodes(compareDepth, n, n2) == 0)
	    continue;
	/* Match found! */
	if (bestnode == 0 || treeTable[n2].depth < bestdepth)
	{
	    bestnode = n2;
	    bestdepth = treeTable[n2].depth;
	}
    }
    if (bestnode && bestdepth < treeTable[n].depth)
    {
	fprintf(treefp, "Merging matched node %d with %d\n", n, bestnode);
	/* Free up node & ancestors and resolve refernces  */
	folded = markDefunct(n, bestnode);
    }
abortCheck:
#endif
    return folded;
}


void 
checkTree()
{
    short done = 0;
#ifndef INTEGRATE
    while (!done)
    {
	ushort n;
	done = 1;
	for (n = 1; n < treeTop; n++)
	{
	    if (treeTable[n].refcnt == 0)
		continue;
	    if (checkNode(n))
		done = 0;
	}
    }
#endif
}

static BOOLEAN 
wrapUpNode(void)
{
    BOOLEAN rtn = FALSE;
    short parent, depth;
    depthNow++;
    if (pathnow == targetNode)
	targetNode = 0;
    if ((treeTable[pathnow].flags & DONE_SEQ) == 0)
    {
	treeTable[pathnow].seqlen = 0;
	if (treeTable[pathnow].seq == 0 && pathpos)
	{
	    /* Make a copy of seq in node */
	    treeTable[pathnow].seq = addSeq(tr_seq, pathpos);
	    treeTable[pathnow].seqlen += pathpos;
	}
	treeTable[pathnow].flags |= DONE_SEQ;
    }
    /*
     * If this results in us being able to merge a node, it must be this one,
     * and we can abort this run.
     */
/*	if (treeTable[pathnow].flags&RIGHTMOST) { */
    if (consecutive_rights >= compareDepth)
    {
	depth = compareDepth;
	parent = pathnow;
	while (depth-- && parent)
	    parent = treeTable[parent].parent;
	if (parent)
	{
	    if (checkNode(parent))
		rtn = TRUE;
	}
    }
    pathpos = 0;
    return rtn;
}

static short 
findNextNode(short target, short *cp)
{
    short c = 0, done = 0;
    while (target && treeTable[target].parent)
    {
	for (c = 0; c < treeTable[pathnow].nchild; c++)
	{
	    if (treeTable[pathnow].child[c] == target)
	    {
		/* Follow this child */
		done = pathnow;
		pathnow = target;
		break;
	    }
	}
	if (!done)
	    target = treeTable[target].parent;
	else
	    break;
    }
    *cp = c;			/* Index of child */
    return done;
}


static void 
makeChildren(short n, short nc, short flag)
{
    treeTable[n].child = (short *)Mem_Calloc(nc, sizeof(short), 543);
    treeTable[n].nchild = nc;
    treeTable[n].flags |= flag;
}

/*
 output_branch is called when we are doing an unreliable OUTPUT.
 If we have been this way before, we
*/
BOOLEAN 
output_branch(BOOLEAN * end_it)
{
    BOOLEAN lose_it = FALSE;
#ifndef INTEGRATE
    short c, newnode = 0;
    /* tie up loose ends with the node */
    if ((*end_it = wrapUpNode()) == TRUE)
	goto done;		/* made a closed loop; try elsewhere */
    /*
     * If we're still heading to the target, we use it to guide our path;
     * else we make a new leaf node
     */
    depth++;
    if (targetNode)
	newnode = findNextNode(targetNode, &c);
    else
    {
	/* Create a child array, if one doesn't already exist, */
	/* and mark this node as a branch due to losses */
	if (treeTable[pathnow].nchild == 0)
	    makeChildren(pathnow, 2, LOSSBRANCH);
	/*
	 * Allocate the new child to the left or right, depending which is
	 * still unexplored
	 */
	for (c = 0; c < 2; c++)
	{
	    if (treeTable[pathnow].child[c] == 0)
	    {
		/* create a new child */
		newnode = allocNode(pathnow, depthNow, TRUE);
		treeTable[pathnow].child[c] = newnode;
		if (c == 1)
		    treeTable[newnode].flags |= RIGHTMOST;
		newnode = pathnow;	/* stay at this node */
		break;
	    }
	}
/*		if (depth>=compareDepth) */
	*end_it = TRUE;		/* terminate this run */
    }
/*	if (newnode==0) Win_Error(NULL,"_Output","Bad or missed target!",TRUE,TRUE); */
    /* if a right child of a loss branch, lose message */
    if (c == 1)
    {
	lose_it = TRUE;
	consecutive_rights++;
    } else
	consecutive_rights = 0;
    assert(c >= 0 && c <= 1 && treeTable[newnode].flags & LOSSBRANCH);
#endif
done:
    return lose_it;
}

/*
 delay_branch is called when the scheduler must advance time. It is
 passed the number of delay transitions the scheduler must choose from,
 and returns a number indicating which one it should be. The scheduler
 maintains an array of triggers, which is used by the routine to set
 the trigger field.
*/

short 
delay_branch(BOOLEAN * end_it, short choices)
{
#ifndef INTEGRATE
    short c, newnode = 0;
    /* tie up loose ends with the node */
    if ((*end_it = wrapUpNode()) == TRUE)
	goto done;		/* made a closed loop; try elsewhere */
    /*
     * If we're still heading to the target, we use it to guide our path;
     * else we make a new leaf node
     */
    depth++;
    if (targetNode)
    {
	newnode = findNextNode(targetNode, &c);
	depth++;
#ifdef TREE_DEBUG
	fprintf(treefp, "delay_branch says follow with node %d\n", newnode);
#endif
    } else
    {
	/* Create a child array, if one doesn't already exist, */
	/* and mark this node as a branch due to losses */
	if (treeTable[pathnow].nchild == 0)
	    makeChildren(pathnow, choices, DELAYBRANCH);
	/* Allocate the new child to an unexplored child entry */
	for (c = 0; c < choices; c++)
	{
	    if (treeTable[pathnow].child[c] == 0)
	    {
		/* create a new child */
		newnode = allocNode(pathnow, depthNow, FALSE);
		treeTable[pathnow].child[c] = newnode;
		if (c == (choices - 1))
		    treeTable[newnode].flags |= RIGHTMOST;
#ifdef TREE_DEBUG
		fprintf(treefp, "delay_output creates new child %d (index %d) of %d\n",
			newnode, c, pathnow);
#endif
		newnode = pathnow;	/* stay at this node */
		break;
	    }
	}
/*		if (depth>=compareDepth) */
	*end_it = TRUE;		/* terminate this run */
    }
/*	if (newnode==0) Win_Error(NULL,"_Output","Bad or missed target!",TRUE,TRUE); */
    assert(newnode);
#endif
    if (c == (choices - 1))
	consecutive_rights++;
    else
	consecutive_rights = 0;
done:
    return c;
}


void 
addToPath(ushort idx, BOOLEAN * end_it)
{
#ifndef INTEGRATE
    int pos = pathpos;
    int s = treeTable[pathnow].seq;
    if (s)
	if (seqTable[s].seqArray[pos] == idx)
	    pathpos++;
	else
	{
	    (void)Win_Error(NULL, "addToPath", "Path conflict with previously existing path!", TRUE, TRUE);
	    fprintf(treefp, "Current path node is %d\n", pathnow);
	    fprintf(treefp, "Current position in path is %d\n", pos + 1);
	    fprintf(treefp, "Stored index is %d\n", seqTable[treeTable[pathnow].seq].seqArray[pos]);
	    fprintf(treefp, "Conflicting index is %d\n", idx);
	    fprintf(treefp, "Target node is %d (parent %d)\n", targetNode, treeTable[targetNode].parent);
	    *end_it = TRUE;
	}
    else if (pos < SEQ_LEN)
    {
	tr_seq[pathpos++] = idx;/* Add to new path */
    } else
	(void)Win_Error(NULL, "Schedule", "Linear path is too long!", TRUE, TRUE);
#endif
}

/* The next routine is called upon system deadlock */

void 
closePath()
{
#ifndef INTEGRATE
    /*
     * create a new, empty node with a single outgoing arc forming a loop in
     * which nothing happens, and then set all the pointers out of the
     * current node to point to this new node.
     */
    short i, newnode = allocNode(pathnow, treeTable[pathnow].depth + 1, FALSE);
    treeTable[newnode].child = (short *)Mem_Calloc(1, sizeof(short), 876);
    treeTable[newnode].child[0] = newnode;
    for (i = 0; i < treeTable[pathnow].nchild; i++)
	treeTable[pathnow].child[i] = newnode;
#endif
}

static void 
checkCandidate(ushort * target, ushort n, ushort * depth)
{
    if (*target == 0)
    {
	*target = n;
	*depth = (short)treeTable[n].depth;
    } else
    {
	if ((short)treeTable[n].depth < *depth)
	{
	    *target = n;
	    *depth = (short)treeTable[n].depth;
	}
    }				/* else */
}

void 
initTreeRun()
{
    depth = 0;
    consecutive_rights = 0;
}

ushort 
getTarget(void)
{
    static char first = 1;
    ushort depth;
    ushort n, target = 0;
    /* Choose a target node */
    if (first)
    {
	target = 1;
	first = 0;
    } else
	for (n = 1; n < treeTop; n++)
	{
	    short c;
	    if (treeTable[n].refcnt == 0)
		continue;
	    if (treeTable[n].nchild == 0)
		checkCandidate(&target, n, &depth);
	    else
		for (c = 0; c < treeTable[n].nchild; c++)
		{
		    if (treeTable[n].child[c] == 0)
		    {
			checkCandidate(&target, n, &depth);
			break;
		    }		/* if (tr */
		}		/* else for (c */
	}			/* for (n */
/*	printf("Target node is %d, depth %d\n",(int)target,(int)treeTable[target].depth);*/
    return target;
}
