#include "buttons.h"
#include "camera.h"

Mouse_state
get_mouse_state(void)
{
	int x, y;
	Mouse_state mouse = {0};
	uint32_t flags = SDL_GetMouseState(&x, &y);
	mouse.pos = vec2i(x, y);
	mouse.flags = flags;
	return mouse;
}
