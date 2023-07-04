#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <string.h>
 

#define NEQ 43
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


real stim(real t, real i_Stim_Start, real i_Stim_End, real i_Stim_Amplitude, real i_Stim_Period, real i_Stim_PulseDuration) {
    if(((t>=i_Stim_Start)&&(t<=i_Stim_End))&&(((t-i_Stim_Start)-(floor(((t-i_Stim_Start)/i_Stim_Period))*i_Stim_Period))<=i_Stim_PulseDuration)) {
        return i_Stim_Amplitude;
    } else {
        return 0.000000e+00;
    }

}

real calc_cmdnmax(real celltype, real cmdnmax_b) {
    if(celltype==1.000000e+00) {
        return (cmdnmax_b*1.300000e+00);
    } else {
        return cmdnmax_b;
    }

}

real calc_ah(real v) {
    if(v>=(-4.000000e+01)) {
        return 0.000000e+00;
    } else {
        return (5.700000e-02*exp(((-(v+8.000000e+01))/6.800000e+00)));
    }

}

real calc_bh(real v) {
    if(v>=(-4.000000e+01)) {
        return (7.700000e-01/(1.300000e-01*(1.000000e+00+exp(((-(v+1.066000e+01))/1.110000e+01)))));
    } else {
        return ((2.700000e+00*exp((7.900000e-02*v)))+(3.100000e+05*exp((3.485000e-01*v))));
    }

}

real calc_aj(real v) {
    if(v>=(-4.000000e+01)) {
        return 0.000000e+00;
    } else {
        return (((((-2.542800e+04)*exp((2.444000e-01*v)))-(6.948000e-06*exp(((-4.391000e-02)*v))))*(v+3.778000e+01))/(1.000000e+00+exp((3.110000e-01*(v+7.923000e+01)))));
    }

}

real calc_bj(real v) {
    if(v>=(-4.000000e+01)) {
        return ((6.000000e-01*exp((5.700000e-02*v)))/(1.000000e+00+exp(((-1.000000e-01)*(v+3.200000e+01)))));
    } else {
        return ((2.424000e-02*exp(((-1.052000e-02)*v)))/(1.000000e+00+exp(((-1.378000e-01)*(v+4.014000e+01)))));
    }

}

real calc_GNaL(real celltype, real GNaL_b) {
    if(celltype==1.000000e+00) {
        return (GNaL_b*6.000000e-01);
    } else {
        return GNaL_b;
    }

}

real calc_delta_epi(real celltype, real v, real EKshift) {
    if(celltype==1.000000e+00) {
        return (1.000000e+00-(9.500000e-01/(1.000000e+00+exp((((v+EKshift)+7.000000e+01)/5.000000e+00)))));
    } else {
        return 1.000000e+00;
    }

}

real calc_Gto(real celltype, real Gto_b) {
    if(celltype==1.000000e+00) {
        return (Gto_b*2.000000e+00);
    } else {
        if(celltype==2.000000e+00) {
            return (Gto_b*2.000000e+00);
        } else {
            return Gto_b;
        }

    }

}

real calc_dss(real v) {
    if(v>=3.149780e+01) {
        return 1.000000e+00;
    } else {
        return (1.076300e+00*exp(((-1.007000e+00)*exp(((-8.290000e-02)*v)))));
    }

}

real calc_PCa(real celltype, real PCa_b) {
    if(celltype==1.000000e+00) {
        return (PCa_b*1.200000e+00);
    } else {
        if(celltype==2.000000e+00) {
            return (PCa_b*2.000000e+00);
        } else {
            return PCa_b;
        }

    }

}

real calc_GKr(real celltype, real GKr_b) {
    if(celltype==1.000000e+00) {
        return (GKr_b*1.300000e+00);
    } else {
        if(celltype==2.000000e+00) {
            return (GKr_b*8.000000e-01);
        } else {
            return GKr_b;
        }

    }

}

real calc_GKs(real celltype, real GKs_b) {
    if(celltype==1.000000e+00) {
        return (GKs_b*1.400000e+00);
    } else {
        return GKs_b;
    }

}

real calc_GK1(real celltype, real GK1_b) {
    if(celltype==1.000000e+00) {
        return (GK1_b*1.200000e+00);
    } else {
        if(celltype==2.000000e+00) {
            return (GK1_b*1.300000e+00);
        } else {
            return GK1_b;
        }

    }

}

real calc_Gncx(real celltype, real Gncx_b) {
    if(celltype==1.000000e+00) {
        return (Gncx_b*1.100000e+00);
    } else {
        if(celltype==2.000000e+00) {
            return (Gncx_b*1.400000e+00);
        } else {
            return Gncx_b;
        }

    }

}

real calc_Pnak(real celltype, real Pnak_b) {
    if(celltype==1.000000e+00) {
        return (Pnak_b*9.000000e-01);
    } else {
        if(celltype==2.000000e+00) {
            return (Pnak_b*7.000000e-01);
        } else {
            return Pnak_b;
        }

    }

}

real calc_GKb(real celltype, real GKb_b) {
    if(celltype==1.000000e+00) {
        return (GKb_b*6.000000e-01);
    } else {
        return GKb_b;
    }

}

real calc_Jrel_inf(real celltype, real Jrel_inf_b) {
    if(celltype==2.000000e+00) {
        return (Jrel_inf_b*1.700000e+00);
    } else {
        return Jrel_inf_b;
    }

}

real calc_tau_rel(real tau_rel_b) {
    if(tau_rel_b<1.000000e-03) {
        return 1.000000e-03;
    } else {
        return tau_rel_b;
    }

}

real calc_Jrel_infp(real celltype, real Jrel_infp_b) {
    if(celltype==2.000000e+00) {
        return (Jrel_infp_b*1.700000e+00);
    } else {
        return Jrel_infp_b;
    }

}

real calc_tau_relp(real tau_relp_b) {
    if(tau_relp_b<1.000000e-03) {
        return 1.000000e-03;
    } else {
        return tau_relp_b;
    }

}

real calc_upScale(real celltype) {
    if(celltype==1.000000e+00) {
        return 1.300000e+00;
    } else {
        return 1.000000e+00;
    }

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
    x0[11] = values[11]; //h
    x0[12] = values[12]; //j
    x0[13] = values[13]; //hp
    x0[14] = values[14]; //jp
    x0[15] = values[15]; //mL
    x0[16] = values[16]; //hL
    x0[17] = values[17]; //hLp
    x0[18] = values[18]; //a
    x0[19] = values[19]; //iF
    x0[20] = values[20]; //iS
    x0[21] = values[21]; //ap
    x0[22] = values[22]; //iFp
    x0[23] = values[23]; //iSp
    x0[24] = values[24]; //d
    x0[25] = values[25]; //ff
    x0[26] = values[26]; //fs
    x0[27] = values[27]; //fcaf
    x0[28] = values[28]; //fcas
    x0[29] = values[29]; //jca
    x0[30] = values[30]; //ffp
    x0[31] = values[31]; //fcafp
    x0[32] = values[32]; //nca_ss
    x0[33] = values[33]; //nca_i
    x0[34] = values[34]; //C3
    x0[35] = values[35]; //C2
    x0[36] = values[36]; //C1
    x0[37] = values[37]; //O
    x0[38] = values[38]; //I
    x0[39] = values[39]; //xs1
    x0[40] = values[40]; //xs2
    x0[41] = values[41]; //Jrel_np
    x0[42] = values[42]; //Jrel_p

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
    const real h =  sv[11];
    const real j =  sv[12];
    const real hp =  sv[13];
    const real jp =  sv[14];
    const real mL =  sv[15];
    const real hL =  sv[16];
    const real hLp =  sv[17];
    const real a =  sv[18];
    const real iF =  sv[19];
    const real iS =  sv[20];
    const real ap =  sv[21];
    const real iFp =  sv[22];
    const real iSp =  sv[23];
    const real d =  sv[24];
    const real ff =  sv[25];
    const real fs =  sv[26];
    const real fcaf =  sv[27];
    const real fcas =  sv[28];
    const real jca =  sv[29];
    const real ffp =  sv[30];
    const real fcafp =  sv[31];
    const real nca_ss =  sv[32];
    const real nca_i =  sv[33];
    const real C3 =  sv[34];
    const real C2 =  sv[35];
    const real C1 =  sv[36];
    const real O =  sv[37];
    const real I =  sv[38];
    const real xs1 =  sv[39];
    const real xs2 =  sv[40];
    const real Jrel_np =  sv[41];
    const real Jrel_p =  sv[42];

    //Parameters
    real rad = 1.100000e-03;
    real L = 1.000000e-02;
    real i_Stim_Amplitude = (-5.300000e+01);
    real i_Stim_Start = 0.000000e+00;
    real i_Stim_End = 1.000000e+17;
    real i_Stim_Period = 1.000000e+03;
    real i_Stim_PulseDuration = 1.000000e+00;
    real F = 9.648500e+04;
    real R = 8.314000e+03;
    real T = 3.100000e+02;
    real CaMKo = 5.000000e-02;
    real KmCaM = 1.500000e-03;
    real aCaMK = 5.000000e-02;
    real bCaMK = 6.800000e-04;
    real cmdnmax_b = 5.000000e-02;
    real celltype = 0.000000e+00;
    real kmcmdn = 2.380000e-03;
    real trpnmax = 7.010000e-02;
    real kmtrpn = 5.000000e-04;
    real BSRmax = 4.700000e-02;
    real KmBSR = 8.700000e-04;
    real BSLmax = 1.124000e+00;
    real KmBSL = 8.700000e-03;
    real csqnmax = 1.000000e+01;
    real kmcsqn = 8.000000e-01;
    real zna = 1.000000e+00;
    real nao = 1.400000e+02;
    real zk = 1.000000e+00;
    real ko = 5.000000e+00;
    real PKNa = 1.833000e-02;
    real zcl = (-1.000000e+00);
    real clo = 1.500000e+02;
    real cli = 2.400000e+01;
    real K_o_n = 5.000000e+00;
    real A_atp = 2.000000e+00;
    real K_atp = 2.500000e-01;
    real fkatp = 0.000000e+00;
    real gkatp = 4.319500e+00;
    real KmCaMK = 1.500000e-01;
    real GNa = 1.178020e+01;
    real thL = 2.000000e+02;
    real GNaL_b = 2.790000e-02;
    real EKshift = 0.000000e+00;
    real Gto_b = 1.600000e-01;
    real offset = 0.000000e+00;
    real vShift = 0.000000e+00;
    real Aff = 6.000000e-01;
    real tjca = 7.500000e+01;
    real k2n = 5.000000e+02;
    real Kmn = 2.000000e-03;
    real cao = 1.800000e+00;
    real dielConstant = 7.400000e+01;
    real PCa_b = 8.375700e-05;
    real ICaL_fractionSS = 8.000000e-01;
    real beta_1 = 1.911000e-01;
    real alpha_1 = 1.543750e-01;
    real GKr_b = 3.210000e-02;
    real GKs_b = 1.100000e-03;
    real GK1_b = 6.992000e-01;
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
    real Gncx_b = 3.400000e-03;
    real INaCa_fractionSS = 3.500000e-01;
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
    real MgATP = 9.800010e+00;
    real Kmgatp = 1.698000e-07;
    real k4p = 6.390000e+02;
    real k4m = 4.000000e+01;
    real Pnak_b = 1.545090e+01;
    real GKb_b = 1.890000e-02;
    real PNab = 1.923900e-09;
    real PCab = 5.919400e-08;
    real GpCa = 5.000000e-04;
    real KmCap = 5.000000e-04;
    real Fjunc = 1.000000e+00;
    real GClCa = 2.843000e-01;
    real KdClCa = 1.000000e-01;
    real GClb = 1.980000e-03;
    real tauNa = 2.000000e+00;
    real tauK = 2.000000e+00;
    real tauCa = 2.000000e-01;
    real bt = 4.750000e+00;
    real cajsr_half = 1.700000e+00;
    real Jrel_b = 1.537800e+00;
    real Jup_b = 1.000000e+00;
    real vcell = ((((1.000000e+03*3.140000e+00)*rad)*rad)*L);
    real Ageo = ((((2.000000e+00*3.140000e+00)*rad)*rad)+(((2.000000e+00*3.140000e+00)*rad)*L));
    real Istim = stim(time, i_Stim_Start, i_Stim_End, i_Stim_Amplitude, i_Stim_Period, i_Stim_PulseDuration);
    real vffrt = (((v*F)*F)/(R*T));
    real vfrt = ((v*F)/(R*T));
    real CaMKb = ((CaMKo*(1.000000e+00-CaMKt))/(1.000000e+00+(KmCaM/cass)));
    real cmdnmax = calc_cmdnmax(celltype, cmdnmax_b);
    real Bcass = (1.000000e+00/((1.000000e+00+((BSRmax*KmBSR)/pow((KmBSR+cass), 2.000000e+00)))+((BSLmax*KmBSL)/pow((KmBSL+cass), 2.000000e+00))));
    real Bcajsr = (1.000000e+00/(1.000000e+00+((csqnmax*kmcsqn)/pow((kmcsqn+cajsr), 2.000000e+00))));
    real ENa = (((R*T)/(zna*F))*log((nao/nai)));
    real EK = (((R*T)/(zk*F))*log((ko/ki)));
    real EKs = (((R*T)/(zk*F))*log(((ko+(PKNa*nao))/(ki+(PKNa*nai)))));
    real ECl = (((R*T)/(zcl*F))*log((clo/cli)));
    real akik = pow((ko/K_o_n), 2.400000e-01);
    real bkik = (1.000000e+00/(1.000000e+00+pow((A_atp/K_atp), 2.000000e+00)));
    real mss = (1.000000e+00/pow((1.000000e+00+exp(((-(v+5.686000e+01))/9.030000e+00))), 2.000000e+00));
    real tm = ((1.292000e-01*exp((-pow(((v+4.579000e+01)/1.554000e+01), 2.000000e+00))))+(6.487000e-02*exp((-pow(((v-4.823000e+00)/5.112000e+01), 2.000000e+00)))));
    real hss = (1.000000e+00/pow((1.000000e+00+exp(((v+7.155000e+01)/7.430000e+00))), 2.000000e+00));
    real ah = calc_ah(v);
    real bh = calc_bh(v);
    real aj = calc_aj(v);
    real bj = calc_bj(v);
    real hssp = (1.000000e+00/pow((1.000000e+00+exp(((v+7.755000e+01)/7.430000e+00))), 2.000000e+00));
    real mLss = (1.000000e+00/(1.000000e+00+exp(((-(v+4.285000e+01))/5.264000e+00))));
    real tmL = ((1.292000e-01*exp((-pow(((v+4.579000e+01)/1.554000e+01), 2.000000e+00))))+(6.487000e-02*exp((-pow(((v-4.823000e+00)/5.112000e+01), 2.000000e+00)))));
    real hLss = (1.000000e+00/(1.000000e+00+exp(((v+8.761000e+01)/7.488000e+00))));
    real hLssp = (1.000000e+00/(1.000000e+00+exp(((v+9.381000e+01)/7.488000e+00))));
    real thLp = (3.000000e+00*thL);
    real GNaL = calc_GNaL(celltype, GNaL_b);
    real ass = (1.000000e+00/(1.000000e+00+exp(((-((v+EKshift)-1.434000e+01))/1.482000e+01))));
    real ta = (1.051500e+00/((1.000000e+00/(1.208900e+00*(1.000000e+00+exp(((-((v+EKshift)-1.840990e+01))/2.938140e+01)))))+(3.500000e+00/(1.000000e+00+exp((((v+EKshift)+1.000000e+02)/2.938140e+01))))));
    real iss = (1.000000e+00/(1.000000e+00+exp((((v+EKshift)+4.394000e+01)/5.711000e+00))));
    real delta_epi = calc_delta_epi(celltype, v, EKshift);
    real tiF_b = (4.562000e+00+(1.000000e+00/((3.933000e-01*exp(((-((v+EKshift)+1.000000e+02))/1.000000e+02)))+(8.004000e-02*exp((((v+EKshift)+5.000000e+01)/1.659000e+01))))));
    real tiS_b = (2.362000e+01+(1.000000e+00/((1.416000e-03*exp(((-((v+EKshift)+9.652000e+01))/5.905000e+01)))+(1.780000e-08*exp((((v+EKshift)+1.141000e+02)/8.079010e+00))))));
    real AiF = (1.000000e+00/(1.000000e+00+exp((((v+EKshift)-2.136000e+02)/1.512000e+02))));
    real assp = (1.000000e+00/(1.000000e+00+exp(((-((v+EKshift)-2.434000e+01))/1.482000e+01))));
    real dti_develop = (1.354000e+00+(1.000000e-04/(exp((((v+EKshift)-1.674000e+02)/1.589000e+01))+exp(((-((v+EKshift)-1.223000e+01))/2.154000e-01)))));
    real dti_recover = (1.000000e+00-(5.000000e-01/(1.000000e+00+exp((((v+EKshift)+7.000000e+01)/2.000000e+01)))));
    real Gto = calc_Gto(celltype, Gto_b);
    real dss = calc_dss(v);
    real td = ((offset+6.000000e-01)+(1.000000e+00/(exp(((-5.000000e-02)*((v+vShift)+6.000000e+00)))+exp((9.000000e-02*((v+vShift)+1.400000e+01))))));
    real fss = (1.000000e+00/(1.000000e+00+exp(((v+1.958000e+01)/3.696000e+00))));
    real tff = (7.000000e+00+(1.000000e+00/((4.500000e-03*exp(((-(v+2.000000e+01))/1.000000e+01)))+(4.500000e-03*exp(((v+2.000000e+01)/1.000000e+01))))));
    real tfs = (1.000000e+03+(1.000000e+00/((3.500000e-05*exp(((-(v+5.000000e+00))/4.000000e+00)))+(3.500000e-05*exp(((v+5.000000e+00)/6.000000e+00))))));
    real Afs = (1.000000e+00-Aff);
    real tfcaf = (7.000000e+00+(1.000000e+00/((4.000000e-02*exp(((-(v-4.000000e+00))/7.000000e+00)))+(4.000000e-02*exp(((v-4.000000e+00)/7.000000e+00))))));
    real tfcas = (1.000000e+02+(1.000000e+00/((1.200000e-04*exp(((-v)/3.000000e+00)))+(1.200000e-04*exp((v/7.000000e+00))))));
    real Afcaf = (3.000000e-01+(6.000000e-01/(1.000000e+00+exp(((v-1.000000e+01)/1.000000e+01)))));
    real jcass = (1.000000e+00/(1.000000e+00+exp(((v+1.808000e+01)/2.791600e+00))));
    real km2n = (jca*1.000000e+00);
    real Io = ((5.000000e-01*(((nao+ko)+clo)+(4.000000e+00*cao)))/1.000000e+03);
    real Iss = ((5.000000e-01*(((nass+kss)+cli)+(4.000000e+00*cass)))/1.000000e+03);
    real constA = (1.820000e+06*pow((dielConstant*T), (-1.500000e+00)));
    real PCa = calc_PCa(celltype, PCa_b);
    real Ii = ((5.000000e-01*(((nai+ki)+cli)+(4.000000e+00*cai)))/1.000000e+03);
    real GKr = calc_GKr(celltype, GKr_b);
    real xs1ss = (1.000000e+00/(1.000000e+00+exp(((-(v+1.160000e+01))/8.932000e+00))));
    real txs1 = (8.173000e+02+(1.000000e+00/((2.326000e-04*exp(((v+4.828000e+01)/1.780000e+01)))+(1.292000e-03*exp(((-(v+2.100000e+02))/2.300000e+02))))));
    real txs2 = (1.000000e+00/((1.000000e-02*exp(((v-5.000000e+01)/2.000000e+01)))+(1.930000e-02*exp(((-(v+6.654010e+01))/3.100000e+01)))));
    real KsCa = (1.000000e+00+(6.000000e-01/(1.000000e+00+pow((3.800000e-05/cai), 1.400000e+00))));
    real GKs = calc_GKs(celltype, GKs_b);
    real GK1 = 0.000000e+00;
    GK1 = calc_GK1(celltype, GK1_b);
    real h4_i = (1.000000e+00+((nai/kna1)*(1.000000e+00+(nai/kna2))));
    real h10_i = ((kasymm+1.000000e+00)+((nao/kna1)*(1.000000e+00+(nao/kna2))));
    real k2_i = kcaoff;
    real k5_i = kcaoff;
    real allo_i = (1.000000e+00/(1.000000e+00+pow((KmCaAct/cai), 2.000000e+00)));
    real Gncx = calc_Gncx(celltype, Gncx_b);
    real h4_ss = (1.000000e+00+((nass/kna1)*(1.000000e+00+(nass/kna2))));
    real h10_ss = ((kasymm+1.000000e+00)+((nao/kna1)*(1.000000e+00+(nao/kna2))));
    real k2_ss = kcaoff;
    real k5_ss = kcaoff;
    real allo_ss = (1.000000e+00/(1.000000e+00+pow((KmCaAct/cass), 2.000000e+00)));
    real P = (eP/(((1.000000e+00+(H/Khp))+(nai/Knap))+(ki/Kxkur)));
    real b1 = (k1m*MgADP);
    real a2 = k2p;
    real a4 = (((k4p*MgATP)/Kmgatp)/(1.000000e+00+(MgATP/Kmgatp)));
    real Pnak = calc_Pnak(celltype, Pnak_b);
    real xkb = (1.000000e+00/(1.000000e+00+exp(((-(v-1.089680e+01))/2.398710e+01))));
    real GKb = calc_GKb(celltype, GKb_b);
    real IpCa = ((GpCa*cai)/(KmCap+cai));
    real JdiffNa = ((nass-nai)/tauNa);
    real JdiffK = ((kss-ki)/tauK);
    real Jdiff = ((cass-cai)/tauCa);
    real a_rel = ((5.000000e-01*bt)/1.000000e+00);
    real tau_rel_b = (bt/(1.000000e+00+(1.230000e-02/cajsr)));
    real btp = (1.250000e+00*bt);
    real upScale = calc_upScale(celltype);
    real Jleak = ((4.882500e-03*cansr)/1.500000e+01);
    real Jtr = ((cansr-cajsr)/6.000000e+01);
    real Acap = (2.000000e+00*Ageo);
    real vmyo = (6.800000e-01*vcell);
    real vnsr = (5.520000e-02*vcell);
    real vjsr = (4.800000e-03*vcell);
    real vss = (2.000000e-02*vcell);
    real CaMKa = (CaMKb+CaMKt);
    real Bcai = (1.000000e+00/((1.000000e+00+((cmdnmax*kmcmdn)/pow((kmcmdn+cai), 2.000000e+00)))+((trpnmax*kmtrpn)/pow((kmtrpn+cai), 2.000000e+00))));
    real I_katp = ((((fkatp*gkatp)*akik)*bkik)*(v-EK));
    real th = (1.000000e+00/(ah+bh));
    real jss = hss;
    real tj = (1.000000e+00/(aj+bj));
    real tiF = (tiF_b*delta_epi);
    real tiS = (tiS_b*delta_epi);
    real AiS = (1.000000e+00-AiF);
    real f = ((Aff*ff)+(Afs*fs));
    real fcass = fss;
    real Afcas = (1.000000e+00-Afcaf);
    real tffp = (2.500000e+00*tff);
    real fp = ((Aff*ffp)+(Afs*fs));
    real tfcafp = (2.500000e+00*tfcaf);
    real anca_ss = (1.000000e+00/((k2n/km2n)+pow((1.000000e+00+(Kmn/cass)), 4.000000e+00)));
    real gamma_cass = exp((((-constA)*4.000000e+00)*((pow(Iss, (1.000000e+00/2.000000e+00))/(1.000000e+00+pow(Iss, (1.000000e+00/2.000000e+00))))-(3.000000e-01*Iss))));
    real gamma_cao = exp((((-constA)*4.000000e+00)*((pow(Io, (1.000000e+00/2.000000e+00))/(1.000000e+00+pow(Io, (1.000000e+00/2.000000e+00))))-(3.000000e-01*Io))));
    real gamma_nass = exp((((-constA)*1.000000e+00)*((pow(Iss, (1.000000e+00/2.000000e+00))/(1.000000e+00+pow(Iss, (1.000000e+00/2.000000e+00))))-(3.000000e-01*Iss))));
    real gamma_nao = exp((((-constA)*1.000000e+00)*((pow(Io, (1.000000e+00/2.000000e+00))/(1.000000e+00+pow(Io, (1.000000e+00/2.000000e+00))))-(3.000000e-01*Io))));
    real gamma_kss = exp((((-constA)*1.000000e+00)*((pow(Iss, (1.000000e+00/2.000000e+00))/(1.000000e+00+pow(Iss, (1.000000e+00/2.000000e+00))))-(3.000000e-01*Iss))));
    real gamma_ko = exp((((-constA)*1.000000e+00)*((pow(Io, (1.000000e+00/2.000000e+00))/(1.000000e+00+pow(Io, (1.000000e+00/2.000000e+00))))-(3.000000e-01*Io))));
    real PCap = (1.100000e+00*PCa);
    real PCaNa = (1.250000e-03*PCa);
    real PCaK = (3.574000e-04*PCa);
    real anca_i = (1.000000e+00/((k2n/km2n)+pow((1.000000e+00+(Kmn/cai)), 4.000000e+00)));
    real gamma_cai = exp((((-constA)*4.000000e+00)*((pow(Ii, (1.000000e+00/2.000000e+00))/(1.000000e+00+pow(Ii, (1.000000e+00/2.000000e+00))))-(3.000000e-01*Ii))));
    real gamma_nai = exp((((-constA)*1.000000e+00)*((pow(Ii, (1.000000e+00/2.000000e+00))/(1.000000e+00+pow(Ii, (1.000000e+00/2.000000e+00))))-(3.000000e-01*Ii))));
    real gamma_ki = exp((((-constA)*1.000000e+00)*((pow(Ii, (1.000000e+00/2.000000e+00))/(1.000000e+00+pow(Ii, (1.000000e+00/2.000000e+00))))-(3.000000e-01*Ii))));
    real alpha = (1.161000e-01*exp((2.990000e-01*vfrt)));
    real beta = (2.442000e-01*exp(((-1.604000e+00)*vfrt)));
    real alpha_2 = (5.780000e-02*exp((9.710000e-01*vfrt)));
    real beta_2 = (3.490000e-04*exp(((-1.062000e+00)*vfrt)));
    real alpha_i = (2.533000e-01*exp((5.953010e-01*vfrt)));
    real beta_i = (6.525000e-02*exp(((-8.209000e-01)*vfrt)));
    real alpha_C2ToI = (5.200000e-05*exp((1.525000e+00*vfrt)));
    real IKr = (((GKr*pow((ko/5.000000e+00), (1.000000e+00/2.000000e+00)))*O)*(v-EK));
    real xs2ss = xs1ss;
    real IKs = ((((GKs*KsCa)*xs1)*xs2)*(v-EKs));
    real aK1 = (4.094000e+00/(1.000000e+00+exp((1.217000e-01*((v-EK)-4.993400e+01)))));
    real bK1 = (((1.572000e+01*exp((6.740000e-02*((v-EK)-3.257000e+00))))+exp((6.180000e-02*((v-EK)-5.943100e+02))))/(1.000000e+00+exp(((-1.629000e-01)*((v-EK)+1.420700e+01)))));
    real hca = exp((qca*vfrt));
    real hna = exp((qna*vfrt));
    real h5_i = ((nai*nai)/((h4_i*kna1)*kna2));
    real h6_i = (1.000000e+00/h4_i);
    real h11_i = ((nao*nao)/((h10_i*kna1)*kna2));
    real h12_i = (1.000000e+00/h10_i);
    real h5_ss = ((nass*nass)/((h4_ss*kna1)*kna2));
    real h6_ss = (1.000000e+00/h4_ss);
    real h11_ss = ((nao*nao)/((h10_ss*kna1)*kna2));
    real h12_ss = (1.000000e+00/h10_ss);
    real Knai = (Knai0*exp(((delta*vfrt)/3.000000e+00)));
    real Knao = (Knao0*exp((((1.000000e+00-delta)*vfrt)/3.000000e+00)));
    real b3 = (((k3m*P)*H)/(1.000000e+00+(MgATP/Kmgatp)));
    real IKb = ((GKb*xkb)*(v-EK));
    real INab = (((PNab*vffrt)*((nai*exp(vfrt))-nao))/(exp(vfrt)-1.000000e+00));
    real IClCa_junc = (((Fjunc*GClCa)/(1.000000e+00+(KdClCa/cass)))*(v-ECl));
    real IClCa_sl = ((((1.000000e+00-Fjunc)*GClCa)/(1.000000e+00+(KdClCa/cai)))*(v-ECl));
    real IClb = (GClb*(v-ECl));
    real tau_relp_b = (btp/(1.000000e+00+(1.230000e-02/cajsr)));
    real tau_rel = calc_tau_rel(tau_relp_b);
    real a_relp = ((5.000000e-01*btp)/1.000000e+00);
    real Jupnp = (((upScale*5.425000e-03)*cai)/(cai+9.200000e-04));
    real Jupp = ((((upScale*2.750000e+00)*5.425000e-03)*cai)/((cai+9.200000e-04)-1.700000e-04));
    real tjp = (1.460000e+00*tj);
    real fINap = (1.000000e+00/(1.000000e+00+(KmCaMK/CaMKa)));
    real fINaLp = (1.000000e+00/(1.000000e+00+(KmCaMK/CaMKa)));
    real i = ((AiF*iF)+(AiS*iS));
    real tiFp = ((dti_develop*dti_recover)*tiF);
    real tiSp = ((dti_develop*dti_recover)*tiS);
    real ip = ((AiF*iFp)+(AiS*iSp));
    real fItop = (1.000000e+00/(1.000000e+00+(KmCaMK/CaMKa)));
    real fca = ((Afcaf*fcaf)+(Afcas*fcas));
    real fcap = ((Afcaf*fcafp)+(Afcas*fcas));
    real PhiCaL_ss = (((4.000000e+00*vffrt)*(((gamma_cass*cass)*exp((2.000000e+00*vfrt)))-(gamma_cao*cao)))/(exp((2.000000e+00*vfrt))-1.000000e+00));
    real PhiCaNa_ss = (((1.000000e+00*vffrt)*(((gamma_nass*nass)*exp((1.000000e+00*vfrt)))-(gamma_nao*nao)))/(exp((1.000000e+00*vfrt))-1.000000e+00));
    real PhiCaK_ss = (((1.000000e+00*vffrt)*(((gamma_kss*kss)*exp((1.000000e+00*vfrt)))-(gamma_ko*ko)))/(exp((1.000000e+00*vfrt))-1.000000e+00));
    real PCaNap = (1.250000e-03*PCap);
    real PCaKp = (3.574000e-04*PCap);
    real fICaLp = (1.000000e+00/(1.000000e+00+(KmCaMK/CaMKa)));
    real PhiCaL_i = (((4.000000e+00*vffrt)*(((gamma_cai*cai)*exp((2.000000e+00*vfrt)))-(gamma_cao*cao)))/(exp((2.000000e+00*vfrt))-1.000000e+00));
    real PhiCaNa_i = (((1.000000e+00*vffrt)*(((gamma_nai*nai)*exp((1.000000e+00*vfrt)))-(gamma_nao*nao)))/(exp((1.000000e+00*vfrt))-1.000000e+00));
    real PhiCaK_i = (((1.000000e+00*vffrt)*(((gamma_ki*ki)*exp((1.000000e+00*vfrt)))-(gamma_ko*ko)))/(exp((1.000000e+00*vfrt))-1.000000e+00));
    real beta_ItoC2 = (((beta_2*beta_i)*alpha_C2ToI)/(alpha_2*alpha_i));
    real K1ss = (aK1/(aK1+bK1));
    real h1_i = (1.000000e+00+((nai/kna3)*(1.000000e+00+hna)));
    real h7_i = (1.000000e+00+((nao/kna3)*(1.000000e+00+(1.000000e+00/hna))));
    real k1_i = ((h12_i*cao)*kcaon);
    real k6_i = ((h6_i*cai)*kcaon);
    real h1_ss = (1.000000e+00+((nass/kna3)*(1.000000e+00+hna)));
    real h7_ss = (1.000000e+00+((nao/kna3)*(1.000000e+00+(1.000000e+00/hna))));
    real k1_ss = ((h12_ss*cao)*kcaon);
    real k6_ss = ((h6_ss*cass)*kcaon);
    real a1 = ((k1p*pow((nai/Knai), 3.000000e+00))/((pow((1.000000e+00+(nai/Knai)), 3.000000e+00)+pow((1.000000e+00+(ki/Kki)), 2.000000e+00))-1.000000e+00));
    real b2 = ((k2m*pow((nao/Knao), 3.000000e+00))/((pow((1.000000e+00+(nao/Knao)), 3.000000e+00)+pow((1.000000e+00+(ko/Kko)), 2.000000e+00))-1.000000e+00));
    real a3 = ((k3p*pow((ko/Kko), 2.000000e+00))/((pow((1.000000e+00+(nao/Knao)), 3.000000e+00)+pow((1.000000e+00+(ko/Kko)), 2.000000e+00))-1.000000e+00));
    real b4 = ((k4m*pow((ki/Kki), 2.000000e+00))/((pow((1.000000e+00+(nai/Knai)), 3.000000e+00)+pow((1.000000e+00+(ki/Kki)), 2.000000e+00))-1.000000e+00));
    real ICab = ((((PCab*4.000000e+00)*vffrt)*(((gamma_cai*cai)*exp((2.000000e+00*vfrt)))-(gamma_cao*cao)))/(exp((2.000000e+00*vfrt))-1.000000e+00));
    real IClCa = (IClCa_junc+IClCa_sl);
    real tau_relp = calc_tau_relp(tau_relp_b);
    real fJrelp = (1.000000e+00/(1.000000e+00+(KmCaMK/CaMKa)));
    real fJupp = (1.000000e+00/(1.000000e+00+(KmCaMK/CaMKa)));
    real INa = (((GNa*(v-ENa))*pow(m, 3.000000e+00))*((((1.000000e+00-fINap)*h)*j)+((fINap*hp)*jp)));
    real INaL = (((GNaL*(v-ENa))*mL)*(((1.000000e+00-fINaLp)*hL)+(fINaLp*hLp)));
    real Ito = ((Gto*(v-EK))*((((1.000000e+00-fItop)*a)*i)+((fItop*ap)*ip)));
    real ICaL_ss = (ICaL_fractionSS*((((((1.000000e+00-fICaLp)*PCa)*PhiCaL_ss)*d)*((f*(1.000000e+00-nca_ss))+((jca*fca)*nca_ss)))+((((fICaLp*PCap)*PhiCaL_ss)*d)*((fp*(1.000000e+00-nca_ss))+((jca*fcap)*nca_ss)))));
    real ICaNa_ss = (ICaL_fractionSS*((((((1.000000e+00-fICaLp)*PCaNa)*PhiCaNa_ss)*d)*((f*(1.000000e+00-nca_ss))+((jca*fca)*nca_ss)))+((((fICaLp*PCaNap)*PhiCaNa_ss)*d)*((fp*(1.000000e+00-nca_ss))+((jca*fcap)*nca_ss)))));
    real ICaK_ss = (ICaL_fractionSS*((((((1.000000e+00-fICaLp)*PCaK)*PhiCaK_ss)*d)*((f*(1.000000e+00-nca_ss))+((jca*fca)*nca_ss)))+((((fICaLp*PCaKp)*PhiCaK_ss)*d)*((fp*(1.000000e+00-nca_ss))+((jca*fcap)*nca_ss)))));
    real ICaL_i = ((1.000000e+00-ICaL_fractionSS)*((((((1.000000e+00-fICaLp)*PCa)*PhiCaL_i)*d)*((f*(1.000000e+00-nca_i))+((jca*fca)*nca_i)))+((((fICaLp*PCap)*PhiCaL_i)*d)*((fp*(1.000000e+00-nca_i))+((jca*fcap)*nca_i)))));
    real ICaNa_i = ((1.000000e+00-ICaL_fractionSS)*((((((1.000000e+00-fICaLp)*PCaNa)*PhiCaNa_i)*d)*((f*(1.000000e+00-nca_i))+((jca*fca)*nca_i)))+((((fICaLp*PCaNap)*PhiCaNa_i)*d)*((fp*(1.000000e+00-nca_i))+((jca*fcap)*nca_i)))));
    real ICaK_i = ((1.000000e+00-ICaL_fractionSS)*((((((1.000000e+00-fICaLp)*PCaK)*PhiCaK_i)*d)*((f*(1.000000e+00-nca_i))+((jca*fca)*nca_i)))+((((fICaLp*PCaKp)*PhiCaK_i)*d)*((fp*(1.000000e+00-nca_i))+((jca*fcap)*nca_i)))));
    real IK1 = (((GK1*pow((ko/5.000000e+00), (1.000000e+00/2.000000e+00)))*K1ss)*(v-EK));
    real h2_i = ((nai*hna)/(kna3*h1_i));
    real h3_i = (1.000000e+00/h1_i);
    real h8_i = (nao/((kna3*hna)*h7_i));
    real h9_i = (1.000000e+00/h7_i);
    real h2_ss = ((nass*hna)/(kna3*h1_ss));
    real h3_ss = (1.000000e+00/h1_ss);
    real h8_ss = (nao/((kna3*hna)*h7_ss));
    real h9_ss = (1.000000e+00/h7_ss);
    real x1 = (((((a4*a1)*a2)+((b2*b4)*b3))+((a2*b4)*b3))+((b3*a1)*a2));
    real x2 = (((((b2*b1)*b4)+((a1*a2)*a3))+((a3*b1)*b4))+((a2*a3)*b4));
    real x3 = (((((a2*a3)*a4)+((b3*b2)*b1))+((b2*b1)*a4))+((a3*a4)*b1));
    real x4 = (((((b4*b3)*b2)+((a3*a4)*a1))+((b2*a4)*a1))+((b3*b2)*a1));
    real Jrel = (Jrel_b*(((1.000000e+00-fJrelp)*Jrel_np)+(fJrelp*Jrel_p)));
    real Jup = (Jup_b*((((1.000000e+00-fJupp)*Jupnp)+(fJupp*Jupp))-Jleak));
    real ICaL = (ICaL_ss+ICaL_i);
    real ICaNa = (ICaNa_ss+ICaNa_i);
    real ICaK = (ICaK_ss+ICaK_i);
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
    real Jrel_inf_b = ((((-a_rel)*ICaL_ss)/1.000000e+00)/(1.000000e+00+pow((cajsr_half/cajsr), 8.000000e+00)));
    real Jrel_infp_b = ((((-a_relp)*ICaL_ss)/1.000000e+00)/(1.000000e+00+pow((cajsr_half/cajsr), 8.000000e+00)));
    real k3_i = (k3p_i+k3pp_i);
    real k4_i = (k4p_i+k4pp_i);
    real k3_ss = (k3p_ss+k3pp_ss);
    real k4_ss = (k4p_ss+k4pp_ss);
    real JnakNa = (3.000000e+00*((E1*a3)-(E2*b3)));
    real JnakK = (2.000000e+00*((E4*b1)-(E3*a1)));
    real Jrel_inf = calc_Jrel_inf(celltype, Jrel_inf_b);
    real Jrel_infp = calc_Jrel_infp(celltype, Jrel_infp_b);
    real x1_i = (((k2_i*k4_i)*(k7_i+k6_i))+((k5_i*k7_i)*(k2_i+k3_i)));
    real x2_i = (((k1_i*k7_i)*(k4_i+k5_i))+((k4_i*k6_i)*(k1_i+k8_i)));
    real x3_i = (((k1_i*k3_i)*(k7_i+k6_i))+((k8_i*k6_i)*(k2_i+k3_i)));
    real x4_i = (((k2_i*k8_i)*(k4_i+k5_i))+((k3_i*k5_i)*(k1_i+k8_i)));
    real x1_ss = (((k2_ss*k4_ss)*(k7_ss+k6_ss))+((k5_ss*k7_ss)*(k2_ss+k3_ss)));
    real x2_ss = (((k1_ss*k7_ss)*(k4_ss+k5_ss))+((k4_ss*k6_ss)*(k1_ss+k8_ss)));
    real x3_ss = (((k1_ss*k3_ss)*(k7_ss+k6_ss))+((k8_ss*k6_ss)*(k2_ss+k3_ss)));
    real x4_ss = (((k2_ss*k8_ss)*(k4_ss+k5_ss))+((k3_ss*k5_ss)*(k1_ss+k8_ss)));
    real INaK = (Pnak*((zna*JnakNa)+(zk*JnakK)));
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
    real INaCa_i = ((((1.000000e+00-INaCa_fractionSS)*Gncx)*allo_i)*((zna*JncxNa_i)+(zca*JncxCa_i)));
    real INaCa_ss = (((INaCa_fractionSS*Gncx)*allo_ss)*((zna*JncxNa_ss)+(zca*JncxCa_ss)));
    rDY[0] = (-(((((((((((((((((((INa+INaL)+Ito)+ICaL)+ICaNa)+ICaK)+IKr)+IKs)+IK1)+INaCa_i)+INaCa_ss)+INaK)+INab)+IKb)+IpCa)+ICab)+IClCa)+IClb)+I_katp)+Istim));
    rDY[1] = (((aCaMK*CaMKb)*(CaMKb+CaMKt))-(bCaMK*CaMKt));
    rDY[2] = ((((-(((((INa+INaL)+(3.000000e+00*INaCa_i))+ICaNa_i)+(3.000000e+00*INaK))+INab))*Acap)/(F*vmyo))+((JdiffNa*vss)/vmyo));
    rDY[3] = ((((-(ICaNa_ss+(3.000000e+00*INaCa_ss)))*Acap)/(F*vss))-JdiffNa);
    rDY[4] = ((((-((((((((Ito+IKr)+IKs)+IK1)+IKb)+I_katp)+Istim)-(2.000000e+00*INaK))+ICaK_i))*Acap)/(F*vmyo))+((JdiffK*vss)/vmyo));
    rDY[5] = ((((-ICaK_ss)*Acap)/(F*vss))-JdiffK);
    rDY[6] = (Bcai*(((((-(((ICaL_i+IpCa)+ICab)-(2.000000e+00*INaCa_i)))*Acap)/((2.000000e+00*F)*vmyo))-((Jup*vnsr)/vmyo))+((Jdiff*vss)/vmyo)));
    rDY[7] = (Bcass*(((((-(ICaL_ss-(2.000000e+00*INaCa_ss)))*Acap)/((2.000000e+00*F)*vss))+((Jrel*vjsr)/vss))-Jdiff));
    rDY[8] = (Jup-((Jtr*vjsr)/vnsr));
    rDY[9] = (Bcajsr*(Jtr-Jrel));
    rDY[10] = ((mss-m)/tm);
    rDY[11] = ((hss-h)/th);
    rDY[12] = ((jss-j)/tj);
    rDY[13] = ((hssp-hp)/th);
    rDY[14] = ((jss-jp)/tjp);
    rDY[15] = ((mLss-mL)/tmL);
    rDY[16] = ((hLss-hL)/thL);
    rDY[17] = ((hLssp-hLp)/thLp);
    rDY[18] = ((ass-a)/ta);
    rDY[19] = ((iss-iF)/tiF);
    rDY[20] = ((iss-iS)/tiS);
    rDY[21] = ((assp-ap)/ta);
    rDY[22] = ((iss-iFp)/tiFp);
    rDY[23] = ((iss-iSp)/tiSp);
    rDY[24] = ((dss-d)/td);
    rDY[25] = ((fss-ff)/tff);
    rDY[26] = ((fss-fs)/tfs);
    rDY[27] = ((fcass-fcaf)/tfcaf);
    rDY[28] = ((fcass-fcas)/tfcas);
    rDY[29] = ((jcass-jca)/tjca);
    rDY[30] = ((fss-ffp)/tffp);
    rDY[31] = ((fcass-fcafp)/tfcafp);
    rDY[32] = ((anca_ss*k2n)-(nca_ss*km2n));
    rDY[33] = ((anca_i*k2n)-(nca_i*km2n));
    rDY[34] = ((beta*C2)-(alpha*C3));
    rDY[35] = (((alpha*C3)+(beta_1*C1))-((beta+alpha_1)*C2));
    rDY[36] = ((((alpha_1*C2)+(beta_2*O))+(beta_ItoC2*I))-(((beta_1+alpha_2)+alpha_C2ToI)*C1));
    rDY[37] = (((alpha_2*C1)+(beta_i*I))-((beta_2+alpha_i)*O));
    rDY[38] = (((alpha_C2ToI*C1)+(alpha_i*O))-((beta_ItoC2+beta_i)*I));
    rDY[39] = ((xs1ss-xs1)/txs1);
    rDY[40] = ((xs2ss-xs2)/txs2);
    rDY[41] = ((Jrel_inf-Jrel_np)/tau_rel);
    rDY[42] = ((Jrel_infp-Jrel_p)/tau_relp);

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
                if(sv[i] > max[i]) max[i] = sv[i];
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
        fprintf(min_max_file, "%e;%e\n", min[i], max[i]);
    }
    fclose(min_max_file);
    free(min_max);
    
    free(_k1__);
    free(_k2__);
}


int main(int argc, char **argv) {

    real *x0 = (real*) malloc(sizeof(real)*NEQ);

    real values[43];
    values[0] = (-8.876380e+01); //v
    values[1] = 1.110000e-02; //CaMKt
    values[2] = 1.210250e+01; //nai
    values[3] = 1.210290e+01; //nass
    values[4] = 1.423002e+02; //ki
    values[5] = 1.423002e+02; //kss
    values[6] = 8.158300e-05; //cai
    values[7] = 7.030500e-05; //cass
    values[8] = 1.521100e+00; //cansr
    values[9] = 1.521400e+00; //cajsr
    values[10] = 8.057200e-04; //m
    values[11] = 8.286000e-01; //h
    values[12] = 8.284000e-01; //j
    values[13] = 6.707000e-01; //hp
    values[14] = 8.281000e-01; //jp
    values[15] = 1.629000e-04; //mL
    values[16] = 5.255000e-01; //hL
    values[17] = 2.872000e-01; //hLp
    values[18] = 9.509800e-04; //a
    values[19] = 9.996000e-01; //iF
    values[20] = 5.936000e-01; //iS
    values[21] = 4.845400e-04; //ap
    values[22] = 9.996000e-01; //iFp
    values[23] = 6.538000e-01; //iSp
    values[24] = 8.108400e-09; //d
    values[25] = 1.000000e+00; //ff
    values[26] = 9.390000e-01; //fs
    values[27] = 1.000000e+00; //fcaf
    values[28] = 9.999000e-01; //fcas
    values[29] = 1.000000e+00; //jca
    values[30] = 1.000000e+00; //ffp
    values[31] = 1.000000e+00; //fcafp
    values[32] = 6.646200e-04; //nca_ss
    values[33] = 1.200000e-03; //nca_i
    values[34] = 9.981000e-01; //C3
    values[35] = 8.510900e-04; //C2
    values[36] = 7.034400e-04; //C1
    values[37] = 3.758500e-04; //O
    values[38] = 1.328900e-05; //I
    values[39] = 2.480000e-01; //xs1
    values[40] = 1.770700e-04; //xs2
    values[41] = 1.612900e-22; //Jrel_np
    values[42] = 1.247500e-20; //Jrel_p
    
    for(int i = 0; i < 43; i++) {
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
    fprintf(f, "#t, v, CaMKt, nai, nass, ki, kss, cai, cass, cansr, cajsr, m, h, j, hp, jp, mL, hL, hLp, a, iF, iS, ap, iFp, iSp, d, ff, fs, fcaf, fcas, jca, ffp, fcafp, nca_ss, nca_i, C3, C2, C1, O, I, xs1, xs2, Jrel_np, Jrel_p\n");
    fprintf(f, "0.0 ");
    for(int i = 0; i < NEQ; i++) {
        fprintf(f, "%lf ", x0[i]);
    }
    fprintf(f, "\n");


    solve_ode(x0, strtod(argv[1], NULL), f, argv[2]);
    free(x0);
    
    return (0);
}