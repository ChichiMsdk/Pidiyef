#ifndef BUTTONS_H
#define BUTTONS_H

#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_mouse.h>

#include <stdbool.h>

#include "vector.h"

typedef struct Mouse_state
{
	Vec2i				pos;
	uint32_t			flags;
	bool				moved;

}Mouse_state;

typedef struct Button
{
	SDL_FRect			rect;
	SDL_Color			color;
	SDL_Color			color_pressed;
	// SDL_Texture			*text[2];
	bool				pressed, released, hovered;
	void				*(*fn)(void *);
	int					count;
}Button;

enum
{
	B_UP = 0,
	B_DOWN = 1
};

Mouse_state		get_mouse_state(void);
void			timeline_mouse_pressed(Mouse_state mouse);
void			timeline_mouse_released(Mouse_state mouse);

#endif //BUTTONS_H
