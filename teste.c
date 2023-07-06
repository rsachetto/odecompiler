#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <float.h>
#include <string.h>
 

#define NEQ 3
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

real n = 1.000000e+03; //dimensionless
real init_i = 3.000000e+00;

void set_initial_conditions(real *x0, real *values) { 

    x0[0] = values[0]; //S
    x0[1] = values[1]; //I
    x0[2] = values[2]; //R

}

static int solve_model(real time, real *sv, real *rDY) {

    //State variables
    const real S =  sv[0];
    const real I =  sv[1];
    const real R =  sv[2];

    //Parameters
    real beta = (4.000000e-01/n);
    real gamma = 4.000000e-02; //dimensionless
    rDY[0] = (((-beta)*S)*I);
    rDY[1] = (((beta*S)*I)-(gamma*I));
    rDY[2] = (gamma*I);

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

void teste() {
print("END\n");
}


int main(int argc, char **argv) {

    real *x0 = (real*) malloc(sizeof(real)*NEQ);

    real values[3];
    values[0] = (n-init_i); //S
    values[1] = init_i; //I
    values[2] = 0.000000e+00; //R
    
    for(int i = 0; i < 3; i++) {
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
    fprintf(f, "#t, S, I, R\n");
    fprintf(f, "0.0 ");
    for(int i = 0; i < NEQ; i++) {
        fprintf(f, "%lf ", x0[i]);
    }
    fprintf(f, "\n");


    solve_ode(x0, strtod(argv[1], NULL), f, argv[2]);
    free(x0);
    teste();

    return (0);
}