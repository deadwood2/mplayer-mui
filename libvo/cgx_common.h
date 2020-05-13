#ifndef __CGX_COMMON__
#define __CGX_COMMON__

#include <proto/exec.h>
#include <proto/graphics.h>
#include <proto/cybergraphics.h>
#include <proto/layers.h>

#include <intuition/intuition.h>

#include "video_out.h"

#define __OLDMOS__

#ifndef __AROS__ /* Remove compiler warnings */
// Some Libs
extern struct GfxBase *           GfxBase;
extern struct Library *           CyberGfxBase;
extern struct IntuitionBase *     IntuitionBase;
extern struct Library * 		  LayersBase;
#endif

/****/
#define NOBORDER			0
#define TINYBORDER			1
#define TRANSPBORDER		2
#define ALWAYSBORDER		3
extern ULONG Cgx_BorderMode;

/* some flags */
#define CGX_FULLSCREEN	( 1L << 0)	// Fullscreen flags has you guessed
#define CGX_FASTMODE	( 1L << 1) 	// Tell to the driver to privilege speed instead of quality if possible
#define CGX_NOSCALE	    ( 1L << 2)	// Do not scale (this way it's faster but can be very ugly)
#define CGX_BUFFERING	( 1L << 3)	// Ask to use Multiple buffering if available
#define CGX_NOPLANAR	( 1L << 4)	// Don't use planar overlay

#ifndef SA_StopBlanker
#define SA_StopBlanker (SA_Dummy + 121)
#endif

#ifndef SA_ShowPointer
#define SA_ShowPointer (SA_Dummy + 122)
#endif

/***/
extern char * cgx_monitor;

/***************************/
BOOL Cgx_GiveArg(const char *arg);
VOID Cgx_ReleaseArg(VOID);

void Cgx_StartWindow(struct Window *My_Window);
void Cgx_StopWindow(struct Window *My_Window);
void Cgx_HandleBorder(struct Window *My_Window, ULONG handle_mouse);

BOOL Cgx_CheckEvents(struct Screen * My_Screen, struct Window *My_Window, uint32_t *window_height, uint32_t *windon_width,
		uint32_t *window_left, uint32_t *windon_top);
void Cgx_Start(struct Window *My_Window);
void Cgx_Stop(struct Window *My_Window);

void Cgx_Message(void);

void Cgx_ControlBlanker(struct Screen * screen, ULONG enable);
void Cgx_BlankerState(void);
void Cgx_ShowMouse(struct Screen * screen, struct Window * window, ULONG enable);

/******************************/
#define PREPARE_BACKFILL(BufferWidth, BufferHeight) \
	struct BackFillArgs *MyArgs 	= (struct BackFillArgs *) REG_A1; \
	struct RastPort *ArgRP     		= (struct RastPort *) REG_A2;		\
	struct Layer *MyLayer = ArgRP->Layer; \
	\
	struct RastPort	MyRP;	\
	ULONG SizeX;	\
	ULONG SizeY;	\
	ULONG StartX; \
	ULONG StartY;	\
	ULONG BufferStartX;	\
	ULONG BufferStartY;	\
	\
	if (!MyLayer) return; \
	\
	SizeX 				= MyArgs->bounds.MaxX - MyArgs->bounds.MinX + 1;	\
	SizeY 				= MyArgs->bounds.MaxY - MyArgs->bounds.MinY + 1;	\
	StartX 				= MyLayer->bounds.MinX + MyArgs->offsetx;	\
	StartY 				= MyLayer->bounds.MinY + MyArgs->offsety;	\
	BufferStartX 	= MyArgs->offsetx - My_Window->BorderLeft;	\
	BufferStartY 	= MyArgs->offsety - My_Window->BorderTop;	\
	\
	if (SizeX <= 0 	\
			|| SizeY <= 0	\
			|| MyArgs->offsetx < 0	\
			|| MyArgs->offsety < 0	\
			|| BufferStartX < 0	\
			|| BufferStartY < 0	\
			) return;	\
	memcpy(&MyRP, ArgRP, sizeof(struct RastPort) );	\
	MyRP.Layer = NULL;	\
	\
	if ( (SizeX + StartX) > MyLayer->bounds.MaxX) SizeX = MyLayer->bounds.MaxX - StartX;	\
	if ( (SizeY + StartY) > MyLayer->bounds.MaxY) SizeY = MyLayer->bounds.MaxY - StartY;	

/*
	if ( (SizeX + BufferStartX) > BufferWidth) SizeX = BufferWidth - StartX;	\
	if ( (SizeY + BufferStartY) > BufferHeight) SizeY = BufferHeight - StartY;	
*/

#endif

