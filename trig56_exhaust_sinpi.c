/*
 * trig56_exhaust.c -- exhaustive posit32 sin56 verification
 *
 * Splits 2^32 posit32 inputs across N threads (default 64).
 * Each thread checks sin56_ulp() for every input in its range.
 * Reports: total inputs, perfect, off-by-1, worse, wall time.
 *
 * On Pioneer (64-hart SG2042):
 *   64 threads * 67M inputs each = 4.3B inputs total
 *   Expected: ~2-4 hours wall clock
 *
 * Usage: ./trig56_exhaust [nthreads]
 *
 * 2026-11-08 pds  initial cut
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include "posit_trig56.h"

#define DEFAULT_THREADS 64

typedef struct {
    uint32_t start;
    uint32_t end;
    int      thread_id;
    uint64_t perfect;
    uint64_t off1;
    uint64_t off2;
    uint64_t worse;
    uint32_t worst_p;
    int32_t  worst_ulp;
} thread_arg_t;

static void *verify_range(void *arg) {
    thread_arg_t *a = (thread_arg_t *)arg;
    uint64_t perfect=0, off1=0, off2=0, worse=0;
    int32_t worst_ulp=0;
    uint32_t worst_p=0;

    for (uint32_t p = a->start; p != a->end; p++) {
        if ((int32_t)p < 0) continue;
        int32_t ulp = sin_pi56_ulp(p);
        int32_t au  = ulp < 0 ? -ulp : ulp;
        if      (au == 0) perfect++;
        else if (au == 1) off1++;
        else if (au == 2) off2++;
        else              worse++;
        if (au > (worst_ulp < 0 ? -worst_ulp : worst_ulp)) {
            worst_ulp = ulp;
            worst_p   = p;
        }
    }

    a->perfect   = perfect;
    a->off1      = off1;
    a->off2      = off2;
    a->worse     = worse;
    a->worst_p   = worst_p;
    a->worst_ulp = worst_ulp;
    return NULL;
}

int main(int argc, char *argv[]) {
    int nthreads = argc > 1 ? atoi(argv[1]) : DEFAULT_THREADS;
    if (nthreads < 1 || nthreads > 256) nthreads = DEFAULT_THREADS;

    positenv_init(32, 2);

    printf("posit32 sin56 exhaustive verification\n");
    printf("%d threads, %.1fM inputs each\n",
           nthreads, 4294967296.0 / nthreads / 1e6);
    printf("Pioneer SG2042, 64-hart RV64GC\n\n");

    pthread_t    *threads = malloc(nthreads * sizeof(pthread_t));
    thread_arg_t *args    = malloc(nthreads * sizeof(thread_arg_t));
    uint32_t      chunk   = (uint32_t)(4294967296ULL / nthreads);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int i = 0; i < nthreads; i++) {
        args[i].thread_id = i;
        args[i].start     = (uint32_t)(i * (uint64_t)chunk);
        args[i].end       = (uint32_t)((i+1) * (uint64_t)chunk);
        args[i].perfect   = 0;
        args[i].off1      = 0;
        args[i].off2      = 0;
        args[i].worse     = 0;
        args[i].worst_ulp = 0;
        args[i].worst_p   = 0;
        pthread_create(&threads[i], NULL, verify_range, &args[i]);
    }

    for (int i = 0; i < nthreads; i++) {
        pthread_join(threads[i], NULL);
        if ((i+1) % 8 == 0)
            printf("  %d/%d threads done\n", i+1, nthreads);
    }

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed = (t1.tv_sec - t0.tv_sec) +
                     (t1.tv_nsec - t0.tv_nsec) * 1e-9;

    uint64_t total=0, perfect=0, off1=0, off2=0, worse=0;
    int32_t  worst_ulp=0;
    uint32_t worst_p=0;

    for (int i = 0; i < nthreads; i++) {
        perfect += args[i].perfect;
        off1    += args[i].off1;
        off2    += args[i].off2;
        worse   += args[i].worse;
        int32_t au = args[i].worst_ulp < 0 ?
                    -args[i].worst_ulp : args[i].worst_ulp;
        int32_t wb = worst_ulp < 0 ? -worst_ulp : worst_ulp;
        if (au > wb) {
            worst_ulp = args[i].worst_ulp;
            worst_p   = args[i].worst_p;
        }
    }
    total = perfect + off1 + off2 + worse;

    printf("\nResults:\n");
    printf("  total inputs:    %llu\n",   (unsigned long long)total);
    printf("  perfect (ulp=0): %llu (%.4f%%)\n",
           (unsigned long long)perfect, 100.0*perfect/total);
    printf("  off by 1:        %llu\n",   (unsigned long long)off1);
    printf("  off by 2:        %llu\n",   (unsigned long long)off2);
    printf("  worse:           %llu\n",   (unsigned long long)worse);
    printf("  worst ulp:       %d at p=0x%08x (a=%.6f)\n",
           worst_ulp, worst_p, posit_to_double(worst_p));
    printf("  wall time:       %.1f sec (%.1f min)\n",
           elapsed, elapsed/60.0);
    printf("  throughput:      %.1fM inputs/sec\n",
           total/elapsed/1e6);

    if (worse==0 && off1==0 && off2==0)
        printf("\nALL PASS -- posit32_sin56 correctly rounded.\n");
    else
        printf("\n%llu failures -- Table-Maker strategies needed.\n",
               (unsigned long long)(off1+off2+worse));

    free(threads);
    free(args);
    return (worse+off1+off2) > 0 ? 1 : 0;
}
