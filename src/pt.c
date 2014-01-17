#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG

FILE *ifp;

/*
 * set type
 */

static int si;			/* for use in clearSet macro only */

#define MAXSETELTS	128
#define SETLONGS	(MAXSETELTS/(sizeof(long)*8))

typedef unsigned long set[SETLONGS];

#define add2set(s,n)	s[n/8] |= (1<<(n%8))
#define inSet(s,n)	(s[n/8] & (1<<(n%8)))
#define clearSet(s)	for (si=0;si<SETLONGS;si++) s[si]=0l

set idx;

/**********************************************************************
  LOOKUP TABLES. These tables are used to map transition meta-
	indices and enabled dtrans combos to consecutive local
	indices
**********************************************************************/

/* We assume there are no more than 64 different combinations of
	delay transitions enabled at any time */

#define MAXDCOMB	32

short dflags[MAXDCOMB];
int dc = 0;

#define MAXDTRANS	16	/* Max number of delayed transitions */
short dti2dtm[MAXDTRANS];	/* holds meta-indices of the delay
				 * transitions; positions in this table
				 * correspond to bits in dflag in the
				 * schedInfo structure */
int ndti = 0;

#define MAXTRANS	50	/* Max number of transitions in mats */
short ti2tm[MAXTRANS];		/* holds meta-indices of the all the
				 * transitions */
int nti = 0;

short 
lookup(short tbl[], short v, int *top, int maxElt, char *type)
{
    int i;
    for (i = 0; i < (*top); i++)
	if (tbl[i] == v)
	    return i;
    if ((*top) < maxElt)
	(*top)++;
    else
    {
	fprintf(stderr, "Too many %s!\n", type);
	exit(-1);
    }
    tbl[i] = v;
    return i;
}

short 
getDTidx(short t)
{
    return lookup(dti2dtm, t, &ndti, MAXDTRANS, "delay transitions");
}

short 
getidx(short t)
{
    return lookup(ti2tm, t, &nti, MAXTRANS, "transitions");
}

short 
getFlag(short df)
{
    return lookup(dflags, df, &dc, MAXDCOMB, "delay combinations");
}

/***************
 * trace data  *
 ***************/

#define MAXTRACE	12000	/* Max number of transition executions */
#define MAXSCHED	2500	/* Max number of delay transition executions */

/* flags for hi-bits of index trace */

#define F_OUT_FAIL	0x8000
#define F_OUT_OK	0x4000

short indices[MAXTRACE] = {0};	/* the transition sequence */

typedef struct
{
    short time;
    short offset;		/* into the indices table above */
    short dflag;		/* index of flags for which delay transitions
				 * are enabled */
}   schedInfo_t;

schedInfo_t schedInfo[MAXSCHED] = {{0, 0, 0}};

int tlen = 0, slen = 0;		/* Lengths of the above tables */

int addT = 0, lastIdx, line = 0;

/******************
  MODEL MATRICES
*******************/

int seqMat[MAXTRANS][MAXTRANS], transTot[MAXTRANS];
float delayMat[MAXTRANS][MAXTRANS];

static void 
useage(void)
{
    fprintf(stderr, "Useage: pt [ -X ] <tracefile> <range> [ <range>... ]\n");
    fprintf(stderr, "\twhere <range> is a meta-index or two meta-indices\n");
    fprintf(stderr, "\tseparated by a minus (but no space)\n");
    fprintf(stderr, "\tThe -X option causes extra transitions to be created\n");
    exit(-1);
}

static void 
badFile(void)
{
    fprintf(stderr, "Bad execution trace file!\n");
    exit(-1);
}

int 
parseTransList(int sl, char *buff)
{
    int i = 0, l, t;
    short df = 0;
    l = strlen(buff);
    while (i < l)
    {
	if (buff[i] != 't')
	    i++;
	else
	{
	    t = atoi(buff + (++i));
	    df |= (1 << getDTidx((short)t));
	    while (buff[i] != 't' && i < l)
		i++;
	}
    }
    schedInfo[sl].dflag = i = getFlag(df);
    return i;
}

void 
main(int argc, char *argv[])
{
    int i, j, last_i, last_t, this_i, this_t;
    char buff[80];
    if (argc < 3)
	useage();
    i = 1;
    while (argv[i][0] == '-')
    {
	switch (argv[i][1])
	{
	case 'X':
	    addT = 1;
	    break;
	default:
	    useage();
	}
	i++;
    }
    ifp = fopen(argv[i], "r");
    if (ifp == NULL)
    {
	fprintf(stderr, "Cannot open trace file '%s'!\n", argv[i]);
	exit(-1);
    } else
	i++;
    clearSet(idx);
    for (; i < argc; i++)
    {
	int start, end, n;
	n = sscanf(argv[i], "%u-%u", &start, &end);
	if (n == 1)
	    end = start;
	else if (n < 1)
	{
	    fprintf(stderr, "bad range specifier '%s'!\n", argv[i]);
	    exit(-1);
	}
	for (; start <= end; start++)
	    add2set(idx, start);
    }
    fgets(buff, 80, ifp);
    line++;
    if (sscanf(buff, "%d", &lastIdx) != 1)
	badFile();
    while (!feof(ifp))
    {
	int id;
	char f;
	fgets(buff, 80, ifp);
	line++;
	if (buff[0] == '$')
	{
	    int idx;
	    sscanf(buff + 2, "%*d + %*d %d", &schedInfo[slen].time);
	    fgets(buff, 80, ifp);
	    line++;
	    idx = parseTransList(slen, buff);
	    if (addT)
		indices[tlen++] = 200 + idx;
	    schedInfo[slen].offset = tlen;
	    if (slen == 0 || addT || schedInfo[slen - 1].offset != tlen)
		slen++;
	    if (slen >= MAXSCHED)
	    {
		fprintf(stderr, "Out of scheduler records at time %d!\n",
			schedInfo[slen - 1].time);
		exit(-1);
	    }
	} else
	{
	    short fl = 0;
	    if (sscanf(buff, "%*d t%d%c", &id, &f) != 2)
	    {
		if (!feof(ifp))
		    fprintf(stderr, "Warning - bad input on line %d ignored\n", line);
		continue;
	    }
	    if (f == '+')
		fl = F_OUT_OK;
	    else if (f == '-')
		fl = F_OUT_FAIL;
	    if (inSet(idx, id) || (addT && fl))
	    {
		indices[tlen++] = fl | (unsigned short)id;
		if (addT && fl)
		{
		    indices[tlen++] = 300 + 2 * id +
			((fl == F_OUT_OK) ? 0 : 1);
		}
	    }
	}
    }
#ifdef DEBUG
    /*
     * debug for now
     */
    for (i = j = 0; i < tlen;)
    {
	printf("%8d %d\n", schedInfo[j].time, indices[i] & 0xFFF);
	i++;
	if (schedInfo[j + 1].offset == i && j < slen)
	    j++;
    }

#endif
    printf("\n\nDelay Combinations\n");
    for (i = 0; i < dc; i++)
    {
	printf("t%-3d: ", 200 + i);
	for (j = 0; j < 32; j++)
	    if (dflags[i] & (1 << j))
		printf("t%d ", dti2dtm[j]);
	printf("\n");
    }

    /* Now we compute the matrices */

    for (i = 0; i < MAXTRANS; i++)	/* clear */
	for (j = 0; j < MAXTRANS; j++)
	{
	    delayMat[i][j] = 0.;
	    seqMat[i][j] = 0;
	}
    for (i = j = 0; i < tlen;)
    {
	this_t = schedInfo[j].time;
	this_i = getidx(indices[i] & 0xFFF);
	if (i)
	{
	    seqMat[last_i][this_i]++;
	    delayMat[last_i][this_i] += (float)(this_t - last_t);
	}
	last_t = this_t;
	last_i = this_i;
	i++;
	if (schedInfo[j + 1].offset == i && j < slen)
	    j++;
    }
    /* Compute row totals */
    for (i = 0; i < nti; i++)
    {
	transTot[i] = 0;
	for (j = 0; j < MAXTRANS; j++)
	    transTot[i] += seqMat[i][j];
    }
    printf("\n\nImplicit Loss Transitions\n");
    for (i = 0; i < nti; i++)
    {
	int t = ti2tm[i];
	if (t >= 300)
	    printf("t%-3d is for t%-2d with%s loss\n",
		   t, (t - 300) / 2, (t % 2) ? "" : "out");
    }
    /* Print stochastic matrix */
    printf("\n\n\nStochastic Matrix\n\n   ");
    for (i = 0; i < nti; i++)
	printf("%6d", ti2tm[i]);
    printf("\n");
    for (i = 0; i < nti; i++)
	printf("======", ti2tm[i]);
    printf("======\n");
    for (i = 0; i < nti; i++)
    {
	printf("%-4d :", ti2tm[i]);
	for (j = 0; j < nti; j++)
	{
	    float t = (float)seqMat[i][j];
	    if (transTot[i])
		t /= (float)transTot[i];
	    printf("%6.3f", t);
	}
	printf("\n");
    }
    /* Print delay matrix */
    printf("\n\n\nHolding Time Matrix\n\n   ");
    for (i = 0; i < nti; i++)
	printf("%6d", ti2tm[i]);
    printf("\n");
    for (i = 0; i < nti; i++)
	printf("======", ti2tm[i]);
    printf("======\n");
    for (i = 0; i < nti; i++)
    {
	printf("%-4d :", ti2tm[i]);
	for (j = 0; j < nti; j++)
	{
	    float t = delayMat[i][j];
	    if (seqMat[i][j])
		t /= (float)seqMat[i][j];
	    printf("%6.3f", t);
	}
	printf("\n");
    }
}
