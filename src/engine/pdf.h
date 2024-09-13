#ifndef PDF_H
#define PDF_H

#include <mupdf/fitz.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_render.h>
#include <stdbool.h>

#ifdef PLATFORM_LINUX
	typedef pthread_t myThread;
	typedef pthread_mutex_t Mutex;
#elif PLATFORM_WINDOWS
	typedef void* myThread;
	typedef void* Mutex;
#endif

#define MAX_MUTEX 10

/* NOTE: provide function for specific pages ?..*/
typedef struct sInfo
{
	int				pageStart;
	int				nbrPages;
	float			fZoom;
	float			fRotate;
	float			fDpi;
}sInfo;

typedef struct PDFPage
{
	bool			bTextureCache;
	bool			bPpmCache;
	sInfo			sInfo;
	fz_pixmap		*pPix;
	void			*pTexture;
}PDFPage;

typedef struct PDFContext
{
	sInfo			DefaultInfo;
	const char		*pFile;
	fz_context		*pCtx;
	fz_document		*pDoc;
	Mutex			*pFzMutexes;
	PDFPage			*pPages;
	size_t			nbPagesRetrieved;
	size_t			nbOfPages;
	size_t			viewingPage;
}PDFContext;

typedef struct PDFView
{
	SDL_FRect currentView;
	SDL_FRect oldView;
	SDL_FRect nextView;
}PDFView;

extern PDFContext gPdf;
extern PDFView gView;

PDFContext	CreatePDF(char *pFile, int page, float zoom, float rotate);
void		UpdateTextures(SDL_Renderer *pRenderer, int index);
PDFPage		*LoadPagesArray(size_t nbOfPages);
void		*CreatePDFContext(PDFContext *PdfCtx, char *pFile, sInfo sInfo);
SDL_Texture* PixmapToTexture(SDL_Renderer *pRenderer, fz_pixmap *pPix, fz_context *pCtx, SDL_Texture *pTexture);

#endif //PDF_H
