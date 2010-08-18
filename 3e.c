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
int vx=0,vy=100,vz=0;

int style=2;

int sw() { return 800; }
int sh() { return 600; }

void rot(int x0, int y0, float *x, float *y, float a) {
	*x=x0*cos(a)+y0*sin(a);
	*y=-x0*sin(a)+y0*cos(a);
}

void transform(int x0,int y0,int z0,float *x, float *y, float *z) {
	x0-=vx; y0-=vy; z0-=vz;
	rot(x0,z0,x,z,rot_y);
	rot(y0,*z,y,z,rot_x);
	*x*=scale;
	*y*=scale;
	*z*=scale;
}

int fromscreen(int dx) {
	return (dx/300.0)/scale;
}

void stransform(int xs,int ys,int zs,float *x, float *y, float *z) {
	float x0 = fromscreen(xs);
	float y0 = fromscreen(ys);
	float z0 = fromscreen(zs);

	rot(y0,z0,y,z,-rot_x);
	rot(x0,*z,x,z,-rot_y);
}


void focuscenter() {
	float sx=0,sy=0,sz=0;
	int i,n=0;
	for(i=0;i<pointn;i++) {
		struct point *p=point+i;
		if(p->sel) {
 			n++;
			sx=(sx/n)*(n-1)+ p->x/(float)n;
			sy=(sy/n)*(n-1)+ p->y/(float)n;
			sz=(sz/n)*(n-1)+ p->z/(float)n;
		}
	}
	vx=sx; vy=sy; vz=sz;
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

void deselectall() {
	int i; for(i=0;i<pointn;i++) { point[i].sel=0; }
}

void deletetriangle() {
	int i;
	for(i=0;i<trin;i++) {
		struct tri *t=tri+i;
		if(t->v[0]==-1) continue;
		if(point[t->v[0]].sel && point[t->v[1]].sel && point[t->v[2]].sel) {
			t->v[0]=-1;
		}
	}
}

void triangle() {
	struct tri *t=tri+trin;
	int i=pointn-1,j=0;
	for(;i>=0;i--) { if(point[i].sel) { t->v[j++]=i; break; } }
	for(i--;i>=0;i--) { if(point[i].sel) { t->v[j++]=i; break; } }
	for(i--;i>=0;i--) { if(point[i].sel) { t->v[j++]=i; break; } }

	if(j==3) trin++;
}



void move(int dx,int dy,int dz) {
	int i;
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
        "uniform vec3 normal;"
        "uniform vec3 center;"
        "varying vec4 coloro;"
        "varying vec3 normalo;"


	"vec2 rot(float x, float y, float a);"
	"vec2 rot(float x, float y, float a) {"
		"return vec2(x*cos(a)+y*sin(a), -x*sin(a)+y*cos(a));"
	"}"

        "vec3 transform(vec3 v){"
                "vec3 p=v-center;"
		"vec2 r=rot(p.x,p.z,rot_y);"
		"p.x=r.x; p.z=r.y;"
		""
		"r=rot(p.y,r.y,rot_x);"
		"p.y=r.x; p.z=r.y;"
		""
		"p=p*scale;"
		"return p;"
        "}"

        "void main(){"
                "vec3 p=transform(gl_Vertex.xyz);"
		"gl_Position=vec4(p,1);"
		"coloro=color;"
		"if(length(normal)>0) {"
			"normalo=transform(normal);"
		"} else {"
			"normalo=vec3(0,0,0);"
		"}"
        "}"
};

const char *fshader[1]={
        "varying vec4 coloro;"
        "varying vec3 normalo;"
        "void main(){"
		"if(length(normalo)>0) {"
			"vec3 n=normalize(normalo);"
			"vec3 l=normalize(vec3(1,1,1));"
			"float df=clamp(dot(n,l), 0.0, 1.0);"
			"gl_FragColor=coloro*0.25 + 0.75*coloro*df;"
		"} else {"
			"gl_FragColor=coloro;"
		"}"
        "}" };


GLint sh_rot_x;
GLint sh_rot_y;
GLint sh_scale;
GLint sh_normal;
GLint sh_color;
GLint sh_center;

void initgl() {
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
	sh_normal=glGetUniformLocation(p,"normal");
	sh_center=glGetUniformLocation(p,"center");

	glUseProgram(p);
}

void normal(GLuint v[3],float *x,float *y,float *z) {
	float a0=point[v[1]].x-point[v[0]].x,
	      a1=point[v[1]].y-point[v[0]].y,
	      a2=point[v[1]].z-point[v[0]].z;
	float b0=point[v[2]].x-point[v[0]].x,
	      b1=point[v[2]].y-point[v[0]].y,
	      b2=point[v[2]].z-point[v[0]].z;


	// cross
	*x=a1 * b2 - a2 * b1;
	*y=a2 * b0 - a0 * b2,
	*z=a0 * b1 - a1 * b0;
}

void gldraw(int x, int y, int w, int h, float rot_x, float rot_y) {
	glViewport(x,y,w,h);

	glUniform1f(sh_rot_x,rot_x);
	glUniform1f(sh_rot_y,rot_y);
	glUniform1f(sh_scale,scale);
	glUniform3f(sh_center,vx,vy,vz);

	glEnableClientState(GL_VERTEX_ARRAY);
	glPointSize(5);
	glVertexPointer(3, GL_INT, sizeof(struct point), point);

	int i;
	for(i=0;i<trin;i++) {
		if(tri[i].v[0]==-1) continue;
		float x,y,z;
		normal(tri[i].v,&x,&y,&z);

		glUniform3f(sh_normal,x,y,z);
		glUniform4f(sh_color,1,1,1,1);
		glDrawElements(GL_TRIANGLES,3, GL_UNSIGNED_INT, tri[i].v);
	}

	glUniform3f(sh_normal,0,0,0);
	glUniform4f(sh_color,.5,.5,.5,1);
	glDrawArrays(GL_POINTS,0,pointn);

	glDisableClientState(GL_VERTEX_ARRAY);

	glPointSize(10);
	glDisable(GL_DEPTH_TEST);
	glUniform4f(sh_color,1,0,0,1);
	glBegin(GL_POINTS);
	for(i=0;i<pointn;i++) {
		struct point *p=point+i;
		if(p->sel) {
			glVertex3f(p->x,p->y,p->z);
		}
	}
	glEnd();

	glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
	glEnableClientState(GL_VERTEX_ARRAY);
	for(i=0;i<trin;i++) {
		struct tri *t=tri+i;
		if(t->v[0]==-1) continue;
		int c=point[t->v[0]].sel + point[t->v[1]].sel + point[t->v[2]].sel;
		if(c>0) {
			glUniform4f(sh_color,1,0,0,0.33*c);
			glLineWidth(2*c*c);
			glDrawElements(GL_TRIANGLES,3, GL_UNSIGNED_INT, t->v);
		}
	}
	glDisableClientState(GL_VERTEX_ARRAY);
	glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	glEnable(GL_DEPTH_TEST);
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
				if(e.key.keysym.sym==SDLK_ESCAPE) deselectall();
				if(e.key.keysym.sym==SDLK_t) { triangle(); }
				if(e.key.keysym.sym==SDLK_d) { deletetriangle(); }
				if(e.key.keysym.sym==SDLK_c) { focuscenter(); }
				if(e.key.keysym.sym==SDLK_a) {
					if(++style==3) style=0;
				}
			}
			if(e.type==SDL_MOUSEBUTTONDOWN) {
				if(e.button.button==SDL_BUTTON_LEFT) {
					if(SDL_GetModState()&KMOD_CTRL) {
						struct point *p=point+pointn++;

						float x0 = fromscreen(e.button.x-300);
						float y0 = fromscreen(300-e.button.y);

						float x,y,z;

						rot(y0,0,&y,&z,-rot_x);
						rot(x0,z,&x,&z,-rot_y);
						p->x=x+vx; p->y=y+vy; p->z=z+vz; p->sel=1;
					} else {
						if(e.button.x>=600) {
							rot_x=rot_x+M_PI/2;
							rot_y=rot_y+M_PI/2;
							
						} else {
							select_point(e.button.x,e.button.y);
						}
					}
				}
				if(e.button.button==SDL_BUTTON_WHEELDOWN) { scale/=1.5; }
				if(e.button.button==SDL_BUTTON_WHEELUP) { scale*=1.5; }
			}
			if(e.type==SDL_MOUSEMOTION) {
				if(SDL_GetModState()&KMOD_SHIFT) {
					if(e.motion.state&SDL_BUTTON(1)) {
						float rx,ry,rz;
						stransform(e.motion.xrel,-e.motion.yrel,0,&rx,&ry,&rz);
						move(rx,ry,rz);
					}
				} else if(e.motion.state&SDL_BUTTON(2)) {
					rot_y+=(((float)e.motion.xrel)/(sw()/4))*M_PI;
					rot_x+=(((float)e.motion.yrel)/(sh()/4))*M_PI;
				}
			}
		}


		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		gldraw(0,0,600,600,rot_x,rot_y);
		gldraw(600,0,200,200,rot_x+M_PI/2,rot_y+M_PI/2);
		SDL_GL_SwapBuffers();
	}
end:	SDL_Quit();
	return 0;
}

