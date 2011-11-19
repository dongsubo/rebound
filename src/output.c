/**
 * @file 	output.c
 * @brief 	Output routines.
 * @author 	Hanno Rein <hanno@hanno-rein.de>
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
#include "particle.h"
#include "main.h"
#include "tools.h"
#include "output.h"
#include "communication_mpi.h"
#ifdef OPENGL
#include "display.h"
#ifdef LIBPNG
#include <png.h>
#endif // LIBPNG
#ifdef _APPLE
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif  // _APPLE
#endif  // OPENGL
#ifdef MPI
#include "mpi.h"
#endif // MPI

#ifdef INTEGRATOR_SEI 	// Shearing sheet
extern double OMEGA;
#endif

// Check if output is needed

int output_check(double interval){
	return output_check_phase(interval,0);
}

int output_check_phase(double interval,double phase){
	double shift = t+interval*phase;
	if (floor(shift/interval)!=floor((shift-dt)/interval)){
		return 1;
	}
	// Output at beginning or end of simulation
	if (t==0||t==tmax){
		return 1;
	}
	return 0;
}


/**
 * 3D vector struct.
 */
struct vec3 {
	double x;
	double y;
	double z;
};



double output_timing_last = -1; 	/**< Time when output_timing() was called the last time. */
void output_timing(){
#ifdef MPI
	int N_tot = 0;
	MPI_Reduce(&N, &N_tot, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD); 
	if (mpi_id!=0) return;
#else
	int N_tot = N;
#endif
	struct timeval tim;
	gettimeofday(&tim, NULL);
	double temp = tim.tv_sec+(tim.tv_usec/1000000.0);
	if (output_timing_last==-1){
		output_timing_last = temp;
	}else{
		printf("\r");
	}
	if (tmax>0){
		printf("N_tot= %- 9d  t= %- 9f  cpu= %- 9f s  t/tmax= %5.2f%%",N_tot,t,temp-output_timing_last,t/tmax*100);
	}else{
		printf("N_tot= %- 9d  t= %- 9f  cpu= %- 9f s ",N_tot,t,temp-output_timing_last);
	}
	fflush(stdout);
	output_timing_last = temp;
}

void output_append_ascii(char* filename){
#ifdef MPI
	char filename_mpi[1024];
	sprintf(filename_mpi,"%s_%d",filename,mpi_id);
	FILE* of = fopen(filename_mpi,"a"); 
#else // MPI
	FILE* of = fopen(filename,"a"); 
#endif // MPI
	for (int i=0;i<N;i++){
		struct particle p = particles[i];
		fprintf(of,"%e\t%e\t%e\t%e\t%e\t%e\n",p.x,p.y,p.z,p.vx,p.vy,p.vz);
	}
	fclose(of);
}

void output_ascii(char* filename){
#ifdef MPI
	char filename_mpi[1024];
	sprintf(filename_mpi,"%s_%d",filename,mpi_id);
	FILE* of = fopen(filename_mpi,"w"); 
#else // MPI
	FILE* of = fopen(filename,"w"); 
#endif // MPI
	for (int i=0;i<N;i++){
		struct particle p = particles[i];
		fprintf(of,"%e\t%e\t%e\t%e\t%e\t%e\n",p.x,p.y,p.z,p.vx,p.vy,p.vz);
	}
	fclose(of);
}

void output_orbits_append(char* filename){
#ifdef MPI
	char filename_mpi[1024];
	sprintf(filename_mpi,"%s_%d",filename,mpi_id);
	FILE* of = fopen(filename_mpi,"a"); 
#else // MPI
	FILE* of = fopen(filename,"a"); 
#endif // MPI
	for (int i=1;i<N;i++){
		struct orbit o = tools_p2orbit(particles[i],particles[0].m);
		fprintf(of,"%e\t%e\t%e\t%e\t%e\t%e\t%e\t%e\t%e\n",t,o.a,o.e,o.inc,o.Omega,o.omega,o.l,o.P,o.f);
	}
	fclose(of);
}

void output_orbits(char* filename){
#ifdef MPI
	char filename_mpi[1024];
	sprintf(filename_mpi,"%s_%d",filename,mpi_id);
	FILE* of = fopen(filename_mpi,"w"); 
#else // MPI
	FILE* of = fopen(filename,"w"); 
#endif // MPI
	for (int i=1;i<N;i++){
		struct orbit o = tools_p2orbit(particles[i],particles[0].m);
		fprintf(of,"%e\t%e\t%e\t%e\t%e\t%e\t%e\t%e\n",o.a,o.e,o.inc,o.Omega,o.omega,o.l,o.P,o.f);
	}
	fclose(of);
}


void output_binary(char* filename){
#ifdef MPI
	char filename_mpi[1024];
	sprintf(filename_mpi,"%s_%d",filename,mpi_id);
	FILE* of = fopen(filename_mpi,"wb"); 
#else // MPI
	FILE* of = fopen(filename,"wb"); 
#endif // MPI
	fwrite(&N,sizeof(int),1,of);
	fwrite(&t,sizeof(double),1,of);
	for (int i=0;i<N;i++){
		struct particle p = particles[i];
		fwrite(&(p),sizeof(struct particle),1,of);
	}
	fclose(of);
}

void output_binary_positions(char* filename){
#ifdef MPI
	char filename_mpi[1024];
	sprintf(filename_mpi,"%s_%d",filename,mpi_id);
	FILE* of = fopen(filename_mpi,"wb"); 
#else // MPI
	FILE* of = fopen(filename,"wb"); 
#endif // MPI
	for (int i=0;i<N;i++){
		struct vec3 v;
		v.x = particles[i].x;
		v.y = particles[i].y;
		v.z = particles[i].z;
		fwrite(&(v),sizeof(struct vec3),1,of);
	}
	fclose(of);
}

void output_append_velocity_dispersion(char* filename){
	// Algorithm with reduced roundoff errors (see wikipedia)
	struct vec3 A = {.x=0, .y=0, .z=0};
	struct vec3 Q = {.x=0, .y=0, .z=0};
	for (int i=0;i<N;i++){
		struct vec3 Aim1 = A;
		struct particle p = particles[i];
		A.x = A.x + (p.vx-A.x)/(double)(i+1);
#ifdef INTEGRATOR_SEI 	// Shearing sheet
		A.y = A.y + (p.vy+1.5*OMEGA*p.x-A.y)/(double)(i+1);
#else
		A.y = A.y + (p.vy-A.y)/(double)(i+1);
#endif
		A.z = A.z + (p.vz-A.z)/(double)(i+1);
		Q.x = Q.x + (p.vx-Aim1.x)*(p.vx-A.x);
#ifdef INTEGRATOR_SEI 	// Shearing sheet
		Q.y = Q.y + (p.vy+1.5*OMEGA*p.x-Aim1.y)*(p.vy+1.5*OMEGA*p.x-A.y);
#else
		Q.y = Q.y + (p.vy-Aim1.y)*(p.vy-A.y);
#endif
		Q.z = Q.z + (p.vz-Aim1.z)*(p.vz-A.z);
	}
#ifdef MPI
	int N_tot = 0;
	struct vec3 A_tot = {.x=0, .y=0, .z=0};
	struct vec3 Q_tot = {.x=0, .y=0, .z=0};
	MPI_Reduce(&N, &N_tot, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD); 
	MPI_Reduce(&A, &A_tot, 3, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD); 
	MPI_Reduce(&Q, &Q_tot, 3, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD); 
	if (mpi_id!=0) return;
#else
	int N_tot = N;
	struct vec3 A_tot = A;
	struct vec3 Q_tot = Q;
#endif
	Q_tot.x = sqrt(Q_tot.x/(double)N_tot);
	Q_tot.y = sqrt(Q_tot.y/(double)N_tot);
	Q_tot.z = sqrt(Q_tot.z/(double)N_tot);
	FILE* of = fopen(filename,"a"); 
	fprintf(of,"%e\t%e\t%e\t%e\t%e\t%e\t%e\n",t,A_tot.x,A_tot.y,A_tot.z,Q_tot.x,Q_tot.y,Q_tot.z);
	fclose(of);
}

#ifdef OPENGL
#ifdef LIBPNG
unsigned char* 	imgdata = NULL;
int output_png_num = 0;
void output_png(char* dirname){
	char filename[1024];
	sprintf(filename,"%s%09d.png",dirname,output_png_num);
	output_png_num++;
	output_png_single(filename);
}

void output_png_single(char* filename){
	if (display_init_done==0) return;
	// Force redraw
	if (display_pause){
		display_pause=0;
		display();
		display_pause=1;
	}
	// Read Image
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	int width = viewport[2];
	int height = viewport[3];
	glReadBuffer(GL_BACK);
	//glReadBuffer(GL_FRONT);
	if (imgdata==NULL){
		imgdata = calloc(width*height*3,sizeof(unsigned char));
	}
	png_byte* row_pointers[height];
	for (int h = 0; h < height; h++) {
		row_pointers[height-h-1] = (png_bytep) &imgdata[width*3*h];
	}

	glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,imgdata);

	/* open the file */
	FILE *fp;
	png_structp png_ptr;
	png_infop info_ptr;
	fp = fopen(filename, "wb");
	if (fp == NULL) return;

	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

	if (png_ptr == NULL) {
		fclose(fp);
		return;
	}

	/* Allocate/initialize the image information data.  REQUIRED */
	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		fclose(fp);
		png_destroy_write_struct(&png_ptr,  (png_infopp)NULL);
		return;
	}

	/* Set error handling.  REQUIRED if you aren't supplying your own
	* error hadnling functions in the png_create_write_struct() call.
	*/
	/*
	if (setjmp(png_ptr->jmpbuf))
	{
	fclose(fp);
	png_destroy_write_struct(&png_ptr,  (png_infopp)NULL);
	return;
	}
	*/

	/* I/O initialization functions is REQUIRED */
	/* set up the output control if you are using standard C streams */
	png_init_io(png_ptr, fp);

	/* Set the image information here.  Width and height are up to 2^31,
	* bit_depth is one of 1, 2, 4, 8, or 16, but valid values also depend on
	* the color_type selected. color_type is one of PNG_COLOR_TYPE_GRAY,
	* PNG_COLOR_TYPE_GRAY_ALPHA, PNG_COLOR_TYPE_PALETTE, PNG_COLOR_TYPE_RGB,
	* or PNG_COLOR_TYPE_RGB_ALPHA.  interlace is either PNG_INTERLACE_NONE or
	* PNG_INTERLACE_ADAM7, and the compression_type and filter_type MUST
	* currently be PNG_COMPRESSION_TYPE_BASE and PNG_FILTER_TYPE_BASE. REQUIRED
	*/
	png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB,
	PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

	/* Write the file  */
	png_write_info(png_ptr, info_ptr);
	png_write_image(png_ptr, row_pointers);
	png_write_end(png_ptr, info_ptr);

	/* if you allocated any text comments, free them here */

	/* clean up after the write, and free any memory allocated */
	png_destroy_write_struct(&png_ptr, (png_infopp)NULL);

	/* close the file */
	fclose(fp);
}
#endif
#endif

