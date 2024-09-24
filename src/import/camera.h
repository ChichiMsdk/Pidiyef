#ifndef CAMERA_H
#define CAMERA_H

#include <SDL2/SDL_events.h>

extern float foffsetx;
extern float foffsety;
extern float foffset2x;
extern float foffset2y;

extern float fstartPanx;
extern float fstartPany;
extern float fstartmovex;
extern float fstartmovey;

extern float fscalex;
extern float fscaley;

typedef struct Camera2D
{
}Camera2D;

void	world_to_screen(float fworldx, float fworldy, int *sx, int *sy);
void	screen_to_world(int sx, int sy, float *fworldx, float *fworldy);
void	pan_world(void);
void	pan_object(int *x, int *y);
void	zoom_world(SDL_MouseWheelEvent wheel);

#endif
