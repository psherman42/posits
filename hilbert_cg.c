#include <stdio.h>
#include <math.h>

/* N=2, 7, 9, ... */
#define N 9 

void matvec(double A[N][N], double x[N], double y[N]) {
    int i, j;
    for (i = 0; i < N; i++) {
        y[i] = 0.0;
        for (j = 0; j < N; j++) y[i] += A[i][j] * x[j];
    }
}

/* Naive dot: rounds after every multiply-add */
double dot_naive(double a[N], double b[N]) {
    double s = 0.0; int i;
    for (i = 0; i < N; i++) s += a[i] * b[i];
    return s;
}

/* Exact dot: Kahan compensated sum -- captures lost bits */
double dot_exact(double a[N], double b[N]) {
    double sum = 0.0, c = 0.0; int i;
    for (i = 0; i < N; i++) {
        double y = a[i] * b[i] - c;
        double t = sum + y;
        c = (t - sum) - y;      /* capture lost bits */
        sum = t;
    }
    return sum;
}

int cg(double A[N][N], double b[N], double x[N],
       int use_exact, int maxiter, double tol) {
    double r[N], p[N], Ap[N];
    double alpha, beta, rz, rz_new, pAp;
    int i, iter;
    for (i = 0; i < N; i++) x[i] = 0.0;
    for (i = 0; i < N; i++) r[i] = b[i];
    for (i = 0; i < N; i++) p[i] = r[i];
    rz = use_exact ? dot_exact(r,r) : dot_naive(r,r);
    for (iter = 0; iter < maxiter; iter++) {
        matvec(A, p, Ap);
        pAp   = use_exact ? dot_exact(p,Ap) : dot_naive(p,Ap);
        alpha = rz / pAp;
        for (i = 0; i < N; i++) x[i] += alpha * p[i];
        for (i = 0; i < N; i++) r[i] -= alpha * Ap[i];
        rz_new = use_exact ? dot_exact(r,r) : dot_naive(r,r);
        if (sqrt(rz_new) < tol) { iter++; break; }
        beta = rz_new / rz;
        for (i = 0; i < N; i++) p[i] = r[i] + beta * p[i];
        rz = rz_new;
    }
    return iter;
}

int main(void) {
    /* N=2 */
    //double A[2][2] = {{6.0,3.0},{3.0,2.0}};
    //double b[2]      = {6.0, 6.0};
    //double x_star[2] = {-2.0, 6.0};  /* exact solution */

    /* N=7 Scale Factor 360360. Condition number of H(7) = 4.75e+08
     * Result: naive 26 iters err=5.3e-05, exact 25 iters err=7.6e-05
     * Exact is NOT always better -- same lesson as the paper's rajat17.
     * The condition number sets the floor. The quire cannot push past it.
     */
    //double A[7][7] = {
    //    {360360, 180180, 120120,  90090,  72072,  60060,  51480},
    //    {180180, 120120,  90090,  72072,  60060,  51480,  45045},
    //    {120120,  90090,  72072,  60060,  51480,  45045,  40040},
    //    { 90090,  72072,  60060,  51480,  45045,  40040,  36036},
    //    { 72072,  60060,  51480,  45045,  40040,  36036,  32760},
    //    { 60060,  51480,  45045,  40040,  36036,  32760,  30030},
    //    { 51480,  45045,  40040,  36036,  32760,  30030,  27720}
    //};
    //double b[7]      = {360360, 360360, 360360, 360360, 360360, 360360, 360360};
    //double x_star[7] = {7.0, -336.0, 3780.0, -16800.0, 34650.0, -33264.0, 12012.0};

    /* N=9: scaled Hilbert 9x9, scale factor 12252240 = LCM(1..17)
     * condition number 4.932e+11
     * x_star has non-integer entries -- floating point precision ceiling already visible
     * N=9: naive 90 iters err=2.586e+00, exact 86 iters err=3.543e+00
     * error > 1 -- condition number 4.932e+11 consumes all double precision digits
     */
    double A[9][9] = {
        {12252240,  6126120,  4084080,  3063060,  2450448,  2042040,  1750320,  1531530,  1361360},
        { 6126120,  4084080,  3063060,  2450448,  2042040,  1750320,  1531530,  1361360,  1225224},
        { 4084080,  3063060,  2450448,  2042040,  1750320,  1531530,  1361360,  1225224,  1113840},
        { 3063060,  2450448,  2042040,  1750320,  1531530,  1361360,  1225224,  1113840,  1021020},
        { 2450448,  2042040,  1750320,  1531530,  1361360,  1225224,  1113840,  1021020,   942480},
        { 2042040,  1750320,  1531530,  1361360,  1225224,  1113840,  1021020,   942480,   875160},
        { 1750320,  1531530,  1361360,  1225224,  1113840,  1021020,   942480,   875160,   816816},
        { 1531530,  1361360,  1225224,  1113840,  1021020,   942480,   875160,   816816,   765765},
        { 1361360,  1225224,  1113840,  1021020,   942480,   875160,   816816,   765765,   720720}};
    double b[9] = {12252240,12252240,12252240,12252240,12252240,12252240,12252240,12252240,12252240};
    double x_star[9] = {9.0,-720.0,13860.0,-110879.8,450449.4,-1009006.8,1261258.7,-823679.2,218789.8};

    /* ---------------------------------------------- */

    double x[N]; double err; int i, iters;

    printf("exact solution: x* = [%g, %g]\n\n", x_star[0], x_star[1]);

    iters = cg(A, b, x, 0, 100, 1e-12);
    err = 0.0;
    for (i = 0; i < N; i++) err += (x[i]-x_star[i])*(x[i]-x_star[i]);
    printf("naive  (%2d iters): x=[%12.9f,%12.9f] err=%.3e\n",
           iters, x[0], x[1], sqrt(err));

    iters = cg(A, b, x, 1, 100, 1e-12);
    err = 0.0;
    for (i = 0; i < N; i++) err += (x[i]-x_star[i])*(x[i]-x_star[i]);
    printf("exact  (%2d iters): x=[%12.9f,%12.9f] err=%.3e\n",
           iters, x[0], x[1], sqrt(err));

    printf("\nCondition number sets the floor.\n");
    printf("The quire cannot push past it. Neither can hope.\n");
    printf("Same lesson as the paper. See you in Chicago.\n");
    return 0;
}
