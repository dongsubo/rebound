/**
 * @file 	problem.c
 * @brief 	Example problem: shearing sheet.
 * @author 	Hanno Rein <hanno@hanno-rein.de>
 * @detail 	This problem uses shearing sheet boundary
 * conditions. Particle properties resemble those found in 
 * Saturn's rings. 
 * 
 * @section 	LICENSE
 * Copyright (c) 2011 Hanno Rein, Shangfei Liu
 *
 * This file is part of rebound.
 *
 * rebound is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * rebound is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with rebound.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include "main.h"
#include "particle.h"
#include "boundaries.h"
#include "output.h"
#include "communication_mpi.h"
#include "tree.h"
#include "tools.h"
#include "input.h"

extern double OMEGA;
extern double coefficient_of_restitution;
extern double minimum_collision_velocity;

extern double (*coefficient_of_restitution_for_velocity)(double); 
double coefficient_of_restitution_bridges(double v); 

extern double opening_angle2;

int logfile_first = 1;
void logfile(char* data){
	if (logfile_first){
		logfile_first = 0;
		system("rm -fv config.log");
	}
	FILE* file = fopen("config.log","a+");
	fputs(data,file);
	fclose(file);
}

void logfile_double(char* name, double value){
	char data[2048];
	if (value>1e7){
		sprintf(data,"%-35s =         %10e\n",name,value);
	}else{
		if (fabs(fmod(value,1.))>1e-9){
			sprintf(data,"%-35s = %20.10f\n",name,value);
		}else{
			sprintf(data,"%-35s = %11.1f\n",name,value);
		}
	}
	logfile(data);
}
void logfile_int(char* name, int value){
	char data[2048];
	sprintf(data,"%-35s = %9d\n",name,value);
	logfile(data);
}

void problem_init(int argc, char* argv[]){
	// Setup constants
#ifdef GRAVITY_TREE
	opening_angle2	= .5;
#endif
	OMEGA 				= 0.00013143527;	// 1/s
	G 				= 6.67428e-11;		// N / (1e-5 kg)^2 m^2
	softening 			= 0.1;			// m
	dt 				= 1e-3*2.*M_PI/OMEGA;	// s
	tmax				= 10.*2.*M_PI/OMEGA;

	// Setup domain dimensions
	root_nx = input_get_int(argc,argv,"root_nx",1);
	root_ny = input_get_int(argc,argv,"root_ny",1);
	root_nz = input_get_int(argc,argv,"root_nz",1);
	nghostx = 2;
	nghosty = 2;
	nghostz = 0;
	boxsize = input_get_double(argc,argv,"boxsize",-1);
	init_box();
	
	// Setup particle and disk properties
	double surfacedensity 		= 400; 			// kg/m^2
	double particle_density		= 400;			// kg/m^3
	double particle_radius_min 	= 0.1;			// m
	double particle_radius_max 	= 1;			// m
	double particle_radius_slope 	= -3;	
	coefficient_of_restitution_for_velocity	= coefficient_of_restitution_bridges;
	minimum_collision_velocity 		= particle_radius_min*OMEGA*0.05;  // small fraction of the shear

	
	struct 	aabb bb	= { .xmin = -boxsize_x/2., .xmax = boxsize_x/2., .ymin = -boxsize_y/2., .ymax = boxsize_y/2., .zmin = -boxsize_z/2., .zmax = boxsize_z/2.};
	long	_N	= round(surfacedensity*boxsize_x*boxsize_y/(4./3.*M_PI*particle_density* (pow(particle_radius_max,4.+particle_radius_slope) - pow(particle_radius_min,4.+particle_radius_slope)) / (pow(particle_radius_max,1.+particle_radius_slope) - pow(particle_radius_min,1.+particle_radius_slope)) * (1.+particle_radius_slope)/(4.+particle_radius_slope)));


	char dirname[4096];
	strcat(dirname,"out__");
	strcat(dirname,input_arguments);
	char tmpsystem[4096];
#ifdef MPI
	sprintf(tmpsystem,"mpinum_%d__",mpi_num);
	strcat(dirname,tmpsystem);
#endif // MPI
#ifdef MPI
	bb = communication_boundingbox_for_proc(mpi_id);
	_N   /= mpi_num;
	if (mpi_id==0){
#endif // MPI
		sprintf(tmpsystem,"rm -rf %s",dirname);
		system(tmpsystem);
		sprintf(tmpsystem,"mkdir %s",dirname);
		system(tmpsystem);
#ifdef MPI
	}
	MPI_Barrier(MPI_COMM_WORLD);
#endif // MPI
	chdir(dirname);
#ifdef MPI
	if (mpi_id==0){
#endif // MPI
		logfile_double("boxsize",boxsize);
		logfile_int("root_nx",root_nx);
		logfile_int("root_ny",root_ny);
		logfile_int("root_nz",root_nz);
		logfile_int("N",_N);
#ifdef MPI
		logfile_int("N_total",_N*mpi_num);
		logfile_int("mpi_num",mpi_num);
#endif // MPI
		logfile("----------------\n");
		logfile_double("tmax [orbits]",tmax/(2.*M_PI/OMEGA));
		logfile_int("number of timesteps",ceil(tmax/dt));
		system("cat config.log");
#ifdef MPI
	}
#endif // MPI
	for(int i=0;i<_N;i++){
		struct particle pt;
		pt.z 		= tools_normal(1.);	
		if (fabs(pt.z)>=boxsize_z/2.) pt.z = 0;
#ifdef MPI
		int proc_id;
		do{
#endif // MPI
			pt.x 		= tools_uniform(bb.xmin,bb.xmax);
			pt.y 		= tools_uniform(bb.ymin,bb.ymax);
#ifdef MPI
			int rootbox = particles_get_rootbox_for_particle(pt);
			int root_n_per_node = root_n/mpi_num;
			proc_id = rootbox/root_n_per_node;
		}while(proc_id != mpi_id );
#endif // MPI
		
		pt.vx 		= 0;
		pt.vy 		= -1.5*pt.x*OMEGA;
		pt.vz 		= 0;
		pt.ax 		= 0;
		pt.ay 		= 0;
		pt.az 		= 0;
		pt.r 		= tools_powerlaw(particle_radius_min,particle_radius_max,particle_radius_slope);
		pt.m 		= particle_density*4./3.*M_PI*pt.r*pt.r*pt.r;
		
		particles_add(pt);
	}
#ifdef MPI
	MPI_Barrier(MPI_COMM_WORLD);
#endif // MPI
}

double coefficient_of_restitution_bridges(double v){
	// assumes v in units of [m/s]
	double eps = 0.32*pow(fabs(v)*100.,-0.234);
	if (eps>1) eps=1;
	if (eps<0) eps=0;
	return eps;
}

void problem_inloop(){
}

void output_ascii_mod(char* filename){
#ifdef MPI
	char filename_mpi[1024];
	sprintf(filename_mpi,"%s_%d",filename,mpi_id);
	FILE* of = fopen(filename_mpi,"w"); 
#else // MPI
	FILE* of = fopen(filename,"w"); 
#endif // MPI
	if (of==NULL){
		printf("\n\nError while opening file '%s'.\n",filename);
		return;
	}
	for (int i=0;i<N;i++){
		struct particle p = particles[i];
		fprintf(of,"%e\t%e\t%e\t%e\n",p.x,p.y,p.z,p.r);
	}
	fclose(of);
}

int position_id=0;
void problem_output(){
	if (output_check(100.*dt)){
		output_timing();
	}
	if (output_check(2.*M_PI/OMEGA)){
		char filename[256];
		sprintf(filename,"position_%08d.txt",position_id);
		output_ascii_mod(filename);
		position_id++;
	}
}

void problem_finish(){
#ifdef MPI
	if (mpi_id==0){
#endif // MPI
		struct timeval tim;
		gettimeofday(&tim, NULL);
		double timing_final = tim.tv_sec+(tim.tv_usec/1000000.0);
		logfile_double("runtime [s]",timing_final-timing_initial);
		system("cat config.log");
#ifdef MPI
	}
#endif // MPI
}