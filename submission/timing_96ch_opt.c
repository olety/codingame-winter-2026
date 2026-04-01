#pragma GCC optimize("O3,unroll-loops,fast-math")
#pragma GCC target("avx2,fma")
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#define H 24
#define W 44
#define IN 19

/* Option 1: pragma O3 + AVX2 + FMA
   Option 2: reordered loop — accumulate spatially with contiguous memory access */

static float b1[96*H*W], b2[96*H*W];
static int8_t wt1[96*IN*9], wt2[96*96*9], wt3[96*96*9];
static float sc[96], bi[96];

static void conv_int4(const int8_t* __restrict__ wt,
                      const float* __restrict__ sc,
                      const float* __restrict__ bi,
                      int ic, int oc,
                      const float* __restrict__ in,
                      float* __restrict__ out) {
    int hw = H * W;
    /* Init output with bias */
    for (int o = 0; o < oc; o++) {
        float bv = bi[o];
        int b = o * hw;
        for (int i = 0; i < hw; i++) out[b + i] = bv;
    }
    /* Main conv: oc -> ic -> spatial (interior only for timing) */
    for (int o = 0; o < oc; o++) {
        float s = sc[o];
        int ob = o * hw;
        for (int c = 0; c < ic; c++) {
            int ib = c * hw;
            int wb = (o * ic + c) * 9;
            float w0 = (float)wt[wb+0]*s, w1 = (float)wt[wb+1]*s, w2 = (float)wt[wb+2]*s;
            float w3 = (float)wt[wb+3]*s, w4 = (float)wt[wb+4]*s, w5 = (float)wt[wb+5]*s;
            float w6 = (float)wt[wb+6]*s, w7 = (float)wt[wb+7]*s, w8 = (float)wt[wb+8]*s;
            for (int y = 1; y < H - 1; y++) {
                const float *r0 = &in[ib + (y-1)*W];
                const float *r1 = &in[ib + y*W];
                const float *r2 = &in[ib + (y+1)*W];
                float *dst = &out[ob + y*W];
                /* Vectorizable inner loop — contiguous reads and writes */
                for (int x = 1; x < W - 1; x++) {
                    dst[x] += r0[x-1]*w0 + r0[x]*w1 + r0[x+1]*w2
                            + r1[x-1]*w3 + r1[x]*w4 + r1[x+1]*w5
                            + r2[x-1]*w6 + r2[x]*w7 + r2[x+1]*w8;
                }
            }
        }
    }
    /* ReLU */
    for (int i = 0; i < oc * hw; i++) if (out[i] < 0) out[i] = 0;
}

int main() {
    char l[4096]; fgets(l, sizeof(l), stdin);
    int n = atoi(l); for (int i = 0; i <= n; i++) fgets(l, sizeof(l), stdin);

    for (int i = 0; i < IN*H*W; i++) b1[i] = 0.5f;
    for (int i = 0; i < 96*IN*9; i++) wt1[i] = (i%15)-7;
    for (int i = 0; i < 96*96*9; i++) wt2[i] = (i%15)-7;
    for (int i = 0; i < 96*96*9; i++) wt3[i] = (i%15)-7;
    for (int i = 0; i < 96; i++) { sc[i] = 0.1f; bi[i] = 0.01f; }

    int sizes[] = {32, 48, 64, 96};
    int iters[] = {10, 5,  3,  3};
    for (int s = 0; s < 4; s++) {
        int ch = sizes[s], ni = iters[s];
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < ni; i++) {
            conv_int4(wt1, sc, bi, IN, ch, b1, b2);
            conv_int4(wt2, sc, bi, ch, ch, b2, b1);
            conv_int4(wt3, sc, bi, ch, ch, b1, b2);
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double e = (t1.tv_sec-t0.tv_sec) + (t1.tv_nsec-t0.tv_nsec)/1e9;
        fprintf(stderr, "%dch/3L int4: %.2f ms/inf (%d iters)\n", ch, e/ni*1000, ni);
        fflush(stderr);
    }
    printf("WAIT\n");
}
