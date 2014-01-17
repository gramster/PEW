/* Exponential/Poisson Distribution Utilities */
/**********************************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define LIMIT 1000l

#ifdef UNIX
extern double drand48();
#else
#define drand48()	( ((double)rand()) / ((double)RAND_MAX) )
#endif

double 
myrandom(short range)
{
    return (double)(drand48() * (double)range);
}


short 
Dis_Exponential(short Min, short Max, short mean)
{
    short rtn;
    double Mean = (double)mean;
    do
    {
	double tmp = (1. + myrandom(1000)) / 1000.;
	rtn = (short)(-Mean * log(tmp));
    } while (rtn < Min || rtn > Max);
    return rtn;
}

short 
Dis_Poisson(short Min, short Max, short mean)
{
    double b = exp((double)-mean), tr;
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

short 
Dis_Geometric(short Min, short Max, short mean)
{
    short rtn;
    float expekt = log(1. - 1. / ((float)mean));
    do
    {
	rtn = (short)(log(.0001 + drand48()) / expekt);
    } while (rtn < Min || rtn > Max);
    return rtn;
}

/***************/

int bucket[1000];

void 
useage(void)
{
    fprintf(stderr, "Usage: probtest (E|P|G) <min> <mean> <max>\n");
    exit(0);
}

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
    long j;
    int i, Min, Max, Mean, type;
    float splitcnt;
    if (argc != 5)
	useage();
    else
    {
	if (strcmp(argv[1], "E") == 0)
	    type = 1;
	else if (strcmp(argv[1], "P") == 0)
	    type = 2;
	else if (strcmp(argv[1], "G") == 0)
	    type = 3;
	else
	    useage();
	Min = atoi(argv[2]);
	Mean = atoi(argv[3]);
	Max = atoi(argv[4]);
	if (Max < Min || Max < Mean || Min > Mean)
	    useage();
    }
    for (i = 0; i < 1000; i++)
	bucket[i] = 0;
    j = LIMIT;
    if (type == 1)
	while (j--)
	    bucket[Dis_Exponential((short)Min, (short)Max, (short)Mean)]++;
    else if (type == 2)
	while (j--)
	    bucket[Dis_Poisson((short)Min, (short)Max, (short)Mean)]++;
    else if (type == 3)
	while (j--)
	    bucket[Dis_Geometric((short)Min, (short)Max, (short)Mean)]++;
    splitcnt = 0.;
    printf("\n\nPROBABILITY DISTRIBUTION\n=========== ============\n\n");
    for (i = 1; i < 1000; i++)
    {
	if (bucket[i])
	    printf("%3d %.3f\n", i, ((float)bucket[i]) / ((float)LIMIT));
    }
    splitcnt = 0.;
    printf("\n\n\nPROBABILITY DENSITY FUNCTION\n=========== ======= ========\n\n");
    for (i = 1; i < 1000; i++)
    {
	if (bucket[i])
	{
	    splitcnt += ((float)bucket[i]) / ((float)LIMIT);
	    printf("P[t<=%-3d] = %.3f\n", i, splitcnt);
	}
    }
}
