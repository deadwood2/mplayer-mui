/*
 *  vo_cgx_overlay.c
 *  VO module for MPlayer MorphOS
 *  Using CGX/Overlay
 *  Writen by DET Nicolas
*/

#define SYSTEM_PRIVATE

#define USE_VMEM64		1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../morphos_stuff.h"
#include "cgx_common.h"
#include "aspect.h"
#include "config.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "mp_msg.h"
#include "sub/osd.h"
#include "sub/sub.h"
#include "version.h"

#ifdef CONFIG_GUI
#include "gui/interface.h"
#include "gui/morphos/gui.h"
#include "mplayer.h"
#endif

#include <version.h>

//Debug
#if 0
#include <clib/debug_protos.h>
#define kk(x) 
#define KPRINTF KPrintF
#else
#define kk(x)
#define KPRINTF
#endif

// OS specific
#include <libraries/cybergraphics.h>
#include <proto/cybergraphics.h>
#include <proto/cgxvideo.h>
#include <cybergraphx/cgxvideo.h>
#include <proto/graphics.h>
#include <graphics/gfxbase.h>

#include <devices/rawkeycodes.h>
#include <proto/intuition.h>
#include <intuition/extensions.h>
#include <intuition/intuition.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <exec/types.h>

struct Library *CGXVideoBase=NULL;
static int use_overlay_doublebuffer = 0;

static vo_info_t info =
{
	"CyberGraphX video output (Overlay)",
	"cgx_overlay",
	"DET Nicolas",
	"MorphOS rules da world !"
};

LIBVO_EXTERN(cgx_overlay)

// To make gcc happy
extern void mplayer_put_key(int code);

// Some proto for our private func
static void (*vo_draw_alpha_func)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride);

/*************************/
static struct Window *My_Window = NULL;
static struct Screen *My_Screen = NULL;
static APTR VL_Handle = NULL;
extern char PubScreenName[128];

extern ULONG WantedModeID;

UWORD ZoomData [] =
{
	(UWORD) ~0,
	(UWORD) ~0,
	0,
	0
};

//static struct RastPort *rp      = NULL;

// For events
//static struct MsgPort *UserMsg  = NULL;

// not OS specific
static uint32_t   image_width;            // well no comment
static uint32_t   image_height;
static uint32_t   window_width;           // width and height on the window
static uint32_t   window_height;          // can be different from the image

static uint32_t   window_orig_width;
static uint32_t   window_orig_height;

static ULONG pixel_format;

uint32_t   is_fullscreen;

extern UWORD *EmptyPointer;                // Blank pointer

static ULONG      BorderWidth;
static ULONG      BorderHeight;

static uint32_t   win_left;               // offset in the rp where we have to display the image
static uint32_t   win_top;                // ...

static uint32_t (*real_draw_slice) (uint8_t *image[], int stride[], int w,int h,int x,int y);
static uint32_t draw_slice_420p_to_422	(uint8_t *image[], int stride[], int w,int h,int x,int y);
static uint32_t draw_slice_422p_to_420p	(uint8_t *image[], int stride[], int w,int h,int x,int y);
static uint32_t draw_slice_422p_to_422	(uint8_t *image[], int stride[], int w,int h,int x,int y);
static uint32_t draw_slice_422			(uint8_t *image[], int stride[], int w,int h,int x,int y);
//static uint32_t draw_slice_422_scaled	  (uint8_t *image[], int stride[], int w,int h,int x,int y);
static uint32_t draw_slice_420p			(uint8_t *image[], int stride[], int w,int h,int x,int y);
//static uint32_t draw_slice_420p_scaled  (uint8_t *image[], int stride[], int w,int h,int x,int y);

//static void (*real_flip_page) (void);

#define VLMODE_PLANAR 0
#define VLMODE_CHUNKY 1

static ULONG cgx_overlay_mode = VLMODE_PLANAR;

static void wbackfillfunc(void);

static const struct EmulLibEntry wbackfillfunc_gate =
{
	TRAP_LIB,
	0,
	(void (*)(void)) wbackfillfunc
};

static struct Hook wbackfillhook =
{
	{NULL, NULL},
	(HOOKFUNC) &wbackfillfunc_gate,
	NULL,
	NULL
};

#ifdef __GNUC__
# pragma pack(2)
#endif

struct backfillargs
{
	struct Layer     *layer;
	struct Rectangle  bounds;
	LONG              offsetx;
	LONG              offsety;
};

#ifdef __GNUC__
# pragma pack()
#endif

static LONG vlayer_x_offset = 0, vlayer_y_offset = 0;
static LONG scr_shadowpen = 1;
static BOOL scr_truecolor = TRUE;

/*
#undef min
#undef max

#define min(a,b)                \
	({typeof(a) _a = (a);   \
	  typeof(b) _b = (b);   \
	  _a < _b ? _a : _b; })

#define max(a,b)                \
	({typeof(a) _a = (a);   \
	  typeof(b) _b = (b);   \
	  _a > _b ? _a : _b; })
*/

static inline void AndRectRect(struct Rectangle *rect, const struct Rectangle *limit)
{
	rect->MinX = max(rect->MinX, limit->MinX);
	rect->MinY = max(rect->MinY, limit->MinY);
	rect->MaxX = min(rect->MaxX, limit->MaxX);
	rect->MaxY = min(rect->MaxY, limit->MaxY);
}


static inline LONG IsValidRect(const struct Rectangle *rect)
{
	return rect->MinX <= rect->MaxX && rect->MinY <= rect->MaxY;
}

static void update_scrvars(struct Screen *scr)
{
	scr_truecolor = GetBitMapAttr(scr->RastPort.BitMap, BMA_DEPTH) > 8;
	scr_shadowpen = 1;
	if (!scr_truecolor)
	{
		struct DrawInfo *dri;

		dri = GetScreenDrawInfo(scr);
		if (dri)
		{
			if (dri->dri_NumPens > SHADOWPEN)
			{
				scr_shadowpen = dri->dri_Pens[SHADOWPEN];
			}

			FreeScreenDrawInfo(scr, dri);
		}
	}
}

void wbackfillfunc(void)
{
	//struct Hook *hook        = (APTR) REG_A0;
	struct backfillargs *bfa = (APTR) REG_A1;
	struct RastPort *frp     = (APTR) REG_A2;
	struct Rectangle b1, b2, k;
	struct Layer *l = frp->Layer;
	ULONG bleft, btop, bright, bbottom;
	struct RastPort rp;
	/*
	kprintf("frp %08lx layer 0x%08lx %ld,%ld->%ld,%ld offsx %ld offsy %ld\n",
	        frp, l,
	        bfa->bounds.MinX, bfa->bounds.MinY,
	        bfa->bounds.MaxX+1, bfa->bounds.MaxY+1,
			bfa->offsetx, bfa->offsety);*/

	if (!l) return;

	if(!VL_Handle) return;

	memcpy(&rp, frp, sizeof(*frp));
	rp.Layer = NULL;

	bleft   = My_Window->BorderLeft;
	btop    = My_Window->BorderTop;
	bright  = My_Window->BorderRight;
	bbottom = My_Window->BorderBottom;

	/* key rect */
	k.MinX = l->bounds.MinX + bleft + vlayer_x_offset;
	k.MinY = l->bounds.MinY + btop + vlayer_y_offset;
	k.MaxX = l->bounds.MaxX - bright - vlayer_x_offset;
	k.MaxY = l->bounds.MaxY - bbottom - vlayer_y_offset;

	AndRectRect(&k, &bfa->bounds);

	if (vlayer_x_offset || vlayer_y_offset)
	{
		if (vlayer_x_offset)
		{
			/* left rect */
			b1.MinX = l->bounds.MinX + bleft;
			b1.MinY = l->bounds.MinY + btop;
			b1.MaxX = l->bounds.MinX + bleft + vlayer_x_offset;
			b1.MaxY = l->bounds.MaxY - bbottom;

			/* right rect */
			b2.MinX = l->bounds.MaxX - bright - vlayer_x_offset;
			b2.MinY = l->bounds.MinY + btop;
			b2.MaxX = l->bounds.MaxX - bright;
			b2.MaxY = l->bounds.MaxY - bbottom;
		}
		else if (vlayer_y_offset)
		{
			/* top rect */
			b1.MinX = l->bounds.MinX + bleft;
			b1.MinY = l->bounds.MinY + My_Window->BorderTop;
			b1.MaxX = l->bounds.MaxX - bright;
			b1.MaxY = l->bounds.MinY + My_Window->BorderTop + vlayer_y_offset;

			/* bottom rect */
			b2.MinX = l->bounds.MinX + bleft;
			b2.MinY = l->bounds.MaxY - My_Window->BorderBottom - vlayer_y_offset;
			b2.MaxX = l->bounds.MaxX - bright;
			b2.MaxY = l->bounds.MaxY - My_Window->BorderBottom;
		}

		AndRectRect(&b1, &bfa->bounds);
		AndRectRect(&b2, &bfa->bounds);
	}

	/* draw rects, if visible */

	if (vlayer_x_offset || vlayer_y_offset)
	{
		if (IsValidRect(&b1))
		{
			if (scr_truecolor)
			{
				FillPixelArray(&rp, b1.MinX, b1.MinY,
				               b1.MaxX - b1.MinX + 1, b1.MaxY - b1.MinY + 1,
				               0x00000000);
			}
			else
			{
				SetAPen(&rp, scr_shadowpen);
				RectFill(&rp, b1.MinX, b1.MinY, b1.MaxX, b1.MaxY);
			}
		}

		if (IsValidRect(&b2))
		{
			if (scr_truecolor)
			{
				FillPixelArray(&rp, b2.MinX, b2.MinY,
				               b2.MaxX - b2.MinX + 1, b2.MaxY - b2.MinY + 1,
				               0x00000000);
			}
			else
			{
				SetAPen(&rp, scr_shadowpen);
				RectFill(&rp, b2.MinX, b2.MinY, b2.MaxX, b2.MaxY);
			}
		}
	}

	if (IsValidRect(&k))
	{
		if (scr_truecolor)
		{
			FillPixelArray(&rp, k.MinX, k.MinY,
			               k.MaxX - k.MinX + 1, k.MaxY - k.MinY + 1,
			               GetVLayerAttr(VL_Handle, VOA_ColorKey));
		}
		else
		{
			SetAPen(&rp, GetVLayerAttr(VL_Handle, VOA_ColorKeyPen));
			RectFill(&rp, k.MinX, k.MinY, k.MaxX, k.MaxY);
		}
	}
}

/******************************************************/
/* Put Yuv data in the right format for the cgxvideo.library */
#if 0
#define PLANAR2_2_CHUNKY(Py, Py2, Pu, Pv, dst, dst2)	 \
	{									\
	register ULONG Y = *++Py;			\
	register ULONG U = *++Pu;			\
	register ULONG V = *++Pv;			\
											\
	*++dst = ( (Y ) & 0xFF000000 ) | ( ( U >>8) & 0x00FF0000 ) | ( (Y>>8) & 0x0000FF00) | ( (V >> 24 ) & 0x000000FF);	\
	*++dst = ( (Y << 16) & 0xFF000000 ) | ( ( U ) & 0x00FF0000 ) | ( (Y<< 8)  & 0x0000FF00) | ((V >> 16) & 0x000000FF);\
											\
	Y = *++Py;							\
	*++dst = ( (Y ) & 0xFF000000 ) | ( ( U << 8 ) & 0x00FF0000 ) | ((Y >>8) & 0x0000FF00) | ((V>>8) & 0x000000FF);		\
	*++dst = ( (Y << 16) & 0xFF000000 ) | ( ( U << 16) & 0x00FF0000 ) | ( (Y<<8)  & 0x0000FF00) | ( (V) & 0x000000FF);	\
											\
	Y = *++Py2;						\
	*++dst2 = ( (Y ) & 0xFF000000 ) | ( ( U >>8) & 0x00FF0000 ) | ( (Y>>8) & 0x0000FF00) | ( (V >> 24 ) & 0x000000FF); \
	*++dst2 = ( (Y << 16) & 0xFF000000 ) | ( ( U ) & 0x00FF0000 ) | ( (Y<< 8)  & 0x0000FF00) | ((V >> 16) & 0x000000FF);\
											\
	Y = *++Py2;						\
	*++dst2 = ( (Y ) & 0xFF000000 ) | ( ( U << 8 ) & 0x00FF0000 ) | ((Y >>8) & 0x0000FF00) | ((V>>8) & 0x000000FF);		\
	*++dst2 = ( (Y << 16) & 0xFF000000 ) | ( ( U << 16) & 0x00FF0000 ) | ( (Y<<8)  & 0x0000FF00) | ( (V) & 0x000000FF); \
	}
#else
#define PLANAR2_2_CHUNKY(Py, Py2, Pu, Pv, dst, dst2) \
	{											\
	register ULONG Y = *++Py;				\
	register ULONG Y2= *++Py;				\
	register ULONG U = *++Pu;				\
	register ULONG V = *++Pv;				\
												\
	__asm__ __volatile__ (					\
		"rlwinm %%r3,%1,0,0,7;"			\
		"rlwinm %%r4,%1,16,0,7;"   		\
		"rlwimi %%r3,%2,24,8,15;"		\
		"rlwimi %%r4,%2,0,8,15;"		\
		"rlwimi %%r3,%1,24,16,23;"		\
		"rlwimi %%r4,%1,8,16,23;"		\
		"rlwimi %%r3,%3,8,24,31;"		\
		"rlwimi %%r4,%3,16,24,31;"		\
												\
		"stwu %%r3,4(%0);"			\
		"stwu %%r4,4(%0);"				\
												\
	    : "+b" (dst)						\
	    : "r" (Y), "r" (U), "r" (V) 		\
	    : "r3", "r4");				\
												\
	Y = *++Py2;							\
	__asm__ __volatile__ (				\
		"rlwinm %%r3,%1,0,0,7;"			\
		"rlwinm %%r4,%1,16,0,7;"			\
		"rlwimi %%r3,%2,8,8,15;"			\
		"rlwimi %%r4,%2,16,8,15;"		\
		"rlwimi %%r3,%1,24,16,23;"		\
		"rlwimi %%r4,%1,8,16,23;"		\
		"rlwimi %%r3,%3,24,24,31;"		\
		"rlwimi %%r4,%3,0,24,31;"		\
												\
		"stwu %%r3,4(%0);"		\
		"stwu %%r4,4(%0);"				\
											\
	    : "+b" (dst)						\
	    : "r" (Y2), "r" (U), "r" (V) 		\
	    : "r3", "r4");				\
												\
	Y2= *++Py2;							\
	__asm__ __volatile__ (				\
		"rlwinm %%r3,%1,0,0,7;" 		\
		"rlwinm %%r4,%1,16,0,7;"   		\
		"rlwimi %%r3,%2,24,8,15;"		\
		"rlwimi %%r4,%2,0,8,15;"		\
		"rlwimi %%r3,%1,24,16,23;"		\
		"rlwimi %%r4,%1,8,16,23;"		\
		"rlwimi %%r3,%3,8,24,31;"		\
		"rlwimi %%r4,%3,16,24,31;"		\
												\
		"stwu %%r3,4(%0);"			\
		"stwu %%r4,4(%0);"				\
												\
	    : "+b" (dst2)						\
	    : "r" (Y), "r" (U), "r" (V) 		\
	    : "r3", "r4");				\
									\
	__asm__ __volatile__ (				\
		"rlwinm %%r3,%1,0,0,7;"		\
		"rlwinm %%r4,%1,16,0,7;"		\
		"rlwimi %%r3,%2,8,8,15;"		\
		"rlwimi %%r4,%2,16,8,15;"		\
		"rlwimi %%r3,%1,24,16,23;"		\
		"rlwimi %%r4,%1,8,16,23;"		\
		"rlwimi %%r3,%3,24,24,31;"		\
		"rlwimi %%r4,%3,0,24,31;"		\
												\
		"stwu %%r3,4(%0);"				\
		"stwu %%r4,4(%0);"				\
												\
	    : "+b" (dst2)						\
	    : "r" (Y2), "r" (U), "r" (V) 		\
	    : "r3", "r4");					\
	}

#define PLANAR_2_CHUNKY(Py, Pu, Pv, dst)	\
	{											\
		 register ULONG Y = *++Py;				\
		 register ULONG Y2= *++Py;				\
         register ULONG U = *++Pu;				\
         register ULONG V = *++Pv;				\
												\
		 __asm__ __volatile__ (					\
               	"rlwinm %%r3,%1,0,0,7;"			\
               	"rlwinm %%r4,%1,16,0,7;"   		\
               	"rlwimi %%r3,%2,24,8,15;"		\
               	"rlwimi %%r4,%2,0,8,15;"		\
               	"rlwimi %%r3,%1,24,16,23;"		\
               	"rlwimi %%r4,%1,8,16,23;"		\
               	"rlwimi %%r3,%3,8,24,31;"		\
               	"rlwimi %%r4,%3,16,24,31;"		\
												\
				"stwu %%r3,4(%0);"             	\
               	"stwu %%r4,4(%0);"				\
												\
			: "+b" (dst)						\
            : "r" (Y), "r" (U), "r" (V) 		\
            : "r3", "r4");        				\
												\
			__asm__ __volatile__ (				\
               	"rlwinm %%r3,%1,0,0,7;"			\
               	"rlwinm %%r4,%1,16,0,7;"    	\
				"rlwimi %%r3,%2,8,8,15;"		\
				"rlwimi %%r4,%2,16,8,15;"		\
               	"rlwimi %%r3,%1,24,16,23;"		\
               	"rlwimi %%r4,%1,8,16,23;"		\
				"rlwimi %%r3,%3,24,24,31;"		\
				"rlwimi %%r4,%3,0,24,31;"		\
												\
               	"stwu %%r3,4(%0);"             	\
               	"stwu %%r4,4(%0);"				\
               									\
            : "+b" (dst)						\
			: "r" (Y2), "r" (U), "r" (V) 		\
            : "r3", "r4");        				\
	}

#if USE_VMEM64
#define PLANAR2_2_CHUNKY_64(Py, Py2, Pu, Pv, dst, dst2, tmp) \
	{											\
	register ULONG Y = *++Py;				\
	register ULONG Y2= *++Py;				\
	register ULONG U = *++Pu;				\
	register ULONG V = *++Pv;				\
												\
	__asm__ __volatile__ (					\
		"rlwinm %%r3,%1,0,0,7;"			\
		"rlwinm %%r4,%1,16,0,7;"   		\
		"rlwimi %%r3,%2,24,8,15;"		\
		"rlwimi %%r4,%2,0,8,15;"		\
		"rlwimi %%r3,%1,24,16,23;"		\
		"rlwimi %%r4,%1,8,16,23;"		\
		"rlwimi %%r3,%3,8,24,31;"		\
		"rlwimi %%r4,%3,16,24,31;"		\
												\
		"stw %%r3,0(%4);"			\
		"stw %%r4,4(%4);"			\
		"lfd %%f1,0(%4);"			\
		"stfdu %%f1,8(%0);"			\
												\
	    : "+b" (dst)						\
	    : "r" (Y), "r" (U), "r" (V), "r" (tmp) 		\
	    : "r3", "r4", "fr1");				\
												\
	Y = *++Py2;							\
	__asm__ __volatile__ (				\
		"rlwinm %%r3,%1,0,0,7;"			\
		"rlwinm %%r4,%1,16,0,7;"			\
		"rlwimi %%r3,%2,8,8,15;"			\
		"rlwimi %%r4,%2,16,8,15;"		\
		"rlwimi %%r3,%1,24,16,23;"		\
		"rlwimi %%r4,%1,8,16,23;"		\
		"rlwimi %%r3,%3,24,24,31;"		\
		"rlwimi %%r4,%3,0,24,31;"		\
												\
		"stw %%r3,0(%4);"			\
		"stw %%r4,4(%4);"			\
		"lfd %%f1,0(%4);"			\
		"stfdu %%f1,8(%0);"			\
											\
	    : "+b" (dst)						\
	    : "r" (Y2), "r" (U), "r" (V), "r" (tmp)		\
	    : "r3", "r4", "fr1");				\
												\
	Y2= *++Py2;							\
	__asm__ __volatile__ (				\
		"rlwinm %%r3,%1,0,0,7;" 		\
		"rlwinm %%r4,%1,16,0,7;"   		\
		"rlwimi %%r3,%2,24,8,15;"		\
		"rlwimi %%r4,%2,0,8,15;"		\
		"rlwimi %%r3,%1,24,16,23;"		\
		"rlwimi %%r4,%1,8,16,23;"		\
		"rlwimi %%r3,%3,8,24,31;"		\
		"rlwimi %%r4,%3,16,24,31;"		\
												\
		"stw %%r3,0(%4);"			\
		"stw %%r4,4(%4);"			\
		"lfd %%f1,0(%4);"			\
		"stfdu %%f1,8(%0);"			\
												\
	    : "+b" (dst2)						\
	    : "r" (Y), "r" (U), "r" (V), "r" (tmp) 		\
	    : "r3", "r4", "fr1");				\
									\
	__asm__ __volatile__ (				\
		"rlwinm %%r3,%1,0,0,7;"		\
		"rlwinm %%r4,%1,16,0,7;"		\
		"rlwimi %%r3,%2,8,8,15;"		\
		"rlwimi %%r4,%2,16,8,15;"		\
		"rlwimi %%r3,%1,24,16,23;"		\
		"rlwimi %%r4,%1,8,16,23;"		\
		"rlwimi %%r3,%3,24,24,31;"		\
		"rlwimi %%r4,%3,0,24,31;"		\
												\
		"stw %%r3,0(%4);"			\
		"stw %%r4,4(%4);"			\
		"lfd %%f1,0(%4);"			\
		"stfdu %%f1,8(%0);"			\
												\
	    : "+b" (dst2)						\
	    : "r" (Y2), "r" (U), "r" (V), "r" (tmp) 		\
	    : "r3", "r4", "fr1");					\
	}

#define PLANAR_2_CHUNKY_64(Py, Pu, Pv, dst, tmp)	\
	{											\
	register ULONG Y = *++Py;				\
	register ULONG Y2= *++Py;				\
	register ULONG U = *++Pu;				\
	register ULONG V = *++Pv;				\
												\
	__asm__ __volatile__ (					\
		"rlwinm %%r3,%1,0,0,7;"			\
		"rlwinm %%r4,%1,16,0,7;"   		\
		"rlwimi %%r3,%2,24,8,15;"		\
		"rlwimi %%r4,%2,0,8,15;"		\
		"rlwimi %%r3,%1,24,16,23;"		\
		"rlwimi %%r4,%1,8,16,23;"		\
		"rlwimi %%r3,%3,8,24,31;"		\
		"rlwimi %%r4,%3,16,24,31;"		\
												\
		"stw %%r3,0(%4);"			\
		"stw %%r4,4(%4);"			\
		"lfd %%f1,0(%4);"			\
		"stfdu %%f1,8(%0);"			\
												\
	    : "+b" (dst)						\
	    : "r" (Y), "r" (U), "r" (V), "r" (tmp) 		\
	    : "r3", "r4", "fr1");				\
												\
	__asm__ __volatile__ (				\
		"rlwinm %%r3,%1,0,0,7;"			\
		"rlwinm %%r4,%1,16,0,7;"			\
		"rlwimi %%r3,%2,8,8,15;"			\
		"rlwimi %%r4,%2,16,8,15;"		\
		"rlwimi %%r3,%1,24,16,23;"		\
		"rlwimi %%r4,%1,8,16,23;"		\
		"rlwimi %%r3,%3,24,24,31;"		\
		"rlwimi %%r4,%3,0,24,31;"		\
												\
		"stw %%r3,0(%4);"			\
		"stw %%r4,4(%4);"			\
		"lfd %%f1,0(%4);"			\
		"stfdu %%f1,8(%0);"			\
											\
	    : "+b" (dst)						\
	    : "r" (Y2), "r" (U), "r" (V), "r" (tmp)		\
	    : "r3", "r4", "fr1");				\
	}
#endif
#endif


/******************************** DRAW ALPHA ******************************************/
// Draw_alpha series !
static void vo_draw_alpha_null (int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)
{
// Nothing
}

static void draw_alpha_422(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)
{
	register int x=0;
	register UWORD *dstbase;
	register UWORD *dst;
	ULONG width;

	if(!LockVLayer(VL_Handle)) return ;

	width = GetVLayerAttr(VL_Handle, VOA_Modulo) >> 1;
	dstbase = (UWORD *) (GetVLayerAttr(VL_Handle, VOA_BaseAddress) + (((y0 * width) + x0) << 1));

	while( h--){
		for(x=0;x<w;x++)
		{
	        dst = dstbase;
	        if(srca[x] != 0 && srca[x] < 192)
	        __asm__ __volatile__(
	        		"li %%r4,0x0080;"
	        		"rlwimi %%r4,%1,8,16,23;"
	        		"sth %%r4,0(%0);"
	        	:
	        	: "b" ( dst + x), "r" (src[x])
	        	: "r3", "r4"
	       	);
	    }
	    src+=       stride;
	    srca+=      stride;
	    dstbase+=   width;
	    }

	UnlockVLayer(VL_Handle);

}

static void draw_alpha_420p(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)
{
#if 1
	register UBYTE *dstbase;
	ULONG width;
	register int x = 0;

	if(!LockVLayer(VL_Handle)) return ;

	width = GetVLayerAttr(VL_Handle, VOA_Modulo) >> 1;
	dstbase = (UBYTE *)(GetVLayerAttr(VL_Handle, VOA_BaseAddress) + ((y0 * width) + x0));

	while(h--)
	{
		for(x=0;x<w;x++)
		{
            if(srca[x]) dstbase[x]=((dstbase[x]*srca[x])>>8)+src[x];
        }
		src+=stride;
		srca+=stride;
		dstbase+=width;
    }

	UnlockVLayer(VL_Handle);
#else

	register int x=0;
	register UBYTE *dstbase;
	register UBYTE *dst;
	ULONG width;

	if(!LockVLayer(VL_Handle)) return ;

	width = GetVLayerAttr(VL_Handle, VOA_Modulo) >> 1;
	dstbase = (UBYTE *)(GetVLayerAttr(VL_Handle, VOA_BaseAddress) + ((y0 * width) + x0));

	while( h--){
	    for(x=0;x<w;x++){
	        dst = dstbase;

	        if(srca[x] != 0 && srca[x] < 192)
			  dst[x] = src[x];
	    }
	    src+=       stride;
	    srca+=      stride;
	    dstbase+=   width;
	    }

	UnlockVLayer(VL_Handle);
#endif
}

/*****************************/

static inline void FillScreenWindow(void)
{
	if(!VL_Handle) return;

	// fill the whole display black hiding the colorkey
	if (vlayer_x_offset || vlayer_y_offset)
	{
		if (scr_truecolor)
		{
			FillPixelArray(My_Window->RPort,
						   0,
						   0,
						   My_Window->Width,
			               My_Window->Height,
			               0x00000000);
		}
		else
		{
			SetAPen(My_Window->RPort, scr_shadowpen);
			RectFill(My_Window->RPort,
					 0,
					 0,
					 My_Window->Width - 1,
					 My_Window->Height - 1);
		}
	}

	//Window size has changed !
	SetVLayerAttrTags(VL_Handle,
			VOA_LeftIndent,		vlayer_x_offset,
			VOA_RightIndent,	vlayer_x_offset,
			VOA_TopIndent,		vlayer_y_offset,
			VOA_BottomIndent,	vlayer_y_offset,
		TAG_DONE);

	// restore the colorkey

	// Always resore the display,
	// If the user set back the window size to the original one
	// then we still need to restore
	// if (x_offset || y_offset)
	{
		if (scr_truecolor)
		{
			FillPixelArray(My_Window->RPort,
					 vlayer_x_offset,
					 vlayer_y_offset,
					 My_Window->Width - vlayer_x_offset * 2,
					 My_Window->Height - vlayer_y_offset * 2,
			         GetVLayerAttr(VL_Handle, VOA_ColorKey));
		}
		else
		{
			SetAPen(My_Window->RPort, GetVLayerAttr(VL_Handle, VOA_ColorKeyPen));
			RectFill(My_Window->RPort,
					 vlayer_x_offset,
					 vlayer_y_offset,
					 My_Window->Width - 1 - vlayer_x_offset,
					 My_Window->Height - 1 - vlayer_y_offset);
		}
	}
}

static inline void Update_WinSize(void)
{
	ULONG x_offset;
	ULONG y_offset;

	if(!VL_Handle) return;

	// Change the window size to fill the whole screen with good aspect
	if ( ( (float) window_orig_width / (float)window_orig_height) < ( (float) window_width / (float) window_height) )
	{
		// Width (Longeur) is too big
		y_offset = 0;
		x_offset = window_width - window_height * ( (float) window_orig_width / window_orig_height);
		x_offset /= 2;
	}
	else
	{
		// Height (largeur) too big
		y_offset = window_height - window_width * ( (float) window_orig_height / window_orig_width);
		y_offset /= 2;
		x_offset = 0;
	}

	/* set global values for backfill hook */
	vlayer_x_offset = x_offset;
	vlayer_y_offset = y_offset;

	// fill the whole display black hiding the colorkey
	if (x_offset || y_offset)
	{
		if (scr_truecolor)
		{
			FillPixelArray(My_Window->RPort,
			               My_Window->BorderLeft,
			               My_Window->BorderTop,
			               My_Window->Width - My_Window->BorderLeft - My_Window->BorderRight,
			               My_Window->Height - My_Window->BorderTop - My_Window->BorderBottom,
			               0x00000000);
		}
		else
		{
			SetAPen(My_Window->RPort, scr_shadowpen);
			RectFill(My_Window->RPort,
			         My_Window->BorderLeft,
			         My_Window->BorderTop,
			         My_Window->Width - My_Window->BorderRight - 1,
			         My_Window->Height - My_Window->BorderBottom - 1);
		}
	}

	//Window size has changed !
	SetVLayerAttrTags(VL_Handle,
			VOA_LeftIndent,		x_offset,
			VOA_RightIndent,	x_offset,
			VOA_TopIndent,		y_offset,
			VOA_BottomIndent,	y_offset,
		TAG_DONE);

	// restore the colorkey

	// Always resore the display,
	// If the user set back the window size to the original one
	// then we still need to restore 
	// if (x_offset || y_offset)
	{
		if (scr_truecolor)
		{
			FillPixelArray(My_Window->RPort,
			         My_Window->BorderLeft + x_offset,
			         My_Window->BorderTop + y_offset,
			         My_Window->Width - My_Window->BorderLeft - My_Window->BorderRight - x_offset * 2,
			         My_Window->Height - My_Window->BorderTop - My_Window->BorderBottom - y_offset * 2,
			         GetVLayerAttr(VL_Handle, VOA_ColorKey));
		}
		else
		{
			SetAPen(My_Window->RPort, GetVLayerAttr(VL_Handle, VOA_ColorKeyPen));
			RectFill(My_Window->RPort,
			         My_Window->BorderLeft + x_offset,
			         My_Window->BorderTop + y_offset,
			         My_Window->Width - My_Window->BorderRight - 1 - x_offset,
			         My_Window->Height - My_Window->BorderBottom - 1 - y_offset);
		}
	}
}

static BOOL Cgx_Overlay_GiveArg(const char *arg)
{
	STRPTR PtrArg = (STRPTR) arg;

	// Default settings
	//cgx_overlay_mode = VLMODE_CHUNKY;

	/* parse (well sort of) */
	if ( PtrArg )
	{
		if(strstr(PtrArg, "PLANAR"))
		{
			cgx_overlay_mode = VLMODE_PLANAR;
		}
		if(strstr(PtrArg, "NOPLANAR"))
		{
			cgx_overlay_mode = VLMODE_CHUNKY;
		}
	}

	return TRUE;
}

/******************************** PREINIT ******************************************/

static int preinit(const char *arg)
{
	extern int fullscreen;
	mp_msg(MSGT_VO, MSGL_INFO, "VO: [cgx_overlay] Hello!\n");
	if(!CGXVideoBase)
	{
		if ( ! (CGXVideoBase = (struct Library *) OpenLibrary("cgxvideo.library" , 0L) ) )
		{
			return -1;
		}
	}

	/* common arguments  */
	if (!Cgx_GiveArg(arg))
	{
		CloseLibrary(CGXVideoBase);
		CGXVideoBase = NULL;
		return -1;	
	}

	/* common arguments  */
	if (!Cgx_Overlay_GiveArg(arg))
	{
		Cgx_ReleaseArg();
		CloseLibrary(CGXVideoBase);
		CGXVideoBase = NULL;
		return -1;
	}

	if(!fullscreen)
	{
		ULONG available = FALSE;
		struct Screen *screen = LockPubScreen ( PubScreenName[0] ? PubScreenName : NULL);
		if(screen)
		{
			APTR VLHandle = CreateVLayerHandleTags(screen, VOA_SrcType, SRCFMT_RGB16, VOA_SrcWidth, 320, VOA_SrcHeight, 240, TAG_DONE);

			if(VLHandle)
			{
				available = TRUE;
				DeleteVLayerHandle(VLHandle);
				VLHandle = NULL;
			}

			UnlockPubScreen(NULL,screen);
		}

		if(!available)
		{
			Cgx_ReleaseArg();
			CloseLibrary(CGXVideoBase);
			CGXVideoBase = NULL;
			return -1;
		}
	}

	Cgx_Message();

	return 0;
}

/****************************************************************************/
// Open window in FullScreen for MPlayer
static void Open_FullScreen(void)
{
	// if fullscreen -> let's open our own screen
	uint32_t screen_width = window_orig_width;
	uint32_t screen_height = window_orig_height;

	uint32_t this_win_height = screen_height;
	uint32_t this_win_width  = screen_width;

	window_width = window_orig_width;
	window_height = window_orig_height;

	mp_msg(MSGT_VO, MSGL_INFO, "VO: [cgx_overlay] Full screen.\n");
	if (cgx_monitor) mp_msg(MSGT_VO, MSGL_INFO, "VO: [cgx_overlay] Prefered screen is : %s\n", cgx_monitor);
	
	My_Window = NULL; //paranoia
	
	My_Screen = OpenScreenTags ( NULL,
			WantedModeID ? TAG_IGNORE : SA_LikeWorkbench, TRUE,
			WantedModeID ? SA_DisplayID : TAG_IGNORE, WantedModeID,
#if PUBLIC_SCREEN
			SA_Type, PUBLICSCREEN,
			SA_PubName, "MPlayer Screen",
#else
			SA_Type, CUSTOMSCREEN,
#endif
			SA_Title, "MPlayer Screen",
            SA_ShowTitle, FALSE,
			SA_Quiet, TRUE,
	TAG_DONE);

	if ( ! My_Screen )
	{
		uninit();
		return ;
	}

#if PUBLIC_SCREEN
	PubScreenStatus( My_Screen, 0 );
#endif

	screen_width  = My_Screen->Width;
	screen_height = My_Screen->Height;

	update_scrvars(My_Screen);

	if (scr_truecolor)
	{
		FillPixelArray( &(My_Screen->RastPort), 0,0, screen_width, screen_height, 0x00000000);
	}
	else
	{
		SetRast(&(My_Screen->RastPort), scr_shadowpen);
	}

	mp_msg(MSGT_VO, MSGL_INFO, "VO: [cgx_overlay] Screen selected: %ldx%ld.\n", screen_width, screen_height);

	// Change the window size to fill the whole screen with good aspect
	if ( ( (float) this_win_width / this_win_height) > ( (float) screen_width / screen_height) )
	{
		// window_width = screen_width !
		this_win_height = screen_width * window_orig_height / window_orig_width;
		this_win_width = screen_width;
	}
	else
	{
		this_win_width = screen_height * window_orig_width / window_orig_height;
		this_win_height = screen_height;
	}

	this_win_width = ( this_win_width > screen_width ) ? screen_width : this_win_width;
	this_win_height = ( this_win_height > screen_height ) ? screen_height : this_win_height;

	vlayer_x_offset = (screen_width - this_win_width)/2;
	vlayer_y_offset = (screen_height - this_win_height)/2;

	mp_msg(MSGT_VO, MSGL_INFO, "VO: [cgx_overlay] window size: %ldx%ld.\n", this_win_width, this_win_height);

	My_Window = OpenWindowTags( NULL,
#if PUBLIC_SCREEN
				WA_PubScreen,  (ULONG) My_Screen,
#else
				WA_CustomScreen,  (ULONG) My_Screen,
#endif
				WA_Top,           0,
				WA_Left,          0,
				WA_Height,        My_Screen->Height,
				WA_Width,         My_Screen->Width,
				WA_CloseGadget,   FALSE,
				WA_DragBar,       FALSE,
				WA_SizeGadget,	  FALSE,
				WA_Borderless,    TRUE,
				WA_Backdrop,      TRUE,
				WA_Activate,      TRUE,
				WA_IDCMP,         IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY | IDCMP_INACTIVEWINDOW | IDCMP_ACTIVEWINDOW,
				WA_Flags,         WFLG_REPORTMOUSE,
		TAG_DONE);

	if ( ! My_Window)
	{
		uninit();
		return;
	}

	vo_screenwidth = My_Screen->Width;
	vo_screenheight = My_Screen->Height;
	vo_dwidth = vo_screenwidth - 2*vlayer_x_offset;
	vo_dheight = vo_screenheight - 2*vlayer_y_offset;
	vo_fs = 1;

	Cgx_ControlBlanker(My_Screen, FALSE);
}

extern char *filename;
extern char PubScreenName[128];

// Open window for MPlayer
static void Open_Window(void)
{		
	struct Screen *scr = NULL;
	static BOOL FirstTime = TRUE;
	BOOL WindowActivate = TRUE;

	// Window
	mp_msg(MSGT_VO, MSGL_INFO, "VO: [cgx_overlay] Window mode.\n");

	My_Window = NULL;
	My_Screen = NULL;

	window_width = window_orig_width;
	window_height = window_orig_height;

	// Leo: if while mplayer is in fullscreen, we got other screens
	//      than ambient, and we do a full cycle of opened screens,
	//      falling back to window mode could open mplayer window in
	//      another screen than Ambient's screen

	// Leo sucks...

	scr = LockPubScreen ( PubScreenName[0] ? PubScreenName : NULL);

	if (scr)
	{
		struct DrawInfo *dri;
        int screen_width = scr->Width;
        int screen_height = scr->Height;

		if ((dri = GetScreenDrawInfo(scr)))
		{	

			switch(Cgx_BorderMode)
			{
				case NOBORDER:
					BorderWidth = 0;
					BorderHeight = 0;
					break;

				default:
					BorderWidth = GetSkinInfoAttrA(dri, SI_BorderLeft, NULL) +
					GetSkinInfoAttrA(dri, SI_BorderRight, NULL);

					BorderHeight = GetSkinInfoAttrA(dri, SI_BorderTopTitle, NULL) +
					GetSkinInfoAttrA(dri, SI_BorderBottom, NULL);
			}

			if (screen_width < window_width+BorderWidth || screen_height < window_height+BorderHeight)
			{
				if ( ( (float) window_width / window_height) > ( (float) screen_width / screen_height) )
				{
					window_height = screen_width * window_height / window_width;
					window_width = screen_width-BorderWidth;
				}
				else
				{
					window_width = screen_height * window_width / window_height;
					window_height = screen_height-BorderHeight;
				}

			  window_width = ( window_width > (screen_width-BorderWidth) ) ? screen_width-BorderWidth : window_width;
			  window_height = ( window_height > (screen_height-BorderHeight) ) ? screen_height-BorderHeight : window_height;
			}

			if (FirstTime)
			{
				//win_left = (scr->Width - (window_width + BorderWidth)) / 2;
				//win_top  = (scr->Height - (window_height + BorderHeight)) / 2;
				win_left = vo_dx;
				win_top = vo_dy;
				FirstTime = FALSE;
			}

			ZoomData[2] = window_orig_width + BorderWidth;
			ZoomData[3] = window_orig_height + BorderHeight;

			FreeScreenDrawInfo(scr,dri);
		}

		update_scrvars(scr);

#ifdef CONFIG_GUI
	    if (use_gui)
			WindowActivate = FALSE;
#endif

		switch(Cgx_BorderMode)
		{
			case NOBORDER:
				My_Window = OpenWindowTags( NULL,
					WA_PubScreen,     (ULONG) scr,
					WA_Left,          win_left,
					WA_Top,           win_top,
					WA_InnerWidth,    window_width,
					WA_InnerHeight,   window_height,
					WA_MaxWidth,      window_orig_width*4 + BorderWidth,
					WA_MinWidth,      window_orig_width/2 + BorderWidth,
					WA_MaxHeight,     window_orig_height*4 + BorderHeight,
					WA_MinHeight,     window_orig_height/2 + BorderHeight,
					/*WA_SizeNumerator, window_orig_width,
					WA_SizeDenominator, window_orig_height,
					WA_SizeExtraWidth, BorderWidth,
					WA_SizeExtraHeight, BorderHeight,*/
					WA_CloseGadget,   FALSE,
					WA_DragBar,       FALSE,
					WA_Borderless,    TRUE,
					WA_SizeBBottom,   FALSE,
					WA_DepthGadget,   FALSE,
					WA_SizeGadget,    FALSE,
					WA_Activate,      WindowActivate,
					WA_SimpleRefresh, TRUE,
					WA_BackFill,      (ULONG) &wbackfillhook,
					WA_IDCMP,         IDCMP_MOUSEBUTTONS | IDCMP_INACTIVEWINDOW | IDCMP_ACTIVEWINDOW | IDCMP_CHANGEWINDOW | IDCMP_MOUSEMOVE | IDCMP_REFRESHWINDOW | IDCMP_RAWKEY /*| IDCMP_VANILLAKEY*/ | IDCMP_CLOSEWINDOW,
					WA_Flags,         WFLG_REPORTMOUSE,
					WA_SkinInfo,      NULL,
				TAG_DONE);
			break;

			case TINYBORDER:
				My_Window = OpenWindowTags( NULL,
					WA_PubScreen,     (ULONG) scr,
					WA_Left,          win_left,
					WA_Top,           win_top,
					WA_InnerWidth,    window_width,
					WA_InnerHeight,   window_height,
					WA_MaxWidth,      window_orig_width*4 + BorderWidth,
					WA_MinWidth,      window_orig_width/2 + BorderWidth,
					WA_MaxHeight,     window_orig_height*4 + BorderHeight,
					WA_MinHeight,     window_orig_height/2 + BorderHeight,
					/*WA_SizeNumerator, window_orig_width,
					WA_SizeDenominator, window_orig_height,
					WA_SizeExtraWidth, BorderWidth,
					WA_SizeExtraHeight, BorderHeight,*/
					WA_CloseGadget,   FALSE,
					WA_DragBar,       FALSE,
					WA_Borderless,    FALSE,
					WA_SizeBBottom,   FALSE,
					WA_DepthGadget,   FALSE,
					WA_SizeGadget,    FALSE,
					WA_Activate,      WindowActivate,
					WA_SimpleRefresh, TRUE,
					WA_BackFill,      (ULONG) &wbackfillhook,
					WA_IDCMP,         IDCMP_MOUSEBUTTONS | IDCMP_INACTIVEWINDOW | IDCMP_ACTIVEWINDOW | IDCMP_CHANGEWINDOW | IDCMP_MOUSEMOVE | IDCMP_REFRESHWINDOW | IDCMP_RAWKEY /*| IDCMP_VANILLAKEY*/ | IDCMP_CLOSEWINDOW,
					WA_Flags,         WFLG_REPORTMOUSE,
					WA_SkinInfo,      NULL,
				TAG_DONE);
			break;

			default:
				My_Window = OpenWindowTags( NULL,
					WA_PubScreen,     (ULONG) scr,
					WA_Title,         (ULONG) filename ? MorphOS_GetWindowTitle() : "MPlayer for MorphOS",
					WA_ScreenTitle,   (ULONG) "MPlayer " VERSION " for MorphOS",
					WA_Left,          win_left,
					WA_Top,           win_top,
					WA_InnerWidth,    window_width,
					WA_InnerHeight,   window_height,
					WA_MaxWidth,      window_orig_width*4 + BorderWidth,
					WA_MinWidth,      window_orig_width/2 + BorderWidth,
					WA_MaxHeight,     window_orig_height*4 + BorderHeight,
					WA_MinHeight,     window_orig_height/2 + BorderHeight,
					/*WA_SizeNumerator, window_orig_width,
					WA_SizeDenominator, window_orig_height,
					WA_SizeExtraWidth, BorderWidth,
					WA_SizeExtraHeight, BorderHeight,*/
					WA_CloseGadget,   TRUE,
					WA_DragBar,       TRUE,
					WA_Borderless,    FALSE,
					WA_SizeBBottom,   TRUE,
					WA_DepthGadget,   TRUE,
					WA_SizeGadget,    TRUE,
					WA_Zoom,          ZoomData,
					WA_Activate,      WindowActivate,
					WA_SimpleRefresh, TRUE,
					WA_BackFill,      (ULONG) &wbackfillhook,
					WA_IDCMP,         IDCMP_MOUSEBUTTONS | IDCMP_INACTIVEWINDOW | IDCMP_ACTIVEWINDOW | IDCMP_CHANGEWINDOW | IDCMP_MOUSEMOVE | IDCMP_REFRESHWINDOW | IDCMP_RAWKEY /*| IDCMP_VANILLAKEY*/ | IDCMP_CLOSEWINDOW,
					WA_Flags,         WFLG_REPORTMOUSE,
					WA_SkinInfo,      NULL,
			TAG_DONE);

		}

		vo_screenwidth = scr->Width;
		vo_screenheight = scr->Height;
		vo_dwidth = My_Window->Width - (My_Window->BorderLeft + My_Window->BorderRight);
		vo_dheight = My_Window->Height - (My_Window->BorderBottom + My_Window->BorderTop);
		vo_fs = 0;

		UnlockPubScreen(NULL, scr);

		if ( ! My_Window)
		{	
			uninit();
			return;
		}

		mp_msg(MSGT_VO, MSGL_INFO, "VO: [cgx_overlay] : window size: %ldx%ld.\n", window_width, window_height);

		ScreenToFront(My_Window->WScreen);

		Cgx_StartWindow(My_Window);

		Cgx_ControlBlanker(My_Screen, FALSE);
	}
}

/***********************/
static BOOL Make_VLayer(ULONG Mode)
{
	char* draw_primitive = "unknown";

	if(!CGXVideoBase)
	{
		CGXVideoBase = (struct Library *) OpenLibrary("cgxvideo.library" , 0L);
	}

	if(!CGXVideoBase)
	{
		mp_msg(MSGT_VO, MSGL_INFO, "VO: [cgx_overlay] : cgxvideo.library couldn't be opened.\n");
		return FALSE;
	}

	if (CGXVideoBase &&  (CGXVideoBase->lib_Version > 43 || (CGXVideoBase->lib_Version == 43 && CGXVideoBase->lib_Revision >= 6)))
	{
		mp_msg(MSGT_VO, MSGL_INFO, "VO: [cgx_overlay] : cgxvideo.library >= 43.6 detected, using doublebuffer\n");
		use_overlay_doublebuffer = 1;
	}
	else
	{
		use_overlay_doublebuffer = 0;
	}

	if (CGXVideoBase->lib_Version > 43 || (CGXVideoBase->lib_Version == 43 && CGXVideoBase->lib_Revision >= 11))
	{
		 ULONG features  = QueryVLayerAttr(My_Window->WScreen, VSQ_SupportedFeatures);
		 ULONG formats   = QueryVLayerAttr(My_Window->WScreen, VSQ_SupportedFormats);
		 ULONG max_width = QueryVLayerAttr(My_Window->WScreen, VSQ_MaxWidth);

		 if (image_width > max_width)
			 image_width = max_width & -8;

		 if (use_overlay_doublebuffer)
			 use_overlay_doublebuffer = (features & VSQ_FEAT_DOUBLEBUFFER);

		 if (Mode == VLMODE_PLANAR)
			 Mode = (formats & VSQ_FMT_YUV420_PLANAR) ? VLMODE_PLANAR : VLMODE_CHUNKY;
	}

	switch(Mode)
	{
		case VLMODE_PLANAR:
			VL_Handle = CreateVLayerHandleTags(My_Window->WScreen,
								   VOA_SrcType,      SRCFMT_YCbCr420,
		                           VOA_UseColorKey,  TRUE,
								   VOA_UseBackfill,  FALSE,
		                           VOA_SrcWidth,     image_width,
		                           VOA_SrcHeight,    image_height,
								   use_overlay_doublebuffer ? VOA_DoubleBuffer : TAG_IGNORE, TRUE,
								   TAG_DONE);

			if(!VL_Handle)
			{
				mp_msg(MSGT_VO, MSGL_INFO, "VO: [cgx_overlay_gui] : PLANAR overlay mode couldn't be created, using CHUNKY overlay mode instead\n");
				// Fall through to chunky mode
			}
			else
			{
				break;
			}

		case VLMODE_CHUNKY:
			VL_Handle = CreateVLayerHandleTags(My_Window->WScreen,
									VOA_SrcType, SRCFMT_YCbCr16,
									VOA_UseColorKey,  TRUE,
									VOA_UseBackfill,  FALSE,
									VOA_SrcWidth,     image_width,
									VOA_SrcHeight,    image_height,
									use_overlay_doublebuffer ? VOA_DoubleBuffer : TAG_IGNORE, TRUE,
								TAG_DONE);

			if(!VL_Handle)
			{
				mp_msg(MSGT_VO, MSGL_INFO, "VO: [cgx_overlay] : CHUNKY overlay mode couldn't be created.\n");
			}

			break;

	}

	if(!VL_Handle)
	{
		// Problem
		VL_Handle = NULL;
		uninit();
		return FALSE;
	}

	/* Set drawing functions depending on overlay target mode */
	switch(Mode)
	{
		case VLMODE_CHUNKY:
			vo_draw_alpha_func = draw_alpha_422;
			break;

		case VLMODE_PLANAR:
			if(pixel_format == IMGFMT_422P)
			{
				real_draw_slice = draw_slice_422p_to_420p;
			}
			else
			{
				real_draw_slice = draw_slice_420p;
			}
			vo_draw_alpha_func = draw_alpha_420p;
			break;
	}

	/* User feedback */
	if(real_draw_slice == draw_slice_420p_to_422)
	{
		draw_primitive = "draw_slice_420p_to_422 (CHUNKY)";
	}
	else if(real_draw_slice == draw_slice_422p_to_420p)
	{
		draw_primitive = "draw_slice_422p_to_420p (PLANAR)";
	}
	else if(real_draw_slice == draw_slice_422p_to_422)
	{
		draw_primitive = "draw_slice_422p_to_422 (CHUNKY)";
	}
	else if(real_draw_slice == draw_slice_422)
	{
		draw_primitive = "draw_slice_422 (CHUNKY)";
	}
	else if(real_draw_slice == draw_slice_420p)
	{
		draw_primitive = "draw_slice_420p (PLANAR)";
	}

	mp_msg(MSGT_VO, MSGL_INFO, "VO: [cgx_overlay] : using %s primitive\n", draw_primitive);


	if ( AttachVLayerTags(VL_Handle, My_Window, TAG_DONE))
	{
		// Problem
		DeleteVLayerHandle(VL_Handle);
		VL_Handle = NULL;
		uninit();
		return FALSE;
	}

	return TRUE;
}


/******************************** CONFIG ******************************************/
static int config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format)
{
	real_draw_slice = draw_slice_420p_to_422; /* default rountine, don't overwrite without reason */
	vo_draw_alpha_func = 0;

	if (My_Window)
	{
		Cgx_ControlBlanker(My_Screen, TRUE);

		if(VL_Handle)
		{
			DetachVLayer(VL_Handle);
			DeleteVLayerHandle(VL_Handle);
			VL_Handle = NULL;
		}

		Cgx_Stop(My_Window);

#ifdef CONFIG_GUI
		if (use_gui)
			gui(GUI_SET_WINDOW_PTR, NULL);
#endif

        Cgx_StopWindow(My_Window);
		CloseWindow(My_Window);

		if(is_fullscreen)
		{
			CloseScreen(My_Screen);
		}
	}

	is_fullscreen = fullscreen;

	My_Screen = NULL;
	My_Window = NULL;

	// backup info
	image_width = width & -8;
	image_height = height & -2;

	window_width = d_width & -8;
	window_height = d_height & -2;

	window_orig_width = d_width & -8;
	window_orig_height = d_height & -2;

	pixel_format = format;

	if ( (query_format(format) & VFCAP_CSP_SUPPORTED) == 0)
	{
		uninit();
		return -1;
	}

	/* default drawing routines, ignoring target overlay mode at this point */
	switch(format)
	{
		case IMGFMT_YUY2:
			cgx_overlay_mode = VLMODE_CHUNKY;
			real_draw_slice	= draw_slice_422;
			 /* fall through */

		case IMGFMT_YV12:
		case IMGFMT_I420:
		case IMGFMT_IYUV:
			vo_draw_alpha_func = draw_alpha_422;
			break;

		case IMGFMT_422P:
			real_draw_slice = draw_slice_422p_to_422;
			vo_draw_alpha_func = draw_alpha_422;
			break;
	}

	if ( fullscreen )
	{
		Open_FullScreen();
	}
	else
	{
		Open_Window();
	}
	
	EmptyPointer = AllocVec(16, MEMF_PUBLIC | MEMF_CLEAR);

	if (! My_Window || !EmptyPointer)
	{	
		uninit();
		return -1;
	}

	if ( ! Make_VLayer(cgx_overlay_mode) )
	{	
		uninit();
		return -1;
	}

	if ( !fullscreen )
		My_Screen = NULL;	

	if( fullscreen )
	{
		FillScreenWindow();
	}

	Cgx_Start(My_Window);

#ifdef CONFIG_GUI
    if (use_gui)
		gui(GUI_SET_WINDOW_PTR, (void *) My_Window);
#endif

	return 0; // -> Ok
}

/******************************** DRAW_SLICE ******************************************/

static int draw_slice(uint8_t *image[], int stride[], int w, int h, int x, int y)
{
	return real_draw_slice(image, stride, w, h, x, y);
}

static uint32_t draw_slice_420p(uint8_t *image[], int stride[], int w, int h, int x, int y)
{
	UBYTE *pY, *pCb, *pCr;
	UBYTE *sY, *sCb, *sCr;
	ULONG ptY, stY, ptCb, stCb, ptCr, stCr;
	ULONG w2 = w >> 1;

	if (y+h > image_height)
	{
		if ((h = image_height - y) < 2)
			return -1;
	}

	h &= -2;
	w &= -2;
	sY = image[0];
	sCb = image[1];
	sCr = image[2];
	stY = stride[0];
	stCb = stride[1];
	stCr = stride[2];

	if (!LockVLayer(VL_Handle))
		return -1;

	ptY = GetVLayerAttr(VL_Handle, VOA_Modulo) >> 1;
	ptCr = ptCb = ptY >> 1;
	pY = (UBYTE *)GetVLayerAttr(VL_Handle, VOA_BaseAddress);
	pCb = pY + (ptY * image_height);
	pCr = pCb + ((ptCb * image_height) >> 1);
	pY += (y * ptY) + x;
	pCb += ((y * ptCb) >> 1) + (x >> 1);
	pCr += ((y * ptCr) >> 1) + (x >> 1);

	if (stY == ptY && w == image_width)
	{
		CopyMem(sY, pY, ptY * h);
		CopyMem(sCb, pCb, (ptCb * h) >> 1);
		CopyMem(sCr, pCr, (ptCr * h) >> 1);
	}
	else do
	{
		CopyMem(sY, pY, w);

		pY += ptY;
		sY += stY;

		CopyMem(sY, pY, w);
		CopyMem(sCb, pCb, w2);
		CopyMem(sCr, pCr, w2);

		sY += stY;
		sCb += stCb;
		sCr += stCr;

		pY += ptY;
		pCb += ptCb;
		pCr += ptCr;

		h -= 2;
	} while (h > 0);

	UnlockVLayer(VL_Handle);

	return 0;
}

static uint32_t draw_slice_422(uint8_t *image[], int stride[], int w, int h, int x, int y)
{
	UBYTE *dYUV;
	UBYTE *sYUV;
	ULONG dtYUV, stYUV;

	if (y+h > image_height)
	{
		if ((h = image_height - y) < 1)
			return -1;
	}

	sYUV = image[0];
	stYUV = stride[0];

	if (!LockVLayer(VL_Handle))
		return -1;

	dtYUV = GetVLayerAttr(VL_Handle, VOA_Modulo);
	dYUV = (UBYTE *)GetVLayerAttr(VL_Handle, VOA_BaseAddress);
	dYUV += (y * dtYUV) + x;

	if (stYUV == dtYUV && w == image_width)
	{
		CopyMem(sYUV, dYUV, dtYUV * h);
	}
	else do
	{
		CopyMem(sYUV, dYUV, w);
		dYUV += dtYUV;
		sYUV += stYUV;
	} while (--h > 0);

	UnlockVLayer(VL_Handle);

	return 0;
}

static uint32_t draw_slice_422p_to_420p(uint8_t *image[], int stride[], int w, int h, int x, int y)
{
	UBYTE *pY, *pCb, *pCr;
	UBYTE *sY, *sCb, *sCr;
	ULONG ptY, stY, ptCb, stCb, ptCr, stCr;
	ULONG w2 = w >> 1;

	if (y+h > image_height)
	{
		if ((h = image_height - y) < 2)
			return -1;
	}

	h &= -2;
	w &= -2;
	sY = image[0];
	sCb = image[1];
	sCr = image[2];
	stY = stride[0];
	stCb = stride[1] << 1;
	stCr = stride[2] << 1;

	if (!LockVLayer(VL_Handle))
		return -1;

	ptY = GetVLayerAttr(VL_Handle, VOA_Modulo) >> 1;
	ptCr = ptCb = ptY >> 1;
	pY = (UBYTE *)GetVLayerAttr(VL_Handle, VOA_BaseAddress);
	pCb = pY + (ptY * image_height);
	pCr = pCb + ((ptCb * image_height) >> 1);
	pY += (y * ptY) + x;
	pCb += ((y * ptCb) >> 1) + (x >> 1);
	pCr += ((y * ptCr) >> 1) + (x >> 1);

	do
	{
		CopyMem(sY, pY, w);

		pY += ptY;
		sY += stY;

		CopyMem(sY, pY, w);
		CopyMem(sCb, pCb, w2);
		CopyMem(sCr, pCr, w2);

		sY += stY;
		sCb += stCb;
		sCr += stCr;

		pY += ptY;
		pCb += ptCb;
		pCr += ptCr;

		h -= 2;
	} while (h > 0);

	UnlockVLayer(VL_Handle);

	return 0;
}

static uint32_t draw_slice_420p_to_422(uint8_t *image[], int stride[], int w, int h, int x, int y)
{
	unsigned int _w2;
	ULONG dststride;

	if (y+h > image_height)
	{
		if ((h = image_height - y) < 2)
			return -1;
	}

	_w2 = w >> 3; // Because we write 8 pixels (aka 16 bit word) at each loop
	h &= -2;

	if (h > 0 && _w2)
	{
		ULONG *dstdata, *dstdata2;
		ULONG *_py, *_py2, *_pu, *_pv;
		ULONG stride0, stride0_2, stride1, stride2;

		// The data must be stored like this:
		// YYYYYYYY UUUUUUUU : pixel 0
		// YYYYYYYY VVVVVVVV : pixel 1

		// We will read one word of Y (4 byte)
		// then read one word of U and V
		// After another Y word because we need 2 Y for 1 (U & V)

		stride0 = stride[0];
		stride1 = stride[1];
		stride2 = stride[2];
		stride0_2 = stride0 << 1;

		_py = (ULONG *) image[0];
		_pu = (ULONG *) image[1];
		_pv = (ULONG *) image[2];

		_py--;
		_pu--;
		_pv--;

		if (!LockVLayer(VL_Handle))
			return -1;

		dststride = GetVLayerAttr(VL_Handle, VOA_Modulo);
		dstdata = (ULONG *)(GetVLayerAttr(VL_Handle, VOA_BaseAddress) + ((y * dststride) + (x << 1)));

#if USE_VMEM64
		/* If aligned buffer, use 64bit writes. It might be worth
		   the effort to manually align in other cases, but that'd
		   need to handle all conditions such like:
		   a) UWORD, UQUAD ... [end aligment]
		   b) ULONG, UQUAD ... [end aligment]
		   c) ULONG, UWORD, UQUAD ... [end aligment]
		   - Piru
		*/
		if (!(((IPTR)dstdata) & 7))
		{
			ULONG _tmp[3];
			// this alignment crap is needed in case we don't have aligned stack
			register ULONG *tmp = (APTR)((((ULONG) _tmp) + 4) & -8);

			dstdata -= 2;

			do
			{
				ULONG *py, *py2, *pu, *pv;
				int w2 = _w2;

				h -= 2;

				_py2 = (ULONG *)(((ULONG)_py) + stride0);

				py = _py;
				py2 = _py2;
				pu = _pu;
				pv = _pv;

				dstdata2 = (ULONG *)(((ULONG)dstdata) + dststride);

				do
				{
					// Y is like that : Y1 Y2 Y3 Y4
					// U is like that : U1 U2 U3 U4
					// V is like that : V1 V2 V3 V4

					PLANAR2_2_CHUNKY_64(py, py2, pu, pv, dstdata, dstdata2, tmp);

				} while (--w2);

				dstdata = (ULONG *)(((ULONG)dstdata) + dststride);
				_py = (ULONG *)(((ULONG)_py) + stride0_2);
				_pu = (ULONG *)(((ULONG)_pu) + stride1);
				_pv = (ULONG *)(((ULONG)_pv) + stride2);

			} while (h > 0);
		}
		else
#endif
		{
			dstdata--;

			do
			{
				ULONG *py, *py2, *pu, *pv;
				int w2 = _w2;

				h -= 2;

				_py2 = (ULONG *)(((ULONG)_py) + stride0);

				py = _py;
				py2 = _py2;
				pu = _pu;
				pv = _pv;

				dstdata2 = (ULONG *)(((ULONG)dstdata) + dststride);

				do
				{
					// Y is like that : Y1 Y2 Y3 Y4
					// U is like that : U1 U2 U3 U4
					// V is like that : V1 V2 V3 V4

					PLANAR2_2_CHUNKY(py, py2, pu, pv, dstdata, dstdata2);

				} while (--w2);

				dstdata = (ULONG *)(((ULONG)dstdata) + dststride);
				_py = (ULONG *)(((ULONG)_py) + stride0_2);
				_pu = (ULONG *)(((ULONG)_pu) + stride1);
				_pv = (ULONG *)(((ULONG)_pv) + stride2);

			} while (h > 0);
		}

		UnlockVLayer(VL_Handle);
	}

	return 0;
}

static uint32_t draw_slice_422p_to_422(uint8_t *image[], int stride[], int w, int h, int x, int y)
{
	unsigned int _w2;
	ULONG dststride;

	if (y+h > image_height)
	{
		if ((h = image_height - y) < 1)
			return -1;
	}

	_w2 = w >> 2; // Because we write 4 pixels (aka 16 bit word) at each loop

	if (h > 0 && _w2)
	{
		ULONG *dstdata;
		ULONG *_py, *_pu, *_pv;
		ULONG stride0, stride1, stride2;

		// The data must be stored like this:
		// YYYYYYYY UUUUUUUU : pixel 0
		// YYYYYYYY VVVVVVVV : pixel 1

		// We will read one word of Y (4 byte)
		// then read one word of U and V
		// After another Y word because we need 2 Y for 1 (U & V)

		stride0 = stride[0];
		stride1 = stride[1];
		stride2 = stride[2];

		_py = (ULONG *) image[0];
		_pu = (ULONG *) image[1];
		_pv = (ULONG *) image[2];

		_py--;
		_pu--;
		_pv--;

		if (!LockVLayer(VL_Handle))
			return -1;

		dststride = GetVLayerAttr(VL_Handle, VOA_Modulo);
		dstdata = (ULONG *)(GetVLayerAttr(VL_Handle, VOA_BaseAddress) + ((y * dststride) + (x << 1)));

#if USE_VMEM64
		/* If aligned buffer, use 64bit writes. It might be worth
		   the effort to manually align in other cases, but that'd
		   need to handle all conditions such like:
		   a) UWORD, UQUAD ... [end aligment]
		   b) ULONG, UQUAD ... [end aligment]
		   c) ULONG, UWORD, UQUAD ... [end aligment]
		   - Piru
		*/
		if (!(((IPTR)dstdata) & 7))
		{
			ULONG _tmp[3];
			// this alignment crap is needed in case we don't have aligned stack
			register ULONG *tmp = (APTR)((((ULONG) _tmp) + 4) & -8);

			dstdata -= 2;

			do
			{
				ULONG *py, *pu, *pv, *dst;
				int w2 = _w2;

				dst = dstdata;
				py = _py;
				pu = _pu;
				pv = _pv;

				do
				{
					// Y is like that : Y1 Y2 Y3 Y4
					// U is like that : U1 U2 U3 U4
					// V is like that : V1 V2 V3 V4

					PLANAR_2_CHUNKY_64(py, pu, pv, dst, tmp);

				} while (--w2);

				dstdata = (ULONG *)(((ULONG)dstdata) + dststride);
				_py = (ULONG *)(((ULONG)_py) + stride0);
				_pu = (ULONG *)(((ULONG)_pu) + stride1);
				_pv = (ULONG *)(((ULONG)_pv) + stride2);

			} while (--h);
		}
		else
#endif
		{
			dstdata--;

			do
			{
				ULONG *py, *pu, *pv, *dst;
				int w2 = _w2;

				dst = dstdata;
				py = _py;
				pu = _pu;
				pv = _pv;

				do
				{
					// Y is like that : Y1 Y2 Y3 Y4
					// U is like that : U1 U2 U3 U4
					// V is like that : V1 V2 V3 V4

					PLANAR_2_CHUNKY(py, pu, pv, dst);

				} while (--w2);

				dstdata = (ULONG *)(((ULONG)dstdata) + dststride);
				_py = (ULONG *)(((ULONG)_py) + stride0);
				_pu = (ULONG *)(((ULONG)_pu) + stride1);
				_pv = (ULONG *)(((ULONG)_pv) + stride2);

			} while (--h);
		}

		UnlockVLayer(VL_Handle);
	}

	return 0;
}

/******************************** DRAW_OSD ******************************************/

static void draw_osd (void)
{
	vo_draw_text(image_width,image_height,vo_draw_alpha_func);
}

/******************************** FLIP_PAGE ******************************************/
static void flip_page (void)
{
	if(use_overlay_doublebuffer)
		SwapVLayerBuffer(VL_Handle);
}
/******************************** DRAW_FRAME ******************************************/

static int draw_frame(uint8_t *src[])
{
	return -1;
}

/******************************** CONTROL ******************************************/
static int control(uint32_t request, void *data)
{
	switch (request)
	{
		case VOCTRL_GUISUPPORT:
			return VO_TRUE;

		case VOCTRL_FULLSCREEN:

#ifdef CONFIG_GUI
			if (use_gui)
				gui(GUI_SET_WINDOW_PTR, NULL);
#endif

			if(is_fullscreen)
			{
				
				Cgx_ControlBlanker(My_Screen, TRUE);

				// From FS -> Window
				DetachVLayer(VL_Handle);
				DeleteVLayerHandle(VL_Handle);
				VL_Handle = NULL;

				Cgx_Stop(My_Window);
                Cgx_StopWindow(My_Window);
				CloseWindow(My_Window);
				My_Window = NULL;

				CloseScreen(My_Screen);
				My_Screen = NULL;

				Open_Window();

				if (!My_Window) return VO_FALSE;

				Cgx_Start(My_Window);

				is_fullscreen = 0;
			}
			else
			{
				Cgx_ControlBlanker(My_Screen, TRUE);

				// From Window -> FS
				DetachVLayer(VL_Handle);
				DeleteVLayerHandle(VL_Handle);
				VL_Handle = NULL;

				Cgx_Stop(My_Window);
                Cgx_StopWindow(My_Window);
				CloseWindow(My_Window);
				My_Window = NULL;

				Open_FullScreen();
				if (!My_Window) return VO_FALSE;

				Cgx_Start(My_Window);
				Cgx_StopWindow(My_Window);
				Cgx_ShowMouse(My_Screen, My_Window, TRUE);

				is_fullscreen = 1;
			}

			if ( ! Make_VLayer(cgx_overlay_mode) ) return VO_FALSE;
			if ( !is_fullscreen ) My_Screen = NULL;	
			if ( is_fullscreen )  FillScreenWindow();
			if ( !is_fullscreen ) Update_WinSize();

#ifdef CONFIG_GUI
			if (use_gui)
				gui(GUI_SET_WINDOW_PTR, (void *) My_Window);
#endif

			return VO_TRUE;

		case VOCTRL_PAUSE:
			if (My_Window) Cgx_Stop(My_Window);
			Cgx_ControlBlanker(My_Screen, TRUE);
			return VO_TRUE;					

		case VOCTRL_RESUME:
			if (My_Window) Cgx_Start(My_Window);
			Cgx_ControlBlanker(My_Screen, FALSE);
			return VO_TRUE;

		case VOCTRL_QUERY_FORMAT:
			return query_format(*(ULONG *)data);

		case VOCTRL_UPDATE_SCREENINFO:
			if (is_fullscreen)
			{
				vo_screenwidth = My_Screen->Width;
				vo_screenheight = My_Screen->Height;
			}
			else
			{
				struct Screen *s = LockPubScreen ( PubScreenName[0] ? PubScreenName : NULL);
				if(s)
				{
					vo_screenwidth = s->Width;
					vo_screenheight = s->Height;
					UnlockPubScreen(NULL, s);
				}
	        }
	        aspect_save_screenres(vo_screenwidth, vo_screenheight);
			return VO_TRUE;
	}

	return VO_NOTIMPL;
}

static void check_events(void)
{
	uint32_t oldwidth = window_width, oldheight = window_height;

	if ( Cgx_CheckEvents(My_Screen, My_Window, &window_height, &window_width, &win_left, &win_top) &&
	     (window_width != oldwidth || window_height != oldheight))
	{
		Update_WinSize();
	}
}

/******************************** UNINIT    ******************************************/
static void uninit (void)
{
#ifdef CONFIG_GUI
	if (use_gui)
	{
		gui(GUI_SET_WINDOW_PTR, NULL);
		mygui->screen = My_Screen;
	}
#endif

	Cgx_ControlBlanker(My_Screen, TRUE);

	Cgx_Stop(My_Window);

	if (EmptyPointer)
	{
		FreeVec(EmptyPointer);
		EmptyPointer=NULL;
	}

	if (CGXVideoBase)
	{
		if (VL_Handle)
		{
			DetachVLayer(VL_Handle);
			DeleteVLayerHandle(VL_Handle);
			VL_Handle = NULL;
		}
		CloseLibrary(CGXVideoBase);
		CGXVideoBase=NULL;
	}

	if (My_Window)
	{
        Cgx_StopWindow(My_Window);
		CloseWindow(My_Window);
		My_Window = NULL;
	}

	if (My_Screen)
	{
#ifdef CONFIG_GUI
		if(use_gui)
		{
			if(CloseScreen(My_Screen))
			{
				mygui->screen = NULL;
			}
		}
		else
		{
			CloseScreen(My_Screen);
		}
#else
		CloseScreen(My_Screen);
#endif
		My_Screen = NULL;
	}

	Cgx_ReleaseArg();
}

static int query_format(uint32_t format)
{
	int flag = VFCAP_CSP_SUPPORTED | VFCAP_CSP_SUPPORTED_BY_HW |
			   VFCAP_HWSCALE_UP | VFCAP_HWSCALE_DOWN | VFCAP_OSD |
			   VFCAP_ACCEPT_STRIDE;

	switch( format)
	{
		case IMGFMT_YUY2:
		case IMGFMT_YV12:
		case IMGFMT_I420:
		case IMGFMT_IYUV:
		case IMGFMT_422P:
			return flag;
		default:
			return VO_FALSE;
	}
}
