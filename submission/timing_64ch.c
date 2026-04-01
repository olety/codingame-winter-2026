#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#define H 24
#define W 44
#define CH 64
#define IN 19
static float b1[CH*H*W], b2[CH*H*W];
static int8_t wt1[CH*IN*9], wt2[CH*CH*9], wt3[CH*CH*9];
static float sc[CH], bi[CH];
void conv_int4(const int8_t*wt,const float*sc,const float*bi,int ic,int oc,const float*in,float*out){
 int hw=H*W;
 for(int o=0;o<oc;o++){float bv=bi[o];int b=o*hw;for(int i=0;i<hw;i++)out[b+i]=bv;}
 for(int o=0;o<oc;o++){
  float s=sc[o]; int ob=o*hw;
  for(int c=0;c<ic;c++){
   int ib=c*hw, wb=(o*ic+c)*9;
   float w[9]; for(int k=0;k<9;k++) w[k]=(float)wt[wb+k]*s;
   for(int y=1;y<H-1;y++){int r=y*W;
    for(int x=1;x<W-1;x++){
     float a=0;
     a+=in[ib+(y-1)*W+(x-1)]*w[0]; a+=in[ib+(y-1)*W+x]*w[1]; a+=in[ib+(y-1)*W+(x+1)]*w[2];
     a+=in[ib+y*W+(x-1)]*w[3]; a+=in[ib+y*W+x]*w[4]; a+=in[ib+y*W+(x+1)]*w[5];
     a+=in[ib+(y+1)*W+(x-1)]*w[6]; a+=in[ib+(y+1)*W+x]*w[7]; a+=in[ib+(y+1)*W+(x+1)]*w[8];
     out[ob+r+x]+=a;
  }}
 }}
 for(int i=0;i<oc*hw;i++) if(out[i]<0)out[i]=0;
}
int main(){
 char l[4096];fgets(l,sizeof(l),stdin);int n=atoi(l);for(int i=0;i<=n;i++)fgets(l,sizeof(l),stdin);
 for(int i=0;i<IN*H*W;i++)b1[i]=0.5f;
 for(int i=0;i<CH*IN*9;i++)wt1[i]=(i%15)-7;
 for(int i=0;i<CH*CH*9;i++)wt2[i]=(i%15)-7;
 for(int i=0;i<CH*CH*9;i++)wt3[i]=(i%15)-7;
 for(int i=0;i<CH;i++){sc[i]=0.1f;bi[i]=0.01f;}
 struct timespec t0,t1;
 clock_gettime(CLOCK_MONOTONIC,&t0);
 for(int i=0;i<5;i++){conv_int4(wt1,sc,bi,IN,CH,b1,b2);conv_int4(wt2,sc,bi,CH,CH,b2,b1);conv_int4(wt3,sc,bi,CH,CH,b1,b2);}
 clock_gettime(CLOCK_MONOTONIC,&t1);
 double e=(t1.tv_sec-t0.tv_sec)+(t1.tv_nsec-t0.tv_nsec)/1e9;
 fprintf(stderr,"%dch/3L int4: %.2f ms/inf (5 iters)\n",CH,e/5*1000);
 printf("WAIT\n");
}
