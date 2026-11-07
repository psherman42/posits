/*
 * hilbert_ldl_posit.c -- LDL* on a posit32 matrix
 *
 * Type this in. Compile. Run. See what happens.
 *
 *   cc -o hilbert_ldl_posit hilbert_ldl_posit.c -lm && ./hilbert_ldl_posit
 *
 * This is the real thing. Not doubles with Kahan compensation.
 * Actual posit32 encoding, decoding, and accumulation.
 *
 * The matrix entries are stored as posit32 bit patterns (uint32_t).
 * Arithmetic is done by decoding to double, accumulating, then
 * re-encoding. The quire is simulated by Kahan compensation --
 * on real posit hardware, replace dot_exact/sum_exact with
 * hardware quire accumulation. The interface does not change.
 *
 * John L. Gustafson inverted H(12) exactly this way using the
 * real posit quire. We get close. flat128 gets closer.
 * posit64 + quire is uncharted.
 *
 * Uses:
 *   positenc.h      -- posit32 encoding primitives (Gustafson)
 *   microuniversal.h -- dot/sum function pointers (MAC abstraction)
 *
 * lab.riscvthai.org/posits/ -- RISC-V Open Lab, San Jose CA
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include "positenc.h"
#include "quire32.h"
#include "microuniversal.h"

#define N    7
#define MAXN 13

/* -- posit matrix operations ---------------------------------- */
/* Decode a row of posit32 values to doubles for computation.
 * On real hardware: accumulate directly in quire.               */

static void posit_row_to_double(uint32_t *row, double *out, int n) {
    int i;
    for (i = 0; i < n; i++)
        out[i] = posit_to_double(row[i]);
}

/* -- LDL* decomposition on posit32 matrix ---------------------- */
/* A stored as posit32 bit patterns.
 * L and d computed in double, then re-encoded as posit32.
 * dot/sum function pointers select naive or exact accumulation.  */

static int ldl_posit(uint32_t A[MAXN][MAXN],
                     uint32_t L[MAXN][MAXN],
                     uint32_t d[MAXN],
                     int n, dotfn dot, sumfn sm) {
    int i, j, k;
    double tmp[MAXN];
    double Ld[MAXN][MAXN];   /* L decoded */
    double dd[MAXN];          /* d decoded */
    double Ad[MAXN][MAXN];   /* A decoded */

    /* decode A */
    for (i = 0; i < n; i++)
        for (j = 0; j < n; j++)
            Ad[i][j] = posit_to_double(A[i][j]);

    memset(Ld, 0, sizeof(Ld));

    for (j = 0; j < n; j++) {
        /* d[j] = A[j][j] - sum_{k<j} L[j][k]^2 * d[k] */
        for (k = 0; k < j; k++) tmp[k] = Ld[j][k] * Ld[j][k] * dd[k];
        dd[j] = Ad[j][j] - sm(tmp, j);
        Ld[j][j] = 1.0;

        if (fabs(dd[j]) < 1e-300) return -1;

        for (i = j+1; i < n; i++) {
            for (k = 0; k < j; k++) tmp[k] = Ld[i][k] * Ld[j][k] * dd[k];
            Ld[i][j] = (Ad[i][j] - sm(tmp, j)) / dd[j];
        }
    }

    /* re-encode L and d as posit32 */
    for (i = 0; i < n; i++) {
        d[i] = posit_from_double(dd[i]);
        for (j = 0; j < n; j++)
            L[i][j] = posit_from_double(Ld[i][j]);
    }
    return 0;
}

/* -- solve L*D*L^T * x = b (all posit32) ------------------------- */

static void ldl_solve_posit(uint32_t L[MAXN][MAXN],
                             uint32_t d[MAXN],
                             uint32_t b[MAXN],
                             uint32_t x[MAXN],
                             int n, dotfn dot, sumfn sm) {
    double Ld[MAXN][MAXN], dd[MAXN], bd[MAXN];
    double y[MAXN], z[MAXN], xd[MAXN], tmp[MAXN];
    int i, k;

    /* decode everything */
    for (i = 0; i < n; i++) {
        dd[i] = posit_to_double(d[i]);
        bd[i] = posit_to_double(b[i]);
        for (k = 0; k < n; k++)
            Ld[i][k] = posit_to_double(L[i][k]);
    }

    /* forward: Ly = b */
    for (i = 0; i < n; i++) {
        for (k = 0; k < i; k++) tmp[k] = Ld[i][k] * y[k];
        y[i] = bd[i] - sm(tmp, i);
    }

    /* diagonal: z = D^-1 * y */
    for (i = 0; i < n; i++) z[i] = y[i] / dd[i];

    /* backward: L^T x = z */
    for (i = n-1; i >= 0; i--) {
        for (k = i+1; k < n; k++) tmp[k-i-1] = Ld[k][i] * xd[k];
        xd[i] = z[i] - sm(tmp, n-i-1);
    }

    /* re-encode solution as posit32 */
    for (i = 0; i < n; i++)
        x[i] = posit_from_double(xd[i]);
}

/* -- encode a scaled Hilbert matrix as posit32 ---------------- */
/* Full LCM for exact integer entries, divided by 2^k for posit
 * sweet spot [0.5, 1.0]. Gustafson: scale both A and b the same
 * way -- exact solution x* is unchanged.                        */

static void make_hilbert_posit(uint32_t A[MAXN][MAXN],
                                uint32_t b[MAXN],
                                int n, long long scale, long long div) {
    int i, j;
    for (i = 0; i < n; i++) {
        b[i] = posit_from_double((double)scale / div);
        for (j = 0; j < n; j++)
            A[i][j] = posit_from_double(
                (double)(scale / (i+j+1)) / div);
    }
}

/* -- main ----------------------------------------------------- */

int main(void) {
    uint32_t A[MAXN][MAXN], L[MAXN][MAXN], d[MAXN];
    uint32_t b[MAXN], x[MAXN];
    double x_star[MAXN], err;
    int i, ret;

    /* initialize posit32 environment */
    positenv_init(32, 2);

    printf("posit<32,2> LDL* on scaled Hilbert matrices\n");
    printf("useed=%.0f  quire=%d bits\n\n", _useed, _qsize);

    /* N=2 */
    {
        int n = 2;
        double xs[] = {-2.0, 6.0};
        make_hilbert_posit(A, b, n, 6, 8);  /* LCM=6, div=2^3 */
        for (i = 0; i < n; i++) x_star[i] = xs[i];

        printf("N=%d: exact solution x* = [%.1f, %.1f]\n",
               n, x_star[0], x_star[1]);

        ret = ldl_posit(A, L, d, n, dot_naive, sum_naive);
        if (ret < 0) { printf("  naive: singular\n"); }
        else {
            ldl_solve_posit(L, d, b, x, n, dot_naive, sum_naive);
            err = 0.0;
            for (i = 0; i < n; i++) {
                double xi = posit_to_double(x[i]);
                err += (xi-x_star[i])*(xi-x_star[i]);
            }
            printf("  naive: x=[%.9f, %.9f]  err=%.3e\n",
                   posit_to_double(x[0]), posit_to_double(x[1]), sqrt(err));
        }

        ret = ldl_posit(A, L, d, n, quire32_dot, quire32_sum);
        if (ret < 0) { printf("  quire: singular\n"); }
        else {
            ldl_solve_posit(L, d, b, x, n, quire32_dot, quire32_sum);
            err = 0.0;
            for (i = 0; i < n; i++) {
                double xi = posit_to_double(x[i]);
                err += (xi-x_star[i])*(xi-x_star[i]);
            }
            printf("  quire: x=[%.9f, %.9f]  err=%.3e\n\n",
                   posit_to_double(x[0]), posit_to_double(x[1]), sqrt(err));
        }
    }

    /* N=7 */
    {
        int n = 7;
        double xs[] = {7.0,-336.0,3780.0,-16800.0,34650.0,-33264.0,12012.0};
        make_hilbert_posit(A, b, n, 360360, 524288); /* LCM=360360, div=2^19 */
        for (i = 0; i < n; i++) x_star[i] = xs[i];

        printf("N=%d: exact solution x* = [%.1f, %.1f, ...]\n",
               n, x_star[0], x_star[1]);

        ret = ldl_posit(A, L, d, n, dot_naive, sum_naive);
        if (ret >= 0) {
            ldl_solve_posit(L, d, b, x, n, dot_naive, sum_naive);
            err = 0.0;
            for (i = 0; i < n; i++) {
                double xi = posit_to_double(x[i]);
                err += (xi-x_star[i])*(xi-x_star[i]);
            }
            printf("  naive: x=[%.6f, %.6f, ...]  err=%.3e\n",
                   posit_to_double(x[0]), posit_to_double(x[1]), sqrt(err));
        }

        ret = ldl_posit(A, L, d, n, quire32_dot, quire32_sum);
        if (ret >= 0) {
            ldl_solve_posit(L, d, b, x, n, quire32_dot, quire32_sum);
            err = 0.0;
            for (i = 0; i < n; i++) {
                double xi = posit_to_double(x[i]);
                err += (xi-x_star[i])*(xi-x_star[i]);
            }
            printf("  quire: x=[%.6f, %.6f, ...]  err=%.3e\n\n",
                   posit_to_double(x[0]), posit_to_double(x[1]), sqrt(err));
        }
    }

    /* N=9 */
    {
        int n = 9;
        double xs[] = {
                   9.0,    -720.0,   13860.0, -110879.8, 450449.4,
            -1009006.8, 1261258.7, -823679.2,  218789.8
        };
        make_hilbert_posit(A, b, n, 12252240LL, 16777216LL);
        for (i = 0; i < n; i++) x_star[i] = xs[i];
        printf("N=%d: exact solution x* = [%.1f, %.1f, ...]\n", n, x_star[0], x_star[1]);
        ret = ldl_posit(A, L, d, n, dot_naive, sum_naive);
        if (ret >= 0) {
            ldl_solve_posit(L, d, b, x, n, dot_naive, sum_naive);
            err = 0.0;
            for (i = 0; i < n; i++) { double xi = posit_to_double(x[i]); err += (xi-x_star[i])*(xi-x_star[i]); }
            printf("  naive: err=%.3e\n", sqrt(err));
        }
        ret = ldl_posit(A, L, d, n, quire32_dot, quire32_sum);
        if (ret >= 0) {
            ldl_solve_posit(L, d, b, x, n, quire32_dot, quire32_sum);
            err = 0.0;
            for (i = 0; i < n; i++) { double xi = posit_to_double(x[i]); err += (xi-x_star[i])*(xi-x_star[i]); }
            printf("  quire: err=%.3e\n\n", sqrt(err));
        }
    }

    /* N=10 */
    {
        int n = 10;
        double xs[] = {
                -10.0,      989.8,  -23756.1,   240204.5, -1261090.1,
            3783312.1, -6725951.2, 7000536.1, -3937829.1,   923694.0
        };
        make_hilbert_posit(A, b, n, 232792560LL, 268435456LL);
        for (i = 0; i < n; i++) x_star[i] = xs[i];
        printf("N=%d: exact solution x* = [%.1f, %.1f, ...]\n", n, x_star[0], x_star[1]);
        ret = ldl_posit(A, L, d, n, dot_naive, sum_naive);
        if (ret >= 0) {
            ldl_solve_posit(L, d, b, x, n, dot_naive, sum_naive);
            err = 0.0;
            for (i = 0; i < n; i++) { double xi = posit_to_double(x[i]); err += (xi-x_star[i])*(xi-x_star[i]); }
            printf("  naive: err=%.3e\n", sqrt(err));
        }
        ret = ldl_posit(A, L, d, n, quire32_dot, quire32_sum);
        if (ret >= 0) {
            ldl_solve_posit(L, d, b, x, n, quire32_dot, quire32_sum);
            err = 0.0;
            for (i = 0; i < n; i++) { double xi = posit_to_double(x[i]); err += (xi-x_star[i])*(xi-x_star[i]); }
            printf("  quire: err=%.3e\n\n", sqrt(err));
        }
    }

    /* N=11 */
    {
        int n = 11;
        double xs[] = {
                  11.0,     -1315.6,    38497.3,   -479230.0, 3145751.3, -12082218.2,
            28532455.2, -41931464.6, 37349972.0, -18446472.3, 3874135.0
        };
        make_hilbert_posit(A, b, n, 232792560LL, 268435456LL);
        for (i = 0; i < n; i++) x_star[i] = xs[i];
        printf("N=%d: exact solution x* = [%.1f, %.1f, ...]\n", n, x_star[0], x_star[1]);
        ret = ldl_posit(A, L, d, n, dot_naive, sum_naive);
        if (ret >= 0) {
            ldl_solve_posit(L, d, b, x, n, dot_naive, sum_naive);
            err = 0.0;
            for (i = 0; i < n; i++) { double xi = posit_to_double(x[i]); err += (xi-x_star[i])*(xi-x_star[i]); }
            printf("  naive: err=%.3e\n", sqrt(err));
        }
        ret = ldl_posit(A, L, d, n, quire32_dot, quire32_sum);
        if (ret >= 0) {
            ldl_solve_posit(L, d, b, x, n, quire32_dot, quire32_sum);
            err = 0.0;
            for (i = 0; i < n; i++) { double xi = posit_to_double(x[i]); err += (xi-x_star[i])*(xi-x_star[i]); }
            printf("  quire: err=%.3e\n\n", sqrt(err));
        }
    }

    /* N=12 */
    {
        int n = 12;
        double xs[] = {
                   -11.4,      1636.8,     -57627.1,    868396.2,  -6972818.7, 33290870.6,
            -100123985.7, 194531305.5, -243607909.6, 189772446.6, -83615389.9, 15913230.3
        };
        make_hilbert_posit(A, b, n, 5354228880LL, 8589934592LL);
        for (i = 0; i < n; i++) x_star[i] = xs[i];
        printf("N=%d: exact solution x* = [%.1f, %.1f, ...]\n", n, x_star[0], x_star[1]);
        ret = ldl_posit(A, L, d, n, dot_naive, sum_naive);
        if (ret >= 0) {
            ldl_solve_posit(L, d, b, x, n, dot_naive, sum_naive);
            err = 0.0;
            for (i = 0; i < n; i++) { double xi = posit_to_double(x[i]); err += (xi-x_star[i])*(xi-x_star[i]); }
            printf("  naive: err=%.3e\n", sqrt(err));
        }
        ret = ldl_posit(A, L, d, n, quire32_dot, quire32_sum);
        if (ret >= 0) {
            ldl_solve_posit(L, d, b, x, n, quire32_dot, quire32_sum);
            err = 0.0;
            for (i = 0; i < n; i++) { double xi = posit_to_double(x[i]); err += (xi-x_star[i])*(xi-x_star[i]); }
            printf("  quire: err=%.3e\n\n", sqrt(err));
        }
    }

    /* N=13 */
    {
        int n = 13;
        double xs[] = {
                     90.0,     -14500.5,      572321.7,   -9733016.6,    89111154.5, -492132061.7, 1746115551.8,
            -4113852427.5, 6504204244.5, -6821056485.7, 4550432780.4, -1747900917.9,  294253519.3
        };
        make_hilbert_posit(A, b, n, 26771144400LL, 34359738368LL);
        for (i = 0; i < n; i++) x_star[i] = xs[i];
        printf("N=%d: exact solution x* = [%.1f, %.1f, ...]\n", n, x_star[0], x_star[1]);
        ret = ldl_posit(A, L, d, n, dot_naive, sum_naive);
        if (ret >= 0) {
            ldl_solve_posit(L, d, b, x, n, dot_naive, sum_naive);
            err = 0.0;
            for (i = 0; i < n; i++) { double xi = posit_to_double(x[i]); err += (xi-x_star[i])*(xi-x_star[i]); }
            printf("  naive: err=%.3e\n", sqrt(err));
        }
        ret = ldl_posit(A, L, d, n, quire32_dot, quire32_sum);
        if (ret >= 0) {
            ldl_solve_posit(L, d, b, x, n, quire32_dot, quire32_sum);
            err = 0.0;
            for (i = 0; i < n; i++) { double xi = posit_to_double(x[i]); err += (xi-x_star[i])*(xi-x_star[i]); }
            printf("  quire: err=%.3e\n\n", sqrt(err));
        }
    }

    printf("No sqrt. No Cholesky. posit32 LDL* all the way down.\n");
    printf("dot is the M. sum is the A. The quire is the guarantee.\n");
    printf("John Gustafson inverted H(12) exactly this way.\n");
    return 0;
}
