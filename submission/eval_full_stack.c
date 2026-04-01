#pragma GCC optimize("O3,unroll-loops,fast-math")
#pragma GCC target("avx2,fma")
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <time.h>

#define H 24
#define W 44
#define HW (H * W)
#define IN_CH 19
#define SCALARS 6
#define POLICY_OUT 20  /* 4 birds × 5 actions */
#define VALUE_OUT 1
#define MAX_CH 96

/* Buffers */
static float grid[IN_CH * HW];
static float scalar_feat[SCALARS];
static float buf_a[MAX_CH * HW];
static float buf_b[MAX_CH * HW];
static float pool[MAX_CH + SCALARS];
static float policy_logits[POLICY_OUT];
static float policy_probs[POLICY_OUT];
static float value_out;

/* Weights — random but realistic range */
static int8_t wt1[MAX_CH * IN_CH * 9];
static int8_t wt2[MAX_CH * MAX_CH * 9];
static int8_t wt3[MAX_CH * MAX_CH * 9];
static float sc1[MAX_CH], bi1[MAX_CH];
static float sc2[MAX_CH], bi2[MAX_CH];
static float sc3[MAX_CH], bi3[MAX_CH];
static float pw[POLICY_OUT * (MAX_CH + SCALARS)], pb[POLICY_OUT];
static float vw[MAX_CH + SCALARS], vb;

/* ---- Conv3x3 int4 with dequant (our best: O3+AVX2+FMA baseline) ---- */
static void conv3x3_int4(const int8_t* __restrict__ wt, const float* __restrict__ sc,
                          const float* __restrict__ bi, int ic, int oc,
                          const float* __restrict__ in, float* __restrict__ out) {
    for (int o = 0; o < oc; o++) {
        float bv = bi[o]; int b = o * HW;
        for (int i = 0; i < HW; i++) out[b + i] = bv;
    }
    for (int o = 0; o < oc; o++) {
        float s = sc[o]; int ob = o * HW;
        for (int c = 0; c < ic; c++) {
            int ib = c * HW, wb = (o * ic + c) * 9;
            float w0=(float)wt[wb+0]*s, w1=(float)wt[wb+1]*s, w2=(float)wt[wb+2]*s;
            float w3=(float)wt[wb+3]*s, w4=(float)wt[wb+4]*s, w5=(float)wt[wb+5]*s;
            float w6=(float)wt[wb+6]*s, w7=(float)wt[wb+7]*s, w8=(float)wt[wb+8]*s;
            for (int y = 1; y < H - 1; y++) {
                const float *r0 = &in[ib + (y-1)*W], *r1 = &in[ib + y*W], *r2 = &in[ib + (y+1)*W];
                float *dst = &out[ob + y * W];
                for (int x = 1; x < W - 1; x++) {
                    dst[x] += r0[x-1]*w0 + r0[x]*w1 + r0[x+1]*w2
                            + r1[x-1]*w3 + r1[x]*w4 + r1[x+1]*w5
                            + r2[x-1]*w6 + r2[x]*w7 + r2[x+1]*w8;
                }
            }
            /* Edges: top/bottom rows */
            for (int edge = 0; edge < 2; edge++) {
                int y = edge == 0 ? 0 : H - 1;
                float ws[9] = {w0,w1,w2,w3,w4,w5,w6,w7,w8};
                for (int x = 0; x < W; x++) {
                    float a = 0;
                    for (int ky = 0; ky < 3; ky++) { int iy = y+ky-1; if (iy<0||iy>=H) continue;
                        for (int kx = 0; kx < 3; kx++) { int ix = x+kx-1; if (ix<0||ix>=W) continue;
                            a += in[ib + iy*W + ix] * ws[ky*3+kx]; }}
                    out[ob + y*W + x] += a;
                }
            }
            /* Edges: left/right columns (excluding corners) */
            for (int y = 1; y < H-1; y++) {
                float ws[9] = {w0,w1,w2,w3,w4,w5,w6,w7,w8};
                for (int xi = 0; xi < 2; xi++) { int x = xi == 0 ? 0 : W-1;
                    float a = 0;
                    for (int ky = 0; ky < 3; ky++) { int iy = y+ky-1;
                        for (int kx = 0; kx < 3; kx++) { int ix = x+kx-1; if (ix<0||ix>=W) continue;
                            a += in[ib + iy*W + ix] * ws[ky*3+kx]; }}
                    out[ob + y*W + x] += a;
                }
            }
        }
    }
    for (int i = 0; i < oc * HW; i++) if (out[i] < 0) out[i] = 0;
}

/* ---- Global average pool ---- */
static void global_avg_pool(const float* __restrict__ in, float* out, int ch) {
    float inv = 1.0f / HW;
    for (int c = 0; c < ch; c++) {
        float s = 0;
        const float *src = &in[c * HW];
        for (int i = 0; i < HW; i++) s += src[i];
        out[c] = s * inv;
    }
}

/* ---- Linear layer ---- */
static void linear(const float* __restrict__ wt, const float* __restrict__ bi,
                   const float* __restrict__ in, float* __restrict__ out,
                   int in_f, int out_f) {
    for (int o = 0; o < out_f; o++) {
        float s = bi[o];
        const float *wr = &wt[o * in_f];
        for (int i = 0; i < in_f; i++) s += wr[i] * in[i];
        out[o] = s;
    }
}

/* ---- Softmax ---- */
static void softmax(float* x, int n) {
    float mx = x[0];
    for (int i = 1; i < n; i++) if (x[i] > mx) mx = x[i];
    float s = 0;
    for (int i = 0; i < n; i++) { x[i] = expf(x[i] - mx); s += x[i]; }
    float inv = 1.0f / s;
    for (int i = 0; i < n; i++) x[i] *= inv;
}

/* ---- Full inference ---- */
static void nn_forward(int ch) {
    conv3x3_int4(wt1, sc1, bi1, IN_CH, ch, grid, buf_a);
    conv3x3_int4(wt2, sc2, bi2, ch, ch, buf_a, buf_b);
    conv3x3_int4(wt3, sc3, bi3, ch, ch, buf_b, buf_a);
    global_avg_pool(buf_a, pool, ch);
    /* Append scalar features */
    for (int i = 0; i < SCALARS; i++) pool[ch + i] = scalar_feat[i];
    int feat = ch + SCALARS;
    /* Policy head */
    linear(pw, pb, pool, policy_logits, feat, POLICY_OUT);
    memcpy(policy_probs, policy_logits, POLICY_OUT * sizeof(float));
    softmax(policy_probs, POLICY_OUT);
    /* Value head */
    float v;
    linear(vw, &vb, pool, &v, feat, 1);
    value_out = tanhf(v);
}

/* ---- Simulate feature encoding cost ---- */
static void encode_features(void) {
    /* Simulate building 19-channel binary grid from game state.
       Real cost: iterate birds, walls, apples, set grid cells.
       Approximate with memset + scattered writes. */
    memset(grid, 0, sizeof(grid));
    /* Simulate ~500 nonzero cells across ~8 active channels */
    for (int c = 0; c < 8; c++)
        for (int i = 0; i < 60; i++)
            grid[c * HW + (i * 17 + c * 7) % HW] = 1.0f;
    /* Scalars */
    for (int i = 0; i < SCALARS; i++) scalar_feat[i] = 0.5f;
}

int main() {
    char l[4096];
    fgets(l, sizeof(l), stdin);
    int n = atoi(l);
    for (int i = 0; i <= n; i++) fgets(l, sizeof(l), stdin);

    /* Init weights */
    for (int i = 0; i < MAX_CH*IN_CH*9; i++) wt1[i] = (i%15)-7;
    for (int i = 0; i < MAX_CH*MAX_CH*9; i++) wt2[i] = (i%15)-7;
    for (int i = 0; i < MAX_CH*MAX_CH*9; i++) wt3[i] = (i%15)-7;
    for (int i = 0; i < MAX_CH; i++) { sc1[i]=sc2[i]=sc3[i]=0.1f; bi1[i]=bi2[i]=bi3[i]=0.01f; }
    for (int i = 0; i < POLICY_OUT*(MAX_CH+SCALARS); i++) pw[i] = 0.01f;
    for (int i = 0; i < POLICY_OUT; i++) pb[i] = 0.0f;
    for (int i = 0; i < MAX_CH+SCALARS; i++) vw[i] = 0.01f;
    vb = 0.0f;

    int sizes[] = {32, 48, 64, 96};
    int iters[] = {20, 10, 6, 3};

    for (int s = 0; s < 4; s++) {
        int ch = sizes[s], ni = iters[s];
        struct timespec t0, t1;

        /* Warmup */
        encode_features();
        nn_forward(ch);

        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (int i = 0; i < ni; i++) {
            encode_features();
            nn_forward(ch);
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double e = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
        double ms = e / ni * 1000;
        fprintf(stderr, "%dch: %.2fms/eval  (%d in 40ms, %d in 850ms)  policy[0]=%.3f val=%.3f\n",
                ch, ms, (int)(40.0/ms), (int)(850.0/ms), policy_probs[0], value_out);
        fflush(stderr);
    }

    printf("WAIT\n");
    return 0;
}
