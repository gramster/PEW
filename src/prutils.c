#include<stdio.h>
#include<assert.h>
#include <math.h>
#include "prune.h"

/* portable I/O */

static short 
readshort(FILE * fp)
{
    char msb, lsb;
    fread(&msb, 1, sizeof(char), fp);
    fread(&lsb, 1, sizeof(char), fp);
    return (msb << 8) + lsb;
}

/* Sort the link table, using plain old bubble sort */

static void 
Sort(void)
{
    short i, j, tmp0, tmp1;
    puts("Sorting links...");
    for (i = 1; i < (linkTop - 1); i++)
	for (j = i + 1; j < linkTop; j++)
	    if (links[i][0] > links[j][0])
	    {
		tmp0 = links[i][0];
		tmp1 = links[i][1];
		links[i][0] = links[j][0];
		links[i][1] = links[j][1];
		links[j][0] = tmp0;
		links[j][1] = tmp1;
	    }
}

/* Find the new node number corresponding to an old one */

static short 
newnumber(short n)
{
    short bot = 1, top = linkTop - 1, mid;
    if (n == 0)
	return 0;
    while (mid = bot + (top - bot) / 2, links[mid][0] != n)
    {
	if (links[mid][0] < n)
	    bot = mid + 1;
	else
	    top = mid - 1;
    }
    return links[mid][1];
}

/* Add a parent to a child's parent array */

static void 
addParent(short n, short p)
{
    short i, np = treeTable[n].nparent, *newp;
    for (i = 0; i < np; i++)
	if (treeTable[n].parent[i] == p)
	{
	    return;		/* already got it */
	}
    newp = (short *)Mem_Calloc(++treeTable[n].nparent, sizeof(short), 712);
    for (i = 0; i < np; i++)
    {
	newp[i] = treeTable[n].parent[i];
    }
    newp[np] = p;
    if (treeTable[n].parent)
    {
	Mem_Free(treeTable[n].parent);
    }
    treeTable[n].parent = newp;
}

/* Fix up node references using the link table */

static void 
fixup(void)
{
    int n;
    printf("Fixing links");
    fflush(stdout);
    treeTable[1].refcnt = 1;
    for (n = 1; n < linkTop; n++)
    {
	int c, ch;
	short *tbl = treeTable[n].child;
	/* printf("\tNode %d\n",n); */
	putchar('.');
	fflush(stdout);
	ch = treeTable[n].nchild;
	for (c = 0; c < ch; c++)
	{
	    tbl[c] = newnumber(tbl[c]);
	    treeTable[tbl[c]].refcnt++;
	}
	treeTable[n].parent = NULL;
	treeTable[n].nparent = 0;
/*		treeTable[n].trigger = newnumber(treeTable[n].trigger); */
    }
    printf("\n\nAdding backward links");
    fflush(stdout);
    for (n = 1; n < linkTop; n++)
    {
	int c, ch;
	short *tbl = treeTable[n].child;
	putchar('.');
	fflush(stdout);
	ch = treeTable[n].nchild;
	for (c = 0; c < ch; c++)
	    addParent(tbl[c], n);
    }
    printf("\n\nThe following nodes have multiple parents:\n");
    for (n = 1; n < linkTop; n++)
    {
	short p, np = treeTable[n].nparent;
	if (n > 1)
	    assert(np == treeTable[n].refcnt);
	if (np > 1)
	{
	    printf("\nNode %-3d : ", n);
	    for (p = 0; p < np; p++)
		printf("%-3d ", (int)treeTable[n].parent[p]);
	}
    }
    printf("\n\n************************************************\n\n");
}

short 
readNodes(void)
{
    FILE *binTreefp;
    short n = 1;
    puts("Reading nodes");
    treeTable = (treeNode *) Mem_Calloc(MAXNODES, sizeof(treeNode), 66);
    if (treeTable == NULL)
    {
	puts("Cannot allocate graph memory!");
	return 0;
    }
    if ((binTreefp = fopen("tree.bin", "rb")) == NULL)
	return (short)0;
    while (!feof(binTreefp))
    {
	short c, ch;
	links[n][0] = readshort(binTreefp);
	if (links[n][0] == 0 || feof(binTreefp))
	    break;
	links[n][1] = n;
	treeTable[n].refcnt = 0;
	fread(&treeTable[n].flags, 1, sizeof(char), binTreefp);
	fread(&treeTable[n].depth, 1, sizeof(char), binTreefp);
	fread(&treeTable[n].nchild, 1, sizeof(char), binTreefp);
#if 0
	treeTable[n].trigger = readshort(binTreefp);
#endif
	fread(&treeTable[n].seq, 1, sizeof(char), binTreefp);
	/* Allocate memory for children */
	ch = treeTable[n].nchild;
	treeTable[n].child = (short *)Mem_Calloc(ch, sizeof(short), 1108);
	for (c = 0; c < ch; c++)
	    treeTable[n].child[c] = readshort(binTreefp);
	/* Allocate memory for probabilities, delays and trigger history */
	treeTable[n].prob = (float *)Mem_Calloc(ch, sizeof(float), 1);
	treeTable[n].Delay = (float *)Mem_Calloc(ch, sizeof(float), 2);
	n++;
    }
    assert(n <= MAXNODES);
    linkTop = n;
    Sort();
    fixup();
    fclose(binTreefp);
    return n;
}

int 
readTriggers(void)
{
    FILE *fp = fopen("trig.dat", "rt");
    if (fp)
    {
	short i = 0;
	int buf;
	fscanf(fp, "%d", &buf);
	assert(buf <= MAXPARAMS);
	trigTop = buf;
	while (i < trigTop)
	{
	    short l, j;
	    char typ;
	    fscanf(fp, "%d=%c", &buf, &typ);
	    trigTable[i].t = (short)buf - 1;
	    trigTable[i].typ = typ;
	    fscanf(fp, "%d(", &buf);
	    l = trigTable[i].n = (short)buf;
	    trigTable[i].trig = (short *)Mem_Calloc(l, sizeof(short), 529);
	    for (j = 0; j < l; j++)
	    {
		fscanf(fp, "%d", &buf);
		trigTable[i].trig[j] = (short)buf - 1;
	    }
	    while (fgetc(fp) != '\n');
	    i++;
	}
	fclose(fp);
    } else
	return 0;
    return 1;
}

int 
readKillers(void)
{
    FILE *fp = fopen("kill.bin", "rb");
    if (fp)
    {
	short i = 0;
	killTop = readshort(fp);
	assert(killTop <= MAXPARAMS);
	while (i < killTop)
	{
	    short l, j;
	    l = killTable[i].n = readshort(fp);
	    killTable[i].t = readshort(fp);
	    killTable[i].trig = (short *)Mem_Calloc(l, sizeof(short), 529);
	    for (j = 0; j < l; j++)
		killTable[i].trig[j] = readshort(fp);
	    i++;
	}
	fclose(fp);
    } else
	return 0;
    return 1;
}

void 
clearMarks(void)
{
    short i;
    for (i = 1; i < linkTop; i++)
	treeTable[i].flags &= (0xFF ^ MARKED);
}

void 
readSeq(void)
{
    short i;
#ifdef UNIX
    FILE *seqBin = fopen("seq.bin", "r");
#else
    FILE *seqBin = fopen("seq.bin", "rb");
#endif
    puts("Reading sequences");
    if (seqBin == NULL)
    {
	puts("Couldn't open file seq.bin");
	exit(0);
    }
    seqTop = readshort(seqBin);
    assert(seqTop <= MAX_SEQ);
    printf("There are %d sequences\n", seqTop);
    for (i = 0; i < seqTop; i++)
    {
	ushort *tbl = 0;
	short j, l;
	l = readshort(seqBin);
	seqTable[i].seqLen = l;
	printf("Sequence %d has length %d\n", i + 1, l);
	if (l)
	    tbl = seqTable[i].seqArray = (ushort *) Mem_Calloc(l, sizeof(short), 1212);
	else
	    seqTable[i].seqArray = NULL;
	for (j = 0; j < l; j++)
	{
	    tbl[j] = readshort(seqBin);
	}
    }
    fclose(seqBin);
}


short 
readParams(void)
{
    short i;
#ifdef UNIX
    FILE *pfp = fopen("param.dat", "r");
#else
    FILE *pfp = fopen("param.dat", "rt");
#endif
    (void)sin(0.);
    puts("Reading parameters");
    if (pfp == NULL)
	return 0;
    paramTop = delayTop = 0;
    for (i = 0; i < MAXTRANS; i++)
    {
	isLoss[i] = isDelay[i] = 0;
    }
    while (!feof(pfp))
    {
	char c;
	int t, d;
	float p;
	fscanf(pfp, "%c%d ", &c, &t);
	if (feof(pfp))
	    break;
	t--;
	if (c == 'L')
	{
	    fscanf(pfp, "%f", &p);	/* loss probability */
	    d = 0;
	    isLoss[t] = 1;
	} else if (c == 'D')
	{
	    fscanf(pfp, "%d", &d);	/* delay value */
	    p = 0.;
	    isDelay[t] = delayTop + 1;
	    delayTrans[delayTop] = t;
	    delayVals[delayTop++] = d;
	    assert(delayTop <= MAX_DELAYS);
	} else if (c == 0xA)
	    continue;
	else
	{
	    fprintf(stderr, "Bad parameter %c%d\n", c, t);
	    break;
	}
	params[paramTop].trans = t;
	params[paramTop].type = c;
	params[paramTop].Delay = d;
	params[paramTop].prob = p;
	paramTop++;
	assert(paramTop <= MAXPARAMS);
	while (fgetc(pfp) != 0xA && !feof(pfp));
    }
    fclose(pfp);
    return paramTop;
}

void 
printNode(FILE * fp, short n)
{
#ifndef INTEGRATE
    int i;
    short lim, si = treeTable[n].seq;
    if (n == 0)
	return;
    if (treeTable[n].refcnt == 0)
	return;
#if 0
    fprintf(fp, "%3d>%c T%-3d [", n, (treeTable[n].flags & LOSSBRANCH) ? 'L' : 'D',
	    treeTable[n].trigger + 1);
#else
    fprintf(fp, "%3d>%c [", n, (treeTable[n].flags & LOSSBRANCH) ? 'L' : 'D');
#endif
    for (i = 0; i < treeTable[n].nchild; i++)
    {
	if (i)
	    fprintf(fp, ",");
	fprintf(fp, "%3d", treeTable[n].child[i]);
    }
    fprintf(fp, "] ( ");
    lim = seqTable[si].seqLen;
/*	fprintf(fp, "S%-2d ",si+1); */
    if (lim)
    {
	int j;
	ushort *tbl = seqTable[si].seqArray;
	for (j = 0; j < lim; j++)
	{
	    fprintf(fp, "%-2d ", tbl[j] + 1);
	}
    }
    fprintf(fp, "\tPrb: ");
    for (i = 0; i < treeTable[n].nchild; i++)
	fprintf(fp, "%5.3f  ", treeTable[n].prob[i]);
    fprintf(fp, "\tDly: ");
    for (i = 0; i < treeTable[n].nchild; i++)
	fprintf(fp, "%-3f  ", treeTable[n].Delay[i]);
    fprintf(fp, ")\n");
#endif
}

static void 
dumpTreeRecursively(FILE * fp, short n)
{
#ifndef INTEGRATE
    int i;
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
    dumpTreeRecursively(fp, n);
}

static void 
markTree(short n)
{
    short c;
    if (treeTable[n].flags & MARKED)
	return;
    treeTable[n].flags |= MARKED;
    printf("Marking node %d\n", (int)n);
    for (c = 0; c < treeTable[n].nchild; c++)
	if (treeTable[n].prob[c] > 0.)
	    markTree(treeTable[n].child[c]);
}

static short 
markDefunct(short n, short equiv)
{
    int i;
    short folded = 0;
    /* Fix up the cross references */
    for (i = 1; i < linkTop; i++)
    {
	int j;
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
/*	freeNode(n);*/
    return folded;
}

static short 
compareNodes(short depth, short n1, short n2)
{
    register treeNode *t1, *t2;
    short same = 1;
#ifndef INTEGRATE
    t1 = &treeTable[n1];
    t2 = &treeTable[n2];
    if (t1->seq != t2->seq)
	same = 0;
    if (same)
    {				/* compare children */
	short c = t1->nchild;
	if (c != t2->nchild)
	    same = 0;
	else if (depth)
	{
	    int i;
	    for (i = 0; i < c && same; i++)
	    {
		float dif = t1->prob[i] - t2->prob[i];
		if (dif < 0.)
		    dif = -dif;
		if (dif > .001)
		    same = 0;
		else if (t1->Delay[i] != t2->Delay[i])
		    same = 0;
		else
		    same *= compareNodes(depth - 1, t1->child[i], t2->child[i]);
	    }
	}
    }
#endif
    return same;
}

static short 
checkNode(short n)
{
    short n2, bestnode = 0, folded = 0;
    extern short comparedepth;
#ifndef INTEGRATE
    uchar bestdepth = 0;
    for (n2 = 1; n2 < linkTop; n2++)
    {
	if (treeTable[n2].refcnt == 0 || n2 == n)
	    continue;
	if (compareNodes(comparedepth, n, n2) == 0)
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
	printf("Merging matched node %d with %d\n", n, bestnode);
	/* Free up node & ancestors and resolve references  */
	folded = markDefunct(n, bestnode);
    }
#endif
    return folded;
}

static char ch[4] = {'/', '-', '\\', '|'};

static void near 
flash(void)
{
    static int i = 0;
    fputc(8, stderr);
    fputc(ch[i++], stderr);
    i %= 4;
    fflush(stderr);
}

void 
checkTree(void)
{
    /* look for matching nodes and merge */
    short done = 0, i;
    for (i = 1; i < linkTop; i++)
    {
	short c, ch = treeTable[i].nchild;
	for (c = 0; c < ch; c++)
	    if (treeTable[i].prob[c] == 1.)
	    {
		/* If only child, make leftmost */
		treeTable[i].prob[c] = 0.;
		treeTable[i].prob[0] = 1.;
		treeTable[i].child[0] = treeTable[i].child[c];
		if ((treeTable[i].Delay[0] = treeTable[i].Delay[c]) == 0)
		    if (treeTable[treeTable[i].child[0]].refcnt == 1)
		    {
			/*
			 * No other paths to child, and zero delay, so
			 * combine the nodes
			 */

		    }
		treeTable[i].nchild = 1;
		break;
	    }
    }
    clearMarks();
    markTree(1);
    for (i = 1; i < linkTop; i++)
	if ((treeTable[i].flags & MARKED) == 0)
	    treeTable[i].refcnt = 0;
    printf("Checking for matching nodes:  ");
    fflush(stdout);
    while (!done)
    {
	short n;
	done = 1;
	for (n = 1; n < linkTop; n++)
	{
	    if (treeTable[n].refcnt == 0)
		continue;
	    flash();
	    if (checkNode(n))
		done = 0;
	}
    }
}
