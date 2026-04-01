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

/* IM2COL + GEMM: reshape conv into matrix multiply.
   im2col: (H*W) x (ic*9) matrix of input patches
   weights: oc x (ic*9) matrix (dequantized)
   output = weights × im2col^T → oc x (H*W)
   The matmul vectorizes perfectly. */

static float col_buf[H*W * 96*9]; /* max ic*9 for 96ch middle layer */
static float wt_f32[96 * 96 * 9]; /* dequantized weight matrix */

static void im2col(const float* __restrict__ in, float* __restrict__ col, int ic) {
    int hw = H * W;
    int col_w = ic * 9; /* columns per row */
    memset(col, 0, hw * col_w * sizeof(float));
    for (int c = 0; c < ic; c++) {
        int ib = c * hw;
        int cbase = c * 9;
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                int row = y * W + x;
                float *dst = &col[row * col_w + cbase];
                for (int ky = 0; ky < 3; ky++) {
                    int iy = y + ky - 1;
                    if (iy < 0 || iy >= H) continue;
                    for (int kx = 0; kx < 3; kx++) {
                        int ix = x + kx - 1;
                        if (ix < 0 || ix >= W) continue;
                        dst[ky*3+kx] = in[ib + iy*W + ix];
                    }
                }
            }
        }
    }
}

static void dequant_weights(const int8_t* __restrict__ wt, const float* __restrict__ sc,
                            float* __restrict__ out, int ic, int oc) {
    for (int o = 0; o < oc; o++) {
        float s = sc[o];
        int base = o * ic * 9;
        for (int i = 0; i < ic * 9; i++)
            out[base + i] = (float)wt[base + i] * s;
    }
}

/* C = A(oc x K) × B(K x hw)^T, but B is stored as (hw x K) so C = A × B^T
   Actually: out[oc][hw] = wt_f32[oc][ic*9] × col[hw][ic*9]^T
   Reformulate: for each pixel p, out[o][p] = dot(wt_f32[o], col[p]) */
static void gemm_add_bias(const float* __restrict__ Wm, const float* __restrict__ col,
                          const float* __restrict__ bi, float* __restrict__ out,
                          int oc, int K, int hw) {
    for (int o = 0; o < oc; o++) {
        const float *wr = &Wm[o * K];
        float bv = bi[o];
        int ob = o * hw;
        for (int p = 0; p < hw; p++) {
            const float *cr = &col[p * K];
            float acc = bv;
            for (int k = 0; k < K; k++) acc += wr[k] * cr[k];
            out[ob + p] = acc > 0 ? acc : 0; /* fused ReLU */
        }
    }
}

static void conv(const int8_t* __restrict__ wt, const float* __restrict__ sc,
                 const float* __restrict__ bi, int ic, int oc,
                 const float* __restrict__ in, float* __restrict__ out) {
    int K = ic * 9;
    im2col(in, col_buf, ic);
    dequant_weights(wt, sc, wt_f32, ic, oc);
    gemm_add_bias(wt_f32, col_buf, bi, out, oc, K, H*W);
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
    fprintf(stderr,"IM2COL 96ch: %.2f ms/inf (3 iters)\n",e/3*1000); fflush(stderr);
    printf("WAIT\n");
}
