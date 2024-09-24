#include "utils.h"
#include "init.h"
#include "import/camera.h"

void
InitView(void)
{
	int z = 0;
	for (int i = 0; i < 4; i++)
	{
		gView3.currentView2[i].x = gView3.currentView.x; gView3.currentView2[i].w = gView3.currentView.w;
		gView3.currentView2[i].h = gView3.currentView.h;

		gView3.nextView2[i].x = gView3.nextView.x; gView3.nextView2[i].w = gView3.nextView.w;
		gView3.nextView2[i].h = gView3.nextView.h;

		gView3.oldView2[i].x = gView3.oldView.x; gView3.oldView2[i].w = gView3.oldView.w;
		gView3.oldView2[i].h = gView3.oldView.h;

		int bord = 0;
		gView3.currentView2[i].y = SDL_fmodf(((gView3.currentView2[i].h + bord)* i) + z, (float)4 * gView3.currentView.h);
		gView3.nextView2[i].y = SDL_fmodf(((gView3.currentView2[i].h + bord)* i) + z, (float)4 * gView3.currentView.h);
		gView3.oldView2[i].y = SDL_fmodf(((gView3.currentView2[i].h + bord)* i) + z, (float)4 * gView3.currentView.h);
	}
	printf("currentView.h: %f\t currentView.w: %f\n", gView3.currentView.h, gView3.currentView.w);
}

void
Reload(int i)
{
	/* printf("page requested is : %d/%zu\n", i, gPdf.nbOfPages); */
	sInfo sInfo = {
		.fDpi = 100 * gZoom,
		.fZoom = 100,
		.fRotate = 0,
		.nbrPages = 1,
		.pageStart = i
	};

	if (!gPdf.pPages[i].bPpmCache)
	{
		gPdf.pPages[i].pPix = CreatePDFPage(gPdf.pCtx, gPdf.pFile, &sInfo);
		gPdf.pPages[i].bPpmCache = true;
	}
	if (!gInst.pMainTexture)
	{
		gInst.pMainTexture = LoadTextures(gInst.pRenderer, gPdf.pPages[i].pPix, gPdf.pCtx, SDL_TEXTUREACCESS_STREAMING);
		gPdf.pPages[i].bTextureCache = true;
	}
	gInst.pMainTexture = PixmapToTexture(gInst.pRenderer, gPdf.pPages[i].pPix, gPdf.pCtx, gInst.pMainTexture);
	if (!gInst.pMainTexture)
		exit(1);
	gPdf.pPages[i].bTextureCache = true;
	gRender = true;
}

void
printCache(EventQueue *q)
{
	printf("\n");
	for (int i = 0; i < q->capacity; i++)
		printf("[%d]\t", i);
	printf("\n");
	for (int i = 0; i < q->capacity; i++)
		printf("%d\t", q->q[i]);
	printf("\n");
	printf("\n");
}

fz_rect
FzRectFromSDL(SDL_FRect rect);

/*
 * NOTE:
 * Using this im currently not freeing any textures, everything is
 * cached .. I think ? (kinda forgot)
 */
void
MegaLoop(void)
{
	// WARN: MAGIC NUMBER !!!!!!!
	int height = 842, gap = 20;
	int guess = 0; static int oldGuess = 0; guess = (gView.y / (height + gap));
	if (oldGuess != guess) { oldGuess = guess; printf("Page: %d\n", guess); } if (guess > 0) guess--;

	float offsetCam = 0; float offsetReal = 0; float offsetx = 0; float offsetx2 = 0;
	float factor = 0.4; int tmpCount = 0; int i = 0; int j = 0;

	for(i = 0; i < gPdf.nbOfPages; i++, j++)
	{
		int rx = 0, ry = 0; 
		int rx2 = 0, ry2 = 0; 
		int rw = gPdf.pPages[i].position.w, rh = gPdf.pPages[i].position.h; 

		world_to_screen(gPdf.pPages[i].views[0].x, gPdf.pPages[i].views[0].y, &rx, &ry);
		/* world_to_screen(gPdf.pPages[i].views[2].x, gPdf.pPages[i].views[2].y, &rx2, &ry2); */

		if (i == 0 && ry > 0)
		{
			offsetCam = ry - 0;
			offsetReal =  0;
			/* offsetReal = ry2 - 0; */
		}
		if (i == 0 && (rx >= gInst.width || rx + rw <= 0))
		{
			offsetx = 0;
		}

		/* ME = GENIUS (NO) */
		gPdf.pPages[i].views[2].x = gPdf.pPages[i].position.x - (gView.x);
		gPdf.pPages[i].views[2].y = gPdf.pPages[i].position.y - (gView.y);
	
		SDL_FRect tmp = {
			.x = rx,
			.y = ry - offsetCam,
			.w = gPdf.pPages[i].position.w * fscalex,
			.h = gPdf.pPages[i].position.h * fscalex 
		};

		SDL_FRect pageBbox = {
			.x = rx,
			.y = ry - offsetCam,
			.w = gPdf.pPages[i].position.w * fscalex,
			.h = gPdf.pPages[i].position.h * fscalex
		};
		fz_rect bbox = FzRectFromSDL(pageBbox);

		if (tmp.y <= gInst.height && tmp.y + tmp.h > 0)
		{
			/* gVisible.array[gVisible.count + tmpCount] = i; */
			tmpCount++;
			// index for the text array

			sInfo sInfo = { 
				.fDpi = 100.0f, .fZoom = 100.0f, .fRotate = 0.0f,
				.nbrPages = 1, .pageStart = i,
				.rectView = pageBbox,
				.bbox = bbox
			};

			if (gPdf.pPages[i].bPpmCache == false)
			{
				gPdf.pPages[i].pPix = CreatePDFPage(gPdf.pCtx, gPdf.pFile, &sInfo);
				gPdf.pPages[i].bPpmCache = true;
			}

			int z = isCachedQueue(&gCacheQueue, i);
			if (z < 0)
			{
				PushEvent(&gCacheQueue, i);
				z = isCachedQueue(&gCacheQueue, i);
					if (!gInst.pTextureMap[z].pTexture)
						gInst.pTextureMap[z].pTexture = LoadTextures(gInst.pRenderer, gPdf.pPages[i].pPix, gPdf.pCtx, SDL_TEXTUREACCESS_STREAMING);
				if (gPdf.pPages[i].bTextureCache == false)
				{
					gInst.pTextureMap[z].pTexture = PixmapToTexture(gInst.pRenderer, gPdf.pPages[i].pPix, gPdf.pCtx, gInst.pTextureMap[z].pTexture);
					gInst.pTextureMap[z].pageIndex = i;
					gPdf.pPages[i].bTextureCache = true;
				}
			}
			/* printf("tmp.x = %f\t tmp.y = %f\t, tmp.w = %f\t, tmp.h = %f\n", tmp.x, tmp.y, tmp.w , tmp.h); */
			int w = 0, h = 0;
			SDL_QueryTexture(gInst.pTextureMap[z].pTexture, NULL, NULL, &w, &h);
            /*
			 * tmp.w = w;
			 * tmp.h = h;
             */
			SDL_RenderCopyF(gInst.pRenderer, gInst.pTextureMap[z].pTexture, NULL, &tmp);
		} 
		else
		{
			fz_drop_pixmap(gPdf.pCtx, gPdf.pPages[i].pPix);
			gPdf.pPages[i].pPix = NULL;
			gPdf.pPages[i].bTextureCache = false;
			gPdf.pPages[i].bPpmCache = false;

			gPdf.pPages[i].views[0].x = gPdf.pPages[i].position.x - gView.x;
			gPdf.pPages[i].views[0].y = gPdf.pPages[i].position.y - gView.y;
			if (tmpCount >= 5)
			{
				gVisible.count += tmpCount;
				break;
			}
		}
		mUpdateSmooth2(factor, i, &gPdf.pPages);
	}
	static int oldCount = 0;
	if (oldCount != gVisible.count)
	{
		/* printf("cache items: %d\n", gVisible.count); */
		oldCount = gVisible.count;
	}
}

int
isCachedQueue(EventQueue *cache, int toCheck)
{
	int i = 0;
	while (i < cache->capacity)
	{
		if (cache->q[i] == toCheck)
			return i;
		i++;
	}
	return -1;
}

int
isCached(VisibleArray *va, int toCheck, int tmpCount)
{
	int i = 0;
	while (i < va->count + tmpCount)
	{
		if (va->array[i] == toCheck)
			return i;
		i++;
	}
	return -1;
}

void
clearCache(EventQueue *q)
{
	for (int i = 0; i < q->capacity; i++)
	{
		q->q[i] = 0;
	}
}

void
resize(void)
{
		// Because resizing
        /*
		 * for (int i = 0; i < gVisible.count; i++)
		 * {
		 * 	SDL_DestroyTexture(gInst.ppTextArray[i]);
		 * 	gInst.ppTextArray[i] = NULL;
         * 
		 * 	SDL_DestroyTexture(gInst.pTextureMap[i].pTexture);
		 * 	gInst.pTextureMap[i].pageIndex = -1;
		 * 	gInst.pTextureMap[i].pTexture = NULL;
         * 
		 * 	fz_drop_pixmap(gPdf.pCtx, gPdf.pPages[gVisible.array[i]].pPix);
		 * 	gPdf.pPages[gVisible.array[i]].pPix = NULL;
		 * 	gPdf.pPages[gVisible.array[i]].bTextureCache = false;
		 * 	gPdf.pPages[gVisible.array[i]].bPpmCache = false;
		 * }
         */
		for (int i = 0; i < gCacheQueue.capacity; i++)
		{
			SDL_DestroyTexture(gInst.pTextureMap[i].pTexture);
			gInst.pTextureMap[i].pageIndex = -1;
			gInst.pTextureMap[i].pTexture = NULL;

			fz_drop_pixmap(gPdf.pCtx, gPdf.pPages[gCacheQueue.q[i]].pPix);
			gPdf.pPages[gCacheQueue.q[i]].pPix = NULL;
			gPdf.pPages[gCacheQueue.q[i]].bTextureCache = false;
			gPdf.pPages[gCacheQueue.q[i]].bPpmCache = false;
		}
		clearCache(&gCacheQueue);
}
