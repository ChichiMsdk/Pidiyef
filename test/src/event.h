#ifndef EVENT_H
#define EVENT_H

#include <SDL2/SDL_events.h>

typedef enum DIRECTION
{
	NEXT_P = 0,
	BACK_P = 1
}DIRECTION;

void	Event(SDL_Event *e);
void	ChangePage(DIRECTION direction);

#endif //EVENT_H
