#include "init.h"
#include "utils.h"
#include "sdl_utils.h"

#include "engine/pdf.h"
#include "import/gui.h"
#include "platform/os_threads.h"
#include "import/containers.h"
#include "import/camera.h"
#include "import/buttons.h"
#include "colors.h"

#include <tracy/TracyC.h>

#include <stdio.h>
#include <stdbool.h>

void 		AppUpdate(void (*Update)(SDL_Event *e), SDL_Event *e);
void 		AppQuit();
void 		Version1(SDL_Event *e);
void 		Version2(SDL_Event *e);
static inline void checkValue(sArray *Array, sArray *tmps);
void DrawPages(SDL_Renderer *r, sArray ArrayPage, SDL_FRect rect, Canvas canvas);

fz_pixmap	*RenderPdfPage(fz_context *pCtx, const char *pFile, sInfo *sInfo, fz_document *pDoc);
Canvas gCanvas = {0};
float gScale = 1.0f;

Instance gInst = {.running = true, .width = 1300, .height = 900, .pWin = NULL, .pMutexes = NULL};

PDFView gView3 = {0};
SDL_FRect gView = {0};

PDFContext gPdf = {0};
float gZoom = 1.0f;


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
	static float temp;
	sArray Array = {0};
	int i = 0;
	int j = 0;
	assert(pageHeight > 0);
	int gap = 20;
	float start = abs(canvas.y) / (float)((pageHeight) + gap);
    /*
	 * printf("canvas->h: %d\n", canvas->h);
	 * printf("pPages[%zu].position.y: %f\n", gPdf.nbOfPages - 1, gPdf.pPages[gPdf.nbOfPages-1].position.h);
     */
	if (start > 0)
		start-= gScale;
	if (temp != start) { printf("temp \t%d / %d = %f\n", abs(canvas.y), pageHeight, start); temp = start; }

	for (i = start; i < start + MAX_VISIBLE_PAGES && i < gPdf.nbOfPages && j < MAX_VISIBLE_PAGES; i++, j++)
	{
		if (((gPdf.pPages[i].position.y * gScale) + canvas.y) > gInst.height)
		{
			break;
		}
		Array.pArray[j] = i;
	}
	Array.size = j;
	for (int z = 0; z < Array.size; z++)
	{
		if (Array.pArray[z] < Array.pArray[0])
		{
			printf("pArray[%d]: %d < %d\n", z, Array.pArray[z], Array.pArray[0]);
			printf("Array: size %d\n", Array.size);
			printf(" i: %d + 20 = %d\n\n", i, i + 20);
		}
	}
	return Array;
}
/*
 * TODO: Probably should use a hashmap here.
 * But must measure the speed compared to linear lookup
 */
int
UpdatePageMap(PDFPage *pPage, int pageNb, int fstPage)
{
	int index;
	TextureMap *pMap = gInst.pTextureMap;
	for (index = 0; index < MAX_VISIBLE_PAGES; index++)
	{
		// NOTE: Very unsure of this condition
		if ((pMap[index].pageIndex < fstPage
				|| pMap[index].pageIndex >= fstPage + MAX_VISIBLE_PAGES))
		{
			SDL_DestroyTexture(pMap[index].pTexture);
			pMap[index].pTexture = NULL;
			if (pMap[index].pageIndex >= 0)
				gPdf.pPages[pMap[index].pageIndex].bTextureCache = false;

			if (pPage->pPix)
			{
				pMap[index].pTexture = LoadTextures(gInst.pRenderer, pPage->pPix, gPdf.pCtx, SDL_TEXTUREACCESS_STREAMING);
				pMap[index].pageIndex = pageNb; 
			}
			/* assert(pMap[index].pTexture); // maybe just log instead of crash */
			return index;
		}
	}
	printf("pageNb: %d fstPage: %d\n", pageNb, fstPage);
	for(int i = 0; i < MAX_VISIBLE_PAGES; i++)
		printf("pMap[%d]: %d\n", i, pMap[i].pageIndex);
	return -1;
}

static inline int
GetTextureIndex(int pageNb)
{
	int index;
	for (index = 0; index < MAX_VISIBLE_PAGES; index++)
	{
		if (gInst.pTextureMap[index].pageIndex == pageNb)
			return index;
	}
	return -1;
}

void
RenderPage(SDL_Renderer *r, SDL_FRect rect, Canvas canvas, PDFPage *pPage, int pageNb, int fstPage)
{
	bool invalid = false;
	int index = -1;
	if (pPage->bTextureCache == false)
	{
		UpdatePageMap(pPage, pageNb, fstPage);
	}
	index = GetTextureIndex(pageNb);
	rect.y = (pPage->position.y * gScale) + canvas.y;
	rect.x = canvas.x;
	SDL_Rect tmp = {.x = rect.x, .y = rect.y, .w = rect.w, .h = rect.h};
	if (index != -1)
	{
		if (!PixmapToTexture(r, pPage->pPix, gPdf.pCtx, gInst.pTextureMap[index].pTexture))
			invalid = true;
		SDL_RenderCopyF(gInst.pRenderer, gInst.pTextureMap[index].pTexture, NULL, &rect);
	}
	else
	{
		RenderDrawRectColorFill(gInst.pRenderer, &tmp, SACRAMENTO_GREEN);
	}

	/* SDL_RenderCopyF(gInst.pRenderer, gInst.pMainTexture, NULL, &rect); */
	/*
	 * fz_drop_pixmap(gPdf.pCtx, gPdf.pPages[arr[i]].pPix);
	 * gPdf.pPages[arr[i]].pPix = NULL;
	 */
}

int PixMapFromThreads(fz_context *pCtx, const char *pFile, sInfo sInfo, fz_document *pDoc);
void
DrawPage(SDL_Renderer *r, SDL_FRect rect, Canvas canvas, PDFPage *pPage, int pageNb, int fstPage, sInfo sInfo)
{
	/* sInfo sInfo = {.fRotate = 0.0f, .fDpi = 72.0f, .fZoom = 100.0f, .pageStart = pageNb}; */
		/*
		 * if (pPage->bPpmCache == false)
		 * {
		 * 	pPage->pPix = CreatePDFPage(gPdf.pCtx, gPdf.pFile, &sInfo);
		 * 	pPage->bPpmCache = true;
		 * }
		 * if (!gInst.pMainTexture)
		 * 	gInst.pMainTexture = LoadTextures(r, pPage->pPix, gPdf.pCtx, SDL_TEXTUREACCESS_STREAMING);
		 * gInst.pMainTexture = PixmapToTexture(r, pPage->pPix, gPdf.pCtx, gInst.pMainTexture);
		 */

    /*
	 * if (pPage->bPpmCache == false)
	 * {
	 * 	// TODO: multi-thread this part (OPENGL's limitations)
	 * 	pPage->pPix = CreatePDFPage(gPdf.pCtx, gPdf.pFile, &sInfo);
	 * 	pPage->bPpmCache = true;
	 * }
     */
	PixMapFromThreads(gPdf.pCtx, gPdf.pFile, sInfo, gPdf.pDoc);
	for (int i = 0; i < sInfo.nbrPages; i++)
	{
		gPdf.pPages[sInfo.pageIndex[i]].bPpmCache = true;
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
	/* rTextureDimensions.x *= gScale; */
	rTextureDimensions.y -= abs(canvas.y);

	sArray Array = GetVisiblePages(canvas, rTextureDimensions.h);
	/* RenderDrawRectColorFill(gInst.pRenderer, &rect, canvas.color); */
	/* DrawPages(gInst.pRenderer, Array, rTextureDimensions, canvas); */
	sInfo sInfo = {0};
	int *arr = Array.pArray;
	for (int i = 0; i < Array.size; i++)
	{
		if (gPdf.pPages[arr[i]].bPpmCache == false)
		{
			sInfo.pageIndex[sInfo.nbrPages] = arr[i];
			sInfo.nbrPages++;
		}
	}
	DrawPage(gInst.pRenderer, rTextureDimensions, canvas, &gPdf.pPages[arr[0]], arr[0], arr[0], sInfo);
	for (int i = 0; i < Array.size; i++)
		RenderPage(gInst.pRenderer, rTextureDimensions, canvas, &gPdf.pPages[arr[i]], arr[i], arr[0]);
    /*
	 * for (int i = 0; i < Array.size; i++)
	 * 	DrawPage(gInst.pRenderer, rTextureDimensions, canvas, &gPdf.pPages[arr[i]], arr[i], arr[0]);
     */

	/* if (!SDL_FRectEquals(&rTextureDimensions, &tmp)){ PrintRect(&rTextureDimensions); tmp = rTextureDimensions; } */
}

const char* __asan_default_options() { return "detect_leaks=0"; }

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

void
AppUpdate(void(*Update)(SDL_Event *e), SDL_Event *e)
{
	Update(e);
	/* SDL_Delay(16); */
}

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

static inline void
checkValue(sArray *Array, sArray *tmps)
{
	if (!ArrayEquals(Array, tmps))
	{
        /*
		 * for (int i = 0; i < Array.size; i++) 
		 *    printf("Array[%d]: %d\ttmps[%d]: %d\n", i, Array.pArray[i], i, tmps.pArray[i]);
         */
		memcpy(tmps->pArray, Array->pArray, Array->size * sizeof(int));
		tmps->size = Array->size;
	}
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
