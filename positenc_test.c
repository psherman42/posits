#include <stdio.h>
#include "positenc.h"
int main(void) {

    /* 3.14159 on posit<8,0>: rounds to 2.625 -- only 5 fraction bits, expected
     * 3.14159 on posit<32,2>: err=8e-10 -- irrational number, finite precision
     */

    positenv_init(8, 0);
    printf("nbits=%d es=%d npat=%d NaR=%d\n", _nbits, _es, _npat, _NaR);
    printf("useed=%.1f minpos=%.6f maxpos=%.1f\n", _useed, _minpos, _maxpos);
    printf("quire size=%d bits\n", _qsize);
    /* test round-trip */
    double vals[] = {1.0, 0.5, 2.0, -1.0, 0.25, 3.14159};
    int i;
    printf("\nRound-trip test (posit<8,0>):\n");
    for (i = 0; i < 6; i++) {
        uint32_t p = posit_from_double(vals[i]);
        double x   = posit_to_double(p);
        printf("  x=%8.5f -> p=0x%02x -> x'=%8.5f  err=%.3e\n",
               vals[i], p, x, fabs(x - vals[i]));
    }

    positenv_init(32, 2);
    printf("\nposit<32,2>:\n");
    printf("nbits=%d es=%d npat=%d NaR=%d\n", _nbits, _es, _npat, _NaR);
    printf("useed=%.1f minpos=%.3e maxpos=%.3e\n", _useed, _minpos, _maxpos);
    printf("quire size=%d bits\n", _qsize);
    printf("\nRound-trip test (posit<32,2>):\n");
    for (i = 0; i < 6; i++) {
        uint32_t p = posit_from_double(vals[i]);
        double x   = posit_to_double(p);
        printf("  x=%8.5f -> p=0x%08x -> x'=%8.5f  err=%.3e\n",
               vals[i], p, x, fabs(x - vals[i]));
    }

    return 0;
}
