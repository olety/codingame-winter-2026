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

/* TILE 8 OC: process 8 output channels at once.
   Each input pixel read once, multiplied by 8 different weight sets.
   Reduces input memory reads by 8x. */
static void conv(const int8_t* __restrict__ wt, const float* __restrict__ sc,
                 const float* __restrict__ bi, int ic, int oc,
                 const float* __restrict__ in, float* __restrict__ out) {
    int hw = H * W;
    for (int o = 0; o < oc; o++) { float bv = bi[o]; int b = o*hw; for (int i = 0; i < hw; i++) out[b+i] = bv; }

    int oc8 = oc & ~7; /* round down to multiple of 8 */
    for (int o0 = 0; o0 < oc8; o0 += 8) {
        for (int c = 0; c < ic; c++) {
            int ib = c * hw;
            /* Dequant weights for 8 output channels */
            float ws[8][9];
            for (int t = 0; t < 8; t++) {
                float s = sc[o0+t];
                int wb = ((o0+t) * ic + c) * 9;
                for (int k = 0; k < 9; k++) ws[t][k] = (float)wt[wb+k] * s;
            }
            for (int y = 1; y < H-1; y++) {
                const float *r0=&in[ib+(y-1)*W], *r1=&in[ib+y*W], *r2=&in[ib+(y+1)*W];
                for (int x = 1; x < W-1; x++) {
                    float v0=r0[x-1], v1=r0[x], v2=r0[x+1];
                    float v3=r1[x-1], v4=r1[x], v5=r1[x+1];
                    float v6=r2[x-1], v7=r2[x], v8=r2[x+1];
                    for (int t = 0; t < 8; t++) {
                        out[(o0+t)*hw + y*W + x] +=
                            v0*ws[t][0] + v1*ws[t][1] + v2*ws[t][2] +
                            v3*ws[t][3] + v4*ws[t][4] + v5*ws[t][5] +
                            v6*ws[t][6] + v7*ws[t][7] + v8*ws[t][8];
                    }
                }
            }
        }
    }
    /* Remainder output channels */
    for (int o = oc8; o < oc; o++) {
        float s = sc[o]; int ob = o * hw;
        for (int c = 0; c < ic; c++) {
            int ib = c * hw, wb = (o * ic + c) * 9;
            float w0=(float)wt[wb+0]*s,w1=(float)wt[wb+1]*s,w2=(float)wt[wb+2]*s;
            float w3=(float)wt[wb+3]*s,w4=(float)wt[wb+4]*s,w5=(float)wt[wb+5]*s;
            float w6=(float)wt[wb+6]*s,w7=(float)wt[wb+7]*s,w8=(float)wt[wb+8]*s;
            for (int y = 1; y < H-1; y++) {
                const float *r0=&in[ib+(y-1)*W],*r1=&in[ib+y*W],*r2=&in[ib+(y+1)*W];
                float *dst=&out[ob+y*W];
                for (int x = 1; x < W-1; x++)
                    dst[x]+=r0[x-1]*w0+r0[x]*w1+r0[x+1]*w2+r1[x-1]*w3+r1[x]*w4+r1[x+1]*w5+r2[x-1]*w6+r2[x]*w7+r2[x+1]*w8;
            }
        }
    }
    for (int i = 0; i < oc*hw; i++) if (out[i] < 0) out[i] = 0;
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
    fprintf(stderr,"TILE8OC 96ch: %.2f ms/inf (3 iters)\n",e/3*1000); fflush(stderr);
    printf("WAIT\n");
}
