/*
 *  vo_cgx_overlay_gui.c
 *  VO module for MPlayer MorphOS
 *  Using CGX/Overlay in GUI context
 *  Writen by Coeurjoly Fabien
*/
#define SYSTEM_PRIVATE

#define USE_VMEM64		1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../morphos_stuff.h"
#include "aspect.h"
#include "cgx_common.h"
#include "config.h"
#include "mp_msg.h"
#include "version.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "sub/osd.h"
#include "sub/sub.h"

#ifdef CONFIG_GUI
#include "mplayer.h"
#include "gui/interface.h"
#include "gui/morphos/gui.h"

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

extern struct Library *CGXVideoBase;
static int use_overlay_doublebuffer = 0;

static vo_info_t info =
{
	"CyberGraphX video output (Overlay) in GUI context",
	"cgx_overlay_gui",
	"Coeurjoly Fabien",
	"For the GUI fans"
};

LIBVO_EXTERN(cgx_overlay_gui)

extern void mplayer_put_key(int code);

// Some proto for our private func
static void (*vo_draw_alpha_func)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride);

/*************************/
extern char *filename;
extern char PubScreenName[128];
static struct Window *My_Window = NULL;
static APTR VL_Handle = NULL;

// not OS specific
static uint32_t   image_width;            // well no comment
static uint32_t   image_height;
static uint32_t   window_width;           // width and height on the window
static uint32_t   window_height;          // can be different from the image

static uint32_t   window_orig_width;
static uint32_t   window_orig_height;

static ULONG pixel_format;

uint32_t   is_fullscreen;

extern UWORD *EmptyPointer;               // Blank pointer
extern ULONG p_mics1, p_mics2, p_secs1, p_secs2;
extern BOOL mouse_hidden;

static uint32_t (*real_draw_slice) (uint8_t *image[], int stride[], int w,int h,int x,int y);
static uint32_t draw_slice_420p_to_422	(uint8_t *image[], int stride[], int w,int h,int x,int y);
static uint32_t draw_slice_422p_to_420p	(uint8_t *image[], int stride[], int w,int h,int x,int y);
static uint32_t draw_slice_422p_to_422	(uint8_t *image[], int stride[], int w,int h,int x,int y);
static uint32_t draw_slice_422			(uint8_t *image[], int stride[], int w,int h,int x,int y);
//static uint32_t draw_slice_422_scaled	  (uint8_t *image[], int stride[], int w,int h,int x,int y);
static uint32_t draw_slice_420p			(uint8_t *image[], int stride[], int w,int h,int x,int y);
//static uint32_t draw_slice_420p_scaled  (uint8_t *image[], int stride[], int w,int h,int x,int y);

#define VLMODE_PLANAR 0
#define VLMODE_CHUNKY 1

static ULONG cgx_overlay_mode = VLMODE_PLANAR;

static LONG vlayer_x_offset = 0, vlayer_y_offset = 0;
static LONG scr_shadowpen = 1;
static BOOL scr_truecolor = TRUE;

/*
#define min(a,b)                \
	({typeof(a) _a = (a);   \
	  typeof(b) _b = (b);   \
	  _a < _b ? _a : _b; })

#define max(a,b)                \
	({typeof(a) _a = (a);   \
	  typeof(b) _b = (b);   \
	  _a > _b ? _a : _b; })
*/

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

static inline void Update_WinSize(void)
{
	ULONG x_offset;
	ULONG y_offset;

	// Change the window size to fill the whole screen with good aspect
	if ( ( (float) window_orig_width / (float)window_orig_height) < ( (float) window_width / (float) window_height) )
	{
		// Width is too big
		y_offset = 0;
		x_offset = window_width - window_height * ( (float) window_orig_width / window_orig_height);
		x_offset /= 2;
	}
	else
	{
		// Height too big
		y_offset = window_height - window_width * ( (float) window_orig_height / window_orig_width);
		y_offset /= 2;
		x_offset = 0;
	}

	/* set global values for backfill hook */
	vlayer_x_offset = x_offset;
	vlayer_y_offset = y_offset;

	mygui->x_offset = x_offset;
	mygui->y_offset = y_offset;

	vo_dwidth = mygui->video_right - mygui->video_left + 1 - x_offset*2;
	vo_dheight = mygui->video_bottom - mygui->video_top + 1 - y_offset*2;

	// fill the whole display black hiding the colorkey

	if (x_offset || y_offset)
	{
		if (scr_truecolor)
		{
			FillPixelArray(mygui->videowindow->RPort,
						   mygui->video_left,
						   mygui->video_top,
						   mygui->video_right - mygui->video_left + 1,
						   mygui->video_bottom - mygui->video_top + 1,
						   0x00000000);
		}
		else
		{
			SetAPen(mygui->videowindow->RPort, scr_shadowpen);
			RectFill(mygui->videowindow->RPort,
					 mygui->video_left,
					 mygui->video_top,
					 mygui->video_right,
					 mygui->video_bottom);
		}
	}

	// Window size has changed
	SetVLayerAttrTags(VL_Handle,
			VOA_LeftIndent,		x_offset + mygui->video_left - mygui->videowindow->BorderLeft,
			VOA_RightIndent,	x_offset + mygui->videowindow->Width - mygui->video_right - 1 - mygui->videowindow->BorderRight,
			VOA_TopIndent,		y_offset + mygui->video_top - My_Window->BorderTop,
			VOA_BottomIndent,	y_offset + mygui->videowindow->Height - mygui->video_bottom - 1 - mygui->videowindow->BorderBottom,
		TAG_DONE);

	// restore the colorkey

	// Always resore the display,
	// If the user set back the window size to the original one
	// then we still need to restore
	// if (x_offset || y_offset)

	{
		if (scr_truecolor)
		{
			FillPixelArray(mygui->videowindow->RPort,
					 mygui->video_left + x_offset,
					 mygui->video_top + y_offset,
					 mygui->video_right - mygui->video_left + 1 - 2*x_offset,
					 mygui->video_bottom - mygui->video_top + 1 - 2*y_offset,
			         GetVLayerAttr(VL_Handle, VOA_ColorKey));
		}
		else
		{
			SetAPen(mygui->videowindow->RPort, GetVLayerAttr(VL_Handle, VOA_ColorKeyPen));
			RectFill(mygui->videowindow->RPort,
					 mygui->video_left  + x_offset,
					 mygui->video_top  + y_offset,
					 mygui->video_right - x_offset,
					 mygui->video_bottom - y_offset);
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

static BOOL Cgx_Overlay_GiveArg(const char *arg)
{
	STRPTR PtrArg = (STRPTR) arg;

	if (PtrArg)
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
	int rc = 0;
	extern int fullscreen;

	mp_msg(MSGT_VO, MSGL_INFO, "VO: [cgx_overlay_gui] Hello!\n");

	if(!use_gui)
	{
		rc = -1;
		goto quit;
	}

	if(!CGXVideoBase)
	{
		if ( ! (CGXVideoBase = (struct Library *) OpenLibrary("cgxvideo.library" , 0L) ) )
		{
			rc = -1;
			goto quit;
		}
	}

	/* common arguments  */
	if (!Cgx_GiveArg(arg))
	{
		CloseLibrary(CGXVideoBase);
		CGXVideoBase = NULL;
		rc = -1;
		goto quit;
	}

	/* common arguments  */
	if (!Cgx_Overlay_GiveArg(arg))
	{
        Cgx_ReleaseArg();
		CloseLibrary(CGXVideoBase);
		CGXVideoBase = NULL;
		rc = -1;
		goto quit;
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
			rc = -1;
			goto quit;
		}
	}

	Cgx_Message();

quit:
	if(use_gui)
	{
		mygui->embedded = (rc != -1);
	}

	return rc;
}

extern void update_windowdimensions(void);

// Open window for MPlayer
static void Open_Window(void)
{		
	struct Screen *scr = NULL;

	// Window
	mp_msg(MSGT_VO, MSGL_INFO, "VO: [cgx_overlay_gui] Window mode.\n");

	My_Window = mygui->videowindow;

	scr = mygui->videowindow->WScreen;

	if (scr)
	{
		update_scrvars(scr);

		vo_screenwidth = scr->Width;
		vo_screenheight = scr->Height;

		vo_dwidth = mygui->video_right - mygui->video_left + 1;
		vo_dheight = mygui->video_bottom - mygui->video_top + 1;

		vo_fs = FALSE; //(is_fullscreen == 0);

		if (!My_Window)
		{	
			uninit();
			return;
		}

		Cgx_ControlBlanker(mygui->screen, FALSE);

		mp_msg(MSGT_VO, MSGL_INFO, "VO: [cgx_overlay_gui] : window size: %ldx%ld.\n", window_width, window_height);
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
		mp_msg(MSGT_VO, MSGL_INFO, "VO: [cgx_overlay_gui] : cgxvideo.library couldn't be opened.\n");
		return FALSE;
	}

	if ((CGXVideoBase->lib_Version > 43 || (CGXVideoBase->lib_Version == 43 && CGXVideoBase->lib_Revision >= 6)))
	{
		mp_msg(MSGT_VO, MSGL_INFO, "VO: [cgx_overlay_gui] : cgxvideo.library >= 43.6 detected, using doublebuffer\n");
		use_overlay_doublebuffer = 1;
	}
	else
	{
		use_overlay_doublebuffer = 0;
	}

	if (CGXVideoBase->lib_Version > 43 || (CGXVideoBase->lib_Version == 43 && CGXVideoBase->lib_Revision >= 11))
	{
		 ULONG features  = QueryVLayerAttr(mygui->videowindow->WScreen, VSQ_SupportedFeatures);
		 ULONG formats   = QueryVLayerAttr(mygui->videowindow->WScreen, VSQ_SupportedFormats);
		 ULONG max_width = QueryVLayerAttr(mygui->videowindow->WScreen, VSQ_MaxWidth);

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
			VL_Handle = CreateVLayerHandleTags(mygui->videowindow->WScreen,
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
			VL_Handle = CreateVLayerHandleTags(mygui->videowindow->WScreen,
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

	mp_msg(MSGT_VO, MSGL_INFO, "VO: [cgx_overlay_gui] : using %s primitive\n", draw_primitive);


	/* Fill the window area with colorkey */
	mygui->colorkey = GetVLayerAttr(VL_Handle, VOA_ColorKey);

	if (scr_truecolor)
	{
		FillPixelArray(mygui->videowindow->RPort,
				 mygui->video_left,
				 mygui->video_top,
				 mygui->video_right - mygui->video_left + 1,
				 mygui->video_bottom - mygui->video_top + 1,
		         mygui->colorkey);
	}
	else
	{
		SetAPen(mygui->videowindow->RPort, mygui->colorkey);
		RectFill(mygui->videowindow->RPort,
				 mygui->video_left,
				 mygui->video_top,
				 mygui->video_right,
				 mygui->video_bottom);
	}

	if ( AttachVLayerTags(VL_Handle, mygui->videowindow,
							VOA_LeftIndent,	    mygui->video_left,
							VOA_RightIndent,    mygui->videowindow->Width - mygui->video_right,
						    VOA_TopIndent,	    mygui->video_top,
							VOA_BottomIndent,   mygui->videowindow->Height - mygui->video_bottom, TAG_DONE))
	{
		DeleteVLayerHandle(VL_Handle);
		VL_Handle = NULL;
		uninit();
		return FALSE;
	}

	mygui->vo_opened = TRUE;

	return TRUE;
}


/******************************** CONFIG ******************************************/
static int config(uint32_t width, uint32_t height, uint32_t d_width, uint32_t d_height, uint32_t fullscreen, char *title, uint32_t format)
{
	ULONG resize_window = TRUE;
	
	if(!use_gui || !mygui->embedded)
		return -1;

	real_draw_slice = draw_slice_420p_to_422; /* default routine, don't overwrite without reason */
	vo_draw_alpha_func = 0;

	/* reopen in case uninit closed it and preinit wasn't called again */
	if (!CGXVideoBase)
	{
		CGXVideoBase = (struct Library *) OpenLibrary("cgxvideo.library", 0L);
		if(!CGXVideoBase) return -1;
	}

	if (My_Window)
	{
		// don't enforce original movie dimensions on reconfigurations
		resize_window = FALSE;

		Cgx_ControlBlanker(mygui->screen, TRUE);

		if (VL_Handle)
		{
			DetachVLayer(VL_Handle);
			DeleteVLayerHandle(VL_Handle);
			VL_Handle = NULL;
		}

		mygui->vo_opened = FALSE; // gui waits for this to close window
	}

	is_fullscreen = fullscreen;

	My_Window = NULL;

	// backup info
	image_width = width & -8;
	image_height = height & -2;

	window_width = d_width & -8;
	window_height = d_height & -2;

	window_orig_width = d_width & -8;
	window_orig_height = d_height & -2;

	pixel_format = format;

	if ((query_format(format) & VFCAP_CSP_SUPPORTED) == 0)
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

	Open_Window();

	guiInfo.VideoWidth = window_orig_width;
	guiInfo.VideoHeight = window_orig_height;

	if(resize_window)
	{
		update_windowdimensions();
	}

	EmptyPointer = AllocVec(16, MEMF_PUBLIC | MEMF_CLEAR);
	
	if (!My_Window || !EmptyPointer )
	{	
		uninit();
		return -1;
	}

	if (!Make_VLayer(cgx_overlay_mode))
	{	
		uninit();
		return -1;
	}

    Update_WinSize();

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
		case VOCTRL_GUI_NOWINDOW: // Use this id to stop vo while gui is hidden

			Cgx_ControlBlanker(mygui->screen, TRUE);

			if (VL_Handle)
			{
				DetachVLayer(VL_Handle);
				DeleteVLayerHandle(VL_Handle);
				VL_Handle = NULL;
			}
			My_Window = NULL;

			mygui->vo_opened = FALSE; // gui waits for this

			while(!mygui->gui_ready)  // waiting for gui to be shown again
			{
				if(mygui->fatalerror) return VO_FALSE;
				if(!mygui->running) return VO_FALSE;
				Delay(1);
			}

			Open_Window();

			if ( !Make_VLayer(cgx_overlay_mode) ) return VO_FALSE;
			Update_WinSize();

			break;

		case VOCTRL_FULLSCREEN:

			Cgx_ControlBlanker(mygui->screen, TRUE);

			if (VL_Handle)
			{
				DetachVLayer(VL_Handle);
				DeleteVLayerHandle(VL_Handle);
				VL_Handle = NULL;
			}
			My_Window = NULL;

			mygui->vo_opened = FALSE; // gui waits for this
			mygui->gui_ready = FALSE;

			is_fullscreen = mygui->fullscreen;

			if(is_fullscreen)
			{
				gui(GUI_SET_WINDOW_PTR, (void *) FALSE);

				is_fullscreen = FALSE;
			}
			else
			{
				gui(GUI_SET_WINDOW_PTR, (void *) TRUE);

				is_fullscreen = TRUE;
			}

			while(!mygui->gui_ready)  // waiting for gui to be shown again
			{
				if(mygui->fatalerror) return VO_FALSE;
				if(!mygui->running) return VO_FALSE;
				Delay(1);
			}

			Open_Window();

			if ( !Make_VLayer(cgx_overlay_mode) ) return VO_FALSE;
			Update_WinSize();

			if(is_fullscreen)
			{
				Cgx_ShowMouse(mygui->screen, mygui->videowindow, FALSE);
			}

			return VO_TRUE;

		case VOCTRL_GUISUPPORT:
			return VO_TRUE;

		case VOCTRL_PAUSE:
			Cgx_ControlBlanker(mygui->screen, TRUE);
			return VO_TRUE;					

		case VOCTRL_RESUME:
			Cgx_ControlBlanker(mygui->screen, FALSE);
			return VO_TRUE;

		case VOCTRL_QUERY_FORMAT:
			return query_format(*(ULONG *)data);

		case VOCTRL_UPDATE_SCREENINFO:
		{
			struct Screen *s = (struct Screen *)mygui->screen;

			if(!s)
			{
				s = LockPubScreen ( PubScreenName[0] ? PubScreenName : NULL);
				if(s)
				{
					vo_screenwidth = s->Width;
					vo_screenheight = s->Height;
					UnlockPubScreen(NULL, s);
				}
			}
			else
			{
				vo_screenwidth = s->Width;
				vo_screenheight = s->Height;					
			}

	        aspect_save_screenres(vo_screenwidth, vo_screenheight);
			return VO_TRUE;
		}
	}

	return VO_NOTIMPL;
}

static void check_events(void)
{
	uint32_t oldwidth = window_width, oldheight = window_height;

	window_width = mygui->video_right - mygui->video_left + 1;
	window_height = mygui->video_bottom - mygui->video_top + 1;

	if (window_width != oldwidth || window_height != oldheight)
	{
		Update_WinSize();
	}

	is_fullscreen = mygui->fullscreen;

	if (is_fullscreen && !mouse_hidden)
	{
		if (!p_secs1 && !p_mics1)
		{
			CurrentTime(&p_secs1, &p_mics1);
		}
		else
		{
			CurrentTime(&p_secs2, &p_mics2);
			if (p_secs2 - p_secs1 >= 2)
			{
				Cgx_ShowMouse(mygui->screen, mygui->videowindow, FALSE);
				p_secs1 = p_secs2 = p_mics1 = p_mics2 = 0;
			}
		}
	}
}

/******************************** UNINIT    ******************************************/
static void uninit (void)
{
	Cgx_ControlBlanker(mygui->screen, TRUE);

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
		CGXVideoBase = NULL;

		My_Window = NULL;

		mygui->colorkey = 0;

		if(mygui->videowindow)
		{
			if (scr_truecolor)
			{
				FillPixelArray(mygui->videowindow->RPort,
						 mygui->video_left,
						 mygui->video_top,
						 mygui->video_right - mygui->video_left + 1,
						 mygui->video_bottom - mygui->video_top + 1,
				         mygui->colorkey);
			}
			else
			{
				SetAPen(mygui->videowindow->RPort, mygui->colorkey);
				RectFill(mygui->videowindow->RPort,
						 mygui->video_left,
						 mygui->video_top,
						 mygui->video_right,
						 mygui->video_bottom);
			}
		}
	}

	Cgx_ReleaseArg();

	mygui->vo_opened = FALSE;
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

#endif

