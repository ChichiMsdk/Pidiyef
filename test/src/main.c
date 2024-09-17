#include "init.h"
#include "engine/pdf.h"
#include "gui.h"
#include "platform/os_threads.h"
#include "containers.h"
#include "camera.h"
#include "buttons.h"

#include <tracy/TracyC.h>

#include <stdio.h>
#include <stdbool.h>

static inline void UpdateSmooth(float factor);

extern int gRender;
extern Instance gInst;
Instance gInst = {.running = true, .width = 1000, .height = 700, .pWin = NULL, .pMutexes = NULL};
int gRender = false;
EventQueue gEventQueue = {0};
PDFView gView = {0};
PDFContext gPdf = {0};
int mode = 0;
int z = 0;
int old = 0;
int count = 0;
uint64_t jump = 0;
uint64_t oldJump = 0;
float max = 0;
int incr = -900;
bool stop = false;
double timing = 0.1;
bool redraw = false;
int num = 0;

#define MAX_TEXTURE_ARRAY_SIZE 10

typedef struct TextureArray
{
	SDL_Texture *ppTexture[MAX_TEXTURE_ARRAY_SIZE];
	SDL_Rect r;
	SDL_FRect fr;
	size_t first;
	size_t last;
	size_t currentSize;
}TextureArray;

typedef struct page
{
	SDL_Rect r;
	SDL_FRect fR;
	SDL_Texture *pT;
	SDL_Color c;
	SDL_FRect views[3];
	bool isCached;
}page;

SDL_Texture* 
LoadTextures(SDL_Renderer *pRenderer, fz_pixmap *pPix, fz_context *pCtx, int textureFormat);
void ReloadPage(void);
fz_pixmap * CreatePDFPage(fz_context *pCtx, const char *pFile, sInfo *sInfo);
static inline void UpdateSmooth2(float factor, int i, page **p);
void Warmup(page **p);
void idiot(void);

SDL_FRect gview = {0};
void
event(SDL_Event *es)
{
	SDL_Event e = *es;
	SDL_PollEvent(&e); 
	const uint8_t *arr = SDL_GetKeyboardState(&num);

	if (e.type == SDL_QUIT) exit(1); 
	else if (e.type == SDL_WINDOWEVENT_RESIZED || e.type == SDL_WINDOWEVENT_SIZE_CHANGED) { SDL_GetWindowSize(gInst.pWin, &gInst.width, &gInst.height); printf("%d\t%d\n", gInst.width, gInst.height); }
	else if (e.type == SDL_KEYDOWN)
	{ 
		if (arr[SDL_SCANCODE_LCTRL] && arr[SDL_SCANCODE_O]) { int tmp = gPdf.viewingPage; gPdf.viewingPage = oldJump; oldJump = tmp; redraw = true; }
		else if (arr[SDL_SCANCODE_LSHIFT] && arr[SDL_SCANCODE_K]) { fscalex /= 1.1f; if (gview.w <= 0.1f || gview.h <= 0.1f) gview.w = gInst.width; gview.h = gInst.height;}
		else if (arr[SDL_SCANCODE_LSHIFT] && arr[SDL_SCANCODE_J]) { fscalex *= 1.1f; if (gview.w >= 100000.0f || gview.h <= 100000.0f) gview.w = gInst.width; gview.h = gInst.height;}
		else if (e.key.keysym.sym == SDLK_ESCAPE) { exit(1);}
		else if (e.key.keysym.sym == SDLK_k) { gview.y += (300 / fscalex); if (gview.y >= 4000000000) gview.y = 0.0f;}
		else if (e.key.keysym.sym == SDLK_j) { gview.y -= (300 / fscalex); if (gview.y < 0.0f) gview.y = 0.0f;}
		else if (e.key.keysym.sym == SDLK_UP) { incr = 900; printf("Down\n");}
		else if (e.key.keysym.sym == SDLK_DOWN) { incr = -900; printf("Up\n");}
		else if (e.key.keysym.sym == SDLK_SPACE) { stop = !stop; printf("stop is %d\n", stop);}
		else if (e.key.keysym.sym == SDLK_RIGHT) { timing -= 0.1; if (timing <= 0) timing = 1; printf("%f\n", timing);}
		else if (e.key.keysym.sym == SDLK_LEFT) { timing += 0.1; if (timing >= 1000) timing = 1000; printf("%f\n", timing);}
		else if (e.key.keysym.sym == SDLK_SEMICOLON && e.key.keysym.sym == SDLK_LSHIFT) { mode = 2; }
		switch (e.key.keysym.mod) { case (true): if (e.key.keysym.sym == SDLK_SEMICOLON) { mode = 2; } }
		if (mode == 2)
		{
			switch (e.key.keysym.sym)
			{
				case (SDLK_0): jump = jump * 10 + 0; printf("\r:%zu", jump); if (jump >= 1000000) jump = 0; break;
				case (SDLK_1): jump = jump * 10 + 1; printf("\r:%zu", jump); if (jump >= 1000000) jump = 0; break;
				case (SDLK_2): jump = jump * 10 + 2; printf("\r:%zu", jump); if (jump >= 1000000) jump = 0; break;
				case (SDLK_3): jump = jump * 10 + 3; printf("\r:%zu", jump); if (jump >= 1000000) jump = 0; break;
				case (SDLK_4): jump = jump * 10 + 4; printf("\r:%zu", jump); if (jump >= 1000000) jump = 0; break;
				case (SDLK_5): jump = jump * 10 + 5; printf("\r:%zu", jump); if (jump >= 1000000) jump = 0; break;
				case (SDLK_6): jump = jump * 10 + 6; printf("\r:%zu", jump); if (jump >= 1000000) jump = 0; break;
				case (SDLK_7): jump = jump * 10 + 7; printf("\r:%zu", jump); if (jump >= 1000000) jump = 0; break;
				case (SDLK_8): jump = jump * 10 + 8; printf("\r:%zu", jump); if (jump >= 1000000) jump = 0; break;
				case (SDLK_9): jump = jump * 10 + 9; printf("\r:%zu", jump); if (jump >= 1000000) jump = 0; break;
				case (SDLK_RETURN):
							   mode = 0;
							   printf("\rjumping to: %zu\n", jump);
							   if (jump < gPdf.nbOfPages)
							   {
								   oldJump = gPdf.viewingPage;
								   gPdf.viewingPage = jump;
								   redraw = true;
							   }
							   jump = 0;
							   break;
			}
		}
	}
	else if (e.type == SDL_MOUSEWHEEL)
	{
		// the max nbpage * height + gap
        /*
		 * if (e.wheel.y > 0) { gview.y -= 50; if (gview.y < 0.0f) gview.y = 0.0f;}
		 * else if (e.wheel.y < 0) { gview.y += 50; if (gview.y > 4000000000.0f) gview.y = 0.0f;} 
         */
		zoom_world(e.wheel);
	}
}

typedef struct visible
{
	int array[20];
	int count;
}visible;

visible gvis = {0};

int
main(int Argc, char **ppArgv)
{
	Init(Argc, ppArgv, &gInst);

	SDL_Event e;

	int err = 0;
	float factor = 0.4f;
	gInst.lastPoll = GetCounter();
	uint64_t timer = GetCounter();

	float width = 210;
	float height = 297;
	float ratio = 1 / 1.41;
	float gap = 20;

	int TOTAL = 500000;

	page *p = malloc(sizeof(page)*TOTAL);
	SDL_FRect ph = {0};

	srand(GetCounter());
	for (int i = 0; i < TOTAL; i++)
	{
		p[i].c.r = rand() % 255 + 1;
		p[i].c.g = rand() % 255;
		p[i].c.b = rand() % 255;
		p[i].c.a = rand() % 255 + 180;
		p[i].fR.h = height; p[i].fR.w = width;
		p[i].fR.x = (gInst.width / 2.0f) - (p[i].fR.w / 2);
		p[i].fR.y = (i  * (height + gap));
		p[i].views[0] = p[i].fR;
		p[i].views[1] = p[i].fR;
		p[i].views[2] = p[i].fR;
	}

	int current = 0;

	struct TextureArray ta = {0};

	gview.w = gInst.width;
	gview.h = gInst.height;

	uint64_t startTime = GetCounter();
	uint64_t startTime2 = GetCounter();
	int guess = 0;
	int i = 0;
	int old = 0;
	float offset = 0;
	float offset2 = 0;
	while(gInst.running)
	{
		event(&e);


		SDL_SetRenderDrawColor(gInst.pRenderer, 0x00, 0x00, 0x00, 0xFF);
		/* SDL_SetRenderDrawColor(gInst.pRenderer, 0xFF, 0xFF, 0xFF, 0xFF); */
		SDL_RenderClear(gInst.pRenderer);

        /*
		 * idiot();
		 * SDL_RenderPresent(gInst.pRenderer);
		 * continue;
         */

   		gvis.count = 0;
		int count = 0;
		guess = (gview.y / (height + gap));

		if (old != guess)
		{
			old = guess;
			printf("Page: %d\n", guess);
		}
		guess--;
		offset = 0;
		offset2 = 0;
		TracyCZoneNC(ctx, "Loop", 0x00FFFF, 1);
		for(i = 0; i < TOTAL; i++)
		{
			int rx = 0, ry = 0; 
			int rw = p[i].fR.w, rh = p[i].fR.h; 

			world_to_screen(p[i].views[0].x, p[i].views[0].y, &rx, &ry);

			if (i == 0 && ry > 0)
			{
				offset = ry - 0;
				offset2 = p[i].views[2].y - 0;
			}

			/* ME = GENIUS (NO) */
			p[i].views[2].x = p[i].fR.x - (gview.x);
			p[i].views[2].y = p[i].fR.y - (gview.y) - offset2;

			SDL_FRect tmp = {
				.x = (gInst.width / 2.0f) - (p[i].fR.w * fscalex / 2),
				.y = ry - offset,
				.w = p[i].fR.w * fscalex,
				.h = p[i].fR.h * fscalex 
			};

			if (tmp.y <= gInst.height && tmp.y + tmp.h >= 0)
			{
				gvis.array[gvis.count] = i;
				gvis.count++;
				SDL_SetRenderDrawColor(gInst.pRenderer, p[i].c.r, p[i].c.g, p[i].c.b, 0xFF);
				TracyCZoneNC(ctx1, "FillRect", 0x00FF00, 1);
				SDL_RenderFillRectF(gInst.pRenderer, &tmp);
				TracyCZoneEnd(ctx1);
				UpdateSmooth2(factor, i, &p);
			} 
			else
			{
				p[i].views[0].x = p[i].fR.x - gview.x;
				p[i].views[0].y = p[i].fR.y - gview.y;
				UpdateSmooth2(factor, i, &p);
				if (gvis.count >= 4 || gvis.count >= 3)
					break;
			}
		}
		TracyCZoneEnd(ctx);
		double elapsed = GetElapsed(GetCounter(), startTime2);
		if (elapsed >= timing)
		{
			/* printf("count: %d\n", count); */
			startTime = GetCounter();
			z += incr;
		} 
		count = 0;
		SDL_RenderPresent(gInst.pRenderer);
	}
	SDL_DestroyRenderer(gInst.pRenderer);
	SDL_DestroyWindow(gInst.pWin);
	SDL_Quit();
	return 0;
}

static inline void
UpdateSmooth2(float factor, int i, page **p)
{
	(*p)[i].views[0].w
		= mInterpolate2((*p)[i].views[1].w, (*p)[i].views[2].w, factor);
	(*p)[i].views[0].x
		= mInterpolate2((*p)[i].views[1].x, (*p)[i].views[2].x, factor);
	(*p)[i].views[0].h
		= mInterpolate2((*p)[i].views[1].h, (*p)[i].views[2].h, factor);
	(*p)[i].views[0].y
		= mInterpolate2((*p)[i].views[1].y, (*p)[i].views[2].y, factor);
	(*p)[i].views[1].x = (*p)[i].views[0].x; (*p)[i].views[1].y = (*p)[i].views[0].y;
	(*p)[i].views[1].w = (*p)[i].views[0].w; (*p)[i].views[1].h = (*p)[i].views[0].h;
}

static inline void
UpdateSmooth(float factor)
{
	gView.currentView.x = mInterpolate2(gView.oldView.x, gView.nextView.x, factor);
	gView.currentView.y = mInterpolate2(gView.oldView.y, gView.nextView.y, factor);
	gView.currentView.w = mInterpolate2(gView.oldView.w, gView.nextView.w, factor);
	gView.currentView.h = mInterpolate2(gView.oldView.h, gView.nextView.h, factor);
	gView.oldView.x = gView.currentView.x; gView.oldView.y = gView.currentView.y;
	gView.oldView.w = gView.currentView.w; gView.oldView.h = gView.currentView.h;
}

void
Warmup(page **pag)
{
	page *p = *pag;
	uint64_t startTime2 = GetCounter();
	uint64_t startTime3 = GetCounter();
	SDL_FRect fake = gview;
	float factor = 0.4f;
	int guess = 0;
	int counting = 0;

	while (1)
	{
		counting++;
		guess = (fake.y / (297 + 20));
		if (guess >= 4999)
			return;

		for(int i = guess; i < 5000; i++)
		{
			p[i].views[2].x = p[i].fR.x - gview.x;
			p[i].views[2].y = p[i].fR.y - gview.y;
			SDL_FRect tmp = {
				.x = p[i].views[0].x,
				.y = p[i].views[0].y,
				.w = p[i].fR.w,
				.h = p[i].fR.h
			};
			if (tmp.y <= gInst.height && tmp.y + tmp.h >= 0)
			{
				UpdateSmooth2(factor, i, &p);
				continue;
			} 
			for (int j = 0; j < gvis.count; j++)
			{
				if (gvis.array[j] == i)
				{
					goto a;
				}
			}
			memset(gvis.array, 0, gvis.count);
			/* printf("going out !\n"); */
			goto b;
a:
			UpdateSmooth2(factor, i, &p);
			continue;
b:
			UpdateSmooth2(factor, i, &p);
			/* printf("i:%d\n", i); */
			break;
		}
		double elapsed = GetElapsed(GetCounter(), startTime2);
		double elapsed2 = GetElapsed(GetCounter(), startTime3);
		if (elapsed2 >= 1000)
		{
			printf("counting: %d\n", counting);
			printf("guess: %d\n", guess);
			printf("fake.y: %f\n", fake.y);
			startTime3 = GetCounter();
		}
		if (elapsed >= timing)
		{
			startTime2 = GetCounter();
			fake.y += 100;
		} 
	}
}
