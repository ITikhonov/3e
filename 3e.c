#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_gfxPrimitives.h>

#include <math.h>
#include <limits.h>

#include "monkey.h"

SDL_Surface *screen;

struct point {
	int x,y,z,sx,sy,sz,sel;
} point[10240];

int pointn=0;
int minz=INT_MAX,maxz=INT_MIN;

float rot_y=0;
float rot_x=0;
float scale=1;

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
	for(i=0;i<1968;i++) {
		struct point *p=point+pointn++;
		p->x=monkey[i][0]*100;
		p->y=-monkey[i][1]*100;
		p->z=monkey[i][2]*100;
	}

	for(;;) {
		SDL_Event e;
		while(SDL_PollEvent(&e)) {
			if(e.type==SDL_QUIT) goto end;
			if(e.type==SDL_KEYDOWN) {
				if(e.key.keysym.sym==SDLK_q) goto end;
				if(e.key.keysym.sym==SDLK_RETURN) {
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
						select_point(e.button.x-sw()/2,e.button.y-sh()/2);
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

		
		int minz1=minz,spanz=maxz-minz;

		maxz=INT_MIN; minz=INT_MAX;
		SDL_FillRect(screen,0,0xffffff);
		int i=0;
		for(;i<pointn;i++) {
			struct point *p=point+i;

			int x,y,z; transform(p->x,p->y,p->z,&x,&y,&z);

			p->sx=x; p->sy=y; p->sz=z;
			if(z<minz) minz=z; if(z>maxz) maxz=z;

			float c=(z-minz1)/(float)spanz;
			unsigned int color=0xff-0xff*c;
			color=(color<<16)|(color<<8)|color;

			x+=sw()/2;
			y+=sh()/2;
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

