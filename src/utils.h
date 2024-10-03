#ifndef UTILS_H
#define UTILS_H

#include "import/containers.h"
#include "import/gui.h"

#ifndef MAX_VISIBLE_PAGES
#define MAX_VISIBLE_PAGES 20
#endif


// static inline void UpdateSmooth2(float factor, int i, PDFPage **p);
// static inline void UpdateSmooth(float factor);

#define mUpdateSmooth2(factor, i, p)\
	(*p)[i].views[0].w = mInterpolate2((*p)[i].views[1].w, (*p)[i].views[2].w, factor);\
	(*p)[i].views[0].x = mInterpolate2((*p)[i].views[1].x, (*p)[i].views[2].x, factor);\
	(*p)[i].views[0].h = mInterpolate2((*p)[i].views[1].h, (*p)[i].views[2].h, factor);\
	(*p)[i].views[0].y = mInterpolate2((*p)[i].views[1].y, (*p)[i].views[2].y, factor);\
	(*p)[i].views[1].x = (*p)[i].views[0].x; (*p)[i].views[1].y = (*p)[i].views[0].y;\
	(*p)[i].views[1].w = (*p)[i].views[0].w; (*p)[i].views[1].h = (*p)[i].views[0].h;

#define mUpdateSmooth(factor)\
	gView3.currentView.x = mInterpolate2(gView3.oldView.x, gView3.nextView.x, factor);\
	gView3.currentView.y = mInterpolate2(gView3.oldView.y, gView3.nextView.y, factor);\
	gView3.currentView.w = mInterpolate2(gView3.oldView.w, gView3.nextView.w, factor);\
	gView3.currentView.h = mInterpolate2(gView3.oldView.h, gView3.nextView.h, factor);\
	gView3.oldView.x = gView3.currentView.x; gView3.oldView.y = gView3.currentView.y;\
	gView3.oldView.w = gView3.currentView.w; gView3.oldView.h = gView3.currentView.h;

typedef struct VisibleArray
{
	int array[20]; // magic number
	int count;
}VisibleArray;

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

extern VisibleArray gVisible;

bool	ArrayEquals(sArray *a, sArray *b);
void	ReloadPage(void);
void	MegaLoop(void);
void	resize(void);
void	Reload(int i);
void	printCache(EventQueue *q);
int		isCachedQueue(EventQueue *cache, int toCheck);
int		isCached(VisibleArray *va, int toCheck, int tmpCount);
void	clearCache(EventQueue *q);
void	resize(void);
void	InitView();

#endif //UTILS_H
