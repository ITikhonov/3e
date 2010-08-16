#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_gfxPrimitives.h>

#include <math.h>
#include <limits.h>

#undef NDEBUG
#include <assert.h>

#include "m/a.h"

SDL_Surface *screen;

struct point {
	int x,y,z,sx,sy,sz,sel;
} point[102400];

struct tri {
	int v[3];
} tri[409600];

int pointn=0;
int trin=0;

int minz=INT_MAX,maxz=INT_MIN;

float rot_y=0;
float rot_x=0;
float scale=1/128.0;

int style=1;

float zbuf[800][600];

int sw() { return 800; }
int sh() { return 600; }

void select_point(int x,int y) {
	int i,z=INT_MIN,n=-1;
	for(i=0;i<pointn;i++) {
		struct point *p=point+i;
		if(abs(x-p->sx)<3 && abs(y-p->sy)<3 && p->sz>z) {
			n=i; z=p->sz;
		}
	}

	point[n].sel=!point[n].sel;
}

void rot(int x0, int y0, int *x, int *y, float a) {
	*x=x0*cos(a)+y0*sin(a);
	*y=-x0*sin(a)+y0*cos(a);
}

void transform(int x0,int y0,int z0,int *x, int *y, int *z) {
	rot(x0,z0,x,z,rot_y);
	rot(y0,*z,y,z,rot_x);
	*x*=scale;
	*y*=scale;
	*z*=scale;
}

void draw_span_line(int x0,int x1,int y,float z0,float z1,Uint32 color) {
	int x;
	int dx=x0<x1?1:-1;
	float dz=(z1-z0)/(x1-x0);
	for(x=x0;x!=x1;x+=dx) {
		if(z0>zbuf[x][y]) {
			pixelColor(screen,x,y,color);
			//pixelColor(screen,x,y,((int)(128*(z0+300)/600)<<8)|0xff);
			zbuf[x][y]=z0;
		}
		z0+=dz;
	}
}

void draw_span(struct point *f, struct point *m, struct point *l, Uint32 color) {
	float dx0=(l->sx-f->sx)/(float)(l->sy-f->sy);
	float x0=f->sx;

	float dz0=(l->sz-f->sz)/(float)(l->sy-f->sy);
	float z0=f->sz;
	if(f->sy!=m->sy) {
		int y=f->sy;

		float dx1=(m->sx-f->sx)/(float)(m->sy-f->sy);
		float x1=f->sx;

		float dz1=(m->sz-f->sz)/(float)(m->sy-f->sy);
		float z1=f->sz;

		for(;y<m->sy;y++) {
			draw_span_line(x0,x1,y,z0,z1,color);
			pixelColor(screen,x0,y,0xFF0000FF);
			pixelColor(screen,x1,y,0x0000FFFF);
			x0+=dx0;
			x1+=dx1;
			z0+=dz0;
			z1+=dz1;
		}
	}

	if(m->sy!=l->sy) {
		int y=m->sy;

		float dx1=(l->sx-m->sx)/(float)(l->sy-m->sy);
		float x1=m->sx;

		float dz1=(l->sz-m->sz)/(float)(l->sy-m->sy);
		float z1=m->sz;

		for(;y<l->sy;y++) {
			draw_span_line(x0,x1,y,z0,z1,color);
			pixelColor(screen,x0,y,0xFF0000FF);
			pixelColor(screen,x1,y,0x0000FFFF);
			x0+=dx0;
			x1+=dx1;
			z0+=dz0;
			z1+=dz1;
		}
	}
}


void draw_tri(int v1, int v2, int v3, Uint32 color) {
	struct point *a=point+v1, *b=point+v2, *c=point+v3;
	struct point *f,*l,*m;

	int ab = a->sy < b->sy;
	int bc = b->sy < c->sy;
	int ac = a->sy < c->sy;

	if(ab) {
		if(bc) { f=a;m=b;l=c; }
		else { if(ac) { f=a;m=c;l=b; } else { f=c;m=a;l=b;} }
	} else {
		if(!bc) { f=c;m=b;l=a; }
		else { if(ac) { f=b;m=a;l=c; } else { f=b;m=c;l=a;} }
	}

	assert(f->sy<=m->sy);
	assert(m->sy<=l->sy);

	draw_span(f,m,l,color);
}

void draw_tri_wire(int v1, int v2, int v3) {
	struct point *a=point+v1, *b=point+v2, *c=point+v3;
	aalineColor(screen,a->sx,a->sy,b->sx,b->sy,0xCCCCCCFF);
	aalineColor(screen,b->sx,b->sy,c->sx,c->sy,0xCCCCCCFF);
	aalineColor(screen,c->sx,c->sy,a->sx,a->sy,0xCCCCCCFF);
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

int main() {
	SDL_Init(SDL_INIT_VIDEO);

	screen=SDL_SetVideoMode(800,600,32,SDL_HWSURFACE);
	

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

						rot(y0,0,&p->y,&p->z,-rot_x);
						rot(x0,p->z,&p->x,&p->z,-rot_y);
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
					rot_y+=(((float)e.motion.xrel)/(sw()/4))*M_PI;
					rot_x+=(((float)e.motion.yrel)/(sh()/4))*M_PI;
				}
			}
		}


		SDL_FillRect(screen,0,0xffffff);
		int i;

		for(i=0;i<800*600;i++) {
			*(((float*)zbuf)+i)=-INFINITY;
		}

		if(style==1) {
			for(i=0;i<trin;i++) {
				draw_tri(tri[i].v[0],tri[i].v[1],tri[i].v[2], (0xFF<<(8+i*2))|0xFF );
			}
		} else if(style==2) {
			for(i=0;i<trin;i++) {
				draw_tri_wire(tri[i].v[0],tri[i].v[1],tri[i].v[2]);
			}
		}

		
		int minz1=minz,spanz=maxz-minz;
		maxz=INT_MIN; minz=INT_MAX;
		for(i=0;i<pointn;i++) {
			struct point *p=point+i;

			int x,y,z; transform(p->x,p->y,p->z,&x,&y,&z);

			x+=sw()/2;
			y+=sh()/2;

			p->sx=x; p->sy=y; p->sz=z;
			if(z<minz) minz=z; if(z>maxz) maxz=z;

			float c=(z-minz1)/(float)spanz;
			unsigned int color=0xff-0xff*c;
			color=(color<<16)|(color<<8)|color;

			SDL_Rect dh={x,y-2,1,5};
			SDL_FillRect(screen,&dh,color);
			SDL_Rect dv={x-2,y,5,1};
			SDL_FillRect(screen,&dv,color);
			if(p->sel) {
				SDL_Rect ds={x-2,y-2,5,5};
				SDL_FillRect(screen,&ds,0xAAAAAA);

				int nx,ny,nz;

#if 0
				transform(p->x,p->y+10,p->z,&nx,&ny,&nz);
				aalineColor(screen,x,y,nx+sw()/2,ny+sh()/2,0xFF0000FF);

				transform(p->x+10,p->y,p->z,&nx,&ny,&nz);
				aalineColor(screen,x,y,nx+sw()/2,ny+sh()/2,0x00FF00FF);

				transform(p->x,p->y,p->z+10,&nx,&ny,&nz);
				aalineColor(screen,x,y,nx+sw()/2,ny+sh()/2,0x0000FFFF);
#endif

				if(mx||my||mz) {
					transform(p->x+mx,p->y+my,p->z+mz,&nx,&ny,&nz);
					int rx=nx+sw()/2,ry=ny+sh()/2;

					SDL_Rect ds={nx+sw()/2-2,ny+sh()/2-2,5,5};
					SDL_FillRect(screen,&ds,0xFF0000);
					aalineColor(screen,x,y,nx+sw()/2,ny+sh()/2,0xAAAAAAFF);

					transform(p->x+mx,p->y,p->z,&nx,&ny,&nz);
					aalineColor(screen,x,y,nx+sw()/2,ny+sh()/2,0xFF0000FF);

					transform(p->x,p->y+my,p->z,&nx,&ny,&nz);
					aalineColor(screen,x,y,nx+sw()/2,ny+sh()/2,0x00FF00FF);

					transform(p->x,p->y,p->z+mz,&nx,&ny,&nz);
					aalineColor(screen,x,y,nx+sw()/2,ny+sh()/2,0x0000FFFF);
				}
			}
		}
		SDL_Flip(screen);

	}
end:	SDL_Quit();
}

