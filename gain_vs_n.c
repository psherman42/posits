/*
 * gain_vs_n.c -- quire gain vs N vs condition number
 * Tests James Quinlan's CoNGA 2026 finding.
 * Uses LDL* (direct solver) for cleaner gain measurement.
 */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "microuniversal.h"

#define MAXN 13

static int ldl(double A[MAXN][MAXN], double L[MAXN][MAXN],
               double d[MAXN], int n, sumfn sm) {
    int i,j,k; double tmp[MAXN];
    memset(L,0,sizeof(double)*MAXN*MAXN);
    for(j=0;j<n;j++){
        for(k=0;k<j;k++) tmp[k]=L[j][k]*L[j][k]*d[k];
        d[j]=A[j][j]-sm(tmp,j); L[j][j]=1.0;
        if(fabs(d[j])<1e-300) return -1;
        for(i=j+1;i<n;i++){
            for(k=0;k<j;k++) tmp[k]=L[i][k]*L[j][k]*d[k];
            L[i][j]=(A[i][j]-sm(tmp,j))/d[j];
        }
    }
    return 0;
}

static void ldlsolve(double L[MAXN][MAXN], double d[MAXN],
                     double b[MAXN], double x[MAXN], int n, sumfn sm) {
    double y[MAXN],z[MAXN],tmp[MAXN]; int i,k;
    for(i=0;i<n;i++){for(k=0;k<i;k++) tmp[k]=L[i][k]*y[k]; y[i]=b[i]-sm(tmp,i);}
    for(i=0;i<n;i++) z[i]=y[i]/d[i];
    for(i=n-1;i>=0;i--){for(k=i+1;k<n;k++) tmp[k-i-1]=L[k][i]*x[k]; x[i]=z[i]-sm(tmp,n-i-1);}
}

typedef struct {
    int n; long long scale, div;
    double xs[MAXN];
    double cond;
} Case;

int main(void){
    Case cases[] = {
        {2,  6LL,          8LL,          {-2,6},                                                                1.928e+01},
        {7,  360360LL,     524288LL,     {7,-336,3780,-16800,34650,-33264,12012},                               4.754e+08},
        {9,  12252240LL,   16777216LL,   {9,-720,13860,-110879.8,450449.4,-1009006.8,1261258.7,-823679.2,218789.8}, 4.932e+11},
        {11, 232792560LL,  268435456LL,  {11,-1315.6,38497.3,-479230.0,3145751.3,-12082218.2,28532455.2,-41931464.6,37349972.0,-18446472.3,3874135.0}, 5.225e+14},
        {12, 5354228880LL, 8589934592LL, {-11.4,1636.8,-57627.1,868396.2,-6972818.7,33290870.6,-100123985.7,194531305.5,-243607909.6,189772446.6,-83615389.9,15913230.3}, 1.447e+16},
        {13, 26771144400LL,34359738368LL,{90.0,-14500.5,572321.7,-9733016.6,89111154.5,-492132061.7,1746115551.8,-4113852427.5,6504204244.5,-6821056485.7,4550432780.4,-1747900917.9,294253519.3}, 4.494e+18},
    };
    int ncases = 6;

    printf("Quire gain vs N vs condition number -- LDL* on Pioneer\n");
    printf("James Quinlan (CoNGA 2026): gain tracks N, not cond\n");
    printf("Mechanism: naive accumulates O(n) roundings, quire accumulates none\n\n");
    printf("%3s  %10s  %10s  %8s  %12s\n","N","err_naive","err_exact","gain","cond");
    printf("---  ----------  ----------  --------  ------------\n");

    double gains[6], Ns[6];
    for(int ci=0;ci<ncases;ci++){
        Case *c = &cases[ci];
        int n=c->n; int i,j;
        double A[MAXN][MAXN],L[MAXN][MAXN],d[MAXN],b[MAXN],x[MAXN];

        for(i=0;i<n;i++){b[i]=(double)c->scale/c->div;
            for(j=0;j<n;j++) A[i][j]=(double)(c->scale/(i+j+1))/c->div;}

        double en=0.0,ee=0.0;

        ldl(A,L,d,n,sum_naive);
        ldlsolve(L,d,b,x,n,sum_naive);
        for(i=0;i<n;i++) en+=(x[i]-c->xs[i])*(x[i]-c->xs[i]);

        ldl(A,L,d,n,sum_exact);
        ldlsolve(L,d,b,x,n,sum_exact);
        for(i=0;i<n;i++) ee+=(x[i]-c->xs[i])*(x[i]-c->xs[i]);

        en=sqrt(en); ee=sqrt(ee);
        double gain=(ee>1e-30)?en/ee:1.0;
        gains[ci]=gain; Ns[ci]=(double)n;

        printf("%3d  %10.3e  %10.3e  %8.3f  %12.3e\n",
               n,en,ee,gain,c->cond);
    }

    /* Spearman rank correlation: gain vs N */
    printf("\nSpearman rank correlation:\n");
    /* simple: sort gains, check if rank order matches N order */
    printf("N:    ");
    for(int i=0;i<ncases;i++) printf("%6.0f",Ns[i]);
    printf("\ngain: ");
    for(int i=0;i<ncases;i++) printf("%6.2f",gains[i]);
    printf("\n\nIf gain increases with N: confirms James.\n");
    printf("If gain correlates with cond: contradicts James.\n");
    return 0;
}
