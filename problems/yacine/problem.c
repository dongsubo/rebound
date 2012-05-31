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
#include "main.h"
#include "particle.h"
#include "boundaries.h"
#include "output.h"
#include "communication_mpi.h"
#include "tree.h"
#include "tools.h"

extern double opening_angle2;
long* velocity_bins_electrons;
long* velocity_bins_protons;
int velocity_bins_N	= 256;
double velocity_bins_max = 1;

void problem_init(int argc, char* argv[]){
	// Setup constants
#ifdef GRAVITY_TREE
	opening_angle2	= .5;
#endif
	velocity_bins_electrons = calloc(sizeof(long),velocity_bins_N);
	velocity_bins_protons = calloc(sizeof(long),velocity_bins_N);
	G 				= 1; 	// Used to be the gravitational constant 
	softening 			= 0.1;
	dt 				= 1e-2;
	nghostx = 1; nghosty = 1; nghostz = 1; 		
	boxsize 			= 100;
	init_box();
	
	for (int i=0;i<1000;i++){
		struct particle pt;
		pt.x 		= tools_uniform(-boxsize_x/2.,boxsize_x/2.);
		pt.y 		= tools_uniform(-boxsize_y/2.,boxsize_y/2.);
		pt.z 		= tools_uniform(-boxsize_z/2.,boxsize_z/2.);
		pt.vx 		= 0;
		pt.vy 		= 0;
		pt.vz 		= 0;
		pt.ax 		= 0;
		pt.ay 		= 0;
		pt.az 		= 0;
		if (i%2==0){
			// Proton
			pt.m 		= 1;	// mass
			pt.q 		= 1;	// charge
		}else{
			// Electron
			pt.m 		= 0.1;	// mass
			pt.q 		= -1;	// charge
		}
		particles_add(pt);
	}
}


void problem_inloop(){
}

void velocity_bins_output(char* filename){
	FILE* of = fopen(filename,"w"); 
	for (int i=0;i<velocity_bins_N;i++){
		fprintf(of,"%e\t%ld\t%ld\n",((double)i)/((double)velocity_bins_N)*velocity_bins_max,velocity_bins_protons[i],velocity_bins_electrons[i]);
	}
	fclose(of);
}
void velocity_bins_add(){
	for (int i=0;i<N;i++){
		struct particle p = particles[i];
		double v = sqrt(p.vx*p.vx + p.vy*p.vy + p.vz*p.vz);
		int bin = (int)ceil(v/velocity_bins_max*(double)velocity_bins_N);
		if (bin<0||bin>velocity_bins_N) continue;
		if (p.q>0){
			velocity_bins_protons[bin]++;
		}else{
			velocity_bins_electrons[bin]++;
		}
	}
}

void problem_output(){
	velocity_bins_add();
	if (output_check(1.)){
		velocity_bins_output("velocity_bins.txt");
		output_timing();
	}
}

void problem_finish(){
}
