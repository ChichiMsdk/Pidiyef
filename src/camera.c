#include "camera.h"
#include "buttons.h"


float foffsetx = 0.0f;
float foffsety = 0.0f;
float foffset2x = 0.0f;
float foffset2y = 0.0f;

float fstartPanx = 0.0f;
float fstartPany = 0.0f;
float fstartmovex = 0.0f;
float fstartmovey = 0.0f;

float fscalex = 1.0f;
float fscaley = 1.0f;

void	
world_to_screen(float fworldx, float fworldy, int *sx, int *sy)
{
	// note: cast for compilers ?

	*sx = (int)((fworldx - foffsetx) * fscalex);
	*sy = (int)((fworldy - foffsety) * fscaley);
}

void
screen_to_world(int sx, int sy, float *fworldx, float *fworldy)
{
	*fworldx = (int)(sx) / fscalex + foffsetx;
	*fworldy = (int)(sy) / fscaley + foffsety;
}

extern float gZoom;
#include <stdio.h>
void
zoom_world(SDL_MouseWheelEvent wheel)
{
	float mousewX_before, mousewY_before;
	Mouse_state m = get_mouse_state();
	screen_to_world(m.pos.x, m.pos.y, &mousewX_before, &mousewY_before);

	if (wheel.y > 0)
	{
		fscalex *= 1.1f;
		fscaley *= 1.1f;
	}
	else if (wheel.y < 0)
	{
		fscalex *= 0.9f;
		fscaley *= 0.9f;
	}

	float mousewX_after, mousewY_after;
	screen_to_world(m.pos.x, m.pos.y, &mousewX_after, &mousewY_after);
	foffsetx += (mousewX_before - mousewX_after);
	foffsety += (mousewY_before - mousewY_after);
}

void
pan_object(int *x, int *y)
{
	Mouse_state m = get_mouse_state();
	float mx, my;
	screen_to_world(m.pos.x, m.pos.y, &mx, &my);
	foffset2x -= (mx - fstartmovex);
	foffset2y -= (my - fstartmovey);
    /*
	 * foffset2x -= (m.pos.x - fstartmovex) / fscalex;
	 * foffset2y -= (m.pos.y - fstartmovey) / fscaley;
     */
	fstartmovex = mx;
	fstartmovey = my;
	*x = -foffset2x;
	*y = -foffset2y;
}

void
pan_world(void)
{
	Mouse_state mouse = get_mouse_state();
	foffsetx -= (mouse.pos.x - fstartPanx) / fscalex;
	foffsety -= (mouse.pos.y - fstartPany) / fscaley;
	fstartPanx = mouse.pos.x;
	fstartPany = mouse.pos.y;
}
