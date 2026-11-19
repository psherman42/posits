/* posit_hello.c -- posit4, es=0, useed=2
 * No libraries. Verify with pencil.
 * gcc -o posit_hello posit_hello.c
 *
 * 2026-11-18  pds/claude
 */
#include <stdio.h>

int main(void)
{
    static const char *bits[] = {
        "0000","0001","0010","0011","0100","0101","0110","0111",
        "1000","1001","1010","1011","1100","1101","1110","1111"
    };
    static const char *vals[] = {
        "0",   "1/4", "1/2", "3/4", "1",   "3/2", "2",   "4",
        "NaR", "-4",  "-2",  "-3/2","-1",  "-3/4","-1/2","-1/4"
    };

    printf("posit4  es=0  useed=2\n");
    printf(" #   bits    value\n");
    for (int i = 0; i < 16; i++)
        printf("%2d   %s    %s\n", i, bits[i], vals[i]);

    printf("\n1/2 + 3/4 + 3/4:\n");
    printf("  without quire: 3/2  (wrong -- should be 2)\n");

    int quire = 0;
    quire +=  8;   /* 1/2  =  8/16 */
    quire += 12;   /* 3/4  = 12/16 */
    quire += 12;   /* 3/4  = 12/16 */
    printf("  with quire:    %d/16 = %d  (exact)\n", quire, quire/16);

    return 0;
}
