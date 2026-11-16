#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "quire32.h"
static int pass=0, fail=0;
static void chk(const char *n, uint32_t got, uint32_t want){
    if(got==want){printf("PASS %s\n",n);pass++;}
    else{printf("FAIL %s: got %08x want %08x\n",n,got,want);fail++;}
}
int main(void){
    quire32_t q;
    quire32_clear(&q);
    quire32_add_posit(&q,0); chk("zero",quire32_round_posit(&q),0);
    quire32_clear(&q);
    quire32_add_posit(&q,0x80000000u); chk("nar",quire32_round_posit(&q),0x80000000u);
    quire32_clear(&q);
    quire32_add_posit(&q,0x40000000u); chk("one",quire32_round_posit(&q),0x40000000u);
    quire32_clear(&q);
    quire32_add_posit(&q,0x38000000u); chk("half",quire32_round_posit(&q),0x38000000u);
    quire32_clear(&q);
    quire32_add_posit(&q,0xC0000000u); chk("neg_one",quire32_round_posit(&q),0xC0000000u);
    quire32_clear(&q);
    quire32_add_posit(&q,0x40000000u);
    quire32_add_posit(&q,0xC0000000u); chk("1-1",quire32_round_posit(&q),0);
    quire32_clear(&q);
    quire32_add_posit(&q,0x7FFFFFFFu);
    quire32_add_posit(&q,0x00000001u);
    quire32_add_posit(&q,0x80000001u); chk("gustafson",quire32_round_posit(&q),0x00000001u);
    printf("%d passed, %d failed\n",pass,fail);
    return fail?1:0;
}
