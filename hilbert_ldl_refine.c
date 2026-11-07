/*
 * hilbert_ldl_refine.c -- LDL* with iterative residual refinement
 *
 * Type this in. Compile. Run. See what happens.
 *
 *   cc -o hilbert_ldl_refine hilbert_ldl_refine.c -lm && ./hilbert_ldl_refine
 *
 * Iterative refinement:
 *   1. x = LDL* solve(A, b)
 *   2. r = b - A*x          (residual -- quire helps most here)
 *   3. dx = LDL* solve(A, r)
 *   4. x = x + dx
 *   5. repeat until ||r|| < tolerance or MAXITER
 *
 * John Gustafson needed 4 refinement passes to exactly invert H(12)
 * using posit32 + real quire. We use double + Kahan as the accumulator.
 * How many passes do we need?
 *
 * Uses microuniversal.h: dot/sum function pointers.
 * lab.riscvthai.org/posits/
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "quire32_int.h"
#include "microuniversal.h"

#define MAXN    13
#define MAXITER 20
#define TOL     1e-10

/* -- LDL* decomposition ----------------------------------------- */

static int ldl_decomp(double A[MAXN][MAXN], double L[MAXN][MAXN],
                      double d[MAXN], int n, dotfn dot, sumfn sm) {
    int i, j, k;
    double tmp[MAXN];
    memset(L, 0, sizeof(double)*MAXN*MAXN);
    for (j = 0; j < n; j++) {
        for (k = 0; k < j; k++) tmp[k] = L[j][k]*L[j][k]*d[k];
        d[j] = A[j][j] - sm(tmp, j);
        L[j][j] = 1.0;
        if (fabs(d[j]) < 1e-300) return -1;
        for (i = j+1; i < n; i++) {
            for (k = 0; k < j; k++) tmp[k] = L[i][k]*L[j][k]*d[k];
            L[i][j] = (A[i][j] - sm(tmp, j)) / d[j];
        }
    }
    return 0;
}

/* -- LDL* solve ------------------------------------------------- */

static void ldl_solve(double L[MAXN][MAXN], double d[MAXN],
                      double b[MAXN], double x[MAXN],
                      int n, dotfn dot, sumfn sm) {
    double y[MAXN], z[MAXN], tmp[MAXN];
    int i, k;
    for (i = 0; i < n; i++) {
        for (k = 0; k < i; k++) tmp[k] = L[i][k]*y[k];
        y[i] = b[i] - sm(tmp, i);
    }
    for (i = 0; i < n; i++) z[i] = y[i]/d[i];
    for (i = n-1; i >= 0; i--) {
        for (k = i+1; k < n; k++) tmp[k-i-1] = L[k][i]*x[k];
        x[i] = z[i] - sm(tmp, n-i-1);
    }
}

/* -- matrix-vector multiply: r = b - A*x ----------------------- */
/* This is where the quire helps most -- catastrophic cancellation
 * when x is close to the solution and A*x is close to b.        */

static double matvec_residual(double A[MAXN][MAXN], double b[MAXN],
                               double x[MAXN], double r[MAXN],
                               int n, dotfn dot) {
    int i;
    double norm = 0.0;
    for (i = 0; i < n; i++) {
        r[i] = b[i] - dot(A[i], x, n);
        norm += r[i]*r[i];
    }
    return sqrt(norm);
}

/* -- iterative refinement --------------------------------------- */

static int refine(double A[MAXN][MAXN], double L[MAXN][MAXN],
                  double d[MAXN], double b[MAXN], double x[MAXN],
                  double x_star[MAXN], int n, dotfn dot, sumfn sm) {
    double r[MAXN], dx[MAXN];
    double rnorm, err;
    int i, iter = 0;

    /* initial solve */
    ldl_solve(L, d, b, x, n, dot, sm);

    err = 0.0;
    for (i = 0; i < n; i++) err += (x[i]-x_star[i])*(x[i]-x_star[i]);
    rnorm = matvec_residual(A, b, x, r, n, dot);
    printf("  pass 0: err=%.3e  ||r||=%.3e\n", sqrt(err), rnorm);

    /* refinement loop */
    while (rnorm > TOL && iter < MAXITER) {
        /* solve A*dx = r */
        ldl_solve(L, d, r, dx, n, dot, sm);
        /* update x */
        for (i = 0; i < n; i++) x[i] += dx[i];
        iter++;
        rnorm = matvec_residual(A, b, x, r, n, dot);
        err = 0.0;
        for (i = 0; i < n; i++) err += (x[i]-x_star[i])*(x[i]-x_star[i]);
        printf("  pass %d: err=%.3e  ||r||=%.3e\n", iter, sqrt(err), rnorm);
        if (sqrt(err) < 1e-6) break;
    }
    return iter;
}

/* -- main ------------------------------------------------------- */

int main(void) {
    double A[MAXN][MAXN], L[MAXN][MAXN], d[MAXN];
    double b[MAXN], x[MAXN], x_star[MAXN];
    int i, j, ret, iters;

    printf("LDL* with iterative residual refinement\n");
    printf("Target: John Gustafson needed 4 passes for exact H(12)\n\n");

    /* N=7: LCM=360360, div=2^19 */
    {
        int n = 7;
        long scale = 360360, div = 524288;
        double xs[] = {7,-336,3780,-16800,34650,-33264,12012};
        for(i=0;i<n;i++){b[i]=(double)scale/div; x_star[i]=xs[i];
            for(j=0;j<n;j++) A[i][j]=(double)(scale/(i+j+1))/div;}
        ret = ldl_decomp(A,L,d,n,dot_exact,sum_exact);
        if(ret<0){printf("N=7: singular\n");}
        else {
            printf("N=7 (cond~4.75e8):\n");
            iters = refine(A,L,d,b,x,x_star,n,dot_exact,sum_exact);
            printf("  total passes: %d\n\n", iters);
        }
    }

    /* N=9: LCM=12252240, div=2^24 */
    {
        int n = 9;
        long scale = 12252240, div = 16777216;
        double xs[] = {9,-720,13860,-110879.8,450449.4,-1009006.8,1261258.7,-823679.2,218789.8};
        for(i=0;i<n;i++){b[i]=(double)scale/div; x_star[i]=xs[i];
            for(j=0;j<n;j++) A[i][j]=(double)(scale/(i+j+1))/div;}
        ret = ldl_decomp(A,L,d,n,dot_exact,sum_exact);
        if(ret<0){printf("N=9: singular\n");}
        else {
            printf("N=9 (cond~4.93e11):\n");
            iters = refine(A,L,d,b,x,x_star,n,dot_exact,sum_exact);
            printf("  total passes: %d\n\n", iters);
        }
    }

    /* N=12: LCM=5354228880, div=2^33 */
    {
        int n = 12;
        long long scale = 5354228880LL, div = 8589934592LL;
        double xs[] = {-11.4,1636.8,-57627.1,868396.2,-6972818.7,
                        33290870.6,-100123985.7,194531305.5,
                        -243607909.6,189772446.6,-83615389.9,15913230.3};
        for(i=0;i<n;i++){b[i]=(double)scale/div; x_star[i]=xs[i];
            for(j=0;j<n;j++) A[i][j]=(double)(scale/(i+j+1))/div;}
        ret = ldl_decomp(A,L,d,n,dot_exact,sum_exact);
        if(ret<0){printf("N=12: singular\n");}
        else {
            printf("N=12 (cond~1.44e16) -- John's target, Kahan:\n");
            iters = refine(A,L,d,b,x,x_star,n,dot_exact,sum_exact);
            printf("  total passes: %d\n\n", iters);

            /* reset and retry with pure integer quire */
            memset(x, 0, sizeof(double)*MAXN);
            ldl_decomp(A,L,d,n,quire32i_dot,quire32i_sum);
            printf("N=12 (cond~1.44e16) -- integer quire:\n");
            iters = refine(A,L,d,b,x,x_star,n,quire32i_dot,quire32i_sum);
            printf("  total passes: %d\n\n", iters);

        }
    }

    printf("John needed 4 passes with posit32+quire for exact H(12).\n");
    printf("How many do we need with double+Kahan?\n");
    return 0;
}
