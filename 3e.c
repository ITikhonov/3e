#include <stdio.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_gfxPrimitives.h>

#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>

#include <math.h>
#include <limits.h>
#include <stdint.h>

#undef NDEBUG
#include <assert.h>

#include "m/a.h"

SDL_Surface *screen;

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
float scale=1;
float zshift[2];
int vx=0,vy=100,vz=0;

int style=2;

int sw=800;
int sh=600;

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
	return (dx/(float)(sh/2.0))/scale;
}

void stransform(int xs,int ys,float zs,float *x, float *y, float *z) {
	float x0 = fromscreen(xs);
	float y0 = fromscreen(ys);
	float z0 = zs;

	rot(y0,z0,y,z,-rot_x);
	rot(x0,*z,x,z,-rot_y);
}

#define MAX(x,y) (((x)>(y))?(x):(y))
#define MIN(x,y) (((x)<(y))?(x):(y))

int mxx,mxy,mxz;
int mnx,mny,mnz;
float sx,sy,sz;

void bounds() {
	mxx=mxy=mxz=INT_MIN;
	mnx=mny=mnz=INT_MAX;
	sx=sy=sz=0;

	if(pointn>3) {
		int i,n=0;
		for(i=0;i<pointn;i++) {
			struct point *p=point+i;
			if(p->sel&2) continue;
			n++;
			sx=(sx/n)*(n-1)+ p->x/(float)n;
			sy=(sy/n)*(n-1)+ p->y/(float)n;
			sz=(sz/n)*(n-1)+ p->z/(float)n;
			mxx=MAX(p->x,mxx); mxy=MAX(p->y,mxy); mxz=MAX(p->z,mxz);
			mnx=MIN(p->x,mnx); mny=MIN(p->y,mny); mnz=MIN(p->z,mnz);
		}
	} else {
		mxx=mxy=mxz=1000;
		mnx=mny=mnz=-1000;
	}

	int mx=MAX(MAX(mxx,mxy),mxz);
	int mn=MIN(MIN(mnx,mny),mnz);
	mx=MAX(-mn,mx);
	zshift[0]=-mx;
	zshift[1]=2*mx;
}


void center() {
	bounds();
	float scx=1.0/(mxx-mnx);
	float scy=1.0/(mxy-mny);
	float scz=1.0/(mxz-mnz);

	scale=MIN(scz,MIN(scx,scy));
	vx=sx; vy=sy; vz=sz;
}

void focuscenter() {
	float sx=0,sy=0,sz=0;

	int i,n=0;
	for(i=0;i<pointn;i++) {
		struct point *p=point+i;
		if(p->sel&2) continue;
		if(p->sel) {
 			n++;
			sx=(sx/n)*(n-1)+ p->x/(float)n;
			sy=(sy/n)*(n-1)+ p->y/(float)n;
			sz=(sz/n)*(n-1)+ p->z/(float)n;
		}
	}
	vx=sx; vy=sy; vz=sz;
}

void delete() {
	int i;
	for(i=0;i<trin;i++) {
		struct tri *t=tri+i;
		point[t->v[0]].sel|=0x100;
		point[t->v[1]].sel|=0x100;
		point[t->v[2]].sel|=0x100;
	}

	for(i=0;i<pointn;i++) {
		struct point *p=point+i;
		if(p->sel&2) continue;
		if(p->sel&0x100) { p->sel&=~0x100; continue; }
		if(p->sel) { p->sel|=2; }
	}
}


void select_triangle(float x,float y) {
	int i;
	int n=-1;
	float z=INFINITY;
	for(i=0;i<trin;i++) {
		struct tri *t=tri+i;
		float x0,y0,z0;
		float x1,y1,z1;
		float x2,y2,z2;

		transform(point[t->v[0]].x,point[t->v[0]].y,point[t->v[0]].z,&x0,&y0,&z0);
		transform(point[t->v[1]].x,point[t->v[1]].y,point[t->v[1]].z,&x1,&y1,&z1);
		transform(point[t->v[2]].x,point[t->v[2]].y,point[t->v[2]].z,&x2,&y2,&z2);

		x1-=x0; y1-=y0; z1-=z0;
		x2-=x0; y2-=y0; z2-=z0;
		x0=x-x0; y0=y-y0;

		float d=x1*y2-y1*x2;

		float a=(x0*y2-y0*x2)/d;
		float b=-(x0*y1-y0*x1)/d;

		if(a>0 && b>0 && (a+b)<1) {
			float zv=z0+a*z2+b*z1;
			if(zv<z) { n=i; z=zv; }
		}
	}

	if(n>=0) {
		struct tri *t=tri+n;
		point[t->v[0]].sel=!point[t->v[0]].sel;
		point[t->v[1]].sel=!point[t->v[1]].sel;
		point[t->v[2]].sel=!point[t->v[2]].sel;
	}
}

void select_point(int x,int y) {
	float fx,fy;

	fx=(x-(sh/2))/(sh/2.0);
	fy=-(y-sh/2)/(sh/2.0);
	float ps=5.0/sh;


	int i,z=INT_MIN,n=-1;
	for(i=0;i<pointn;i++) {
		struct point *p=point+i;
		if(p->sel&2) continue;
		float sx,sy,sz;
		transform(p->x,p->y,p->z,&sx,&sy,&sz);
		if(fabsf(fx-sx)<ps && fabsf(fy-sy)<ps && sz>z) {
			n=i; z=sz;
		}
	}

	if(n>=0) point[n].sel=!point[n].sel;
	else select_triangle(fx,fy);
}

void deselectall() {
	int i;
	for(i=0;i<pointn;i++) {
		struct point *p=point+i;
		if(p->sel&2) continue;
 		p->sel=0;
	}
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
		if(p->sel&2) continue;
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
        "uniform vec2 zshift;"
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
		"p.x=p.x*scale;"
		"p.y=p.y*scale;"
		"p.z=(p.z-zshift.x)/(zshift.y - zshift.x);"
		"return p;"
        "}"

        "void main(){"
                "vec3 p=transform(gl_Vertex.xyz);"
		"coloro=color;"
		"if(length(normal)>0) {"
			"normalo=transform(normal);"
			"p.z-=0.0001;"
		"} else {"
			"normalo=vec3(0,0,0);"
		"}"
		"gl_Position=vec4(p,1);"
        "}"
};

const char *fshader[1]={
        "varying vec4 coloro;"
        "varying vec3 normalo;"
        "void main(){"
		"if(length(normalo)>0) {"
			"vec3 n=normalize(normalo);"
			"vec3 l=normalize(vec3(1,1,-1));"
			"vec3 l2=normalize(vec3(1,-1,-1));"
			"float df=clamp(dot(n,l), 0.0, 1.0);"
			"float df2=clamp(dot(n,l2), 0.0, 1.0);"
			"gl_FragColor=coloro*0.25 + 0.50*coloro*df + 0.25*df2;"
		"} else {"
			"gl_FragColor=coloro;"
		"}"
        "}" };


GLint sh_rot_x;
GLint sh_rot_y;
GLint sh_scale;
GLint sh_zshift;
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
	sh_zshift=glGetUniformLocation(p,"zshift");
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

	*x=a1 * b2 - a2 * b1;
	*y=a2 * b0 - a0 * b2,
	*z=a0 * b1 - a1 * b0;
}

void gldraw(int x, int y, int w, int h, float rot_x, float rot_y) {
	glViewport(x,y,w,h);

	bounds();

	glUniform1f(sh_rot_x,rot_x);
	glUniform1f(sh_rot_y,rot_y);
	glUniform1f(sh_scale,scale);
	glUniform2f(sh_zshift,zshift[0],zshift[1]);
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

	glDisableClientState(GL_VERTEX_ARRAY);

	glPointSize(10);
	glLineWidth(2);
	glUniform4f(sh_color,0,0,0,1);
	glBegin(GL_LINES);
	int lines=0.025/scale;
	for(i=0;i<pointn;i++) {
		struct point *p=point+i;
		if(p->sel&2) continue;
		if(p->sel) {
			glEnd();
			glDisable(GL_DEPTH_TEST);
			glLineWidth(3);
			glUniform4f(sh_color,0,1,0,1);
			lines*=3;
			glBegin(GL_LINES);
			glVertex3f(p->x-lines,p->y,p->z);
			glVertex3f(p->x+lines,p->y,p->z);
			glVertex3f(p->x,p->y-lines,p->z);
			glVertex3f(p->x,p->y+lines,p->z);
			glVertex3f(p->x,p->y,p->z-lines);
			glVertex3f(p->x,p->y,p->z+lines);
			glEnd();
			lines/=3;
			glUniform4f(sh_color,0,0,0,1);
			glLineWidth(2);
			glEnable(GL_DEPTH_TEST);
			glBegin(GL_LINES);
		}
		glVertex3f(p->x-lines,p->y,p->z);
		glVertex3f(p->x+lines,p->y,p->z);
		glVertex3f(p->x,p->y-lines,p->z);
		glVertex3f(p->x,p->y+lines,p->z);
		glVertex3f(p->x,p->y,p->z-lines);
		glVertex3f(p->x,p->y,p->z+lines);

	}
	glEnd();

	glDisable(GL_DEPTH_TEST);

	glPointSize(10);
	glUniform4f(sh_color,1,0,0,1);
	glBegin(GL_POINTS);
	glVertex3f(vx,vy,vz);
	glEnd();

	glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
	glEnableClientState(GL_VERTEX_ARRAY);
	for(i=0;i<trin;i++) {
		struct tri *t=tri+i;
		if(t->v[0]==-1) continue;
		int c=point[t->v[0]].sel + point[t->v[1]].sel + point[t->v[2]].sel;
		if(c>0) {
			switch(c) {
			case 1:	glUniform4f(sh_color,1,1,1,1); break;
			case 2:	glUniform4f(sh_color,0,0,1,1); break;
			case 3:	glUniform4f(sh_color,1,0,0,1); break;
			}
			glLineWidth(2*c*c);
			glDrawElements(GL_TRIANGLES,3, GL_UNSIGNED_INT, t->v);
		}
	}
	glDisableClientState(GL_VERTEX_ARRAY);
	glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
	glEnable(GL_DEPTH_TEST);
}

char *name;

void load() {
	FILE *f=fopen(name,"r");
	if(!f) return;
	for(;;) {
		struct point *p=point+pointn;
		uint32_t l,c[3];
		if(fread(&l,sizeof(l),1,f)!=1) return;
		if(!l) break;
		if(fread(c,sizeof(c),1,f)!=1) return;
		p->x=c[0]; p->y=c[1]; p->z=c[2];
		pointn++;
	}

	for(;;) {
		struct tri *t=tri+trin;
		uint32_t l,c[3];
		if(fread(&l,sizeof(l),1,f)!=1) return;
		if(!l) break;
		if(fread(c,sizeof(c),1,f)!=1) return;
		t->v[0]=c[0]; t->v[1]=c[1]; t->v[2]=c[2];
		trin++;
	}
}

void save() {
	FILE *f=fopen(name,"w");
	int i;
	for(i=0;i<pointn;i++) {
		struct point *p=point+i;
		if(p->sel&2) continue;
		int32_t c[4]={sizeof(uint32_t[3]),p->x,p->y,p->z};
		fwrite(c,sizeof(c),1,f);
		
	}
	uint32_t z=0;
	fwrite(&z,sizeof(z),1,f);

	for(i=0;i<trin;i++) {
		struct tri *t=tri+i;
		if(t->v[0]==-1) continue;
		uint32_t c[4]={sizeof(uint32_t[3]),t->v[0],t->v[1],t->v[2]};
		fwrite(c,sizeof(c),1,f);
	}
	fwrite(&z,sizeof(z),1,f);
	fclose(f);
}

int main(int argc, char *argv[]) {
	name=argv[1];
	if(argc==2) load();

	SDL_Init(SDL_INIT_VIDEO);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
	screen=SDL_SetVideoMode(800,600,32,SDL_HWSURFACE|SDL_OPENGLBLIT|SDL_RESIZABLE);
	
	initgl();
	center();

	for(;;) {
		SDL_Event e;
		while(SDL_PollEvent(&e)) {
			if(e.type==SDL_QUIT) goto end;
			if(e.type==SDL_VIDEORESIZE) {
				sw=e.resize.w; sh=e.resize.h;
				screen=SDL_SetVideoMode(sw,sh,32,SDL_HWSURFACE|SDL_OPENGLBLIT|SDL_RESIZABLE);
			}
			if(e.type==SDL_KEYDOWN) {
				if(e.key.keysym.sym==SDLK_q) goto end;
				if(e.key.keysym.sym==SDLK_ESCAPE) deselectall();
				if(e.key.keysym.sym==SDLK_t) { triangle(); }
				if(e.key.keysym.sym==SDLK_d) { deletetriangle(); }
				if(e.key.keysym.sym==SDLK_c) { focuscenter(); }
				if(e.key.keysym.sym==SDLK_s) { save(); }
				if(e.key.keysym.sym==SDLK_r) { delete(); }
				if(e.key.keysym.sym==SDLK_a) {
					if(++style==3) style=0;
				}
			}
			if(e.type==SDL_MOUSEBUTTONDOWN) {
				if(e.button.button==SDL_BUTTON_LEFT) {
					if(SDL_GetModState()&KMOD_CTRL) {
						struct point *p=point+pointn++;

						float x,y,z;
						x=fromscreen(e.button.x-sh/2.0);
						y=fromscreen(sh/2.0-e.button.y);

						rot(y,0,&y,&z,-rot_x);
						rot(x,z,&x,&z,-rot_y);

						p->x=x+vx; p->y=y+vy; p->z=z+vz; p->sel=1;
					} else {
						if(e.button.x>=sh) {
							if(e.button.y>=2*sh/3.0) {
								rot_x=rot_x+M_PI/2;
								rot_y=rot_y+M_PI/2;
							} else if(e.button.y>=sh/3.0) {
								rot_x=0;
								rot_y=0;
							} else {
								rot_x=0;
								rot_y=M_PI/2;
							}
							
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
					rot_y+=(((float)e.motion.xrel)/(sw/4))*M_PI;
					rot_x+=(((float)e.motion.yrel)/(sh/4))*M_PI;
				}
			}
		}


		int sh3=sh/3;
		glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
		gldraw(0,0,sh,sh,rot_x,rot_y);
		gldraw(sh,0,sh3,sh3,rot_x+M_PI/2,rot_y+M_PI/2);
		gldraw(sh,sh3,sh3,sh3,0,0);
		gldraw(sh,2*sh3,sh3,sh3,0,M_PI/2);
		SDL_GL_SwapBuffers();
	}
end:	SDL_Quit();
	return 0;
}

