/**
 * @file 	display.c
 * @brief 	Realtime OpenGL visualization.
 * @author 	Hanno Rein <hanno@hanno-rein.de>, Dave Spiegel <dave@ias.edu>
 * @details 	These functions provide real time visualizations
 * using OpenGL. Screenshots can be saved with the output_png() routine.
 * Tested under Mac OSX Snow Leopard and Linux. 
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
#ifdef OPENGL
#ifdef MPI
#error OpenGL is not compatible with MPI.
#endif //MPI
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#ifdef _APPLE
#include <GLUT/glut.h>
#else // _APPLE
#include <GL/glut.h>
#endif // _APPLE
#include "tools.h"
#include "zpr.h"
#include "main.h"
#include "particle.h"
#include "boundaries.h"
#include "tree.h"
#include "display.h"
#include "output.h"

int display_mode 		= 0;	/**< Switches between point sprite, spheres, and textured spheres. */
int display_init_fancy_done 	= 0;	
int display_pause_sim 		= 0;	/**< Pauses simulation. */
int display_pause 		= 0;	/**< Pauses visualization, but keep simulation running */
int display_tree 		= 0;	/**< Shows/hides tree structure. */
int display_mass 		= 0;	/**< Shows/hides centre of mass in tree structure. */
int display_wire 		= 0;	/**< Shows/hides orbit wires. */
int display_clear 		= 1;	/**< Toggles clearing the display on each draw. */
int display_ghostboxes 		= 0;	/**< Shows/hides ghost boxes. */
#define DEG2RAD (M_PI/180.)

double display_sphere_scale 	= 1.;	
void display_init_fancy();
unsigned int* display_texture_id = NULL;
char** display_texture_name	= NULL;
unsigned int VertexCount, IndicesCount;
unsigned int VertexHandle, IndicesHandle;
typedef struct _vbo {
	GLfloat pos[3];
	GLfloat tex[2];
} vbo;


/**
 * This function is called when the user presses a key. 
 * @param key Character pressed.
 * @param x Position on screen.
 * @param y Position on screen.
 */
void displayKey(unsigned char key, int x, int y){
	switch(key){
		case 'q': case 'Q':
			printf("\nProgram ends.\n");
			exit(0);
			break;
		case ' ':
			display_pause_sim=!display_pause_sim;
			if (display_pause_sim){
				printf("Pause.\n");
				glutIdleFunc(NULL);
			}else{
				printf("Resume.\n");
				glutIdleFunc(iterate);
			}
			break;
		case 's': case 'S':
			display_mode++;
			if (display_mode>2) display_mode=0;
			break;
		case 'g': case 'G':
			display_ghostboxes = !display_ghostboxes;
			break;
		case '+':
			display_sphere_scale *= 1.125;
			break;
		case '-':
			display_sphere_scale /= 1.125;
			break;
		case 'r': case 'R':
			display_sphere_scale = 1.;
			zprReset(0.85/boxsize_max);
			break;
		case 't': case 'T':
			display_mass = 0;
			display_tree = !display_tree;
			break;
		case 'd': case 'D':
			display_pause = !display_pause;
			break;
		case 'm': case 'M':
			display_mass = !display_mass;
			break;
		case 'w': case 'W':
			display_wire = !display_wire;
			break;
		case 'c': case 'C':
			display_clear = !display_clear;
			break;
		case 'p': case 'P':
#ifdef LIBPNG
			output_png_single("screenshot.png");
			printf("\nScreenshot saved as 'screenshot.png'.\n");
#else 	// LIBPNG
			printf("\nNeed LIBPNG to save screenshot.\n");
#endif 	// LIBPNG
			break;
	}
	display();
}

#ifdef TREE
/**
 * Draws a cell and all its daughters.
 * @param node Cell to draw.
 */
void display_cell(struct cell* node){
	if (node == NULL) return;
#ifdef GRAVITY_TREE
	glColor4f(1.0,0.5,1.0,0.4);
	glTranslatef(node->mx,node->my,node->mz);
	glScalef(0.04*node->w,0.04*node->w,0.04*node->w);
	if (display_mass) {
		glutSolidSphere(1,40,10);
	}
	glScalef(25./node->w,25./node->w,25./node->w);
	glTranslatef(-node->mx,-node->my,-node->mz);
#endif
	glColor4f(1.0,0.0,0.0,0.4);
	glTranslatef(node->x,node->y,node->z);
	glutWireCube(node->w);
	glTranslatef(-node->x,-node->y,-node->z);
	for (int i=0;i<8;i++) {
		display_cell(node->oct[i]);
	}
}

/**
 * Draws the entire tree structure.
 */
void display_entire_tree(){
	for(int i=0;i<root_n;i++){
		display_cell(tree_root[i]);
	}
}
#endif

void display(){
	if (display_pause) return;
#ifdef TREE
	if (display_tree){
		tree_update();
#ifdef GRAVITY_TREE
		tree_update_gravity_data();
#endif
	}
#endif
	if (display_clear){
	        glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	}
	switch (display_mode){
		case 0: // points
			glEnable(GL_BLEND);                    
			glDepthMask(GL_FALSE);
			glDisable(GL_DEPTH_TEST);
			glDisable(GL_LIGHTING);
			glDisable(GL_LIGHT0);
			glDisable(GL_TEXTURE_2D);
			break;
		case 1: // spheres
			glDisable(GL_BLEND);                    
			glDepthMask(GL_TRUE);
			glEnable(GL_DEPTH_TEST);
			glEnable(GL_LIGHTING);
			glEnable(GL_LIGHT0);
			glDisable(GL_TEXTURE_2D);
			GLfloat lightpos[] = {0, boxsize_max, boxsize_max, 0.f};
			glLightfv(GL_LIGHT0, GL_POSITION, lightpos);
			break;
		case 2: // textured spheres
			if (display_init_fancy_done == 0){
				display_init_fancy();
			}
			glDisable(GL_LIGHTING);
			glDisable(GL_LIGHT0);
			glDisable(GL_BLEND);                    
			glDepthMask(GL_TRUE);
			glEnable(GL_DEPTH_TEST);
			glEnable(GL_TEXTURE_2D);
			break;
	}
	glTranslatef(0,0,-boxsize_max);
	glEnable(GL_POINT_SMOOTH);
	glVertexPointer(3, GL_DOUBLE, sizeof(struct particle), particles);
	int _N_active = (N_active==-1)?N:N_active;
	for (int i=-display_ghostboxes*nghostx;i<=display_ghostboxes*nghostx;i++){
	for (int j=-display_ghostboxes*nghosty;j<=display_ghostboxes*nghosty;j++){
	for (int k=-display_ghostboxes*nghostz;k<=display_ghostboxes*nghostz;k++){
		struct ghostbox gb = boundaries_get_ghostbox(i,j,k);
		glTranslatef(gb.shiftx,gb.shifty,gb.shiftz);
		if (!(!display_clear&&display_wire)){
			switch (display_mode){
				case 0: // points
					glEnableClientState(GL_VERTEX_ARRAY);
					glPointSize(3.);
					glColor4f(1.0,1.0,1.0,0.5);
					glDrawArrays(GL_POINTS, _N_active, N-_N_active);
					glColor4f(1.0,1.0,0.0,0.9);
					glPointSize(5.);
					glDrawArrays(GL_POINTS, 0, _N_active);
					glDisableClientState(GL_VERTEX_ARRAY);
					break;
				case 1: // spheres
					glColor4f(1.0,1.0,1.0,1.0);
					for (int i=0;i<N;i++){
						struct particle p = particles[i];
						glTranslatef(p.x,p.y,p.z);
#ifdef COLLISIONS_NONE
						double scale = boxsize/100.*display_sphere_scale;
#else 	// COLLISIONS_NONE
						double scale = p.r*display_sphere_scale;
#endif 	// COLLISIONS_NONE
						glScalef(scale,scale,scale);
						glutSolidSphere(1,40,10);
						glScalef(1./scale,1./scale,1./scale);
						glTranslatef(-p.x,-p.y,-p.z);
					}
					break;
				case 2: // textured spheres
					if(display_init_fancy_done>0){
						glColor4f(1.0,1.0,1.0,1.0);
						glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
						glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
						glBindBuffer(GL_ARRAY_BUFFER,VertexHandle);
						glEnableClientState(GL_VERTEX_ARRAY);
						glVertexPointer(3,GL_FLOAT,		sizeof(vbo), (void*)(0));
						glEnableClientState(GL_NORMAL_ARRAY);
						glNormalPointer(GL_FLOAT,		sizeof(vbo), (void*)(0));
						glEnableClientState(GL_TEXTURE_COORD_ARRAY);	
						glTexCoordPointer(2, GL_FLOAT,	sizeof(vbo), (void*)(3*sizeof(float)));

						glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndicesHandle);
						for (int i=0;i<N;i++){
							glBindTexture(GL_TEXTURE_2D,display_texture_id[i]);
							struct particle p = particles[i];
							glTranslatef(p.x,p.y,p.z);
#ifdef COLLISIONS_NONE
							double scale = boxsize/100.*display_sphere_scale;
#else 	// COLLISIONS_NONE
							double scale = p.r*display_sphere_scale;
#endif 	// COLLISIONS_NONE
							glScalef(scale,scale,scale);
							glDrawElements(GL_TRIANGLE_STRIP, IndicesCount, GL_UNSIGNED_SHORT, 0);
							glScalef(1./scale,1./scale,1./scale);
							glTranslatef(-p.x,-p.y,-p.z);
						}
						glDisableClientState(GL_VERTEX_ARRAY);
						glDisableClientState(GL_NORMAL_ARRAY);
						glDisableClientState(GL_TEXTURE_COORD_ARRAY);	
						glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); 	
						glBindBuffer(GL_ARRAY_BUFFER,0);
						glBindTexture(GL_TEXTURE_2D,0);
						break;
					}
			}
		}
		// Drawing wires
		if (display_wire){
			double radius = 0;
			for (int i=1;i<N;i++){
				struct particle p = particles[i];
				if (N_active>0){
					// Different colors for active/test particles
					if (i>=N_active){
						glColor4f(0.9,1.0,0.9,0.9);
					}else{
						glColor4f(1.0,0.9,0.0,0.9);
					}
				}else{
					// Alternating colors
					if (i%2 == 1){
						glColor4f(0.0,1.0,0.0,0.9);
					}else{
						glColor4f(0.0,0.0,1.0,0.9);
					}
				}
				struct orbit o = tools_p2orbit(p,particles[0].m);
				glPushMatrix();
				
				glRotatef(o.Omega/DEG2RAD,0,0,1);
				glRotatef(o.inc/DEG2RAD,1,0,0);
				glRotatef(o.omega/DEG2RAD,0,0,1);
				
				glBegin(GL_LINE_LOOP);
				for (double trueAnom=0; trueAnom < 2.*M_PI; trueAnom+=M_PI/100.) {
					//convert degrees into radians
					radius = o.a * (1. - o.e*o.e) / (1. + o.e*cos(trueAnom));
					glVertex3f(radius*cos(trueAnom),radius*sin(trueAnom),0);
				}
				glEnd();
				glPopMatrix();
			}
		}
		// Drawing Tree
		glColor4f(1.0,0.0,0.0,0.4);
#ifdef TREE
		if (display_tree){
			glColor4f(1.0,0.0,0.0,0.4);
			display_entire_tree();
		}
#endif // TREE
		glTranslatef(-gb.shiftx,-gb.shifty,-gb.shiftz);
	}
	}
	}
	glColor4f(1.0,0.0,0.0,0.4);
	glScalef(boxsize_x,boxsize_y,boxsize_z);
	glutWireCube(1);
	glScalef(1./boxsize_x,1./boxsize_y,1./boxsize_z);
	glutSwapBuffers();
	glTranslatef(0,0,boxsize_max);
}


void display_init(int argc, char* argv[]){
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH );
	glutInitWindowSize(700,700);
	glutCreateWindow("rebound");
	zprInit(0.85/boxsize_max);
	glutDisplayFunc(display);
	glutIdleFunc(iterate);
	glutKeyboardFunc(displayKey);
	glEnable(GL_BLEND);                    
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);  
	
	// Setup lights

	glCullFace(GL_BACK);
	glShadeModel ( GL_SMOOTH );
	glEnable( GL_NORMALIZE );
	glEnable(GL_COLOR_MATERIAL);
	static GLfloat light[] = {0.7f, 0.7f, 0.7f, 1.f};
	static GLfloat lightspec[] = {0.2f, 0.2f, 0.2f, 1.f};
	static GLfloat lmodel_ambient[] = { 0.15, 0.14, 0.13, 1.0 };

	glLightfv(GL_LIGHT0, GL_DIFFUSE, light );
	glLightfv(GL_LIGHT0, GL_SPECULAR, lightspec );
	glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);

	static GLfloat sphere_mat[] = {0.8f, 0.8f, 0.8f, 1.f};
	static GLfloat sphere_spec[] = {1.0f, 1.0f, 1.0f, 1.f};
	glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE, sphere_mat);
	glMaterialfv(GL_FRONT, GL_SPECULAR, sphere_spec);
	glMaterialf(GL_FRONT, GL_SHININESS, 80);


	// Init sphere
	int stacks = 32;
	int slices = 64;
	float drho	= M_PI / (GLfloat) stacks;
	float dtheta	= 2.0f * M_PI / (GLfloat) slices;
	
	VertexCount  =	(slices + 1)	* (stacks + 1);
	IndicesCount = 2*(slices + 1)	* stacks; 
	
	vbo* sphereVBO					= calloc(VertexCount,	sizeof(vbo));	
	GLushort * triangleStripIndices = calloc(IndicesCount,	sizeof(GLushort));
	
	int counter = 0;		
	for (int i = 0; i <= stacks; i++) {
		float rho = i * drho;
		for (int j = 0; j <= slices; j++) {
			float theta = j * dtheta;
			
			sphereVBO[counter].pos[0] = -sinf(theta) * sinf(rho);
			sphereVBO[counter].pos[1] = cosf(theta) * sinf(rho);
			sphereVBO[counter].pos[2] = cosf(rho);
			sphereVBO[counter].tex[0] = theta/(2.f*M_PI);
			sphereVBO[counter].tex[1] = rho/M_PI;
			
			if (i<stacks){
				triangleStripIndices[2*counter]   = (GLushort) counter;
				triangleStripIndices[2*counter+1] = (GLushort)(counter+(slices+1));
			}
			counter++;													 
		}
	}
	
	glGenBuffers(1,&VertexHandle);
	glBindBuffer(GL_ARRAY_BUFFER, VertexHandle);
	glBufferData(GL_ARRAY_BUFFER, VertexCount *sizeof(vbo), sphereVBO, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER,0);
	
	glGenBuffers(1,&IndicesHandle);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndicesHandle);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, IndicesCount * sizeof(GLushort), triangleStripIndices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
	
	free(sphereVBO);
	free(triangleStripIndices);


	glutMainLoop();
}

char* display_texture_path = NULL;
 
GLuint display_load_texture(char* filename, int width, int height){
	GLuint texture;

	// open texture data
	char tmp[4096];
	sprintf(tmp,"%s%s",display_texture_path,filename);
	FILE* file = fopen( tmp, "rb" );
	if ( file == NULL ) return 0;

	// allocate buffer
	char* data = malloc( width * height * 3 );

	// read texture data
	fread( data, width * height * 3, 1, file );
	fclose( file );

	// allocate a texture name
	glGenTextures( 1, &texture );

	// select our current texture
	glBindTexture( GL_TEXTURE_2D, texture );

	// select modulate to mix texture with color for shading
	glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST );
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
	glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );

	// build our texture mipmaps
	gluBuild2DMipmaps( GL_TEXTURE_2D, 3, width, height, GL_RGB, GL_UNSIGNED_BYTE, data );

	// free buffer
	free( data );

	return texture;
}

void display_find_texture_path(){
#define DTESTFILE "test.raw"
	if(display_texture_path==NULL){
		display_texture_path = calloc(sizeof(char),4096);
	}
	char tmp[40096];
	if( access( DTESTFILE, R_OK ) != -1 ) {
		sprintf(display_texture_path, "./"); 
		return;
	}
	sprintf(tmp,"../%s",DTESTFILE);
	if( access( tmp, R_OK ) != -1 ) {
		sprintf(display_texture_path, "../"); 
		return;
	}
	sprintf(tmp,"../resources/%s",DTESTFILE);
	if( access( tmp, R_OK ) != -1 ) {
		sprintf(display_texture_path, "../resources/"); 
		return;
	}
	sprintf(tmp,"../../%s",DTESTFILE);
	if( access( tmp, R_OK ) != -1 ) {
		sprintf(display_texture_path, "../../"); 
		return;
	}
	sprintf(tmp,"../../resources/%s",DTESTFILE);
	if( access( tmp, R_OK ) != -1 ) {
		sprintf(display_texture_path, "../../resources/"); 
		return;
	}
	char* rebound = getenv("REBOUND");
	if (rebound){
		sprintf(tmp,"%s/%s",rebound,DTESTFILE);
		if( access( tmp, R_OK ) != -1 ) {
			sprintf(display_texture_path, "%s/",rebound); 
			return;
		}
		sprintf(tmp,"%s/resources/%s",rebound,DTESTFILE);
		if( access( tmp, R_OK ) != -1 ) {
			sprintf(display_texture_path, "%s/resources/",rebound); 
			return;
		}
	}
	return; 
}


void display_init_fancy(){
	display_find_texture_path();
	if (!strlen(display_texture_path)){
		printf("\nCannot find path for textures. Set environment variable REBOUND.\n");
		display_init_fancy_done = -1; 
		return;
	}
	if (display_texture_name==NULL){
		printf("\nVariable char** display_texture_name not set.\n");
		display_init_fancy_done = -2; 
		return;
	}
	if (display_texture_id==NULL){
		display_texture_id = malloc(sizeof(unsigned int)*N);
	}
	for (int i=0;i<N;i++){
		int done = 0;
		for (int j=0;j<i;j++){
			if(strcmp(display_texture_name[i],display_texture_name[j])==0){ 
				display_texture_id[i] = display_texture_id[j];
				done = 1;
			}
		}
		if (done==0){
			display_texture_id[i] = display_load_texture(display_texture_name[i],512,512);
		}

	}
	display_init_fancy_done = 1; 
}

#endif // OPENGL
