/* gmt.c

   This program reads the MAT file produced by the PEW which contains
   the execution count matrices, and prints them out in the form of TRGs.
   Delays are not accounted for in this process. A TRG can also be
   solved. If a -T flag is used, the program attempts to read a file
   called MAT.txt with the format:
	<n>
	<n probabilities> <delay>
	...
	<n probabilities> <delay>

   That is, the size of the stochastic matrix, and the matrix itself
   including delays. This matrix is then solved.

*/

#include <stdlib.h>
#include <stdio.h>
#ifndef UNIX
#include <malloc.h>
#endif
#include <math.h>
#include "pa.h"

float **BIGTRG = NULL;
static short **GMT = NULL;
static short GMTrows, procs;
struct proc *proctable;
FILE *ofile;

static void **
MatAlloc(short nrows, int size)
{
    short i;
    char *mem = (char *)calloc((int)(nrows * nrows), size), **ndx = (char **)calloc((int)nrows, sizeof(char *));
    if (mem == NULL || ndx == NULL)
    {
	fprintf(stderr, "Allocation of matrix failed!\n");
	exit(0);
    }
    for (i = 0; i < nrows; i++)
    {
	ndx[i] = mem;
	mem += nrows * size;
    }
    return (void **)ndx;
}

static void 
MatFree(void **m)
{
    free(*m);
    free(m);
}

static int 
GMTload(void)
{
    short i, j = 0;
    FILE *fp = fopen("MAT", "rb");
    if (fp == NULL)
    {
	fputs("Cannot open MAT file!\n", stderr);
	return 0;
    }
    fread(&procs, 1, sizeof(short), fp);
    proctable = (struct proc *)calloc((int)procs, sizeof(struct proc));
    for (i = 0; i < procs; i++)
    {
	short n;
	fread(proctable[i].name, 40, 1, fp);
	fread(&proctable[i].nt, 1, 2, fp);
	fread(&proctable[i].mi, 1, 2, fp);
	printf("%s has %d transitions starting from %d\n",
	       proctable[i].name,
	       (int)(n = proctable[i].nt),
	       (int)(proctable[i].mi));
	if (feof(fp))
	{
	    fputs("Illegal MAT file!\n", stderr);
	    return 0;
	}
	/* Read the delays */
	if (n)
	{
	    proctable[i].delays = (float *)calloc((int)n, sizeof(float));
	    j = 0;
	} else
	    proctable[i].delays = NULL;
	while (n--)
	{
	    float d;
	    fread(&d, 1, sizeof(float), fp);
	    proctable[i].delays[j++] = d;
	}
    }
    fread(&GMTrows, 1, sizeof(short), fp);
    printf("GMT has %d rows\n", (int)GMTrows);
    GMT = (short **)MatAlloc(GMTrows, sizeof(short));
    BIGTRG = (float **)MatAlloc(GMTrows, sizeof(float));
    fread(GMT[0], 1, sizeof(short) * GMTrows * GMTrows, fp);
    fclose(fp);
    return 1;
}

char line[82];

static int 
GMTTXTload(void)
{
    short i, j;
    int nt;
    float *delays;
    FILE *fp = fopen("MAT.txt", "rt");
    if (fp == NULL)
    {
	fputs("Cannot open MAT.txt file!\n", stderr);
	return 0;
    }
    fscanf(fp, "%d", &nt);
    GMTrows = nt;
    BIGTRG = (float **)MatAlloc(GMTrows, sizeof(float));
    delays = (float *)calloc(nt, sizeof(float));
    for (i = 0; i < GMTrows; i++)
    {
	for (j = 0; j < GMTrows; j++)
	{
	    fscanf(fp, "%f", &BIGTRG[i][j]);
	}
	delays[i] = 0;
	for (j = 0; j < GMTrows; j++)
	{
	    float d;
	    fscanf(fp, "%f", &d);
	    delays[i] += d * BIGTRG[i][j];
	}
    }
    for (;;)
    {
	fgets(line, 80, fp);
	if (feof(fp))
	    break;
	fputs(line, ofile);
    }
    fclose(fp);
    textsolve(ofile, BIGTRG, delays, GMTrows);
    free(delays);
    return 1;
}

static void 
GMTdump(FILE * fp, short **t, short nrows)
{
    short i, j;
    for (i = 1; i <= nrows; i++)
	fprintf(fp, "%4d ", (int)i);
    fprintf(fp, "\n     ");
    i = nrows;
    while (i--)
	fprintf(fp, "=====");
    for (i = 0; i < nrows; i++)
    {
	fprintf(fp, "\n%4d:", (int)(i + 1));
	for (j = 0; j < nrows; j++)
	    fprintf(fp, "%4d:", (int)(t[i][j]));
    }
}

static void 
TRGdump(int f)
{
    float TRG[50][50];
    int sum[50], i, j;
    int p, r, c;
    for (p = 0; p < procs; p++)
    {
	int start, end;
	start = proctable[p].mi - 1;
	end = start + proctable[p].nt;
	if (f)
	    printf("\n\nTRG for Process %s\n\n", proctable[p].name);
	for (r = start, i = 0; r < end; r++, i++)
	{
	    sum[i] = 0;
	    for (c = start; c < end; c++)
		sum[i] += GMT[r][c];
	}
	if (f)
	    printf("     ");
	for (r = start, i = 0; r < end; r++, i++)
	{
	    if (sum[i] == 0)
		continue;
	    if (f)
		printf("%-6d", r + 1);
	    for (c = start, j = 0; c < end; c++, j++)
		BIGTRG[r][c] = TRG[i][j] = ((float)GMT[r][c]) / ((float)sum[i]);
	}
	if (f)
	{
	    printf("\n");
	    for (r = start, i = 0; r < end; r++, i++)
	    {
		if (sum[i] == 0)
		    continue;
		printf("%-3d", r + 1);
		for (c = start, j = 0; c < end; c++, j++)
		    if (sum[j] != 0)
			printf("%-.3f ", TRG[i][j]);
		printf("\n");
	    }
	}
    }
}

static int 
getProcess(void)
{
    int i;
retry:
    puts("Select process:");
    for (i = 0; i < procs; i++)
	printf("    %d - %s\n", i + 1, proctable[i].name);
    scanf("%d", &i);
    i--;
    if (i < 0 || i >= procs)
    {
	fputs("Bad selection!\n", stderr);
	goto retry;
    }
    return i;
}

static void 
getRange(int p, int *s, int *e)
{
    printf("Process %s has %d transitions\n", proctable[p].name, proctable[p].nt);
retry1:
    printf("   Enter starting transition of TRG: ");
    fflush(stdout);
    scanf("%d", s);
    if (*s < 1 || *s > proctable[p].nt)
	goto retry1;
retry2:
    printf("   Enter ending transition of TRG: ");
    fflush(stdout);
    scanf("%d", e);
    if (*e < *s || *e > proctable[p].nt)
	goto retry2;
    (*e)--;
    (*s)--;
}

static void 
doTRG(int p)
{
    int start, end;
    getRange(p, &start, &end);
    solve(p, start, end);
}

#ifndef UNIX
static void forcefloat(float *p)
{
    float f = *p;
    forcefloat(&f);
}
#endif

char *USAGE[] = {
    "PEW Performance Analyser\n\nThis program solves matrices of linear equations representing\n",
    "\ta Markov process, using the method of Gaussian elimination.\n\n",
    "Usage: pa [ (-O | -A) [ <output file> ]]\n",
    "\nDefault operation is interactive, based on MAT file\n",
    "\nIf -O or -A is used, MAT.TXT is used instead and solved\n",
    "\nMAT.TXT should have the form:\n\n\t<dimension>\n\t<prob row> <delay row>\n\t...\n\t<prob row> <delay row>\n",
    "\n-O truncates the output file, while -A appends to it.\n",
    NULL
};

int 
main(int argc, char *argv[])
{
    ofile = stdout;
    if (argc == 1)
    {
	if (GMTload())
	{
	    int opt;
    retry:
	    puts("Select an option:");
	    puts("    1 - print all TRGs in a readable form");
	    puts("    2 - solve a TRG");
	    puts("    3 - print MAT in readable form");
	    puts("    0 - quit");
	    scanf("%d", &opt);
	    switch (opt)
	    {
	    case 1:
		TRGdump(1);
		break;
	    case 2:
		TRGdump(0);
		doTRG(getProcess());
		break;
	    case 3:
		GMTdump(stdout, GMT, GMTrows);
		break;
	    case 0:
		break;
	    default:
		fputs("Invalid option!\n", stderr);
		goto retry;
	    }
	    if (GMT)
		MatFree((void **)GMT);
	    if (BIGTRG)
		MatFree((void **)BIGTRG);
	}
    } else
    {
	/* read MAT.txt */
	if (argv[1][0] != '-' || (argv[1][1] != 'A' && argv[1][1] != 'O'))
	{
	    int i = 0;
	    while (USAGE[i])
		fprintf(stderr, USAGE[i++]);
	    exit(0);
	}
	if (argc == 3)
	    if ((ofile = fopen(argv[2], argv[1][1] == 'A' ? "a" : "w")) == NULL)
		ofile = stdout;
	if (GMTTXTload())
	    if (BIGTRG)
		MatFree((void **)BIGTRG);
	if (stdout != ofile)
	    fclose(ofile);
    }
    return 0;
}
