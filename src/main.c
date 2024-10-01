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

#define ROYAL_BLUE (SDL_Color) {.r = 0x41, .g = 0x69, .b = 0xE1, .a = 255}
#define DEEPSEA_BLUE (SDL_Color) {.r = 0x12, .g = 0x34, .b = 0x56, .a = 255}
#define SCARLET_RED (SDL_Color) {.r = 0x90, .g = 0x0D, .b = 0x09, .a = 255}
#define SACRAMENTO_GREEN (SDL_Color) {.r = 0x04, .g = 0x39, .b = 0x27, .a = 255}
#define GRAY (SDL_Color) {.r = 0x80, .g = 0x80, .b = 0x80, .a = 255}

#define MAX_VISIBLE_PAGES 20

struct CanvasInfo
{
	fz_pixmap	*pPixMaps;
	SDL_Texture	*ppTexture;
	int			nbPix;
};

typedef struct sArray
{
	int pArray[MAX_VISIBLE_PAGES];
	int size;
}sArray;

Canvas gCanvas = {0};
float gScale = 1.0f;

Instance gInst = {.running = true, .width = 1300, .height = 900, .pWin = NULL, .pMutexes = NULL};

PDFView gView3 = {0};
SDL_FRect gView = {0};

PDFContext gPdf = {0};
float gZoom = 1.0f;

bool ArrayEquals(sArray *a, sArray *b);
void AppUpdate(void (*Update)(SDL_Event *e), SDL_Event *e);
void AppQuit();
void Version1(SDL_Event *e);
void Version2(SDL_Event *e);
void PrintRect(void *rect);
void RenderDrawRectColor(SDL_Renderer *r, SDL_Rect *rect, SDL_Color c);
void RenderDrawRectColorFill(SDL_Renderer *r, SDL_Rect *rect, SDL_Color c);
const char* __asan_default_options() { return "detect_leaks=0"; }

/*
 * TODO: Update only when condition is met
 */
void
UpdateCanvas(Canvas *canvas, SDL_Color c)
{
	static float old;
	int gap = 20;
	/* gPdf.pPages[gPdf.nbOfPages - 1].position.y); */
	canvas->h = (gPdf.nbOfPages - 1) * (gPdf.pPages[0].position.h + gap);
	canvas->h *= gScale;
	canvas->w = gPdf.pPages[0].position.w;
	canvas->w *= gScale;
	if (old != gScale)
	{
		canvas->x *= gScale;
		canvas->y *= gScale;
		old = gScale;
	}
	// TODO: keep the canvas centered around the mouse when zooming
	/* canvas->x = (gInst.width / 2) - (canvas->w / 2); */
	/* canvas->y = 0; */
	canvas->color = c;
}

sArray
GetVisiblePages(Canvas canvas, int pageHeight)
{
	static int temp;
	sArray Array = {0};
	int i = 0;
	int j = 0;
	assert(pageHeight > 0);
	int gap = 20;
	float start = abs(canvas.y) / (pageHeight + gap);
    /*
	 * printf("canvas->h: %d\n", canvas->h);
	 * printf("pPages[%zu].position.y: %f\n", gPdf.nbOfPages - 1, gPdf.pPages[gPdf.nbOfPages-1].position.h);
     */
	if (start > 0)
		start--;
	if (temp != start) { printf("%d / %d = %f\n", abs(canvas.y), pageHeight, start); temp = start; }
	for (i = start; i < start + MAX_VISIBLE_PAGES && i < gPdf.nbOfPages; i++, j++)
	{
		if (((gPdf.pPages[i].position.y * gScale) + canvas.y) > gInst.height)
			break;
		Array.pArray[j] = i;
	}
	Array.size = j;
	return Array;
}
fz_pixmap *
RenderPdfPage(fz_context *pCtx, const char *pFile, sInfo *sInfo, fz_document *pDoc);

void
DrawPages(SDL_Renderer *r, sArray ArrayPage, SDL_FRect rect, Canvas canvas)
{
	int i = 0;
	int *arr = ArrayPage.pArray;
	for (i = 0; i < ArrayPage.size; i++)
	{
		sInfo sInfo = {.fRotate = 0.0f, .fDpi = 72.0f, .fZoom = 100.0f, .pageStart = arr[i]};
		if (gPdf.pPages[arr[i]].bPpmCache == false)
		{
			gPdf.pPages[arr[i]].pPix = CreatePDFPage(gPdf.pCtx, gPdf.pFile, &sInfo);
			gPdf.pPages[arr[i]].bPpmCache = true;
		}
		if (!gInst.pMainTexture)
			gInst.pMainTexture = LoadTextures(r, gPdf.pPages[arr[i]].pPix, gPdf.pCtx, SDL_TEXTUREACCESS_STREAMING);

		gInst.pMainTexture = PixmapToTexture(r, gPdf.pPages[arr[i]].pPix, gPdf.pCtx, gInst.pMainTexture);

		rect.y = (gPdf.pPages[arr[i]].position.y * gScale) + canvas.y;
		rect.x = canvas.x;
		SDL_RenderCopyF(gInst.pRenderer, gInst.pMainTexture, NULL, &rect);
        /*
		 * fz_drop_pixmap(gPdf.pCtx, gPdf.pPages[arr[i]].pPix);
		 * gPdf.pPages[arr[i]].pPix = NULL;
         */
	}
}

/*
 * TODO: The canvas will use a texture ( one ?) to be drawn at its
 * "visible" coordinates -> texture always same dimensions
 * unless window changes
 */
void
DrawCanvas(Canvas canvas, SDL_Texture *texture, PDFPage *pPage)
{
	static SDL_FRect tmp = {0};
	static sArray tmps = {0};
	SDL_Rect rect = { .x = canvas.x, .y = canvas.y, .h = canvas.h, .w = canvas.w };
	SDL_FRect rTextureDimensions = pPage->position;
	rTextureDimensions.w *= gScale;
	rTextureDimensions.h *= gScale;
	/* rTextureDimensions.x -= abs(canvas.x); */
	rTextureDimensions.y -= abs(canvas.y);
	/* rTextureDimensions.x *= gScale; */

	sArray Array = GetVisiblePages(canvas, rTextureDimensions.h);
	if (!ArrayEquals(&Array, &tmps))
	{
        /*
		 * for (int i = 0; i < Array.size; i++) 
		 *    printf("Array[%d]: %d\ttmps[%d]: %d\n", i, Array.pArray[i], i, tmps.pArray[i]);
         */
		memcpy(tmps.pArray, Array.pArray, Array.size * sizeof(int));
		tmps.size = Array.size;
	}
	RenderDrawRectColorFill(gInst.pRenderer, &rect, canvas.color);
	DrawPages(gInst.pRenderer, Array, rTextureDimensions, canvas);

	/* if (!SDL_FRectEquals(&rTextureDimensions, &tmp)){ PrintRect(&rTextureDimensions); tmp = rTextureDimensions; } */
}

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

	int version = ONE;
	uint64_t elapsed;
	gInst.lastPoll = GetCounter();
	/* printf("time: %zu\n", elapsed = GetElapsed(GetCounter(), gInst.lastPoll)); */

	if (!CreatePDFContext(&gPdf, ppArgv[1], sInfo))
	{fprintf(stderr, "PdfContext could not be created\n"); exit(EXIT_FAILURE);}

	SDL_Event e;

	InitView();
	gView = gView3.currentView2[2];
	while(gInst.running)
	{
		AppUpdate(Version[version], &e);
		SDL_Delay(16);
	}
	AppQuit();
	return 0;
}

void
Version1(SDL_Event *e)
{
	Event(e);
	SDL_SetRenderDrawColor(gInst.pRenderer, 0x00, 0x00, 0x00, 0xFF);
	SDL_RenderClear(gInst.pRenderer);
	UpdateCanvas(&gCanvas, GRAY);
	DrawCanvas(gCanvas, NULL, &gPdf.pPages[0]);
	SDL_RenderPresent(gInst.pRenderer);
}

bool
ArrayEquals(sArray *a, sArray *b)
{
	if (a->size != b->size)
		return false;
	for (int i = 0; i < a->size && i < b->size; i++)
	{
		if (a->pArray[i] != b->pArray[i])
			return false;
	}
	return true;
}
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
AppUpdate(void(*Update)(SDL_Event *e), SDL_Event *e)
{
	Update(e);
	/* SDL_Delay(16); */
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
