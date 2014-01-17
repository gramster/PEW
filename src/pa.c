/* Driver program for subroutine GAUSSJ */
/* This version reads a file with the format:
	<size>
	<size> rows of <size+1> coefficients representing the matrix
		and the mean delay of that transition.
   The diagonal has 1 subtracted from each entry, and the RHS vector is set
	to all zeroes.
   The inverse and solution vector are printed out, followed by a check
	of the results.

   NOTE: this version expects delyas as well, and makes use of them.
*/

#include <stdio.h>
#include "nr.h"
#include "nrutil.h"
#include "pa.h"

#define NP 20

void 
solve(int p, int start, int end)
{
    int j, k, l, n, base;
    float **a, **ai, **u, **b, **x, **t, **del;

    a = matrix(1, NP, 1, NP);
    ai = matrix(1, NP, 1, NP);
    u = matrix(1, NP, 1, NP);
    b = matrix(1, NP, 1, 1);
    del = matrix(1, NP, 1, 1);
    x = matrix(1, NP, 1, 1);
    t = matrix(1, NP, 1, 1);
    n = end - start + 1;
    base = proctable[p].mi - 1 + start;
    for (k = 1; k <= n; k++)
    {
	for (l = 1; l <= n; l++)
	    a[l][k] = BIGTRG[base + k - 1][base + l - 1];
	del[k][1] = proctable[p].delays[start + k - 1];
    }
    /* Set up extra equation constraining solutions to add up to 1 */
    n++;
    for (k = 1; k <= n; k++)
    {
	a[n][k] = a[k][n] = 1.;	/* last row and column vectors */
	b[k][1] = 1.;		/* RHS */
    }
    b[n][1] = 2.;
    /* Subtract 1 on diagonal */
    for (k = 1; k < n; k++)
	a[k][k] -= 1.;
    /* Print matrix  and RHS */
#ifdef DEBUG
    for (k = 1; k <= n; k++)
    {
	for (l = 1; l <= n; l++)
	    printf("%5.3f ", a[k][l]);
	printf("   X%-2d     %5.3f\n", k, b[k][1]);
    }
#endif
    /* save matrices for later testing of results */
    for (l = 1; l <= n; l++)
    {
	for (k = 1; k <= n; k++)
	    ai[k][l] = a[k][l];
	x[l][1] = b[l][1];
    }
    /* invert matrix */
    gaussj(ai, n, x, 1);
#ifdef DEBUG
    printf("\nInverse of matrix a : \n");
    for (k = 1; k <= n; k++)
    {
	for (l = 1; l <= n; l++)
	    printf("%12.6f", ai[k][l]);
	printf("\n");
    }
    /* check inverse */
    printf("\na times a-inverse:\n");
    for (k = 1; k <= n; k++)
    {
	for (l = 1; l <= n; l++)
	{
	    u[k][l] = 0.0;
	    for (j = 1; j <= n; j++)
		u[k][l] += (a[k][j] * ai[j][l]);
	}
	for (l = 1; l <= n; l++)
	    printf("%12.6f", u[k][l]);
	printf("\n");
    }
#endif
    /* check vector solutions */
    printf("\nCheck the following for equality:\n");
    printf("%21s %14s\n", "original", "matrix*sol'n");
    for (k = 1; k <= n; k++)
    {
	t[k][1] = 0.0;
	for (j = 1; j <= n; j++)
	    t[k][1] += (a[k][j] * x[j][1]);
	printf("%8s %12.6f %12.6f\n", " ",
	       b[k][1], t[k][1]);
    }
    printf("\nSolution:\n");
    for (j = 1; j < n; j++)
	printf("X%d = %12.6f\n", j, x[j][1]);
    printf("\n\nSolution scaled by delays\n");
    {
	float tm = 0.;
	for (j = 1; j < n; j++)
	    tm += del[j][1] * x[j][1];
	printf("\nReference time is %6.4f\n", tm);
	for (j = 1; j < n; j++)
	    x[j][1] /= tm;
    }
    for (j = 1; j < n; j++)
	printf("X%d = %12.6f\n", j, x[j][1]);

/*	free_matrix(t,1,NP,1,1); */
    free_matrix(x, 1, NP, 1, 1);
    free_matrix(b, 1, NP, 1, 1);
    free_matrix(u, 1, NP, 1, NP);
    free_matrix(ai, 1, NP, 1, NP);
    free_matrix(a, 1, NP, 1, NP);
}

void 
textsolve(FILE * fp, float **StochMat, float *Delays, int n)
{
    int j, k, l;
    float **a, **ai, **u, **b, **x, **t, **del;

    a = matrix(1, NP, 1, NP);
    ai = matrix(1, NP, 1, NP);
    u = matrix(1, NP, 1, NP);
    b = matrix(1, NP, 1, 1);
    del = matrix(1, NP, 1, 1);
    x = matrix(1, NP, 1, 1);
    t = matrix(1, NP, 1, 1);
    for (k = 1; k <= n; k++)
    {
	for (l = 1; l <= n; l++)
	    a[l][k] = StochMat[k - 1][l - 1];
	del[k][1] = Delays[k - 1];
    }
    /* Set up extra equation constraining solutions to add up to 1 */
    n++;
    for (k = 1; k <= n; k++)
    {
	a[n][k] = a[k][n] = 1.;	/* last row and column vectors */
	b[k][1] = 1.;		/* RHS */
    }
    b[n][1] = 2.;
    /* Subtract 1 on diagonal */
    for (k = 1; k < n; k++)
	a[k][k] -= 1.;
    /* Print matrix  and RHS */
#ifdef DEBUG
    for (k = 1; k <= n; k++)
    {
	for (l = 1; l <= n; l++)
	    fprintf(fp, "%5.3f ", a[k][l]);
	fprintf(fp, "   X%-2d     %5.3f\n", k, b[k][1]);
    }
#endif
    /* save matrices for later testing of results */
    for (l = 1; l <= n; l++)
    {
	for (k = 1; k <= n; k++)
	    ai[k][l] = a[k][l];
	x[l][1] = b[l][1];
    }
    /* invert matrix */
    gaussj(ai, n, x, 1);
#ifdef DEBUG
    fprintf(fp, "\nInverse of matrix a : \n");
    for (k = 1; k <= n; k++)
    {
	for (l = 1; l <= n; l++)
	    fprintf(fp, "%12.6f", ai[k][l]);
	fprintf(fp, "\n");
    }
    /* check inverse */
    fprintf(fp, "\na times a-inverse:\n");
    for (k = 1; k <= n; k++)
    {
	for (l = 1; l <= n; l++)
	{
	    u[k][l] = 0.0;
	    for (j = 1; j <= n; j++)
		u[k][l] += (a[k][j] * ai[j][l]);
	}
	for (l = 1; l <= n; l++)
	    fprintf(fp, "%12.6f", u[k][l]);
	fprintf(fp, "\n");
    }
#endif
    /* check vector solutions */
    fprintf(fp, "\nCheck the following for equality:\n");
    fprintf(fp, "%21s %14s\n", "original", "matrix*sol'n");
    for (k = 1; k <= n; k++)
    {
	t[k][1] = 0.0;
	for (j = 1; j <= n; j++)
	    t[k][1] += (a[k][j] * x[j][1]);
	fprintf(fp, "%8s %12.6f %12.6f\n", " ",
		b[k][1], t[k][1]);
    }
    fprintf(fp, "\nSolution:\n");
    for (j = 1; j < n; j++)
	fprintf(fp, "X%d = %12.6f\n", j, x[j][1]);
    fprintf(fp, "\n\nSolution scaled by delays\n");
    {
	float tm = 0.;
	for (j = 1; j < n; j++)
	    tm += del[j][1] * x[j][1];
	fprintf(fp, "\nReference time is %6.4f\n", tm);
	for (j = 1; j < n; j++)
	    x[j][1] /= tm;
    }
    for (j = 1; j < n; j++)
	fprintf(fp, "X%d = %12.6f\n", j, x[j][1]);

/*	free_matrix(t,1,NP,1,1); */
    free_matrix(x, 1, NP, 1, 1);
    free_matrix(b, 1, NP, 1, 1);
    free_matrix(u, 1, NP, 1, NP);
    free_matrix(ai, 1, NP, 1, NP);
    free_matrix(a, 1, NP, 1, NP);
}
