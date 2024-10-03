#ifndef SDL_UTILS_H
#define SDL_UTILS_H

#include <SDL2/SDL.h>
#include <stdio.h>

void	PrintRect(void *rect);
void	RenderDrawRectColor(SDL_Renderer *r, SDL_Rect *rect, SDL_Color c);
void	RenderDrawRectColorFill(SDL_Renderer *r, SDL_Rect *rect, SDL_Color c);

#endif // SDL_UTILS_H

