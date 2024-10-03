#include "sdl_utils.h"

/*
 * TODO: Make my own variadic functions
 * -> PrintRect("%r\n", SDL_Rect);
 * -> PrintRect("%rf\n", SDL_FRect);
 */
void
PrintRect(void *rect)
{
	SDL_FRect r = *(SDL_FRect *)rect;
	printf("x: %f\t", r.x);
	printf("y: %f\t", r.y);
	printf("w: %f\t", r.w);
	printf("h: %f\n", r.h);
}

void
RenderDrawRectColor(SDL_Renderer *r, SDL_Rect *rect, SDL_Color c)
{
	SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
	SDL_RenderDrawRect(r, rect);
}

void
RenderDrawRectColorFill(SDL_Renderer *r, SDL_Rect *rect, SDL_Color c)
{
	SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
	SDL_RenderFillRect(r, rect);
}
