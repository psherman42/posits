#include <stdio.h>
#include <math.h>
#include "posit_math.h"

int main(void) {
    positenv_init(32,2);

    printf("posit_math.h -- posit32 math library\n");
    printf("No FPU. No F. No D. Bare RV32I.\n\n");

    /* sqrt */
    printf("sqrt (Newton-Raphson, 6 iters):\n");
    double sq[]={1.0,2.0,4.0,9.0,0.25,3.14159};
    for(int i=0;i<6;i++){
        double r=_pd(posit32_sqrt(_pe(sq[i])));
        printf("  sqrt(%.4f)=%.6f (exp %.6f) err=%.2e %s\n",
               sq[i],r,sqrt(sq[i]),fabs(r-sqrt(sq[i])),
               fabs(r-sqrt(sq[i]))<1e-6?"PASS":"check");
    }

    /* sin/cos CORDIC */
    printf("\nsin/cos (CORDIC, 20 iters, shifts+adds only):\n");
    double ang[]={0,M_PI/6,M_PI/4,M_PI/3,M_PI/2};
    char *al[]={"0    ","pi/6 ","pi/4 ","pi/3 ","pi/2 "};
    for(int i=0;i<5;i++){
        uint32_t s,c; posit32_sincos(_pe(ang[i]),&s,&c);
        printf("  %s sin=%.4f(%.4f) cos=%.4f(%.4f) %s\n",
               al[i],_pd(s),sin(ang[i]),_pd(c),cos(ang[i]),
               fabs(_pd(s)-sin(ang[i]))<1e-4&&fabs(_pd(c)-cos(ang[i]))<1e-4?"PASS":"check");
    }

    /* erfc */
    printf("\nerfc (Chebyshev A&S 7.1.26):\n");
    double ef[]={0.0,0.5,1.0,1.5,2.0};
    for(int i=0;i<5;i++){
        double r=_pd(posit32_erfc(_pe(ef[i])));
        printf("  erfc(%.1f)=%.6f (exp %.6f) err=%.2e %s\n",
               ef[i],r,erfc(ef[i]),fabs(r-erfc(ef[i])),
               fabs(r-erfc(ef[i]))<1e-5?"PASS":"check");
    }

    /* haversine -- neighborhood walk */
    printf("\nHaversine (neighborhood walk, San Jose):\n");
    double deg2rad=M_PI/180.0;
    uint32_t la1=_pe(37.40058212634326*deg2rad),lo1=_pe(-121.92571153430421*deg2rad);
    uint32_t la2=_pe(37.398728333962005*deg2rad),lo2=_pe(-121.92434360770662*deg2rad);
    uint32_t la3=_pe(37.40053524938718*deg2rad),lo3=_pe(-121.92455818442788*deg2rad);

    printf("  lat1=%.6f lon1=%.6f\n",_pd(la1),_pd(lo1));
    printf("  lat2=%.6f lon2=%.6f\n",_pd(la2),_pd(lo2));

    uint32_t half=_pe(0.5);
    uint32_t dlat_h=p32_mul(p32_sub(la2,la1),half);
    uint32_t dlon_h=p32_mul(p32_sub(lo2,lo1),half);
    printf("  dlat_h=%.8f dlon_h=%.8f\n",_pd(dlat_h),_pd(dlon_h));

    uint32_t sdlat=posit32_sin(dlat_h);
    uint32_t sdlon=posit32_sin(dlon_h);
    uint32_t clat1=posit32_cos(la1);
    uint32_t clat2=posit32_cos(la2);
    printf("  sin(dlat_h)=%.8f\n",_pd(sdlat));
    printf("  sin(dlon_h)=%.8f\n",_pd(sdlon));
    printf("  cos(lat1)=%.8f cos(lat2)=%.8f\n",_pd(clat1),_pd(clat2));

    uint32_t a=p32_add(p32_mul(sdlat,sdlat),
                       p32_mul(p32_mul(clat1,clat2),p32_mul(sdlon,sdlon)));
    printf("  a=%.10f (exp ~0.0000009)\n",_pd(a));

    uint32_t sq_a = posit32_sqrt(a);
    printf("  sqrt(a)=%.10f\n",_pd(sq_a));
    uint32_t asin_a = posit32_asin(sq_a);
    printf("  asin(sqrt(a))=%.10f (exp %.10f)\n",
           _pd(asin_a), asin(sqrt(_pd(a))));
    printf("  2*R*asin=%.2f (exp 238.9)\n",
           2.0*6371000.0*_pd(asin_a));



    double ab=_pd(posit32_haversine(la1,lo1,la2,lo2));
    double bc=_pd(posit32_haversine(la2,lo2,la3,lo3));
    double ac=_pd(posit32_haversine(la1,lo1,la3,lo3));
    printf("  A→B: %.1fm (exp 238.9m) %s\n",ab,fabs(ab-238.9)<5?"PASS":"check");
    printf("  B→C: %.1fm (exp 201.8m) %s\n",bc,fabs(bc-201.8)<5?"PASS":"check");
    printf("  A→C: %.1fm (exp 102.0m) %s\n",ac,fabs(ac-102.0)<5?"PASS":"check");

    /* bearing */
    printf("\nBearing A→B:\n");
    double brg=_pd(posit32_bearing(la1,lo1,la2,lo2))*180.0/M_PI;
    if(brg<0) brg+=360.0;
    printf("  bearing=%.1f° (exp 149.6°) %s\n",brg,fabs(brg-149.6)<1?"PASS":"check");

    /* Chauvenet */
    printf("\nChauvenet's criterion (n=10, outlier at 15.5):\n");
    double vals[]={10.1,10.2,10.0,10.3,9.9,10.1,10.2,10.0,15.5,10.1};
    int n=10; double mean=0,sigma=0;
    for(int i=0;i<n;i++) mean+=vals[i]; mean/=n;
    for(int i=0;i<n;i++) sigma+=(vals[i]-mean)*(vals[i]-mean);
    sigma=sqrt(sigma/(n-1));
    printf("  mean=%.2f sigma=%.2f\n",mean,sigma);
    for(int i=0;i<n;i++){
        int rej=posit32_chauvenet(_pe(vals[i]),_pe(mean),_pe(sigma),n);
        printf("  x=%.1f: %s%s\n",vals[i],rej?"REJECT":"keep",
               (vals[i]==15.5&&rej)||(vals[i]!=15.5&&!rej)?" PASS":"");
    }

    return 0;
}
