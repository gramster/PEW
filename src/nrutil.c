#include <stdlib.h>
#ifndef UNIX
#include <malloc.h>
#endif
#include <stdio.h>

void 
nrerror(char error_text[])
{
    void exit();

    fprintf(stderr, "Numerical Recipes run-time error...\n");
    fprintf(stderr, "%s\n", error_text);
    fprintf(stderr, "...now exiting to system...\n");
    exit(1);
}

float *
vector(int nl, int nh)
{
    float *v;

    v = (float *)malloc((unsigned)(nh - nl + 1) * sizeof(float));
    if (!v)
	nrerror("allocation failure in vector()");
    return v - nl;
}

int *
ivector(int nl, int nh)
{
    int *v;

    v = (int *)malloc((unsigned)(nh - nl + 1) * sizeof(int));
    if (!v)
	nrerror("allocation failure in ivector()");
    return v - nl;
}

double *
dvector(int nl, int nh)
{
    double *v;

    v = (double *)malloc((unsigned)(nh - nl + 1) * sizeof(double));
    if (!v)
	nrerror("allocation failure in dvector()");
    return v - nl;
}



float **
matrix(int nrl, int nrh, int ncl, int nch)
{
    int i;
    float **m;

    m = (float **)malloc((unsigned)(nrh - nrl + 1) * sizeof(float *));
    if (!m)
	nrerror("allocation failure 1 in matrix()");
    m -= nrl;

    for (i = nrl; i <= nrh; i++)
    {
	m[i] = (float *)malloc((unsigned)(nch - ncl + 1) * sizeof(float));
	if (!m[i])
	    nrerror("allocation failure 2 in matrix()");
	m[i] -= ncl;
    }
    return m;
}

double **
dmatrix(int nrl, int nrh, int ncl, int nch)
{
    int i;
    double **m;

    m = (double **)malloc((unsigned)(nrh - nrl + 1) * sizeof(double *));
    if (!m)
	nrerror("allocation failure 1 in dmatrix()");
    m -= nrl;

    for (i = nrl; i <= nrh; i++)
    {
	m[i] = (double *)malloc((unsigned)(nch - ncl + 1) * sizeof(double));
	if (!m[i])
	    nrerror("allocation failure 2 in dmatrix()");
	m[i] -= ncl;
    }
    return m;
}

int **
imatrix(int nrl, int nrh, int ncl, int nch)
{
    int i, **m;

    m = (int **)malloc((unsigned)(nrh - nrl + 1) * sizeof(int *));
    if (!m)
	nrerror("allocation failure 1 in imatrix()");
    m -= nrl;

    for (i = nrl; i <= nrh; i++)
    {
	m[i] = (int *)malloc((unsigned)(nch - ncl + 1) * sizeof(int));
	if (!m[i])
	    nrerror("allocation failure 2 in imatrix()");
	m[i] -= ncl;
    }
    return m;
}



float **
submatrix(float **a, int oldrl, int oldrh, int oldcl, int oldch, int newrl, int newcl)
{
    int i, j;
    float **m;

    m = (float **)malloc((unsigned)(oldrh - oldrl + 1) * sizeof(float *));
    if (!m)
	nrerror("allocation failure in submatrix()");
    m -= newrl;

    for (i = oldrl, j = newrl; i <= oldrh; i++, j++)
	m[j] = a[i] + oldcl - newcl;

    (void)oldch;		/* Avoid compiler warning */
    return m;
}



void 
free_vector(float *v, int nl, int nh)
{
    free((char *)(v + nl));
    (void)nh;			/* Avoid compiler warning */
}

void 
free_ivector(int *v, int nl, int nh)
{
    (void)nh;			/* Avoid compiler warning */
    free((char *)(v + nl));
}

void 
free_dvector(double *v, int nl, int nh)
{
    (void)nh;			/* Avoid compiler warning */
    free((char *)(v + nl));
}



void 
free_matrix(float **m, int nrl, int nrh, int ncl, int nch)
{
    int i;
    (void)nch;			/* Avoid compiler warning */

    for (i = nrh; i >= nrl; i--)
	free((char *)(m[i] + ncl));
    free((char *)(m + nrl));
}

void 
free_dmatrix(double **m, int nrl, int nrh, int ncl, int nch)
{
    int i;

    (void)nch;			/* Avoid compiler warning */
    for (i = nrh; i >= nrl; i--)
	free((char *)(m[i] + ncl));
    free((char *)(m + nrl));
}

void 
free_imatrix(int **m, int nrl, int nrh, int ncl, int nch)
{
    int i;

    (void)nch;			/* Avoid compiler warning */
    for (i = nrh; i >= nrl; i--)
	free((char *)(m[i] + ncl));
    free((char *)(m + nrl));
}

void 
free_submatrix(float **b, int nrl, int nrh, int ncl, int nch)
{
    (void)nrh;			/* Avoid compiler warning */
    (void)ncl;			/* Avoid compiler warning */
    (void)nch;			/* Avoid compiler warning */
    free((char *)(b + nrl));
}



float **
convert_matrix(float *a, int nrl, int nrh, int ncl, int nch)
{
    int i, j, nrow, ncol;
    float **m;

    nrow = nrh - nrl + 1;
    ncol = nch - ncl + 1;
    m = (float **)malloc((unsigned)(nrow) * sizeof(float *));
    if (!m)
	nrerror("allocation failure in convert_matrix()");
    m -= nrl;
    for (i = 0, j = nrl; i <= nrow - 1; i++, j++)
	m[j] = a + ncol * i - ncl;
    return m;
}



void 
free_convert_matrix(float **b, int nrl, int nrh, int ncl, int nch)
{
    (void)nrh;			/* Avoid compiler warning */
    (void)ncl;			/* Avoid compiler warning */
    (void)nch;			/* Avoid compiler warning */
    free((char *)(b + nrl));
}
