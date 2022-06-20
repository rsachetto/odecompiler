#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <string.h>
 

#define NEQ 41
typedef double real;
//------------------ Support functions and data ---------------

#define print(X) \
_Generic((X), \
real: print_real, \
char*: print_string,  \
bool: print_boolean)(X)

void print_string(char *c) {
    printf("%s",c);
}

void print_real(real f) {
    printf("%lf",f);
}

void print_boolean(bool b) {
    printf("%d",b);
}

typedef struct __exposed_ode_value__t {
    real value;
    real time;
} __exposed_ode_value__;

typedef uint64_t u64;
typedef uint32_t u32;

struct header {    
    u64 size;
    u64 capacity;
};


//INTERNALS MACROS
#define __original_address__(__l) ( (char*)(__l) - sizeof(struct header) )
#define __len__(__l) ( ((struct header *)(__original_address__(__l)))->size )
#define __cap__(__l) ( ((struct header* )(__original_address__(__l)))->capacity )
#define __internal_len__(__l) ( ((struct header *)(__l))->size )
#define __internal_cap__(__l) ( ((struct header *)(__l))->capacity )


//API
#define append(__l, __item) ( (__l) = maybe_grow( (__l), sizeof(*(__l)) ), __l[__len__(__l)++] = (__item) )
#define arrlength(__l) ( (__l) ? __len__(__l) : 0 )
#define arrcapacity(__l) ( (__l) ? __cap__(__l) : 0 )
#define arrfree(__l) free(__original_address__(__l))

void * maybe_grow(void *list, u64 data_size_in_bytes) {

    u64 m = 2;

    if(list == NULL) {
        list = malloc(data_size_in_bytes * m + sizeof(struct header));
        if(!list) return NULL;
        __internal_cap__(list) = m;
        __internal_len__(list) = 0;
    }
    else {

        u64 list_size = __len__(list);
        m = __cap__(list);

        if ((list_size + 1) > m) {
            m = m * 2;
            list = realloc(__original_address__(list), data_size_in_bytes * m + sizeof(struct header));
            if(!list) return NULL;
            __internal_cap__(list) = m;
        }
        else {
            return list;
        }
    }

    return (char*) list + sizeof(struct header);
}

__exposed_ode_value__ **__exposed_odes_values__ = NULL;
static int __ode_last_iteration__ = 1;

real ode_get_value(int ode_position, int timestep) {
    return __exposed_odes_values__[ode_position][timestep].value;
}

real ode_get_time(int ode_position, int timestep) {
    return __exposed_odes_values__[ode_position][timestep].time;
}

int ode_get_num_iterations() {
    return __ode_last_iteration__;
}


void set_initial_conditions(real *x0, real *values) { 

    x0[0] = values[0]; //v
    x0[1] = values[1]; //CaMKt
    x0[2] = values[2]; //nai
    x0[3] = values[3]; //nass
    x0[4] = values[4]; //ki
    x0[5] = values[5]; //kss
    x0[6] = values[6]; //cai
    x0[7] = values[7]; //cass
    x0[8] = values[8]; //cansr
    x0[9] = values[9]; //cajsr
    x0[10] = values[10]; //m
    x0[11] = values[11]; //hf
    x0[12] = values[12]; //hs
    x0[13] = values[13]; //j
    x0[14] = values[14]; //hsp
    x0[15] = values[15]; //jp
    x0[16] = values[16]; //mL
    x0[17] = values[17]; //hL
    x0[18] = values[18]; //hLp
    x0[19] = values[19]; //a
    x0[20] = values[20]; //iF
    x0[21] = values[21]; //iS
    x0[22] = values[22]; //ap
    x0[23] = values[23]; //iFp
    x0[24] = values[24]; //iSp
    x0[25] = values[25]; //d
    x0[26] = values[26]; //ff
    x0[27] = values[27]; //fs
    x0[28] = values[28]; //fcaf
    x0[29] = values[29]; //fcas
    x0[30] = values[30]; //jca
    x0[31] = values[31]; //ffp
    x0[32] = values[32]; //fcafp
    x0[33] = values[33]; //nca
    x0[34] = values[34]; //xrf
    x0[35] = values[35]; //xrs
    x0[36] = values[36]; //xs1
    x0[37] = values[37]; //xs2
    x0[38] = values[38]; //xk1
    x0[39] = values[39]; //Jrelnp
    x0[40] = values[40]; //Jrelp

}

static int solve_model(real time, real *sv, real *rDY) {

    //State variables
    const real v =  sv[0];
    const real CaMKt =  sv[1];
    const real nai =  sv[2];
    const real nass =  sv[3];
    const real ki =  sv[4];
    const real kss =  sv[5];
    const real cai =  sv[6];
    const real cass =  sv[7];
    const real cansr =  sv[8];
    const real cajsr =  sv[9];
    const real m =  sv[10];
    const real hf =  sv[11];
    const real hs =  sv[12];
    const real j =  sv[13];
    const real hsp =  sv[14];
    const real jp =  sv[15];
    const real mL =  sv[16];
    const real hL =  sv[17];
    const real hLp =  sv[18];
    const real a =  sv[19];
    const real iF =  sv[20];
    const real iS =  sv[21];
    const real ap =  sv[22];
    const real iFp =  sv[23];
    const real iSp =  sv[24];
    const real d =  sv[25];
    const real ff =  sv[26];
    const real fs =  sv[27];
    const real fcaf =  sv[28];
    const real fcas =  sv[29];
    const real jca =  sv[30];
    const real ffp =  sv[31];
    const real fcafp =  sv[32];
    const real nca =  sv[33];
    const real xrf =  sv[34];
    const real xrs =  sv[35];
    const real xs1 =  sv[36];
    const real xs2 =  sv[37];
    const real xk1 =  sv[38];
    const real Jrelnp =  sv[39];
    const real Jrelp =  sv[40];

    //Parameters
    real time_new = time;
    real rad = 1.100000e-03;
    real L = 1.000000e-02;
    real amp = (-8.000000e+01);
    real duration = 5.000000e-01;
    real F = 9.648500e+04;
    real R = 8.314000e+03;
    real T = 3.100000e+02;
    real CaMKo = 5.000000e-02;
    real KmCaM = 1.500000e-03;
    real aCaMK = 5.000000e-02;
    real bCaMK = 6.800000e-04;
    real cmdnmax_b = 5.000000e-02;
    real celltype = 0.000000e+00;
    real cm = 1.000000e+00;
    real kmcmdn = 2.380000e-03;
    real trpnmax = 7.000000e-02;
    real kmtrpn = 5.000000e-04;
    real BSRmax = 4.700000e-02;
    real KmBSR = 8.700000e-04;
    real BSLmax = 1.124000e+00;
    real KmBSL = 8.700000e-03;
    real csqnmax = 1.000000e+01;
    real kmcsqn = 8.000000e-01;
    real nao = 1.400000e+02;
    real ko = 5.400000e+00;
    real PKNa = 1.833000e-02;
    real mssV1 = 3.957000e+01;
    real mssV2 = 9.871000e+00;
    real mtD1 = 6.765000e+00;
    real mtV1 = 1.164000e+01;
    real mtV2 = 3.477000e+01;
    real mtD2 = 8.552000e+00;
    real mtV3 = 7.742000e+01;
    real mtV4 = 5.955000e+00;
    real hssV1 = 8.290000e+01;
    real hssV2 = 6.086000e+00;
    real Ahf = 9.900000e-01;
    real KmCaMK = 1.500000e-01;
    real GNa = 7.500000e+01;
    real thL = 2.000000e+02;
    real GNaL_b = 7.500000e-03;
    real Gto_b = 2.000000e-02;
    real k2n = 1.000000e+03;
    real Kmn = 2.000000e-03;
    real cao = 1.800000e+00;
    real PCa_b = 1.000000e-04;
    real GKr_b = 4.600000e-02;
    real GKs_b = 3.400000e-03;
    real GK1_b = 1.908000e-01;
    real qca = 1.670000e-01;
    real qna = 5.224000e-01;
    real kna3 = 8.812000e+01;
    real kna1 = 1.500000e+01;
    real kna2 = 5.000000e+00;
    real kasymm = 1.250000e+01;
    real kcaon = 1.500000e+06;
    real kcaoff = 5.000000e+03;
    real wca = 6.000000e+04;
    real wnaca = 5.000000e+03;
    real wna = 6.000000e+04;
    real KmCaAct = 1.500000e-04;
    real Gncx_b = 8.000000e-04;
    real zna = 1.000000e+00;
    real zca = 2.000000e+00;
    real Knai0 = 9.073000e+00;
    real delta = (-1.550000e-01);
    real Knao0 = 2.778000e+01;
    real eP = 4.200000e+00;
    real H = 1.000000e-07;
    real Khp = 1.698000e-07;
    real Knap = 2.240000e+02;
    real Kxkur = 2.920000e+02;
    real k1p = 9.495000e+02;
    real Kki = 5.000000e-01;
    real k1m = 1.824000e+02;
    real MgADP = 5.000000e-02;
    real k2p = 6.872000e+02;
    real k2m = 3.940000e+01;
    real Kko = 3.582000e-01;
    real k3p = 1.899000e+03;
    real k3m = 7.930000e+04;
    real MgATP = 9.800000e+00;
    real Kmgatp = 1.698000e-07;
    real k4p = 6.390000e+02;
    real k4m = 4.000000e+01;
    real Pnak_b = 3.000000e+01;
    real zk = 1.000000e+00;
    real GKb_b = 3.000000e-03;
    real PNab = 3.750000e-10;
    real PCab = 2.500000e-08;
    real GpCa = 5.000000e-04;
    real KmCap = 5.000000e-04;
    real bt = 4.750000e+00;
    real vcell = ((((1.000000e+03*3.140000e+00)*rad)*rad)*L);
    real Ageo = ((((2.000000e+00*3.140000e+00)*rad)*rad)+(((2.000000e+00*3.140000e+00)*rad)*L));
    real Istim = 0.000000e+00;
    real stim_start = 1.000000e+01;
    if((time_new>=stim_start)&&(time_new<=(stim_start+duration))) {
        Istim = amp;

    } else {
        Istim = 0.000000e+00;

    }
    real vffrt = (((v*F)*F)/(R*T));
    real vfrt = ((v*F)/(R*T));
    real CaMKb = ((CaMKo*(1.000000e+00-CaMKt))/(1.000000e+00+(KmCaM/cass)));
    real cmdnmax = 0.000000e+00;
    if(celltype==1.000000e+00) {
        cmdnmax = (cmdnmax_b*1.300000e+00);

    } else {
        cmdnmax = cmdnmax_b;

    }
    real Bcass = (1.000000e+00/((1.000000e+00+((BSRmax*KmBSR)/pow((KmBSR+cass), 2.000000e+00)))+((BSLmax*KmBSL)/pow((KmBSL+cass), 2.000000e+00))));
    real Bcajsr = (1.000000e+00/(1.000000e+00+((csqnmax*kmcsqn)/pow((kmcsqn+cajsr), 2.000000e+00))));
    real ENa = (((R*T)/F)*log((nao/nai)));
    real EK = (((R*T)/F)*log((ko/ki)));
    real EKs = (((R*T)/F)*log(((ko+(PKNa*nao))/(ki+(PKNa*nai)))));
    real mss = (1.000000e+00/(1.000000e+00+exp(((-(v+mssV1))/mssV2))));
    real tm = (1.000000e+00/((mtD1*exp(((v+mtV1)/mtV2)))+(mtD2*exp(((-(v+mtV3))/mtV4)))));
    real hss = (1.000000e+00/(1.000000e+00+exp(((v+hssV1)/hssV2))));
    real thf = (1.000000e+00/((1.432000e-05*exp(((-(v+1.196000e+00))/6.285000e+00)))+(6.149000e+00*exp(((v+5.096000e-01)/2.027000e+01)))));
    real ths = (1.000000e+00/((9.794000e-03*exp(((-(v+1.795000e+01))/2.805000e+01)))+(3.343000e-01*exp(((v+5.730000e+00)/5.666000e+01)))));
    real Ahs = (1.000000e+00-Ahf);
    real tj = (2.038000e+00+(1.000000e+00/((2.136000e-02*exp(((-(v+1.006000e+02))/8.281000e+00)))+(3.052000e-01*exp(((v+9.941000e-01)/3.845000e+01))))));
    real hssp = (1.000000e+00/(1.000000e+00+exp(((v+8.910000e+01)/6.086000e+00))));
    real mLss = (1.000000e+00/(1.000000e+00+exp(((-(v+4.285000e+01))/5.264000e+00))));
    real hLss = (1.000000e+00/(1.000000e+00+exp(((v+8.761000e+01)/7.488000e+00))));
    real hLssp = (1.000000e+00/(1.000000e+00+exp(((v+9.381000e+01)/7.488000e+00))));
    real thLp = (3.000000e+00*thL);
    real GNaL = 0.000000e+00;
    if(celltype==1.000000e+00) {
        GNaL = (GNaL_b*6.000000e-01);

    } else {
        GNaL = GNaL_b;

    }
    real ass = (1.000000e+00/(1.000000e+00+exp(((-(v-1.434000e+01))/1.482000e+01))));
    real ta = (1.051500e+00/((1.000000e+00/(1.208900e+00*(1.000000e+00+exp(((-(v-1.840990e+01))/2.938140e+01)))))+(3.500000e+00/(1.000000e+00+exp(((v+1.000000e+02)/2.938140e+01))))));
    real iss = (1.000000e+00/(1.000000e+00+exp(((v+4.394000e+01)/5.711000e+00))));
    real delta_epi = 0.000000e+00;
    if(celltype==1.000000e+00) {
        delta_epi = (1.000000e+00-(9.500000e-01/(1.000000e+00+exp(((v+7.000000e+01)/5.000000e+00)))));

    } else {
        delta_epi = 1.000000e+00;

    }
    real tiF_b = (4.562000e+00+(1.000000e+00/((3.933000e-01*exp(((-(v+1.000000e+02))/1.000000e+02)))+(8.004000e-02*exp(((v+5.000000e+01)/1.659000e+01))))));
    real tiS_b = (2.362000e+01+(1.000000e+00/((1.416000e-03*exp(((-(v+9.652000e+01))/5.905000e+01)))+(1.780000e-08*exp(((v+1.141000e+02)/8.079000e+00))))));
    real AiF = (1.000000e+00/(1.000000e+00+exp(((v-2.136000e+02)/1.512000e+02))));
    real assp = (1.000000e+00/(1.000000e+00+exp(((-(v-2.434000e+01))/1.482000e+01))));
    real dti_develop = (1.354000e+00+(1.000000e-04/(exp(((v-1.674000e+02)/1.589000e+01))+exp(((-(v-1.223000e+01))/2.154000e-01)))));
    real dti_recover = (1.000000e+00-(5.000000e-01/(1.000000e+00+exp(((v+7.000000e+01)/2.000000e+01)))));
    real Gto = 0.000000e+00;
    if(celltype==1.000000e+00) {
        Gto = (Gto_b*4.000000e+00);

    } else     if(celltype==2.000000e+00) {
        Gto = (Gto_b*4.000000e+00);

    } else {
        Gto = Gto_b;

    }
    real dss = (1.000000e+00/(1.000000e+00+exp(((-(v+3.940000e+00))/4.230000e+00))));
    real td = (6.000000e-01+(1.000000e+00/(exp(((-5.000000e-02)*(v+6.000000e+00)))+exp((9.000000e-02*(v+1.400000e+01))))));
    real fss = (1.000000e+00/(1.000000e+00+exp(((v+1.958000e+01)/3.696000e+00))));
    real tff = (7.000000e+00+(1.000000e+00/((4.500000e-03*exp(((-(v+2.000000e+01))/1.000000e+01)))+(4.500000e-03*exp(((v+2.000000e+01)/1.000000e+01))))));
    real tfs = (1.000000e+03+(1.000000e+00/((3.500000e-05*exp(((-(v+5.000000e+00))/4.000000e+00)))+(3.500000e-05*exp(((v+5.000000e+00)/6.000000e+00))))));
    real Aff = 6.000000e-01;
    real tfcaf = (7.000000e+00+(1.000000e+00/((4.000000e-02*exp(((-(v-4.000000e+00))/7.000000e+00)))+(4.000000e-02*exp(((v-4.000000e+00)/7.000000e+00))))));
    real tfcas = (1.000000e+02+(1.000000e+00/((1.200000e-04*exp(((-v)/3.000000e+00)))+(1.200000e-04*exp((v/7.000000e+00))))));
    real Afcaf = (3.000000e-01+(6.000000e-01/(1.000000e+00+exp(((v-1.000000e+01)/1.000000e+01)))));
    real tjca = 7.500000e+01;
    real km2n = (jca*1.000000e+00);
    real PCa = 0.000000e+00;
    if(celltype==1.000000e+00) {
        PCa = (PCa_b*1.200000e+00);

    } else     if(celltype==2.000000e+00) {
        PCa = (PCa_b*2.500000e+00);

    } else {
        PCa = PCa_b;

    }
    real xrss = (1.000000e+00/(1.000000e+00+exp(((-(v+8.337000e+00))/6.789000e+00))));
    real txrf = (1.298000e+01+(1.000000e+00/((3.652000e-01*exp(((v-3.166000e+01)/3.869000e+00)))+(4.123000e-05*exp(((-(v-4.778000e+01))/2.038000e+01))))));
    real txrs = (1.865000e+00+(1.000000e+00/((6.629000e-02*exp(((v-3.470000e+01)/7.355000e+00)))+(1.128000e-05*exp(((-(v-2.974000e+01))/2.594000e+01))))));
    real Axrf = (1.000000e+00/(1.000000e+00+exp(((v+5.481000e+01)/3.821000e+01))));
    real rkr = (((1.000000e+00/(1.000000e+00+exp(((v+5.500000e+01)/7.500000e+01))))*1.000000e+00)/(1.000000e+00+exp(((v-1.000000e+01)/3.000000e+01))));
    real GKr = 0.000000e+00;
    if(celltype==1.000000e+00) {
        GKr = (GKr_b*1.300000e+00);

    } else     if(celltype==2.000000e+00) {
        GKr = (GKr_b*8.000000e-01);

    } else {
        GKr = GKr_b;

    }
    real xs1ss = (1.000000e+00/(1.000000e+00+exp(((-(v+1.160000e+01))/8.932000e+00))));
    real txs1 = (8.173000e+02+(1.000000e+00/((2.326000e-04*exp(((v+4.828000e+01)/1.780000e+01)))+(1.292000e-03*exp(((-(v+2.100000e+02))/2.300000e+02))))));
    real txs2 = (1.000000e+00/((1.000000e-02*exp(((v-5.000000e+01)/2.000000e+01)))+(1.930000e-02*exp(((-(v+6.654000e+01))/3.100000e+01)))));
    real KsCa = (1.000000e+00+(6.000000e-01/(1.000000e+00+pow((3.800000e-05/cai), 1.400000e+00))));
    real GKs = 0.000000e+00;
    if(celltype==1.000000e+00) {
        GKs = (GKs_b*1.400000e+00);

    } else {
        GKs = GKs_b;

    }
    real xk1ss = (1.000000e+00/(1.000000e+00+exp(((-((v+(2.553800e+00*ko))+1.445900e+02))/((1.569200e+00*ko)+3.811500e+00)))));
    real txk1 = (1.222000e+02/(exp(((-(v+1.272000e+02))/2.036000e+01))+exp(((v+2.368000e+02)/6.933000e+01))));
    real rk1 = (1.000000e+00/(1.000000e+00+exp((((v+1.058000e+02)-(2.600000e+00*ko))/9.493000e+00))));
    real GK1 = 0.000000e+00;
    if(celltype==1.000000e+00) {
        GK1 = (GK1_b*1.200000e+00);

    } else     if(celltype==2.000000e+00) {
        GK1 = (GK1_b*1.300000e+00);

    } else {
        GK1 = GK1_b;

    }
    real hca = exp((((qca*v)*F)/(R*T)));
    real hna = exp((((qna*v)*F)/(R*T)));
    real h4_i = (1.000000e+00+((nai/kna1)*(1.000000e+00+(nai/kna2))));
    real h10_i = ((kasymm+1.000000e+00)+((nao/kna1)*(1.000000e+00+(nao/kna2))));
    real k2_i = kcaoff;
    real k5_i = kcaoff;
    real allo_i = (1.000000e+00/(1.000000e+00+pow((KmCaAct/cai), 2.000000e+00)));
    real Gncx = 0.000000e+00;
    if(celltype==1.000000e+00) {
        Gncx = (Gncx_b*1.100000e+00);

    } else     if(celltype==2.000000e+00) {
        Gncx = (Gncx_b*1.400000e+00);

    } else {
        Gncx = Gncx_b;

    }
    real h4_ss = (1.000000e+00+((nass/kna1)*(1.000000e+00+(nass/kna2))));
    real h10_ss = ((kasymm+1.000000e+00)+((nao/kna1)*(1.000000e+00+(nao/kna2))));
    real k2_ss = kcaoff;
    real k5_ss = kcaoff;
    real allo_ss = (1.000000e+00/(1.000000e+00+pow((KmCaAct/cass), 2.000000e+00)));
    real Knai = (Knai0*exp((((delta*v)*F)/((3.000000e+00*R)*T))));
    real Knao = (Knao0*exp(((((1.000000e+00-delta)*v)*F)/((3.000000e+00*R)*T))));
    real P = (eP/(((1.000000e+00+(H/Khp))+(nai/Knap))+(ki/Kxkur)));
    real b1 = (k1m*MgADP);
    real a2 = k2p;
    real a4 = (((k4p*MgATP)/Kmgatp)/(1.000000e+00+(MgATP/Kmgatp)));
    real Pnak = 0.000000e+00;
    if(celltype==1.000000e+00) {
        Pnak = (Pnak_b*9.000000e-01);

    } else     if(celltype==2.000000e+00) {
        Pnak = (Pnak_b*7.000000e-01);

    } else {
        Pnak = Pnak_b;

    }
    real xkb = (1.000000e+00/(1.000000e+00+exp(((-(v-1.448000e+01))/1.834000e+01))));
    real GKb = 0.000000e+00;
    if(celltype==1.000000e+00) {
        GKb = (GKb_b*6.000000e-01);

    } else {
        GKb = GKb_b;

    }
    real IpCa = ((GpCa*cai)/(KmCap+cai));
    real JdiffNa = ((nass-nai)/2.000000e+00);
    real JdiffK = ((kss-ki)/2.000000e+00);
    real Jdiff = ((cass-cai)/2.000000e-01);
    real a_rel = (5.000000e-01*bt);
    real tau_rel_temp = (bt/(1.000000e+00+(1.230000e-02/cajsr)));
    real btp = (1.250000e+00*bt);
    real upScale = 0.000000e+00;
    if(celltype==1.000000e+00) {
        upScale = 1.300000e+00;

    } else {
        upScale = 1.000000e+00;

    }
    real Jleak = ((3.937500e-03*cansr)/1.500000e+01);
    real Jtr = ((cansr-cajsr)/1.000000e+02);
    real Acap = (2.000000e+00*Ageo);
    real vmyo = (6.800000e-01*vcell);
    real vnsr = (5.520000e-02*vcell);
    real vjsr = (4.800000e-03*vcell);
    real vss = (2.000000e-02*vcell);
    real CaMKa = (CaMKb+CaMKt);
    real Bcai = (1.000000e+00/((1.000000e+00+((cmdnmax*kmcmdn)/pow((kmcmdn+cai), 2.000000e+00)))+((trpnmax*kmtrpn)/pow((kmtrpn+cai), 2.000000e+00))));
    real h = ((Ahf*hf)+(Ahs*hs));
    real jss = hss;
    real thsp = (3.000000e+00*ths);
    real hp = ((Ahf*hf)+(Ahs*hsp));
    real tjp = (1.460000e+00*tj);
    real tmL = tm;
    real tiF = (tiF_b*delta_epi);
    real tiS = (tiS_b*delta_epi);
    real AiS = (1.000000e+00-AiF);
    real Afs = (1.000000e+00-Aff);
    real fcass = fss;
    real Afcas = (1.000000e+00-Afcaf);
    real tffp = (2.500000e+00*tff);
    real tfcafp = (2.500000e+00*tfcaf);
    real anca = (1.000000e+00/((k2n/km2n)+pow((1.000000e+00+(Kmn/cass)), 4.000000e+00)));
    real PhiCaL = (((4.000000e+00*vffrt)*((cass*exp((2.000000e+00*vfrt)))-(3.410000e-01*cao)))/(exp((2.000000e+00*vfrt))-1.000000e+00));
    real PhiCaNa = (((1.000000e+00*vffrt)*(((7.500000e-01*nass)*exp((1.000000e+00*vfrt)))-(7.500000e-01*nao)))/(exp((1.000000e+00*vfrt))-1.000000e+00));
    real PhiCaK = (((1.000000e+00*vffrt)*(((7.500000e-01*kss)*exp((1.000000e+00*vfrt)))-(7.500000e-01*ko)))/(exp((1.000000e+00*vfrt))-1.000000e+00));
    real PCap = (1.100000e+00*PCa);
    real PCaNa = (1.250000e-03*PCa);
    real PCaK = (3.574000e-04*PCa);
    real Axrs = (1.000000e+00-Axrf);
    real xs2ss = xs1ss;
    real IKs = ((((GKs*KsCa)*xs1)*xs2)*(v-EKs));
    real IK1 = ((((GK1*pow(ko, (1.000000e+00/2.000000e+00)))*rk1)*xk1)*(v-EK));
    real h1_i = (1.000000e+00+((nai/kna3)*(1.000000e+00+hna)));
    real h5_i = ((nai*nai)/((h4_i*kna1)*kna2));
    real h6_i = (1.000000e+00/h4_i);
    real h7_i = (1.000000e+00+((nao/kna3)*(1.000000e+00+(1.000000e+00/hna))));
    real h11_i = ((nao*nao)/((h10_i*kna1)*kna2));
    real h12_i = (1.000000e+00/h10_i);
    real h1_ss = (1.000000e+00+((nass/kna3)*(1.000000e+00+hna)));
    real h5_ss = ((nass*nass)/((h4_ss*kna1)*kna2));
    real h6_ss = (1.000000e+00/h4_ss);
    real h7_ss = (1.000000e+00+((nao/kna3)*(1.000000e+00+(1.000000e+00/hna))));
    real h11_ss = ((nao*nao)/((h10_ss*kna1)*kna2));
    real h12_ss = (1.000000e+00/h10_ss);
    real a1 = ((k1p*pow((nai/Knai), 3.000000e+00))/((pow((1.000000e+00+(nai/Knai)), 3.000000e+00)+pow((1.000000e+00+(ki/Kki)), 2.000000e+00))-1.000000e+00));
    real b2 = ((k2m*pow((nao/Knao), 3.000000e+00))/((pow((1.000000e+00+(nao/Knao)), 3.000000e+00)+pow((1.000000e+00+(ko/Kko)), 2.000000e+00))-1.000000e+00));
    real a3 = ((k3p*pow((ko/Kko), 2.000000e+00))/((pow((1.000000e+00+(nao/Knao)), 3.000000e+00)+pow((1.000000e+00+(ko/Kko)), 2.000000e+00))-1.000000e+00));
    real b3 = (((k3m*P)*H)/(1.000000e+00+(MgATP/Kmgatp)));
    real b4 = ((k4m*pow((ki/Kki), 2.000000e+00))/((pow((1.000000e+00+(nai/Knai)), 3.000000e+00)+pow((1.000000e+00+(ki/Kki)), 2.000000e+00))-1.000000e+00));
    real IKb = ((GKb*xkb)*(v-EK));
    real INab = (((PNab*vffrt)*((nai*exp(vfrt))-nao))/(exp(vfrt)-1.000000e+00));
    real ICab = ((((PCab*4.000000e+00)*vffrt)*((cai*exp((2.000000e+00*vfrt)))-(3.410000e-01*cao)))/(exp((2.000000e+00*vfrt))-1.000000e+00));
    real tau_rel = 0.000000e+00;
    if(tau_rel_temp<1.000000e-03) {
        tau_rel = 1.000000e-03;

    } else {
        tau_rel = tau_rel_temp;

    }
    real a_relp = (5.000000e-01*btp);
    real tau_relp_temp = (btp/(1.000000e+00+(1.230000e-02/cajsr)));
    real Jupnp = (((upScale*4.375000e-03)*cai)/(cai+9.200000e-04));
    real Jupp = ((((upScale*2.750000e+00)*4.375000e-03)*cai)/((cai+9.200000e-04)-1.700000e-04));
    real fINap = (1.000000e+00/(1.000000e+00+(KmCaMK/CaMKa)));
    real fINaLp = (1.000000e+00/(1.000000e+00+(KmCaMK/CaMKa)));
    real i = ((AiF*iF)+(AiS*iS));
    real tiFp = ((dti_develop*dti_recover)*tiF);
    real tiSp = ((dti_develop*dti_recover)*tiS);
    real ip = ((AiF*iFp)+(AiS*iSp));
    real fItop = (1.000000e+00/(1.000000e+00+(KmCaMK/CaMKa)));
    real f = ((Aff*ff)+(Afs*fs));
    real fca = ((Afcaf*fcaf)+(Afcas*fcas));
    real fp = ((Aff*ffp)+(Afs*fs));
    real fcap = ((Afcaf*fcafp)+(Afcas*fcas));
    real PCaNap = (1.250000e-03*PCap);
    real PCaKp = (3.574000e-04*PCap);
    real fICaLp = (1.000000e+00/(1.000000e+00+(KmCaMK/CaMKa)));
    real xr = ((Axrf*xrf)+(Axrs*xrs));
    real h2_i = ((nai*hna)/(kna3*h1_i));
    real h3_i = (1.000000e+00/h1_i);
    real h8_i = (nao/((kna3*hna)*h7_i));
    real h9_i = (1.000000e+00/h7_i);
    real k1_i = ((h12_i*cao)*kcaon);
    real k6_i = ((h6_i*cai)*kcaon);
    real h2_ss = ((nass*hna)/(kna3*h1_ss));
    real h3_ss = (1.000000e+00/h1_ss);
    real h8_ss = (nao/((kna3*hna)*h7_ss));
    real h9_ss = (1.000000e+00/h7_ss);
    real k1_ss = ((h12_ss*cao)*kcaon);
    real k6_ss = ((h6_ss*cass)*kcaon);
    real x1 = (((((a4*a1)*a2)+((b2*b4)*b3))+((a2*b4)*b3))+((b3*a1)*a2));
    real x2 = (((((b2*b1)*b4)+((a1*a2)*a3))+((a3*b1)*b4))+((a2*a3)*b4));
    real x3 = (((((a2*a3)*a4)+((b3*b2)*b1))+((b2*b1)*a4))+((a3*a4)*b1));
    real x4 = (((((b4*b3)*b2)+((a3*a4)*a1))+((b2*a4)*a1))+((b3*b2)*a1));
    real tau_relp = 0.000000e+00;
    if(tau_relp_temp<1.000000e-03) {
        tau_relp = 1.000000e-03;

    } else {
        tau_relp = tau_relp_temp;

    }
    real fJrelp = (1.000000e+00/(1.000000e+00+(KmCaMK/CaMKa)));
    real fJupp = (1.000000e+00/(1.000000e+00+(KmCaMK/CaMKa)));
    real INa = (((GNa*(v-ENa))*pow(m, 3.000000e+00))*((((1.000000e+00-fINap)*h)*j)+((fINap*hp)*jp)));
    real INaL = (((GNaL*(v-ENa))*mL)*(((1.000000e+00-fINaLp)*hL)+(fINaLp*hLp)));
    real Ito = ((Gto*(v-EK))*((((1.000000e+00-fItop)*a)*i)+((fItop*ap)*ip)));
    real ICaL = ((((((1.000000e+00-fICaLp)*PCa)*PhiCaL)*d)*((f*(1.000000e+00-nca))+((jca*fca)*nca)))+((((fICaLp*PCap)*PhiCaL)*d)*((fp*(1.000000e+00-nca))+((jca*fcap)*nca))));
    real ICaNa = ((((((1.000000e+00-fICaLp)*PCaNa)*PhiCaNa)*d)*((f*(1.000000e+00-nca))+((jca*fca)*nca)))+((((fICaLp*PCaNap)*PhiCaNa)*d)*((fp*(1.000000e+00-nca))+((jca*fcap)*nca))));
    real ICaK = ((((((1.000000e+00-fICaLp)*PCaK)*PhiCaK)*d)*((f*(1.000000e+00-nca))+((jca*fca)*nca)))+((((fICaLp*PCaKp)*PhiCaK)*d)*((fp*(1.000000e+00-nca))+((jca*fcap)*nca))));
    real IKr = ((((GKr*pow((ko/5.400000e+00), (1.000000e+00/2.000000e+00)))*xr)*rkr)*(v-EK));
    real k3p_i = (h9_i*wca);
    real k3pp_i = (h8_i*wnaca);
    real k4p_i = ((h3_i*wca)/hca);
    real k4pp_i = (h2_i*wnaca);
    real k7_i = ((h5_i*h2_i)*wna);
    real k8_i = ((h8_i*h11_i)*wna);
    real k3p_ss = (h9_ss*wca);
    real k3pp_ss = (h8_ss*wnaca);
    real k4p_ss = ((h3_ss*wca)/hca);
    real k4pp_ss = (h2_ss*wnaca);
    real k7_ss = ((h5_ss*h2_ss)*wna);
    real k8_ss = ((h8_ss*h11_ss)*wna);
    real E1 = (x1/(((x1+x2)+x3)+x4));
    real E2 = (x2/(((x1+x2)+x3)+x4));
    real E3 = (x3/(((x1+x2)+x3)+x4));
    real E4 = (x4/(((x1+x2)+x3)+x4));
    real Jrel = (((1.000000e+00-fJrelp)*Jrelnp)+(fJrelp*Jrelp));
    real Jup = ((((1.000000e+00-fJupp)*Jupnp)+(fJupp*Jupp))-Jleak);
    real k3_i = (k3p_i+k3pp_i);
    real k4_i = (k4p_i+k4pp_i);
    real k3_ss = (k3p_ss+k3pp_ss);
    real k4_ss = (k4p_ss+k4pp_ss);
    real JnakNa = (3.000000e+00*((E1*a3)-(E2*b3)));
    real JnakK = (2.000000e+00*((E4*b1)-(E3*a1)));
    real Jrel_inf_temp = ((a_rel*(-ICaL))/(1.000000e+00+(1.000000e+00*pow((1.500000e+00/cajsr), 8.000000e+00))));
    real Jrel_temp = ((a_relp*(-ICaL))/(1.000000e+00+pow((1.500000e+00/cajsr), 8.000000e+00)));
    real x1_i = (((k2_i*k4_i)*(k7_i+k6_i))+((k5_i*k7_i)*(k2_i+k3_i)));
    real x2_i = (((k1_i*k7_i)*(k4_i+k5_i))+((k4_i*k6_i)*(k1_i+k8_i)));
    real x3_i = (((k1_i*k3_i)*(k7_i+k6_i))+((k8_i*k6_i)*(k2_i+k3_i)));
    real x4_i = (((k2_i*k8_i)*(k4_i+k5_i))+((k3_i*k5_i)*(k1_i+k8_i)));
    real x1_ss = (((k2_ss*k4_ss)*(k7_ss+k6_ss))+((k5_ss*k7_ss)*(k2_ss+k3_ss)));
    real x2_ss = (((k1_ss*k7_ss)*(k4_ss+k5_ss))+((k4_ss*k6_ss)*(k1_ss+k8_ss)));
    real x3_ss = (((k1_ss*k3_ss)*(k7_ss+k6_ss))+((k8_ss*k6_ss)*(k2_ss+k3_ss)));
    real x4_ss = (((k2_ss*k8_ss)*(k4_ss+k5_ss))+((k3_ss*k5_ss)*(k1_ss+k8_ss)));
    real INaK = (Pnak*((zna*JnakNa)+(zk*JnakK)));
    real Jrel_inf = 0.000000e+00;
    if(celltype==2.000000e+00) {
        Jrel_inf = (Jrel_inf_temp*1.700000e+00);

    } else {
        Jrel_inf = Jrel_inf_temp;

    }
    real Jrel_infp = 0.000000e+00;
    if(celltype==2.000000e+00) {
        Jrel_infp = (Jrel_temp*1.700000e+00);

    } else {
        Jrel_infp = Jrel_temp;

    }
    real E1_i = (x1_i/(((x1_i+x2_i)+x3_i)+x4_i));
    real E2_i = (x2_i/(((x1_i+x2_i)+x3_i)+x4_i));
    real E3_i = (x3_i/(((x1_i+x2_i)+x3_i)+x4_i));
    real E4_i = (x4_i/(((x1_i+x2_i)+x3_i)+x4_i));
    real E1_ss = (x1_ss/(((x1_ss+x2_ss)+x3_ss)+x4_ss));
    real E2_ss = (x2_ss/(((x1_ss+x2_ss)+x3_ss)+x4_ss));
    real E3_ss = (x3_ss/(((x1_ss+x2_ss)+x3_ss)+x4_ss));
    real E4_ss = (x4_ss/(((x1_ss+x2_ss)+x3_ss)+x4_ss));
    real JncxNa_i = (((3.000000e+00*((E4_i*k7_i)-(E1_i*k8_i)))+(E3_i*k4pp_i))-(E2_i*k3pp_i));
    real JncxCa_i = ((E2_i*k2_i)-(E1_i*k1_i));
    real JncxNa_ss = (((3.000000e+00*((E4_ss*k7_ss)-(E1_ss*k8_ss)))+(E3_ss*k4pp_ss))-(E2_ss*k3pp_ss));
    real JncxCa_ss = ((E2_ss*k2_ss)-(E1_ss*k1_ss));
    real INaCa_i = (((8.000000e-01*Gncx)*allo_i)*((zna*JncxNa_i)+(zca*JncxCa_i)));
    real INaCa_ss = (((2.000000e-01*Gncx)*allo_ss)*((zna*JncxNa_ss)+(zca*JncxCa_ss)));
    rDY[0] = (-((((((((((((((((INa+INaL)+Ito)+ICaL)+ICaNa)+ICaK)+IKr)+IKs)+IK1)+INaCa_i)+INaCa_ss)+INaK)+INab)+IKb)+IpCa)+ICab)+Istim));
    rDY[1] = (((aCaMK*CaMKb)*(CaMKb+CaMKt))-(bCaMK*CaMKt));
    rDY[2] = (((((-((((INa+INaL)+(3.000000e+00*INaCa_i))+(3.000000e+00*INaK))+INab))*Acap)*cm)/(F*vmyo))+((JdiffNa*vss)/vmyo));
    rDY[3] = (((((-(ICaNa+(3.000000e+00*INaCa_ss)))*cm)*Acap)/(F*vss))-JdiffNa);
    rDY[4] = (((((-((((((Ito+IKr)+IKs)+IK1)+IKb)+Istim)-(2.000000e+00*INaK)))*cm)*Acap)/(F*vmyo))+((JdiffK*vss)/vmyo));
    rDY[5] = (((((-ICaK)*cm)*Acap)/(F*vss))-JdiffK);
    rDY[6] = (Bcai*((((((-((IpCa+ICab)-(2.000000e+00*INaCa_i)))*cm)*Acap)/((2.000000e+00*F)*vmyo))-((Jup*vnsr)/vmyo))+((Jdiff*vss)/vmyo)));
    rDY[7] = (Bcass*((((((-(ICaL-(2.000000e+00*INaCa_ss)))*cm)*Acap)/((2.000000e+00*F)*vss))+((Jrel*vjsr)/vss))-Jdiff));
    rDY[8] = (Jup-((Jtr*vjsr)/vnsr));
    rDY[9] = (Bcajsr*(Jtr-Jrel));
    rDY[10] = ((mss-m)/tm);
    rDY[11] = ((hss-hf)/thf);
    rDY[12] = ((hss-hs)/ths);
    rDY[13] = ((jss-j)/tj);
    rDY[14] = ((hssp-hsp)/thsp);
    rDY[15] = ((jss-jp)/tjp);
    rDY[16] = ((mLss-mL)/tmL);
    rDY[17] = ((hLss-hL)/thL);
    rDY[18] = ((hLssp-hLp)/thLp);
    rDY[19] = ((ass-a)/ta);
    rDY[20] = ((iss-iF)/tiF);
    rDY[21] = ((iss-iS)/tiS);
    rDY[22] = ((assp-ap)/ta);
    rDY[23] = ((iss-iFp)/tiFp);
    rDY[24] = ((iss-iSp)/tiSp);
    rDY[25] = ((dss-d)/td);
    rDY[26] = ((fss-ff)/tff);
    rDY[27] = ((fss-fs)/tfs);
    rDY[28] = ((fcass-fcaf)/tfcaf);
    rDY[29] = ((fcass-fcas)/tfcas);
    rDY[30] = ((fcass-jca)/tjca);
    rDY[31] = ((fss-ffp)/tffp);
    rDY[32] = ((fcass-fcafp)/tfcafp);
    rDY[33] = ((anca*k2n)-(nca*km2n));
    rDY[34] = ((xrss-xrf)/txrf);
    rDY[35] = ((xrss-xrs)/txrs);
    rDY[36] = ((xs1ss-xs1)/txs1);
    rDY[37] = ((xs2ss-xs2)/txs2);
    rDY[38] = ((xk1ss-xk1)/txk1);
    rDY[39] = ((Jrel_inf-Jrelnp)/tau_rel);
    rDY[40] = ((Jrel_infp-Jrelp)/tau_relp);

    return 0;  

}

void solve_ode(real *sv, float final_time, FILE *f, char *file_name) {

    real rDY[NEQ];

    real reltol = 1e-5;
    real abstol = 1e-5;
    real _tolerances_[NEQ];
    real _aux_tol = 0.0;
    //initializes the variables
    real dt = 0.000001;
    real time_new = 0.0;
    real previous_dt = dt;

    real edos_old_aux_[NEQ];
    real edos_new_euler_[NEQ];
    real *_k1__ = (real*) malloc(sizeof(real)*NEQ);
    real *_k2__ = (real*) malloc(sizeof(real)*NEQ);
    real *_k_aux__;

    const real _beta_safety_ = 0.8;

    const real __tiny_ = pow(abstol, 2.0f);

    if(time_new + dt > final_time) {
       dt = final_time - time_new;
    }

    solve_model(time_new, sv, rDY);
    time_new += dt;

    for(int i = 0; i < NEQ; i++){
        _k1__[i] = rDY[i];
    }

    real min[NEQ];
    real max[NEQ];

    for(int i = 0; i < NEQ; i++) {
       min[i] = DBL_MAX;
       max[i] = DBL_MIN;
    }

    while(1) {

        for(int i = 0; i < NEQ; i++) {
            //stores the old variables in a vector
            edos_old_aux_[i] = sv[i];
            //computes euler method
            edos_new_euler_[i] = _k1__[i] * dt + edos_old_aux_[i];
            //steps ahead to compute the rk2 method
            sv[i] = edos_new_euler_[i];
        }

        time_new += dt;
        solve_model(time_new, sv, rDY);
        time_new -= dt;//step back

        double greatestError = 0.0, auxError = 0.0;
        for(int i = 0; i < NEQ; i++) {
            // stores the new evaluation
            _k2__[i] = rDY[i];
            _aux_tol = fabs(edos_new_euler_[i]) * reltol;
            _tolerances_[i] = (abstol > _aux_tol) ? abstol : _aux_tol;

            // finds the greatest error between  the steps
            auxError = fabs(((dt / 2.0) * (_k1__[i] - _k2__[i])) / _tolerances_[i]);

            greatestError = (auxError > greatestError) ? auxError : greatestError;
        }
        ///adapt the time step
        greatestError += __tiny_;
        previous_dt = dt;
        ///adapt the time step
        dt = _beta_safety_ * dt * sqrt(1.0f/greatestError);

        if (time_new + dt > final_time) {
            dt = final_time - time_new;
        }

        //it doesn't accept the solution
        if ((greatestError >= 1.0f) && dt > 0.00000001) {
            //restore the old values to do it again
            for(int i = 0;  i < NEQ; i++) {
                sv[i] = edos_old_aux_[i];
            }
            //throw the results away and compute again
        } else{//it accepts the solutions

            if (time_new + dt > final_time) {
                dt = final_time - time_new;
            }

            _k_aux__ = _k2__;
            _k2__    = _k1__;
            _k1__    = _k_aux__;

            //it steps the method ahead, with euler solution
            for(int i = 0; i < NEQ; i++){
                sv[i] = edos_new_euler_[i];
            }
            fprintf(f, "%lf ", time_new);
            for(int i = 0; i < NEQ; i++) {
                fprintf(f, "%lf ", sv[i]);
                if(sv[i] < min[i]) min[i] = sv[i];
                __exposed_ode_value__ tmp;
                tmp.time = time_new;
                tmp.value = sv[i];
                append(__exposed_odes_values__[i], tmp);

            }

            __ode_last_iteration__ += 1;
            fprintf(f, "\n");

            if(time_new + previous_dt >= final_time) {
                if(final_time == time_new) {
                    break;
                } else if(time_new < final_time) {
                    dt = previous_dt = final_time - time_new;
                    time_new += previous_dt;
                    break;
                }
            } else {
                time_new += previous_dt;
            }

        }
    }

    char *min_max = malloc(strlen(file_name) + 9);
    sprintf(min_max, "%s_min_max", file_name);
    FILE* min_max_file = fopen(min_max, "w");
    for(int i = 0; i < NEQ; i++) {
        fprintf(min_max_file, "%lf;%lf\n", min[i], max[i]);
    }
    fclose(min_max_file);
    free(min_max);
    
    free(_k1__);
    free(_k2__);
}


int main(int argc, char **argv) {

    real *x0 = (real*) malloc(sizeof(real)*NEQ);

    real values[41];
    values[0] = (-8.700000e+01); //v
    values[1] = 0.000000e+00; //CaMKt
    values[2] = 7.000000e+00; //nai
    values[3] = 7.000000e+00; //nass
    values[4] = 1.450000e+02; //ki
    values[5] = 1.450000e+02; //kss
    values[6] = 1.000000e-04; //cai
    values[7] = 1.000000e-04; //cass
    values[8] = 1.200000e+00; //cansr
    values[9] = 1.200000e+00; //cajsr
    values[10] = 0.000000e+00; //m
    values[11] = 1.000000e+00; //hf
    values[12] = 1.000000e+00; //hs
    values[13] = 1.000000e+00; //j
    values[14] = 1.000000e+00; //hsp
    values[15] = 1.000000e+00; //jp
    values[16] = 0.000000e+00; //mL
    values[17] = 1.000000e+00; //hL
    values[18] = 1.000000e+00; //hLp
    values[19] = 0.000000e+00; //a
    values[20] = 1.000000e+00; //iF
    values[21] = 1.000000e+00; //iS
    values[22] = 0.000000e+00; //ap
    values[23] = 1.000000e+00; //iFp
    values[24] = 1.000000e+00; //iSp
    values[25] = 0.000000e+00; //d
    values[26] = 1.000000e+00; //ff
    values[27] = 1.000000e+00; //fs
    values[28] = 1.000000e+00; //fcaf
    values[29] = 1.000000e+00; //fcas
    values[30] = 1.000000e+00; //jca
    values[31] = 1.000000e+00; //ffp
    values[32] = 1.000000e+00; //fcafp
    values[33] = 0.000000e+00; //nca
    values[34] = 0.000000e+00; //xrf
    values[35] = 0.000000e+00; //xrs
    values[36] = 0.000000e+00; //xs1
    values[37] = 0.000000e+00; //xs2
    values[38] = 1.000000e+00; //xk1
    values[39] = 0.000000e+00; //Jrelnp
    values[40] = 0.000000e+00; //Jrelp
    
    for(int i = 0; i < 41; i++) {
        __exposed_ode_value__ *__ode_values_array__ = NULL;
        __exposed_ode_value__ __tmp__;
        append(__exposed_odes_values__, NULL);
        __tmp__.value = values[i];
        __tmp__.time  = 0;
        append(__ode_values_array__, __tmp__);
        __exposed_odes_values__[i] = __ode_values_array__;
    }
    set_initial_conditions(x0, values);
    FILE *f = fopen(argv[2], "w");
    fprintf(f, "#t, v, CaMKt, nai, nass, ki, kss, cai, cass, cansr, cajsr, m, hf, hs, j, hsp, jp, mL, hL, hLp, a, iF, iS, ap, iFp, iSp, d, ff, fs, fcaf, fcas, jca, ffp, fcafp, nca, xrf, xrs, xs1, xs2, xk1, Jrelnp, Jrelp\n");
    fprintf(f, "0.0 ");
    for(int i = 0; i < NEQ; i++) {
        fprintf(f, "%lf ", x0[i]);
    }
    fprintf(f, "\n");


    solve_ode(x0, strtod(argv[1], NULL), f, argv[2]);
    free(x0);
    
    return (0);
}