#include "init.h"
#include "pdf.h"
#include "gui.h"

#include <tracy/TracyC.h>

#include <stdio.h>
#include <stdbool.h>

#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define mIsSame(a, b) ((a.x == b.x && a.y == b.y) ? 1 : 0)


extern Instance gInst;
Instance gInst = {.running = true, .width = 1920, .height = 1080};

PDFView gView = {0};

SDL_Texture* PixmapToTexture(SDL_Renderer *pRenderer, fz_pixmap *pPix, fz_context *pCtx) ;

int
main(int argc, char **ppArgv)
{
	Init(argc, ppArgv, &gInst);
	int page_number = atoi(ppArgv[2]) - 1;
	int zoom = argc > 3 ? atof(ppArgv[3]) : 100;
	int rotate = argc > 4 ? atof(ppArgv[4]) : 0;

	// Wrong naming and design, pdf.ppPix is allocated
	PDF pdf = CreatePDF(ppArgv[1], page_number, zoom, rotate);
	if (!pdf.ppPix){ return 1; }

	pdf.pTexture = PixmapToTexture(gInst.pRenderer, pdf.ppPix[0], pdf.pCtx);
	if (!pdf.pTexture) { fprintf(stderr, "Failed: PixMapToTexture: %s\n", SDL_GetError()); return 1; }

	int w = 0, h = 0; 
	SDL_QueryTexture(pdf.pTexture, NULL, NULL, &w, &h);
	gView.currentView.w = w;
	gView.currentView.h = h;

	gView.currentView.x = gInst.width / 2.0f - gView.currentView.w / 2.0f;
	gView.currentView.y = gInst.height / 2.0f - gView.currentView.h / 2.0f;

	gView.nextView.x = gView.currentView.x;
	gView.nextView.y = gView.currentView.y;
	gView.nextView.w = gView.currentView.w;
	gView.nextView.h = gView.currentView.h;

	gView.oldView.x = gView.currentView.x;
	gView.oldView.y = gView.currentView.y;
	gView.oldView.w = gView.currentView.w;
	gView.oldView.h = gView.currentView.h;

	SDL_Event e;
	SDL_FRect check = {-1, -1, -1, -1};
	SDL_FRect tmpRect = {
		.x = gInst.width / 2.0f - gView.currentView.w / 2.0f,
		.y = gInst.height / 2.0f - gView.currentView.h / 2.0f,
		.w = 100,
		.h = 100
	};

	int err = 0;
	float factor = 0.4f;
	while(gInst.running)
	{
		Event(&e);
			gView.currentView.x = mInterpolate2(gView.oldView.x, gView.nextView.x, factor);
			gView.currentView.y = mInterpolate2(gView.oldView.y, gView.nextView.y, factor);
			gView.currentView.w = mInterpolate2(gView.oldView.w, gView.nextView.w, factor);
			gView.currentView.h = mInterpolate2(gView.oldView.h, gView.nextView.h, factor);
			gView.oldView.x = gView.currentView.x; gView.oldView.y = gView.currentView.y;
			gView.oldView.w = gView.currentView.w; gView.oldView.h = gView.currentView.h;

		// Check if it moved
		if (!SDL_FRectEquals(&check, &gView.currentView))
		{
			SDL_SetRenderDrawColor(gInst.pRenderer, 0x00, 0x00, 0x00, 0xFF);
			SDL_RenderClear(gInst.pRenderer);

			/* SDL_RenderCopyF(gInst.pRenderer, pdf.pTexture, NULL, &gView.currentView); */
			SDL_RenderCopyF(gInst.pRenderer, pdf.pTexture, NULL, &gView.currentView);

			SDL_RenderPresent(gInst.pRenderer);
			check.x = gView.currentView.x;
			check.y = gView.currentView.y;
			check.w = gView.currentView.w;
			check.h = gView.currentView.h;
		}
	}
	fz_drop_pixmap(pdf.pCtx, pdf.ppPix[0]);
	free(pdf.ppPix);
	fz_drop_context(pdf.pCtx);
	SDL_DestroyTexture(pdf.pTexture);
	SDL_DestroyRenderer(gInst.pRenderer);
	SDL_DestroyWindow(gInst.pWin);
	SDL_Quit();
	return 0;
}

SDL_Texture* 
PixmapToTexture(SDL_Renderer *pRenderer, fz_pixmap *pPix, fz_context *pCtx) 
{
	int width = fz_pixmap_width(pCtx, pPix);
	int height = fz_pixmap_height(pCtx, pPix);
	int components = fz_pixmap_components(pCtx, pPix);

	int sdl_pixel_format; // You may need to expand grayscale to RGB
	if (components == 1) 
		sdl_pixel_format = SDL_PIXELFORMAT_RGB24;
	else if (components == 3) 
		sdl_pixel_format = SDL_PIXELFORMAT_RGB24;
	else if (components == 4)
		sdl_pixel_format = SDL_PIXELFORMAT_RGBA32;
	else
		return NULL;
    SDL_Texture *pTexture = SDL_CreateTexture(pRenderer, 
        sdl_pixel_format, 
        SDL_TEXTUREACCESS_STATIC, 
        width, 
        height
    );

	if (!pTexture)
		fprintf(stderr, "Couldn't create texture: %s\n", SDL_GetError());

    if (SDL_UpdateTexture(pTexture, NULL, pPix->samples, width * components))
		return NULL;
    return pTexture;
}
