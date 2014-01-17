/* join.c - build a joint pdf */

#include <stdlib.h>
#include <stdio.h>

#define LIMIT 50

float dat1[LIMIT], dat2[LIMIT], prod[LIMIT][LIMIT];
float sum;

static void forcefloat(float *p)
{
    float f = *p;
    forcefloat(&f);
}

void 
main(argc, argv)
    int argc;
    char *argv[];
{
    int i = 0, j, k, lim;
    FILE *fp;
    if (argc == 1)
	fp = stdin;
    else
	fp = fopen(argv[1], "r");
    if (fp == NULL)
	exit(0);
    for (;;)
    {
	float t1, t2;
	fscanf(fp, "%f %f", &t1, &t2);
	if (feof(fp))
	    break;
	dat1[i] = t1;
	dat2[i] = t2;
	i++;
    }
    lim = i;
    if (fp != stdin)
	fclose(fp);
    for (i = 0; i < lim; i++)
	for (j = 0; j < lim; j++)
	    prod[i][j] = dat1[i] * dat2[j];
    sum = 0.;
    for (k = 0; k < (lim + lim - 1); k++)
    {
	float t = 0.;
	for (i = 0; (i <= k) && (i < lim); i++)
	{
	    j = k - i;
	    if (j >= lim)
		continue;
	    t += prod[i][j];
	}
	sum += t;
	printf("%3d  %.3f   P[t<=%3d] = %.3f\n", k + 2, t, k + 2, sum);
    }
}
