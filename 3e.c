#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

#include <math.h>

#include "monkey.h"

SDL_Surface *screen;

struct point {
	int x,y,z;
} point[10240];

int pointn=0;

float rot_y=0;
float rot_x=0;
float scale=1;

int sw() { return 800; }
int sh() { return 600; }


void rot(int x0, int y0, int *x, int *y, float a) {
	*x=x0*cos(a)+y0*sin(a);
	*y=-x0*sin(a)+y0*cos(a);
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
			}
			if(e.type==SDL_MOUSEBUTTONDOWN) {
				if(e.button.button==SDL_BUTTON_LEFT) {
					struct point *p=point+pointn++;

					float x0 = e.button.x-sw()/2;
					float y0 = e.button.y-sh()/2;
					x0/=scale;
					y0/=scale;

					rot(y0,0,&p->y,&p->z,-rot_x);
					rot(x0,p->z,&p->x,&p->z,-rot_y);
				}
				if(e.button.button==SDL_BUTTON_WHEELDOWN) { scale/=1.5; }
				if(e.button.button==SDL_BUTTON_WHEELUP) { scale*=1.5; }
				
			}
			if(e.type==SDL_MOUSEMOTION) {
				if(e.motion.state&SDL_BUTTON(2)) {
					rot_y+=(((float)e.motion.xrel)/(sw()/4))*M_PI;
					rot_x+=(((float)e.motion.yrel)/(sh()/4))*M_PI;
				}
			}
		}

		SDL_FillRect(screen,0,0xffffff);
		int i=0;
		for(;i<pointn;i++) {
			struct point *p=point+i;

			int x,y=p->y,z;
			rot(p->x,p->z,&x,&z,rot_y);
			rot(y,z,&y,&z,rot_x);

			x*=scale;
			y*=scale;

			SDL_Rect dh={x+sw()/2,y+sh()/2-2,1,5};
			SDL_FillRect(screen,&dh,0x000000);
			SDL_Rect dv={x+sw()/2-2,y+sh()/2,5,1};
			SDL_FillRect(screen,&dv,0x000000);
		}
		SDL_Flip(screen);

	}
end:	SDL_Quit();
}

