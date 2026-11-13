#include <stdio.h>
#include <stdint.h>

extern void cordic56_rv32i_shift_test(
    int64_t val, int32_t i, int64_t *result);

int main(void){
    int64_t k = 43757185469210216LL;  /* CORDIC_K56 */
    int i, pass=0, fail=0;
    
    for(i = 0; i < 56; i++){
        int64_t expected = k >> i;  /* C arithmetic right shift */
        int64_t got;
        cordic56_rv32i_shift_test(k, i, &got);
        if(expected == got){
            pass++;
        } else {
            printf("FAIL i=%d expected=%lld got=%lld\n",
                   i, (long long)expected, (long long)got);
            fail++;
        }
    }
    printf("shift test: %d pass, %d fail\n", pass, fail);
    
    /* test negative value */
    int64_t neg = -43757185469210216LL;
    for(i = 0; i < 56; i++){
        int64_t expected = neg >> i;
        int64_t got;
        cordic56_rv32i_shift_test(neg, i, &got);
        if(expected == got){
            pass++;
        } else {
            printf("FAIL neg i=%d expected=%lld got=%lld\n",
                   i, (long long)expected, (long long)got);
            fail++;
        }
    }
    printf("negative shift test: %d pass, %d fail\n", pass, fail);
    return fail > 0 ? 1 : 0;
}
