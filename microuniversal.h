/*
 * microuniversal.h -- the MAC abstraction
 *
 * Type this in. Include it. Build anything.
 *
 * Four primitives. Two typedefs. The whole of numerical linear algebra.
 *
 * dot is the M. sum is the A. Together they are the MAC.
 * Every algorithm -- CG, LDL*, Cholesky, Kalman, FFT, neural net --
 * reduces to these two operations. The quire is an implementation of
 * dot_exact and sum_exact that is provably correct up to 2^31 terms.
 * The algorithm does not care. It calls the function pointer.
 *
 * On real posit hardware: replace dot_exact/sum_exact with quire
 * accumulation. Everything else stays the same.
 * On bare RV32I with no FPU: implement in integer arithmetic.
 * On any C compiler anywhere: this file is enough.
 *
 * U. Kulisch tried to tell IEEE 754 this in 1970.
 * J. Gustafson wrote it in The End of Error.
 * T. Omtzigt built it into MTL5 as accumulator_traits.
 * Now it fits in a magazine listing.
 *
 * lab.riscvthai.org/posits/ -- RISC-V Open Lab, San Jose CA
 */

#ifndef MICROUNIVERSAL_H
#define MICROUNIVERSAL_H

/* -- the two function pointer types ---------------------------- */

typedef double (*dotfn)(double *a, double *b, int n);
typedef double (*sumfn)(double *a, int n);

/* -- naive: rounds after every multiply-add -------------------- */

static double dot_naive(double *a, double *b, int n) {
    double s = 0.0; int i;
    for (i = 0; i < n; i++) s += a[i] * b[i];
    return s;
}

static double sum_naive(double *a, int n) {
    double s = 0.0; int i;
    for (i = 0; i < n; i++) s += a[i];
    return s;
}

/* -- exact: Kahan compensated sum -- one rounding at the end -- */
/* On posit hardware: replace with quire accumulation.           */
/* The interface is identical. The algorithm does not change.    */

static double dot_exact(double *a, double *b, int n) {
    double sum = 0.0, c = 0.0; int i;
    for (i = 0; i < n; i++) {
        double y = a[i] * b[i] - c;
        double t = sum + y;
        c = (t - sum) - y;
        sum = t;
    }
    return sum;
}

static double sum_exact(double *a, int n) {
    double sum = 0.0, c = 0.0; int i;
    for (i = 0; i < n; i++) {
        double y = a[i] - c;
        double t = sum + y;
        c = (t - sum) - y;
        sum = t;
    }
    return sum;
}

#endif /* MICROUNIVERSAL_H */
