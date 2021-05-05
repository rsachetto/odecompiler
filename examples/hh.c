#include <cvode/cvode.h>
#include <math.h>
#include <nvector/nvector_serial.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sundials/sundials_dense.h>
#include <sundials/sundials_types.h>
#include <sunlinsol/sunlinsol_dense.h> 
#include <sunmatrix/sunmatrix_dense.h> 

#define NEQ 4
typedef realtype real;

real stim(real t) {
    if((t>1.000000e+01)&&(t<1.050000e+01)) {
        return 2.000000e+01;
    }
    return 0.000000e+00;
}

real calc_E_Na(real E_R) {
    return (E_R+1.150000e+02);
}

void set_initial_conditions(N_Vector x0) { 

    NV_Ith_S(x0, 0) = (-7.500000e+01); //V
    NV_Ith_S(x0, 1) = 5.000000e-02; //m
    NV_Ith_S(x0, 2) = 6.000000e-01; //h
    NV_Ith_S(x0, 3) = 3.250000e-01; //n

}

static int solve_model(realtype time, N_Vector sv, N_Vector rDY, void *f_data) {

    //State variables
    const real V =  NV_Ith_S(sv, 0);
    const real m =  NV_Ith_S(sv, 1);
    const real h =  NV_Ith_S(sv, 2);
    const real n =  NV_Ith_S(sv, 3);

    //Parameters
    real Cm = 1.000000e+00;
    real E_R = (-7.500000e+01);
    real g_Na = 1.200000e+02;
    real g_K = 3.600000e+01;
    real g_L = 3.000000e-01;
    real E_Na = calc_E_Na(E_R);
    real alpha_m = (((-1.000000e-01)*(V+5.000000e+01))/(exp(((-(V+5.000000e+01))/1.000000e+01))-1.000000e+00));
    real beta_m = (4.000000e+00*exp(((-(V+7.500000e+01))/1.800000e+01)));
    real alpha_h = (7.000000e-02*exp(((-(V+7.500000e+01))/2.000000e+01)));
    real beta_h = (1.000000e+00/(exp(((-(V+4.500000e+01))/1.000000e+01))+1.000000e+00));
    real E_K = (E_R-1.200000e+01);
    real alpha_n = (((-1.000000e-02)*(V+6.500000e+01))/(exp(((-(V+6.500000e+01))/1.000000e+01))-1.000000e+00));
    real beta_n = (1.250000e-01*exp(((V+7.500000e+01)/8.000000e+01)));
    real E_L = (E_R+1.061300e+01);
    real i_Na = (((g_Na*pow(m, 3.000000e+00))*h)*(V-E_Na));
    real i_K = ((g_K*pow(n, 4.000000e+00))*(V-E_K));
    real i_L = (g_L*(V-E_L));
    E_Na = 1.000000e+00;
    NV_Ith_S(rDY, 0) = ((-((((-stim(time))+i_Na)+i_K)+i_L))/Cm);
    NV_Ith_S(rDY, 1) = ((alpha_m*(1.000000e+00-m))-(beta_m*m));
    NV_Ith_S(rDY, 2) = ((alpha_h*(1.000000e+00-h))-(beta_h*h));
    NV_Ith_S(rDY, 3) = ((alpha_n*(1.000000e+00-n))-(beta_n*n));

	return 0;  

}

static int check_flag(void *flagvalue, const char *funcname, int opt) {

    int *errflag;

    /* Check if SUNDIALS function returned NULL pointer - no memory allocated */
    if(opt == 0 && flagvalue == NULL) {

        fprintf(stderr, "\nSUNDIALS_ERROR: %s() failed - returned NULL pointer\n\n", funcname);
        return (1);
    }

    /* Check if flag < 0 */
    else if(opt == 1) {
        errflag = (int *)flagvalue;
        if(*errflag < 0) {
            fprintf(stderr, "\nSUNDIALS_ERROR: %s() failed with flag = %d\n\n", funcname, *errflag);
            return (1);
        }
    }

    /* Check if function returned NULL pointer - no memory allocated */
    else if(opt == 2 && flagvalue == NULL) {
        fprintf(stderr, "\nMEMORY_ERROR: %s() failed - returned NULL pointer\n\n", funcname);
        return (1);
    }

    return (0);
}
void solve_ode(N_Vector y, float final_t) {

    void *cvode_mem = NULL;
    int flag;

    // Set up solver
    cvode_mem = CVodeCreate(CV_BDF);

    if(cvode_mem == 0) {
        fprintf(stderr, "Error in CVodeMalloc: could not allocate\n");
        return;
    }

    flag = CVodeInit(cvode_mem, solve_model, 0, y);
    if(check_flag(&flag, "CVodeInit", 1))
        return;

    flag = CVodeSStolerances(cvode_mem, 1.49012e-6, 1.49012e-6);
    if(check_flag(&flag, "CVodeSStolerances", 1))
        return;

    // Create dense SUNMatrix for use in linear solver
    SUNMatrix A = SUNDenseMatrix(NEQ, NEQ);
    if(check_flag((void *)A, "SUNDenseMatrix", 0))
        return;

    // Create dense linear solver for use by CVode
    SUNLinearSolver LS = SUNLinSol_Dense(y, A);
    if(check_flag((void *)LS, "SUNLinSol_Dense", 0))
        return;

    // Attach the linear solver and matrix to CVode by calling CVodeSetLinearSolver
    flag = CVodeSetLinearSolver(cvode_mem, LS, A);
    if(check_flag((void *)&flag, "CVodeSetLinearSolver", 1))
        return;

	realtype dt=0.01;
    realtype tout = dt;
    int retval;
    realtype t;

	FILE *f = fopen("out.txt", "w");

	while(tout < final_t) {

		retval = CVode(cvode_mem, tout, y, &t, CV_NORMAL);

		if(retval == CV_SUCCESS) {			
	        fprintf(f, "%lf ", t);
			for(int i = 0; i < NEQ; i++) {
	        	fprintf(f, "%lf ", NV_Ith_S(y,i));
			} 
	        
			fprintf(f, "\n");

			tout+=dt;
		}

	}

    // Free the linear solver memory
    SUNLinSolFree(LS);
    SUNMatDestroy(A);
    CVodeFree(&cvode_mem);
}

int main(int argc, char **argv) {

	N_Vector x0 = N_VNew_Serial(NEQ);

	set_initial_conditions(x0);

	solve_ode(x0, strtod(argv[1], NULL));


	return (0);
}