struct proc
{
    char name[40];
    short nt;
    short mi;
    float *delays;
};

#ifndef GLOBAL
#define GLOBAL extern
#endif

GLOBAL struct proc *proctable;
GLOBAL float **BIGTRG;

void solve(int p, int start, int end);
void textsolve(FILE * fp, float **StochMat, float *Delays, int n);
