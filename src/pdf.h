#ifndef PDF_H
#define PDF_H

#include <mupdf/fitz.h>
#include <SDL2/SDL_rect.h>

#ifdef PLATFORM_LINUX
	typedef pthread_t myThread;
	typedef pthread_mutex_t Mutex;
#elif PLATFORM_WINDOWS
	typedef HANDLE myThread;
	typedef void *pMutex, Mutex;
#endif

typedef struct PDF
{
	const char *pFile;
	void *pTexture;
	fz_context *pCtx;
	fz_document *pDoc;
	fz_pixmap **ppPix;
	size_t page_count;
	int size;
	int page_nbr;
	int zoom;
	float rotate;
	Mutex	pMutexes[FZ_LOCK_MAX];
}PDF;

typedef struct PDFView
{
	SDL_FRect currentView;
	SDL_FRect oldView;
	SDL_FRect nextView;
}PDFView;

extern PDF gPdf;
extern PDFView gView;

PDF CreatePDF(char *pFile, int page, float zoom, float rotate);

#endif //PDF_H
