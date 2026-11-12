#include <stdio.h>
#include <math.h>
#include <pthread.h>
#include "/home/labuser/posits/posit_trig56.h"

#define NTHREADS 64

typedef struct { uint32_t start, end; } targ_t;
static uint32_t fails[300];
static int nfails = 0;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;

void *scan(void *arg){
    targ_t *a = (targ_t*)arg;
    uint32_t p;
    for(p = a->start; p != a->end; p++){
        if((int32_t)p < 0) continue;
        double ad = posit_to_double(p);
        double xd = fmod(ad, 2.0);
        if(xd >= 0.044) continue;  /* only Taylor region */
        int32_t ulp = sin_pi56_ulp(p);
        if(ulp != 0){
            pthread_mutex_lock(&mu);
            if(nfails < 300) fails[nfails++] = p;
            pthread_mutex_unlock(&mu);
        }
    }
    return NULL;
}

int main(void){
    positenv_init(32,2);
    pthread_t th[NTHREADS];
    targ_t args[NTHREADS];
    uint32_t chunk = 0x80000000 / NTHREADS;
    int i;
    for(i=0;i<NTHREADS;i++){
        args[i].start = i*chunk;
        args[i].end   = (i+1)*chunk;
        pthread_create(&th[i], NULL, scan, &args[i]);
    }
    for(i=0;i<NTHREADS;i++) pthread_join(th[i], NULL);
    
    printf("Taylor region failures: %d\n", nfails);
    for(i=0;i<nfails;i++){
        double ad = posit_to_double(fails[i]);
        printf("  p=0x%08x a=%.15e ulp=%d\n",
               fails[i], ad, sin_pi56_ulp(fails[i]));
    }
    return 0;
}
