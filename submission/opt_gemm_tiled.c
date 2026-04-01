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
static float b1[96*H*W], b2[96*H*W];
static int8_t wt1[96*IN*9], wt2[96*96*9], wt3[96*96*9];
static float sc[96], bi[96];

/* TILED GEMM: im2col + cache-friendly tiled matrix multiply.
   Tile over oc(8) and pixels(64) to keep working set in L1.
   The GEMM inner loop (K dimension) is fully vectorizable. */

static float col_buf[H*W * 96*9];
static float wt_f32[96 * 96 * 9];

static void im2col(const float* __restrict__ in, float* __restrict__ col, int ic) {
    int hw = H * W;
    int K = ic * 9;
    memset(col, 0, hw * K * sizeof(float));
    for (int c = 0; c < ic; c++) {
        int ib = c * hw, cbase = c * 9;
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                float *dst = &col[(y*W+x) * K + cbase];
                for (int ky = 0; ky < 3; ky++) {
                    int iy = y+ky-1;
                    if (iy < 0 || iy >= H) continue;
                    for (int kx = 0; kx < 3; kx++) {
                        int ix = x+kx-1;
                        if (ix < 0 || ix >= W) continue;
                        dst[ky*3+kx] = in[ib + iy*W + ix];
                    }
                }
            }
        }
    }
}

static void dequant(const int8_t* __restrict__ wt, const float* __restrict__ sc,
                    float* __restrict__ out, int ic, int oc) {
    for (int o = 0; o < oc; o++) {
        float s = sc[o];
        int base = o * ic * 9;
        for (int i = 0; i < ic*9; i++) out[base+i] = (float)wt[base+i] * s;
    }
}

#define TILE_OC 8
#define TILE_P 64

static void gemm_tiled(const float* __restrict__ Wm, const float* __restrict__ col,
                       const float* __restrict__ bi, float* __restrict__ out,
                       int oc, int K, int hw) {
    /* Tile over output channels and pixels */
    for (int o0 = 0; o0 < oc; o0 += TILE_OC) {
        int oend = o0 + TILE_OC; if (oend > oc) oend = oc;
        for (int p0 = 0; p0 < hw; p0 += TILE_P) {
            int pend = p0 + TILE_P; if (pend > hw) pend = hw;
            /* Init with bias */
            for (int o = o0; o < oend; o++)
                for (int p = p0; p < pend; p++)
                    out[o*hw+p] = bi[o];
            /* Accumulate */
            for (int o = o0; o < oend; o++) {
                const float *wr = &Wm[o * K];
                for (int p = p0; p < pend; p++) {
                    const float *cr = &col[p * K];
                    float acc = 0;
                    for (int k = 0; k < K; k++) acc += wr[k] * cr[k];
                    out[o*hw+p] += acc;
                }
            }
            /* ReLU */
            for (int o = o0; o < oend; o++)
                for (int p = p0; p < pend; p++)
                    if (out[o*hw+p] < 0) out[o*hw+p] = 0;
        }
    }
}

static void conv(const int8_t* __restrict__ wt, const float* __restrict__ sc,
                 const float* __restrict__ bi, int ic, int oc,
                 const float* __restrict__ in, float* __restrict__ out) {
    int K = ic * 9;
    im2col(in, col_buf, ic);
    dequant(wt, sc, wt_f32, ic, oc);
    gemm_tiled(wt_f32, col_buf, bi, out, oc, K, H*W);
}

int main() {
    char l[4096]; fgets(l,sizeof(l),stdin); int n=atoi(l); for(int i=0;i<=n;i++) fgets(l,sizeof(l),stdin);
    for(int i=0;i<IN*H*W;i++) b1[i]=0.5f;
    for(int i=0;i<96*IN*9;i++) wt1[i]=(i%15)-7;
    for(int i=0;i<96*96*9;i++) wt2[i]=(i%15)-7;
    for(int i=0;i<96*96*9;i++) wt3[i]=(i%15)-7;
    for(int i=0;i<96;i++){sc[i]=0.1f;bi[i]=0.01f;}
    struct timespec t0,t1;
    clock_gettime(CLOCK_MONOTONIC,&t0);
    for(int i=0;i<3;i++){conv(wt1,sc,bi,IN,96,b1,b2);conv(wt2,sc,bi,96,96,b2,b1);conv(wt3,sc,bi,96,96,b1,b2);}
    clock_gettime(CLOCK_MONOTONIC,&t1);
    double e=(t1.tv_sec-t0.tv_sec)+(t1.tv_nsec-t0.tv_nsec)/1e9;
    fprintf(stderr,"GEMM_TILED 96ch: %.2f ms/inf (3 iters)\n",e/3*1000); fflush(stderr);
    printf("WAIT\n");
}
