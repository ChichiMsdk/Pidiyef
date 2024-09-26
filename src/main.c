#include "init.h"
#include "utils.h"

#include "engine/pdf.h"
#include "import/gui.h"
#include "platform/os_threads.h"
#include "import/containers.h"
#include "import/camera.h"
#include "import/buttons.h"

#include <tracy/TracyC.h>

#include <stdio.h>
#include <stdbool.h>

typedef struct Canvas
{
	int x, y;
	int w, h;
	SDL_Color color;
}Canvas;

Canvas gCanvas = {0};

Instance gInst = {.running = true, .width = 1300, .heigth = 900, .pWin = NULL, .pMutexes = NULL};

PDFView gView3 = {0};
SDL_FRect gView = {0};

PDFContext gPdf = {0};
float gZoom = 1.0f;

void AppUpdate(void (*Update)(SDL_Event *e), SDL_Event *e);
void AppQuit();
void Version1(SDL_Event *e);
void Version2(SDL_Event *e);
const char* __asan_default_options() { return "detect_leaks=0"; }

void
UpdateCanvas(Canvas *canvas);

int
main(int Argc, char **ppArgv)
{
	void(*Version[2])(SDL_Event *e) = {Version1, Version2};
	enum { ONE = 0, TWO = 1};
	Init(Argc, ppArgv, &gInst);
	sInfo sInfo = {
		.pageStart = Argc > 2 ? atoi(ppArgv[2]) - 1 : 0,
		.fZoom = Argc > 3 ? atof(ppArgv[3]) : 100,
		.fRotate = Argc > 4 ? atof(ppArgv[4]) : 0
	};

	// Set canvas size relative to nb page + size page
	gCanvas.x = 0; gCanvas.y = 0;
	gCanvas.h = 100; gCanvas.w = 500;
	gCanvas.color = (SDL_Color) {.r = 0x80, .g = 0x80, .b = 0x80, .a = 0xFF};

	int version = ONE;
	CreatePDFContext(&gPdf, ppArgv[1], sInfo);
	SDL_Event e;

	gInst.lastPoll = GetCounter();
	InitView();
	gView = gView3.currentView2[2];
	while(gInst.running)
	{
		AppUpdate(Version[version], &e);
	}
	AppQuit();
	return 0;
}

void
AppUpdate(void(*Update)(SDL_Event *e), SDL_Event *e)
{
	Update(e);
	/* SDL_Delay(16); */
}

void
RenderDrawRectColor(SDL_Renderer *r, SDL_Rect *rect, SDL_Color c)
{
	SDL_SetRenderDrawColor(gInst.pRenderer, 0x80, 0x80, 0x80, 0xFF);
	SDL_RenderDrawRect(gInst.pRenderer, rect);
}

void
RenderDrawRectColorFill(SDL_Renderer *r, SDL_Rect *rect, SDL_Color c)
{
	SDL_SetRenderDrawColor(gInst.pRenderer, 0x80, 0x80, 0x80, 0xFF);
	SDL_RenderFillRect(gInst.pRenderer, rect);
}

void
DrawCanvas(Canvas canvas)
{
	SDL_Rect rect = {
		.x = canvas.x,
		.y = canvas.y,
		.h = canvas.h,
		.w = canvas.w
	};
	RenderDrawRectColorFill(gInst.pRenderer, &rect, canvas.color);
}

void
Version1(SDL_Event *e)
{
	Event(e);
	SDL_SetRenderDrawColor(gInst.pRenderer, 0x00, 0x00, 0x00, 0xFF);
	SDL_RenderClear(gInst.pRenderer);
	DrawCanvas(gCanvas);
	SDL_RenderPresent(gInst.pRenderer);
}

void
Version2(SDL_Event *e)
{
	event(e);
	SDL_SetRenderDrawColor(gInst.pRenderer, 0x00, 0x00, 0x00, 0xFF);
	SDL_RenderClear(gInst.pRenderer);
	MegaLoop();
	SDL_RenderPresent(gInst.pRenderer);
}

void
AppQuit()
{
	printf("Quitting\n");
    /*
	 * Loop through the pPages->pPix;
	 * fz_drop_pixmap(pdf.pCtx, pdf.ppPix[0]);
     */
	fz_drop_document(gPdf.pCtx, gPdf.pDoc);
	fz_drop_context(gPdf.pCtx);
	/*
	 * Loop through the pPages->pTextures;
	 * SDL_DestroyTexture();
	 */
	free(gPdf.pPages);
	free(gPdf.pFzMutexes);
	SDL_DestroyRenderer(gInst.pRenderer);
	SDL_DestroyWindow(gInst.pWin);
	SDL_Quit();
}
