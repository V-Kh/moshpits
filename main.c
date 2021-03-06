//===================================================
// Author: Matthew Bierbaum
// Project: Collective motion at heavy metal concerts
//===================================================
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>

#ifdef PLOT
#include "plot.h"
#endif

#ifdef FPS
#include <time.h>
#endif

//===========================================
// do we want to record various measurements?
//#define ANGULARMOM_TIMESERIES
//#define VELOCITY_DISTRIBUTION
//#define TEMPERATURE_BINS
#define SHOWCENTEROFMASS    0
#define SHOWVELOCITYARROWS  1
#define SHOWFORCECOLORS     0
//===========================================

//-----------------------------------------------------------
// some defines and helper functions for NBL
//------------------------------------------------------------
#define pi      3.141592653589
#define EPSILON DBL_EPSILON
#define BLACK   0
#define RED     1
#define RADS    10
#define BINS    50

void simulate(double alpha, double sigma, int seed, double damp);

void   init_circle(double *x, double *v, int *t, double s, long N, double L);
void   temperature(double *x, double *v, int *t, int N, double L, int *pbc, int bins[RADS][BINS]);
void   centerofmass(double *x, int *t, int N, double L, double *cmx, double *cmy);
double angularmom(double *x, double *v, int *t, int N, double L, int *pbc);

void   coords_to_index(double *x, int *size, int *index, double L);
int    mod_rvec(int a, int b, int p, int *image);
double mymod(double a, double b);

void   ran_seed(long j);
double ran_ran2();
unsigned long long int vseed;
unsigned long long int vran;


//===================================================
// the main function
//===================================================
int main(int argc, char **argv){
    double alpha_in = 0.9; 
    double sigma_in = 0.1;
    double damp_in  = 1.0;
    int seed_in     = 0;

    if (argc == 1) 
        simulate(alpha_in, sigma_in, seed_in, damp_in);
    else if (argc == 5){
        alpha_in = atof(argv[1]);
        sigma_in = atof(argv[2]);
        seed_in  = atoi(argv[3]);
        damp_in  = atof(argv[4]);
        simulate(alpha_in, sigma_in, seed_in, damp_in);
    }
    else {
        printf("usage:\n");
        printf("\t./entbody [alpha] [eta] [seed]\n");
    }
    return 0;
}



//==================================================
// simulation
//==================================================
void simulate(double alphain, double sigmain, int seed, double dampin){
    ran_seed(seed);
    int  RIC  = 0;

    int    NMAX    = 50;
    int    N       = 1000;
    double radius  = 1.0;
    double L       = 1.03*sqrt(pi*radius*radius*N);

    int pbc[] = {1,1};

    double epsilon = 25.0;
    double sigma   = sigmain;
    double alpha   = alphain;

    double vhappy_black = 0.0;
    double vhappy_red   = 1.0;
    double damp_coeff   = dampin;

    double dt  = 1e-1;
    double t   = 0.0;
    double R   = 2*radius; 
    double R2  = R*R;
    double FR  = 2*R;
    double FR2 = FR*FR;

    int i, j, k;

    int *type   = (int*)malloc(sizeof(int)*N);
    int *neigh  = (int*)malloc(sizeof(int)*N);
    double *rad = (double*)malloc(sizeof(double)*N); 
    double *col = (double*)malloc(sizeof(double)*N); 
    for (i=0; i<N; i++){ type[i] = neigh[i] = rad[i] = 0;}

    double *x = (double*)malloc(sizeof(double)*2*N);
    double *v = (double*)malloc(sizeof(double)*2*N);
    double *f = (double*)malloc(sizeof(double)*2*N);
    double *w = (double*)malloc(sizeof(double)*2*N);
    double *o = (double*)malloc(sizeof(double)*2*N);
    for (i=0; i<2*N; i++){o[i] = x[i] = v[i] = f[i] = w[i] = 0.0;}

    #ifdef PLOT 
    double time_end = 1e20;
    #else
    double time_end = 1e3;
    #endif

    #ifdef PLOT 
        int *key;
        double kickforce = 2.0;
        plot_init(); 
        #ifdef OPENIL
            plot_initialize_canvas();
        #endif
        plot_clear_screen();
        key = plot_render_particles(x, rad, type, N, L,col,0,0,0,0, pbc,v, SHOWVELOCITYARROWS);
        int showplot = 1;
    #endif

    //-------------------------------------------------
    // initialize
    if (RIC){
        for (i=0; i<N; i++){
            double t = 2*pi*ran_ran2();
    
            rad[i] = radius;
            x[2*i+0] = L*ran_ran2();
            x[2*i+1] = L*ran_ran2();
     
            if (ran_ran2() > 0.16){
                v[2*i+0] = 0.0;
                v[2*i+1] = 0.0;
                type[i] = BLACK;
            }
            else {
                v[2*i+0] = vhappy_red * sin(t);
                v[2*i+1] = vhappy_red * cos(t);
                type[i] = RED;
            } 
        }
    }
    else {
        for (i=0; i<N; i++)
            rad[i] = radius;
        init_circle(x, v, type, vhappy_red, N, L);
    }

    //-------------------------------------------------------
    // make boxes for the neighborlist
    int size[2];
    int size_total = 1;
    for (i=0; i<2; i++){
        size[i] = (int)(L / (FR)); 
        size_total *= size[i];
    }

    int *count = (int*)malloc(sizeof(int)*size_total);
    int *cells = (int*)malloc(sizeof(int)*size_total*NMAX);
    for (i=0; i<size_total; i++)
        count[i] = 0;
    for (i=0; i<size_total*NMAX; i++)
        cells[i] = 0;

    //==========================================================
    // where the magic happens
    //==========================================================
    int frames = 0;

    #ifdef FPS
    struct timespec start;
    clock_gettime(CLOCK_REALTIME, &start);
    #endif

    double angularmom_avg    = 0.0;
    double angularmom_std    = 0.0;
    double angularmom_sq_avg = 0.0;
    double angularmom_sq_std = 0.0;
    int angularmom_count = 0;

    double momentumx_avg = 0.0;
    double momentumx_std = 0.0;
    double momentumy_avg = 0.0;
    double momentumy_std = 0.0;
    int momentum_count = 0;
    
    double momentumsqx_avg = 0.0;
    double momentumsqx_std = 0.0;
    double momentumsqy_avg = 0.0;
    double momentumsqy_std = 0.0;

    #ifdef ANGULARMOM_TIMESERIES
    FILE *file1 = fopen("angularmom.txt", "wb");
    #endif

    #ifdef VELOCITY_DISTRIBUTION
    FILE *file2 = fopen("velocities.txt", "wb");
    #endif

    #ifdef TEMPERATURE_BINS 
    int bins[RADS][BINS];
    char name[80];
    sprintf(name, "temp_%0.2f.txt", damp_coeff);
    FILE *file3 = fopen(name, "w");
    for (i=0; i<RADS; i++){
        for (j=0; j<BINS; j++){
            bins[i][j] = 0;
        }
    }
    #endif

    for (t=0.0; t<time_end; t+=dt){

        int index[2];
        for (i=0; i<size_total; i++)
            count[i] = 0;

        for (i=0; i<N; i++){
            coords_to_index(&x[2*i], size, index, L);
            int t = index[0] + index[1]*size[0];
            cells[NMAX*t + count[t]] = i;
            count[t]++; 
        }

        int tt[2];
        int tix[2];
        int image[2];
        double dx[2];
        int goodcell, ind, n;
        double r0, l, co, co1, dist;
        double wlen, vlen, vhappy;

        #ifdef OPENMP
        #pragma omp parallel for private(i,dx,index,tt,goodcell,tix,ind,j,n,image,k,dist,r0,l,co,wlen,vlen,vhappy)
        #endif 
        for (i=0; i<N; i++){
            f[2*i+0] = 0.0;
            f[2*i+1] = 0.0;
            w[2*i+0] = 0.0;
            w[2*i+1] = 0.0;
            neigh[i] = 0;
            
            coords_to_index(&x[2*i], size, index, L);

            for (tt[0]=-1; tt[0]<=1; tt[0]++){
            for (tt[1]=-1; tt[1]<=1; tt[1]++){
                goodcell = 1;    
                for (j=0; j<2; j++){
                    tix[j] = mod_rvec(index[j]+tt[j],size[j]-1,pbc[j],&image[j]);
                    if (pbc[j] < image[j])
                        goodcell=0;
                }

                if (goodcell){
                    ind = tix[0] + tix[1]*size[0]; 

                    for (j=0; j<count[ind]; j++){
                        n = cells[NMAX*ind+j];

                        dist = 0.0;
                        for (k=0; k<2; k++){
                            dx[k] = x[2*n+k] - x[2*i+k];
                    
                            if (image[k])
                                dx[k] += L*tt[k];
                            dist += dx[k]*dx[k];
                        }

                        //===============================================
                        // force calculation - hertz
                        if (dist > 1e-10 && dist < R2){
                            r0 = R; 
                            l  = sqrt(dist);
                            co1 = (1-l/r0);
                            co = epsilon * co1*sqrt(co1) * (l<r0);
                            for (k=0; k<2; k++){
                                f[2*i+k] += - dx[k]/l * co;
                                col[i] += co*co*dx[k]*dx[k]/dist; 
                            }
                        }
                        //===============================================
                        // add up the neighbor veocities
                        if (dist > 1e-10 && dist < FR2 && type[n] == RED && type[i] == RED){
                             for (k=0; k<2; k++)
                                w[2*i+k] += v[2*n+k];
                            neigh[i]++;
                        }                           
                    }
                }
            } } 

            //=====================================
            // flocking force 
            wlen = sqrt(w[2*i+0]*w[2*i+0] + w[2*i+1]*w[2*i+1]);
            if (type[i] == RED && neigh[i] > 0 && wlen > 1e-6){
                f[2*i+0] += alpha * w[2*i+0] / wlen; 
                f[2*i+1] += alpha * w[2*i+1] / wlen;
            }

            //====================================
            // self-propulsion
            vlen = sqrt(v[2*i+0]*v[2*i+0] + v[2*i+1]*v[2*i+1]);
            vhappy = type[i]==RED?vhappy_red:vhappy_black;
            if (vlen > 1e-6){
                f[2*i+0] += damp_coeff*(vhappy - vlen)*v[2*i+0]/vlen;
                f[2*i+1] += damp_coeff*(vhappy - vlen)*v[2*i+1]/vlen;
            }
        
            //=======================================
            // noise term
            if (type[i] == RED){
                // Box-Muller method
                double u1 = ran_ran2();
                double u2 = 2*pi*ran_ran2();
                double lfac = sqrt(-2*log(u1));
                f[2*i+0] += sigma*lfac*cos(u2);
                f[2*i+1] += sigma*lfac*sin(u2);
            }

            //=====================================
            // kick force
            f[2*i+0] += o[2*i+0]; o[2*i+0] = 0.0;
            f[2*i+1] += o[2*i+1]; o[2*i+1] = 0.0;
        }
        #ifdef OPENMP
        #pragma omp barrier
        #endif

        // now integrate the forces since we have found them
        #ifdef OPENMP
        #pragma omp parallel for private(j)
        #endif 
        for (i=0; i<N;i++){
            // Newton-Stomer-Verlet
            #ifdef PLOT
            if (key['h'] != 1){
            #endif
            v[2*i+0] += f[2*i+0] * dt;
            v[2*i+1] += f[2*i+1] * dt;

            x[2*i+0] += v[2*i+0] * dt;
            x[2*i+1] += v[2*i+1] * dt;
            #ifdef PLOT
            }   
            #endif

            #ifdef VELOCITY_DISTRIBUTION
            double ttt = sqrt(v[2*i+0]*v[2*i+0] + v[2*i+1]*v[2*i+1]) ;
            fwrite(&ttt, sizeof(double), 1, file2);
            #endif

            // boundary conditions 
            for (j=0; j<2; j++){
                if (pbc[j] == 1){
                    if (x[2*i+j] >= L-EPSILON || x[2*i+j] < 0)
                        x[2*i+j] = mymod(x[2*i+j], L);
                }
                else {
                    const double restoration = 1.0;
                    if (x[2*i+j] >= L){x[2*i+j] = 2*L-x[2*i+j]; v[2*i+j] *= -restoration;}
                    if (x[2*i+j] < 0) {x[2*i+j] = -x[2*i+j];    v[2*i+j] *= -restoration;}
                    if (x[2*i+j] >= L-EPSILON || x[2*i+j] < 0){x[2*i+j] = mymod(x[2*i+j], L);}
                }
            }

            // just check for errors
            if (x[2*i+0] >= L || x[2*i+0] < 0.0 ||
                x[2*i+1] >= L || x[2*i+1] < 0.0)
                printf("out of bounds\n");
            
            col[i] = col[i]/12; 
        }
        #ifdef OPENMP
        #pragma omp barrier
        #endif

        #ifdef PLOT 
        int skip = 10; if (RIC == 1) skip *=3;
        int start = 20;
        if (frames % skip == 0 && frames >= start){
            double cmx, cmy;
            centerofmass(x, type, N, L, &cmx, &cmy);
            plot_clear_screen();
            key = plot_render_particles(x, rad, type, N, L,col, SHOWFORCECOLORS, cmx, cmy, SHOWCENTEROFMASS, pbc, v, SHOWVELOCITYARROWS);
           
            #ifdef OPENIL
                char fname[100];
                sprintf(fname, "/media/scratch/moshpits/out%06d.png", frames/skip-start/skip);
                plot_saveimage(fname);
            #endif
        }
        #endif
        frames++;

        angularmom_count++;
        
        double vtemp     = angularmom(x,v,type,N,L,pbc);
        double delta     = vtemp    - angularmom_avg;
        angularmom_avg    = angularmom_avg    + delta    / angularmom_count; 
        angularmom_std    = angularmom_std    + delta    * (vtemp    - angularmom_avg);
        
        double vtemp_sq  = vtemp*vtemp;
        double delta_sq  = vtemp_sq - angularmom_sq_avg;
        angularmom_sq_avg = angularmom_sq_avg + delta_sq / angularmom_count;
        angularmom_sq_std = angularmom_sq_std + delta_sq * (vtemp_sq - angularmom_sq_avg);


        double linearmomx = 0.0;
        double linearmomy = 0.0;
        int linearmomc = 0;
        for (i=0; i<N; i++){
            if (type[i] == RED){
                linearmomx += v[2*i+0];
                linearmomy += v[2*i+1];
                linearmomc++;
            }
        }
        linearmomx /= linearmomc;
        linearmomy /= linearmomc;
        momentum_count++;

        double deltax = linearmomx - momentumx_avg;
        double deltay = linearmomy - momentumy_avg;
        momentumx_avg = momentumx_avg + deltax / momentum_count;
        momentumy_avg = momentumy_avg + deltay / momentum_count;
        momentumx_std = momentumx_std + deltax * (linearmomx - momentumx_avg);
        momentumy_std = momentumy_std + deltay * (linearmomy - momentumy_avg);

        double linearmomsqx = linearmomx*linearmomx;
        double linearmomsqy = linearmomy*linearmomy;
        double deltasqx = linearmomsqx - momentumsqx_avg;
        double deltasqy = linearmomsqy - momentumsqy_avg;
        momentumsqx_avg = momentumsqx_avg + deltasqx / momentum_count;
        momentumsqy_avg = momentumsqy_avg + deltasqy / momentum_count;
        momentumsqx_std = momentumsqx_std + deltasqx * (linearmomsqx - momentumsqx_avg);
        momentumsqy_std = momentumsqy_std + deltasqy * (linearmomsqy - momentumsqy_avg);

        #ifdef TEMPERATURE_BINS
        temperature(x, v, type, N, L, pbc, bins);
        #endif

        #ifdef ANGULARMOM_TIMESERIES
        fwrite(&vtemp, sizeof(double), 1, file1);
        #endif

        #ifdef PLOT
        #ifdef OPENIL
        if (key['p'] == 1)
            plot_saveimage("out.png");
        #endif
        if (key['f'] == 1)
            showplot = !showplot;
        if (key['k'] == 1)
            vhappy_red = 0.0;
        if (key['q'] == 1)
            break;
        if (key['w'] == 1){
            for (i=0; i<N; i++){
                if (type[i] == RED)
                    o[2*i+1] = -kickforce;
            }
        }
        if (key['s'] == 1){
            for (i=0; i<N; i++){
                if (type[i] == RED)
                    o[2*i+1] = kickforce;
            }
        }
        if (key['a'] == 1){
            for (i=0; i<N; i++){
                if (type[i] == RED)
                    o[2*i+0] = -kickforce;
            }
        }
        if (key['d'] == 1){
            for (i=0; i<N; i++){
                if (type[i] == RED)
                    o[2*i+0] = kickforce;
            }
        }
        #endif
    }
    // end of the magic, cleanup
    //----------------------------------------------
    #ifdef FPS
    struct timespec end;
    clock_gettime(CLOCK_REALTIME, &end);
    printf("fps = %f\n", frames/((end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec)/1e9));
    #endif

    #ifdef ANGULARMOM_TIMESERIES
    fclose(file1);
    #endif

    #ifdef VELOCITY_DISTRIBUTION
    fclose(file2);
    #endif

    #ifdef TEMPERATURE_BINS
    for (i=0; i<RADS; i++){
        for (j=0; j<BINS; j++){
            fprintf(file3, "%i ", bins[i][j]);
        }
        fprintf(file3, "\n");
    }
    fclose(file3);
    #endif

    //printf("tend = %f\n", t);
    angularmom_std    = angularmom_std    / (angularmom_count - 1);
    angularmom_sq_std = angularmom_sq_std / (angularmom_count - 1);
 
    momentumx_std = momentumx_std / (momentum_count - 1);
    momentumy_std = momentumy_std / (momentum_count - 1);
 
    printf("%f %f %f %f %f %f %f %f %f %f %f %f\n", 
                                        angularmom_avg, sqrt(angularmom_std), 
                                        angularmom_sq_avg, sqrt(angularmom_sq_std), 
                                        momentumx_avg, sqrt(momentumx_std), 
                                        momentumy_avg, sqrt(momentumy_std),
                                        momentumsqx_avg, sqrt(momentumsqx_std),
                                        momentumsqy_avg, sqrt(momentumsqy_std));

    free(cells);
    free(count);
 
    free(x);
    free(v);
    free(f);
    free(w);
    free(o);
    free(neigh);
    free(rad);
    free(type);
    free(col);

    #ifdef PLOT
    plot_clean(); 
    #endif
}




//=================================================
// extra stuff
//=================================================
void ran_seed(long j){
  vseed = j;  vran = 4101842887655102017LL;
  vran ^= vseed; 
  vran ^= vran >> 21; vran ^= vran << 35; vran ^= vran >> 4;
  vran = vran * 2685821657736338717LL;
}

double ran_ran2(){
    vran ^= vran >> 21; vran ^= vran << 35; vran ^= vran >> 4;
    unsigned long long int t = vran * 2685821657736338717LL;
    return 5.42101086242752217e-20*t;
}

void init_circle(double *x, double *v, 
                 int *type, double speed, long N, double L){
    int i;
    for (i=0; i<N; i++){
        double tx = L*ran_ran2();
        double ty = L*ran_ran2();
        double tt = 2*pi*ran_ran2();

        x[2*i+0] = tx;
        x[2*i+1] = ty;
        
        // the radius for which 30% of the particles are red on avg
        double dd = sqrt((tx-L/2)*(tx-L/2) + (ty-L/2)*(ty-L/2));
        double rad = sqrt(0.16*L*L / pi);

        //if (i<0.15*N)
        if (dd < rad)
            type[i] = RED;
        else
            type[i] = BLACK;

        if (type[i] == RED){
            v[2*i+0] = speed*cos(tt);
            v[2*i+1] = speed*sin(tt);
        }
        else {
            v[2*i+0] = 0.0;
            v[2*i+1] = 0.0;
        }
    }   
} 

//=======================================
// NBL - neighborlist helper functions
//=======================================
inline double mymod(double a, double b){
  return a - b*(int)(a/b) + b*(a<0);
}

inline void coords_to_index(double *x, int *size, int *index, double L){   
    index[0] = (int)(x[0]/L  * size[0]);
    index[1] = (int)(x[1]/L  * size[1]);
}

inline int mod_rvec(int a, int b, int p, int *image){
    *image = 1;
    if (b==0) {if (a==0) *image=0; return 0;}
    if (p != 0){
        if (a>b)  return a-b-1;
        if (a<0)  return a+b+1;
    } else {
        if (a>b)  return b;
        if (a<0)  return 0;
    }
    *image = 0;
    return a;
}



//==========================================
// measurement functions
//=========================================
void centerofmass(double *x, int *t, int N, double L, double *cmx, double *cmy){
    int i;
    double xreal = 0.0;
    double ximag = 0.0;
    double yreal = 0.0;
    double yimag = 0.0;

    for (i=0; i<N; i++){
        if (t[i] == RED){
            xreal += cos(2*pi/L * x[2*i+0]);
            ximag += sin(2*pi/L * x[2*i+0]);
            yreal += cos(2*pi/L * x[2*i+1]);
            yimag += sin(2*pi/L * x[2*i+1]);
        }
    }

    *cmx = atan2(ximag,xreal)/(2*pi) * L;
    *cmy = atan2(yimag,yreal)/(2*pi) * L;

    if (*cmx < 0) *cmx += L;
    if (*cmy < 0) *cmy += L;
}


double angularmom(double *x, double *v, int *t, int N, double L, int *pbc){
    int i=0;
    double ang = 0.0;
    double cmx = 0.0;
    double cmy = 0.0;
    int count = 0;

    centerofmass(x, t, N, L, &cmx, &cmy);

    for (i=0; i<N; i++){
        if (t[i] == RED){
            double tx = x[2*i+0] - cmx;
            double ty = x[2*i+1] - cmy;
            
            if (pbc[0] && tx > L/2)  tx -= L;
            if (pbc[1] && ty > L/2)  ty -= L;
            if (pbc[0] && tx < -L/2) tx += L;
            if (pbc[1] && ty < -L/2) ty += L;
    
            double vx = v[2*i+0];
            double vy = v[2*i+1];
            double tv = vx*ty - vy*tx;
            ang += tv;
            count++;
        }
    }

    return ang/count;
}


void temperature(double *x, double *v, int *t, int N, double L, int *pbc, int bins[RADS][BINS]){
    int i=0;
    double cmx = 0.0;
    double cmy = 0.0;
    int count = 0;

    centerofmass(x, t, N, L, &cmx, &cmy);

    for (i=0; i<N; i++){
        if (t[i] == RED){
            double dx = x[2*i+0] - cmx;
            double dy = x[2*i+1] - cmy;
            if (pbc[0] && dx > L/2)  dx -= L;
            if (pbc[1] && dy > L/2)  dy -= L;
            if (pbc[0] && dx < -L/2) dx += L;
            if (pbc[1] && dy < -L/2) dy += L;

            double rr = sqrt(dx*dx + dy*dy);
            double vv = sqrt(v[2*i+0]*v[2*i+0] + v[2*i+1]*v[2*i+1]);

            int rad = RADS * 2*rr/L;
            int bin = BINS * vv/3;
            if (rad < RADS && bin < BINS){
                bins[rad][bin]++;
            }
            count++;
        }
    }
}


