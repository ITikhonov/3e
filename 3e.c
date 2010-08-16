#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_gfxPrimitives.h>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>

#include <math.h>
#include <limits.h>

#undef NDEBUG
#include <assert.h>

#include "m/a.h"

SDL_Surface *screen, *rscreen;

struct point {
	GLint x,y,z,sel;
} point[102400];

struct tri {
	GLuint v[3];
} tri[409600];

int pointn=0;
int trin=0;

int minz=INT_MAX,maxz=INT_MIN;

float rot_y=0;
float rot_x=0;
float scale=1/100000.0;

int style=2;

int sw() { return 800; }
int sh() { return 600; }

void rot(int x0, int y0, float *x, float *y, float a) {
	*x=x0*cos(a)+y0*sin(a);
	*y=-x0*sin(a)+y0*cos(a);
}

void transform(int x0,int y0,int z0,float *x, float *y, float *z) {
	rot(x0,z0,x,z,rot_y);
	rot(y0,*z,y,z,rot_x);
	*x*=scale;
	*y*=scale;
	*z*=scale;
}

void select_point(int x,int y) {
	float fx,fy;

	fx=(x-300)/300.0;
	fy=-(y-300)/300.0;
	float ps=5.0/600;


	int i,z=INT_MIN,n=-1;
	for(i=0;i<pointn;i++) {
		struct point *p=point+i;
		float sx,sy,sz;
		transform(p->x,p->y,p->z,&sx,&sy,&sz);
		if(fabsf(fx-sx)<ps && fabsf(fy-sy)<ps && sz>z) {
			n=i; z=sz;
		}
	}

	if(n>=0) point[n].sel=!point[n].sel;
}


int mx,my,mz;

void move(int dx,int dy,int dz) {
	int i,z=INT_MIN,n=-1;
	for(i=0;i<pointn;i++) {
		struct point *p=point+i;
		if(p->sel) {
			p->x+=dx;
			p->y+=dy;
			p->z+=dz;
		}
	}
}

const char *vshader[1] = {
        "uniform float rot_x;"
        "uniform float rot_y;"
        "uniform float scale;"
        "uniform vec4 color;"
        "varying vec4 coloro;"


	"vec2 rot(float x, float y, float a);"
	"vec2 rot(float x, float y, float a) {"
		"return vec2(x*cos(a)+y*sin(a), -x*sin(a)+y*cos(a));"
	"}"

        "void main(){"
                "vec3 p=gl_Vertex.xyz;"
		"vec2 r=rot(p.x,p.z,rot_y);"
		"p.x=r.x; p.z=r.y;"

		"r=rot(p.y,r.y,rot_x);"
		"p.y=r.x; p.z=r.y;"

		"p=p*scale;"
		"gl_Position=vec4(p,1);"
		"coloro=color;"
        "}" };

const char *fshader[1]={
        "varying vec4 coloro;"
        "void main(){"
		"float z=gl_FragCoord.z;"
                "gl_FragColor=coloro;"
        "}" };


GLint sh_rot_x;
GLint sh_rot_y;
GLint sh_scale;
GLint sh_color;

void initgl() {

	glViewport(0,0,600,600);

	glClearColor( 1.0f, 1.0f, 1.0f, 1.0f );
	glEnable(GL_DEPTH_TEST);
	glPointSize(5);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );

	GLuint v = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(v,1,vshader,NULL);
	glCompileShader(v);

	{ int sl;
	  glGetShaderiv(v,GL_INFO_LOG_LENGTH,&sl);
	  char infoLog[sl];
	  glGetShaderInfoLog(v,sl,&sl,infoLog);
	  if(sl>0) printf("%s\n",infoLog);
	}

	GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(f,1,fshader,NULL);
	glCompileShader(f);

	{ int sl;
	  glGetShaderiv(f,GL_INFO_LOG_LENGTH,&sl);
	  char infoLog[sl];
	  glGetShaderInfoLog(f,sl,&sl,infoLog);
	  if(sl>0) printf("%s\n",infoLog);
	}

	GLuint p = glCreateProgram();
	glAttachShader(p,v);
	glAttachShader(p,f);
	glLinkProgram(p);

	{ int sl;
	  glGetProgramiv(p,GL_INFO_LOG_LENGTH,&sl);
	  char infoLog[sl];
	  glGetProgramInfoLog(p,sl,&sl,infoLog);
	  if(sl>0) printf("%s\n",infoLog);
	}
	sh_rot_x=glGetUniformLocation(p,"rot_x");
	sh_rot_y=glGetUniformLocation(p,"rot_y");
	sh_scale=glGetUniformLocation(p,"scale");
	sh_color=glGetUniformLocation(p,"color");

	glUseProgram(p);
}

void gldraw() {
	glUniform1f(sh_rot_x,rot_x);
	glUniform1f(sh_rot_y,rot_y);
	glUniform1f(sh_scale,scale);

	glEnableClientState(GL_VERTEX_ARRAY);
	glVertexPointer(3, GL_INT, sizeof(struct point), point);

	int i;
	for(i=0;i<trin;i++) {
		uint32_t c=0xFF<<(i*2);
		glUniform4f(sh_color,(c>>16)&0xff,(c>>8)&0xff,(c)&0xff,1);
		glDrawElements(GL_TRIANGLES,3, GL_UNSIGNED_INT, tri[i].v);
	}

	glUniform4f(sh_color,.5,.5,.5,1);
	glDrawArrays(GL_POINTS,0,pointn);

	glDisableClientState(GL_VERTEX_ARRAY);

	glUniform4f(sh_color,1,0,0,1);
	glBegin(GL_POINTS);
	for(i=0;i<pointn;i++) {
		struct point *p=point+i;
		if(p->sel) glVertex3f(p->x,p->y,p->z);
	}
	glEnd();
}

int main() {
	SDL_Init(SDL_INIT_VIDEO);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
	rscreen=SDL_SetVideoMode(800,600,32,SDL_HWSURFACE|SDL_OPENGLBLIT);
	
	initgl();

	int i;
	for(i=0;i<load_n;i++) {
		struct point *p=point+pointn++;
		p->x=load_v[i][0]*100;
		p->y=-load_v[i][1]*100;
		p->z=load_v[i][2]*100;
		p->sel=0;
	}

	for(i=0;i<load_k;i++) {
		struct tri *p=tri+trin++;
		p->v[0]=load_t[i][0];
		p->v[1]=load_t[i][1];
		p->v[2]=load_t[i][2];
	}

	for(;;) {
		SDL_Event e;
		while(SDL_PollEvent(&e)) {
			if(e.type==SDL_QUIT) goto end;
			if(e.type==SDL_KEYDOWN) {
				if(e.key.keysym.sym==SDLK_q) goto end;
				if(e.key.keysym.sym==SDLK_a) {
					if(++style==3) style=0;
				}
				if(e.key.keysym.sym==SDLK_SPACE) {
					move(mx,my,mz);
					mx=my=mz=0;
				}
			}
			if(e.type==SDL_MOUSEBUTTONDOWN) {
				if(e.button.button==SDL_BUTTON_LEFT) {
					if(SDL_GetModState()&KMOD_CTRL) {
						struct point *p=point+pointn++;

						float x0 = e.button.x-sw()/2;
						float y0 = e.button.y-sh()/2;
						x0/=scale;
						y0/=scale;

						//rot(y0,0,&p->y,&p->z,-rot_x);
						//rot(x0,p->z,&p->x,&p->z,-rot_y);
					} else {
						select_point(e.button.x,e.button.y);
					}
				}
				if(e.button.button==SDL_BUTTON_WHEELDOWN) { scale/=1.5; }
				if(e.button.button==SDL_BUTTON_WHEELUP) { scale*=1.5; }
			}
			if(e.type==SDL_MOUSEMOTION) {
				if(SDL_GetModState()&KMOD_SHIFT) {
					if(e.motion.state&SDL_BUTTON(1)) {
						mx+=e.motion.xrel; my+=e.motion.yrel;
					} else if(e.motion.state&SDL_BUTTON(3)) {
						mz+=e.motion.xrel;
					}
				} else if(e.motion.state&SDL_BUTTON(2)) {
					printf("%0.3f %0.3f ", rot_x, rot_y);
					rot_y+=(((float)e.motion.xrel)/(sw()/4))*M_PI;
					rot_x+=(((float)e.motion.yrel)/(sh()/4))*M_PI;
					printf(" -> %0.3f %0.3f\n", rot_x, rot_y);
				}
			}
		}


		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		gldraw();
		SDL_GL_SwapBuffers();
	}
end:	SDL_Quit();
	return 0;
}

