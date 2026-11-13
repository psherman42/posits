#include <stdio.h>
#include <stdint.h>

extern void cordic56_rv32i_iter_test(
    int64_t x_in, int64_t y_in, int64_t z_in,
    int64_t atan_in, int32_t i,
    int64_t *x_out, int64_t *y_out, int64_t *z_out);

static const int64_t atan56[] = {
    56593902016227520LL, 33409331186036028LL, 17652573055549882LL,
     8960721713639278LL,  4497749271019253LL,  2251067235130761LL,
     1125808294293075LL,   562938500594601LL,   281473545067997LL,
      140737309398766LL,    70368721808055LL,    35184369292629LL,
       17592185694890LL,     8796092978517LL,     4398046505642LL,
        2199023254869LL,     1099511627690LL,      549755813877LL,
         274877906942LL,      137438953471LL,       68719476735LL,
          34359738367LL,       17179869183LL,        8589934591LL,
           4294967295LL,        2147483647LL,        1073741823LL,
            536870912LL,         268435456LL,         134217728LL,
             67108864LL,          33554432LL,          16777216LL,
              8388608LL,           4194304LL,           2097152LL,
              1048576LL,            524288LL,            262144LL,
               131072LL,             65536LL,             32768LL,
                16384LL,              8192LL,              4096LL,
                 2048LL,              1024LL,               512LL,
                  256LL,               128LL,                64LL,
                   32LL,                16LL,                 8LL,
                    4LL,                 2LL
};

static void ref_iter(int64_t *x, int64_t *y, int64_t *z, int64_t atan, int i){
    int64_t d = (*z >= 0) ? 1 : -1;
    int64_t xi = *x - d * (*y >> i);
    int64_t yi = *y + d * (*x >> i);
    int64_t zi = *z - d * atan;
    *x = xi; *y = yi; *z = zi;
}

int main(void){
    int64_t angle = (int64_t)(0.785398163397448 * 2.0 *
                    (double)113187804032455040LL);
    int64_t x0 = 43757185469210216LL;
    int pass=0, fail=0;

    for(int i=0; i<56; i++){
        int64_t xr=x0, yr=0, zr=angle;
        ref_iter(&xr, &yr, &zr, atan56[i], i);

        int64_t xa, ya, za;
        cordic56_rv32i_iter_test(x0, 0, angle, atan56[i], i, &xa, &ya, &za);

        int ok = (xr==xa && yr==ya && zr==za);
        if(!ok) printf("FAIL i=%d x=%lld/%lld y=%lld/%lld z=%lld/%lld\n",
                       i,(long long)xr,(long long)xa,
                       (long long)yr,(long long)ya,
                       (long long)zr,(long long)za);
        ok ? pass++ : fail++;
    }
    printf("all 56 iterations: %d pass %d fail\n", pass, fail);
    return fail>0 ? 1 : 0;
}
