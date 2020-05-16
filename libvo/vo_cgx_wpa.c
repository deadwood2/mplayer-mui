/*
 *  vo_cgx_wpa.c
 *  VO module for MPlayer MorphOS
 *  using CGX WPA
 *  Writen by DET Nicolas
*/

#define SYSTEM_PRIVATE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "config.h"

#include "sub/osd.h"
#include "sub/sub.h"

#include "mp_msg.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "cgx_common.h"

#include "../ffmpeg/libswscale/swscale.h"
#include "../ffmpeg/libswscale/swscale_internal.h" //FIXME
#include "../ffmpeg/libswscale/rgb2rgb.h"
#include "../libmpcodecs/vf_scale.h"

#include "aspect.h"

#include "morphos_stuff.h"

#ifdef CONFIG_GUI
#include "gui/interface.h"
#include "gui/morphos/gui.h"
#include "mplayer.h"
#endif

#include <inttypes.h>		// Fix <postproc/rgb2rgb.h> bug
#include "version.h"

//Debug
#define kk(x)

// OS specific
#include <libraries/cybergraphics.h>
#include <proto/cybergraphics.h>
#include <proto/graphics.h>
#include <graphics/gfxbase.h>

#include <devices/rawkeycodes.h>
#include <proto/intuition.h>
#include <intuition/extensions.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <exec/types.h>
#include <dos/dostags.h>
#include <dos/dos.h>

#if defined(__AROS__)
#include <proto/arossupport.h>
#endif

static vo_info_t info =
{
	"CyberGraphX video output (WPA)",
	"cgx_wpa",
	"DET Nicolas, Krzysztof Smiechowicz",
	"Based on MorphOS version"
};

LIBVO_EXTERN(cgx_wpa)

// Some proto for our private func
static void (*vo_draw_alpha_func)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride);

static struct Window *         My_Window      = NULL;
static struct Screen *         My_Screen      = NULL;

static struct RastPort *       rp             = NULL;

static struct MsgPort *        UserMsg        = NULL;

extern char PubScreenName[128];

/* Information about source image (as in width and height of video file) */
static uint32_t   image_format;
static uint32_t   image_width;
static uint32_t   image_height;

/* Information about the shown video (as in width and height of area where video is displayed
   In window mode, the draw_buffer_width(height) map to Window->InnerWidth(Height)
   In fullscreen mode, the greater size maps to matching screen size. The other
   size is calculated to maintain aspect */
static UBYTE *          draw_buffer_mem = NULL;
static UBYTE *          draw_buffer = NULL;
const static uint32_t   draw_buffer_bpp = 4;
const static uint32_t   draw_buffer_format = IMGFMT_BGRA;
static uint32_t         draw_buffer_width;
static uint32_t         draw_buffer_height;

/* Size of the screen on which video is displayed (fullscreen mode ) */
static uint32_t   screen_width;
static uint32_t   screen_height;

uint32_t	 is_fullscreen;

extern UWORD *EmptyPointer;               // Blank pointer
extern ULONG WantedModeID;

/* Offsets from start of drawable area (RastPort) where video should be rendered
   In windows mode, they equal BorderLeft and BorderTop
   In fullscreen mode they equal half difference between screen dimension and
   draw_buffer_dimension */
static uint32_t   offset_x;
static uint32_t   offset_y;


/* Position where the window should be created */
static uint32_t   win_left;             
static uint32_t   win_top;              
static uint32_t   win_width;
static uint32_t   win_height;
static BOOL       win_init;

static SwsContext *swsContext=NULL;

#ifndef __AROS__
/***************************************/
static void BackFill_Func(void);

static const struct EmulLibEntry BackFill_Func_gate =
{
	TRAP_LIB,
	0,
	(void (*)(void)) BackFill_Func
};

static struct Hook BackFill_Hook =
{
	{NULL, NULL},
	(HOOKFUNC) &BackFill_Func_gate,
	NULL,
	NULL
};

#ifdef __GNUC__
# pragma pack(2)
#endif

struct BackFillArgs
{
	struct Layer     *layer;
	struct Rectangle  bounds;
	LONG              offsetx;
	LONG              offsety;
};

#ifdef __GNUC__
# pragma pack()
#endif

static void BackFill_Func(void)
{
	PREPARE_BACKFILL(draw_buffer_width, draw_buffer_height);
	WritePixelArray(
		draw_buffer,	
		BufferStartX,
		BufferStartY,
		draw_buffer_width*image_bpp,
		&MyRP,
		StartX,
		StartY,
		SizeX,
		SizeY,
		RECTFMT_RGB);
}
#else

struct layerhookmsg
{
    struct Layer *l;
    WORD MinX, MinY, MaxX, MaxY;
    LONG OffsetX, OffsetY;
};

AROS_UFH3(void, BackFill_Func,
    AROS_UFHA(struct Hook *, hook, A0),
    AROS_UFHA(struct RastPort *, rp, A2),
    AROS_UFHA(APTR, message, A1))
{
    AROS_USERFUNC_INIT
    
    struct layerhookmsg * args = (struct layerhookmsg *) message; 
    struct Layer * lay = rp->Layer; 
    struct Rectangle renderableRect, movieRect, requestedRect, resultRect;
    struct RastPort * rptemp = CloneRastPort(rp);
    rptemp->Layer = NULL;

    if (!lay) return;

    requestedRect.MinX = args->MinX;
    requestedRect.MaxX = args->MaxX;
    requestedRect.MinY = args->MinY;
    requestedRect.MaxY = args->MaxY;

    /* 1. Handle the damaged region on the window */
    renderableRect.MinX = lay->bounds.MinX + My_Window->BorderLeft;
    renderableRect.MaxX = lay->bounds.MaxX - My_Window->BorderRight;
    renderableRect.MinY = lay->bounds.MinY + My_Window->BorderTop;
    renderableRect.MaxY = lay->bounds.MaxY - My_Window->BorderBottom;
    
    if (AndRectRect(&renderableRect, &requestedRect, &resultRect))
    {
        if (is_fullscreen)
        {
            /* In full screen repaint in black */
        	FillPixelArray(rptemp, 
        	    resultRect.MinX, resultRect.MinY, 
        	    resultRect.MaxX - resultRect.MinX + 1, 
        	    resultRect.MaxY - resultRect.MinY + 1, 0x00000000);
        }
    }
    
    /* 2. Handle the damaged region in area where movie plays */
    movieRect.MinX = lay->bounds.MinX + offset_x;
    movieRect.MaxX = lay->bounds.MinX + offset_x + draw_buffer_width - 1;
    movieRect.MinY = lay->bounds.MinY + offset_y;
    movieRect.MaxY = lay->bounds.MinY + offset_y + draw_buffer_height - 1;
    
    if (AndRectRect(&movieRect, &requestedRect, &resultRect))
    {
        ULONG BufferStartX  = resultRect.MinX - offset_x - lay->bounds.MinX;
        ULONG BufferStartY 	= resultRect.MinY - offset_y - lay->bounds.MinY;

        WritePixelArray(
            draw_buffer,	
            BufferStartX,
            BufferStartY,
            draw_buffer_width * draw_buffer_bpp,
            rptemp,
            resultRect.MinX,
            resultRect.MinY,
            resultRect.MaxX - resultRect.MinX + 1,
            resultRect.MaxY - resultRect.MinY + 1,
            RECTFMT_BGRA32);
    }

    if (rptemp)
        FreeRastPort(rptemp);
    
    AROS_USERFUNC_EXIT
}

struct Hook BackFill_Hook = 
{
h_Entry: (IPTR (*)())BackFill_Func,
h_Data: NULL,
};
#endif


/******************************** DRAW ALPHA ******************************************/
static void draw_alpha_rgb32 (int x0,int y0, int w,int h, unsigned char* src, unsigned char * srca, int stride)
{
    vo_draw_alpha_rgb32(w, h, src , srca, stride,
				(UBYTE *) ( (IPTR) draw_buffer + (y0 * draw_buffer_width + x0) * draw_buffer_bpp), 
				draw_buffer_width * draw_buffer_bpp);
}

/******************************** PREINIT ******************************************/
static int preinit(const char *arg)
{
	mp_msg(MSGT_VO, MSGL_INFO, "VO: [cgx_wpa] Preinit.\n");

    /* Parse the subdevice */
	if (!Cgx_GiveArg(arg))
	{
		return -1;	
	}
	
	/* Check the if border is disabled by configuration */
	if (!vo_border)
	    Cgx_BorderMode = NOBORDER;

	return 0;
}

extern char * filename;

static ULONG Open_Window(void)
{		
	// Window
	ULONG ModeID = INVALID_ID;

	My_Window = NULL;

	if ( ( My_Screen = LockPubScreen ( PubScreenName[0] ? PubScreenName : NULL) ) )
	{
		struct DrawInfo *dri;
		
		ModeID = GetVPModeID(&My_Screen->ViewPort);

		if ( (dri = GetScreenDrawInfo(My_Screen) ) ) 
		{
			ULONG bw, bh;
#ifdef __AROS__
            bw = 0; bh = 0;            
#else

			switch(Cgx_BorderMode)
			{
				case NOBORDER:
						bw = 0;
						bh = 0;
					break;

				default:
						bw = GetSkinInfoAttrA(dri, SI_BorderLeft, NULL) +
				     	GetSkinInfoAttrA(dri, SI_BorderRight, NULL);

						bh = GetSkinInfoAttrA(dri, SI_BorderTopTitle, NULL) +
				     	GetSkinInfoAttrA(dri, SI_BorderBottom, NULL);
			}
#endif

			if (!win_init)
			{
			    /* Set default values */
				win_left = (My_Screen->Width - (image_width + bw)) / 2;
				win_top  = (My_Screen->Height - (image_height + bh)) / 2;
				win_width = image_width;
				win_height = image_height;
				
				/* Override with geometry */
				geometry(&win_left, &win_top, &win_width, &win_height, My_Screen->Width, My_Screen->Height);
				win_init = TRUE;
			}
			
			switch(Cgx_BorderMode)
			{
				case NOBORDER:
					My_Window = OpenWindowTags( NULL,
						WA_CustomScreen,    (IPTR) My_Screen,
						WA_Left,            win_left,
						WA_Top,             win_top,
						WA_InnerWidth,      win_width,
						WA_InnerHeight,     win_height,
						WA_SimpleRefresh,   TRUE,
						WA_CloseGadget,     FALSE,
						WA_DepthGadget,     FALSE,
						WA_DragBar,         FALSE,
						WA_Borderless,      TRUE,
						WA_SizeGadget,      FALSE,
						WA_Activate,        TRUE,
						WA_IDCMP,           IDCMP_MOUSEBUTTONS | IDCMP_INACTIVEWINDOW | IDCMP_ACTIVEWINDOW  | IDCMP_CHANGEWINDOW | IDCMP_MOUSEMOVE | IDCMP_REFRESHWINDOW | IDCMP_RAWKEY /*| IDCMP_VANILLAKEY*/ | IDCMP_CLOSEWINDOW | IDCMP_NEWSIZE,
						WA_Flags,           WFLG_REPORTMOUSE,
#ifndef __AROS__
						WA_SkinInfo,				NULL,
#endif
					TAG_DONE);
					break;

				case TINYBORDER:
					My_Window = OpenWindowTags( NULL,
						WA_CustomScreen,    (IPTR) My_Screen,
						WA_Left,            win_left,
						WA_Top,             win_top,
						WA_InnerWidth,      win_width,
						WA_InnerHeight,     win_height,
						WA_SimpleRefresh,   TRUE,
						WA_CloseGadget,     FALSE,
						WA_DepthGadget,     FALSE,
						WA_DragBar,         FALSE,
						WA_Borderless,      FALSE,
						WA_SizeGadget,      FALSE,
						WA_Activate,        TRUE,
						WA_IDCMP,           IDCMP_MOUSEBUTTONS | IDCMP_INACTIVEWINDOW | IDCMP_ACTIVEWINDOW  | IDCMP_CHANGEWINDOW | IDCMP_MOUSEMOVE | IDCMP_REFRESHWINDOW | IDCMP_RAWKEY /*| IDCMP_VANILLAKEY*/ | IDCMP_CLOSEWINDOW | IDCMP_NEWSIZE,
						WA_Flags,           WFLG_REPORTMOUSE,
#ifndef __AROS__
						WA_SkinInfo,				NULL,
#endif
					TAG_DONE);	
					break;

				default:
					My_Window = OpenWindowTags( NULL,
						WA_CustomScreen,    (IPTR) My_Screen,
#ifdef __AROS__
						WA_Title,         (IPTR) filename ? MorphOS_GetWindowTitle() : "MPlayer for AROS i386",
						WA_ScreenTitle,     (IPTR) "MPlayer " VERSION " for AROS i386",
#else
						WA_Title,         (IPTR) filename ? MorphOS_GetWindowTitle() : "MPlayer for MorphOS",
						WA_ScreenTitle,     (IPTR) "MPlayer " VERSION " for MorphOS",
#endif
						WA_Left,            win_left,
						WA_Top,             win_top,
						WA_InnerWidth,      win_width,
						WA_InnerHeight,     win_height,
						WA_SimpleRefresh,   TRUE,
						WA_CloseGadget,     TRUE,
						WA_DepthGadget,     TRUE,
						WA_DragBar,         TRUE,
						WA_Borderless,      (Cgx_BorderMode == NOBORDER) ? TRUE : FALSE,
						WA_SizeGadget,      FALSE,
						WA_Activate,        TRUE,
						WA_IDCMP,           IDCMP_MOUSEBUTTONS | IDCMP_INACTIVEWINDOW | IDCMP_ACTIVEWINDOW  | IDCMP_CHANGEWINDOW | IDCMP_MOUSEMOVE | IDCMP_REFRESHWINDOW | IDCMP_RAWKEY /*| IDCMP_VANILLAKEY*/ | IDCMP_CLOSEWINDOW | IDCMP_NEWSIZE,
						WA_Flags,           WFLG_REPORTMOUSE,
#ifndef __AROS__
						WA_SkinInfo,        NULL,
#endif
					TAG_DONE);
			}

			FreeScreenDrawInfo(My_Screen, dri);
		}

		vo_screenwidth = My_Screen->Width;
		vo_screenheight = My_Screen->Height;
		vo_dwidth = My_Window->Width;
		vo_dheight = My_Window->Height;
		vo_fs = 0;

		UnlockPubScreen(NULL, My_Screen);
		My_Screen= NULL;
	}

		EmptyPointer = AllocVec(16, MEMF_PUBLIC | MEMF_CLEAR);

	if ( !My_Window || !EmptyPointer)
	{
		mp_msg(MSGT_VO, MSGL_ERR, "Unable to open a window\n");
		uninit();
		return INVALID_ID;
	}

	offset_x = (Cgx_BorderMode == NOBORDER) ? 0 : My_Window->BorderRight;	
	offset_y = (Cgx_BorderMode == NOBORDER) ? 0 : My_Window->BorderTop;
	draw_buffer_width = My_Window->Width - (My_Window->BorderLeft + My_Window->BorderRight);
	draw_buffer_height = My_Window->Height - (My_Window->BorderTop + My_Window->BorderBottom);

	if ( (ModeID = GetVPModeID(&My_Window->WScreen->ViewPort) ) == INVALID_ID) 
	{
		uninit();
		return INVALID_ID;
	}

	ScreenToFront(My_Window->WScreen);

	Cgx_StartWindow(My_Window);

	Cgx_ControlBlanker(My_Screen, FALSE);

	return ModeID;
}

static ULONG Open_FullScreen(void)
{ 
	// if fullscreen -> let's open our own screen
	// It is not a very clean way to get a good ModeID, but a least it works
	struct Screen *Screen;
	struct DimensionInfo buffer_Dimmension;
	ULONG depth;
	ULONG ModeID;

	if(WantedModeID)
	{
		ModeID = WantedModeID;
	}
	else
	{
		if ( ! ( Screen = LockPubScreen(NULL) ) ) 
		{
			uninit();
			return INVALID_ID;
		}

		ModeID = GetVPModeID(&Screen->ViewPort);

		UnlockPubScreen(NULL, Screen);
	}

	if ( ModeID == INVALID_ID) 
	{
		uninit();
		return INVALID_ID;
	}

	depth = ( FALSE ) ? 16 : GetCyberIDAttr( CYBRIDATTR_DEPTH , ModeID);

    /* Try selecting a mode that is closest to source image size */
	screen_width = image_width;
	screen_height = image_height;

	if(!WantedModeID)
	{
		while (1) 
		{
			ModeID = BestCModeIDTags(
				CYBRBIDTG_Depth, depth,
			    CYBRBIDTG_NominalWidth, screen_width,
			    CYBRBIDTG_NominalHeight, screen_height,
			TAG_DONE);

			if ( ModeID == INVALID_ID ) 
			{
				uninit();
				return INVALID_ID;
			}

			if ( GetDisplayInfoData( NULL, &buffer_Dimmension, sizeof(struct DimensionInfo), DTAG_DIMS, ModeID) == sizeof(struct DimensionInfo) &&
				     buffer_Dimmension.Nominal.MaxX - buffer_Dimmension.Nominal.MinX + 1 >= image_width &&
					 	buffer_Dimmension.Nominal.MaxY - buffer_Dimmension.Nominal.MinY + 1 >= image_height ) 
			{
				break;
			}

		    screen_width  += 10;
		    screen_height += 10;
		}

		screen_width  = buffer_Dimmension.Nominal.MaxX - buffer_Dimmension.Nominal.MinX + 1;
		screen_height = buffer_Dimmension.Nominal.MaxY - buffer_Dimmension.Nominal.MinY + 1;
	}

	mp_msg(MSGT_VO, MSGL_INFO, "VO: [cgx_wpa] Full screen.\n");
	mp_msg(MSGT_VO, MSGL_INFO, "VO: [cgx_wpa] Prefered screen is : %s\n", (cgx_monitor) ? cgx_monitor : "default" );

	My_Screen = OpenScreenTags ( NULL,
		SA_DisplayID,  ModeID,
#if PUBLIC_SCREEN
			SA_Type, PUBLICSCREEN,
			SA_PubName, "MPlayer Screen",
#else
			SA_Type, CUSTOMSCREEN,
#endif
		SA_Title, "MPlayer Screen",
		SA_ShowTitle, FALSE,
		WantedModeID ? TAG_IGNORE : SA_Width,  screen_width,
		WantedModeID ? TAG_IGNORE : SA_Height, screen_height,
	TAG_DONE);

    if ( ! My_Screen ) 
    {
        mp_msg(MSGT_VO, MSGL_ERR, "Unable to open the screen ID:%x\n", (int) ModeID);
        uninit();
        return INVALID_ID;
    }

#if PUBLIC_SCREEN
	PubScreenStatus( My_Screen, 0 );
#endif

	screen_width = My_Screen->Width;
	screen_height = My_Screen->Height;

    /* Set draw buffer sizes based on screen size and source aspect */
    if (image_width > image_height)
    {
        draw_buffer_width = screen_width;
        draw_buffer_height = screen_width * ((float)image_height / (float)image_width);
    }
    else
    {
        draw_buffer_height = screen_height;
        draw_buffer_width = screen_height * ((float)image_width / (float)image_height);
    }

	offset_x = (screen_width - draw_buffer_width) / 2;
	offset_y = (screen_height - draw_buffer_height) / 2;

	My_Window = OpenWindowTags( NULL,
#if PUBLIC_SCREEN
			WA_PubScreen,       (IPTR) My_Screen,
#else
			WA_CustomScreen,    (IPTR) My_Screen,
#endif
		    WA_Top,             0,
		    WA_Left,            0,
		    WA_Height,          screen_height,
		    WA_Width,           screen_width,
		    WA_SimpleRefresh,   TRUE,
		    WA_CloseGadget,     FALSE,
		    WA_DragBar,         FALSE,
			WA_Borderless,      TRUE,
			WA_Backdrop,        TRUE,
		    WA_Activate,        TRUE,
			WA_IDCMP,           IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY | IDCMP_REFRESHWINDOW | IDCMP_INACTIVEWINDOW | IDCMP_ACTIVEWINDOW,
			WA_Flags,           WFLG_REPORTMOUSE,
		TAG_DONE);

	if ( ! My_Window) 
	{
		mp_msg(MSGT_VO, MSGL_ERR, "Unable to open a window\n");
		uninit();
		return INVALID_ID;
	}

	FillPixelArray( My_Window->RPort, 0,0, screen_width, screen_height, 0x00000000);

	vo_screenwidth = My_Screen->Width;
	vo_screenheight = My_Screen->Height;
	vo_dwidth = vo_screenwidth - 2 * offset_x;
	vo_dheight = vo_screenheight - 2 * offset_y;
	vo_fs = 1;

	Cgx_ControlBlanker(My_Screen, FALSE);

	return ModeID;
} 

static int PrepareBuffer(uint32_t in_format, uint32_t out_format)
{
#define ALIGN 64
    draw_buffer_mem = (UBYTE *)AllocVec(draw_buffer_bpp * draw_buffer_width * draw_buffer_height + ALIGN - 1, MEMF_ANY );
    if (!draw_buffer_mem)
    {
        uninit();
        return -1;
    }

    draw_buffer = (APTR) ((((IPTR) draw_buffer_mem) + ALIGN - 1) & ~(ALIGN - 1));
#undef ALIGN
    
    /* Prepare scalling context */
    /* Input: image width, height and format, output buffer width, height and format */
    swsContext= sws_getContextFromCmdLine(image_width, image_height, in_format, draw_buffer_width, draw_buffer_height, out_format );
    if (!swsContext)
    {
        uninit();
        return -1;
    }


	return 0;
}

static int config_internal()
{
    ULONG ModeID = INVALID_ID;

    /* The window is up, do nothing. */
	if (My_Window) return 0;

    if (is_fullscreen) 
        ModeID = Open_FullScreen();
    else
        ModeID = Open_Window();

    rp = My_Window->RPort;
    UserMsg = My_Window->UserPort;

    /* This will repaint window during pause and in full screen */
    InstallLayerHook(rp->Layer, &BackFill_Hook);

    /* CyberIDAttr only works with CGX ID, however on MorphOS there are only CGX Screens
       Anyway, it's easy to check, so lets do it... - Piru */
    if (!IsCyberModeID(ModeID)) 
    {
        uninit();
        return -1;
    }

    if (PrepareBuffer(image_format, draw_buffer_format) < 0) 
    {
        uninit();
        return -1;
    }


    Cgx_Start(My_Window);

#ifdef CONFIG_GUI
    if (use_gui)
        gui(GUI_SET_WINDOW_PTR, (void *) My_Window);
#endif

    return 0; // -> Ok
}

/******************************** CONFIG ******************************************/
static int config(uint32_t width, uint32_t height, uint32_t d_width,
		     uint32_t d_height, uint32_t fullscreen, char *title,
		     uint32_t in_format)
{
	is_fullscreen = fullscreen;
	image_format = in_format;
	image_width = width;
	image_height = height;
    vo_draw_alpha_func = draw_alpha_rgb32;
    win_init = FALSE;

    return config_internal();
}

/******************************** DRAW_SLICE ******************************************/
static int draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
    uint8_t *dst[3];
    int dstStride[3];

    dstStride[0] = draw_buffer_width * draw_buffer_bpp;
    dstStride[1] = 0;
    dstStride[2] = 0;

    dst[0] = (uint8_t *) ( (IPTR) draw_buffer + x * draw_buffer_bpp) ;
    dst[1] = NULL;
    dst[2] = NULL; 

    sws_scale(swsContext,
                        image,
                        stride,
                        y,
                        h,
                        dst,
                        dstStride);

	return 0;
}
/******************************** DRAW_OSD ******************************************/

static void draw_osd(void)
{
	vo_draw_text(draw_buffer_width, draw_buffer_height, vo_draw_alpha_func);
}

/******************************** FLIP_PAGE ******************************************/
static void flip_page(void)
{
	WritePixelArray(
		draw_buffer,	
		0,
		0,
		draw_buffer_width * draw_buffer_bpp,
		rp,
		offset_x,
		offset_y,
		draw_buffer_width,
		draw_buffer_height,
		RECTFMT_BGRA32);
}
/******************************** DRAW_FRAME ******************************************/
static int draw_frame(uint8_t *src[])
{
	/* Nothing */
	return -1;
}


/***********************/
/* Just a litle func to help uninit and control
   it close screen (if fullscreen), the windows, and free all gfx related stuff
   but do not close any libs */

static void FreeGfx(void)
{
#ifdef CONFIG_GUI
    if (use_gui)
    {
        gui(GUI_SET_WINDOW_PTR, (void *) NULL);
        mygui->screen = My_Screen;
    }
#endif

	Cgx_ControlBlanker(My_Screen, TRUE);

	Cgx_Stop(My_Window);

	// if screen : close Window thn screen
	if (My_Screen) 
	{
		CloseWindow(My_Window);
		My_Window=NULL;

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

	if (My_Window) 
	{
		Cgx_StopWindow(My_Window);
		CloseWindow(My_Window);
		My_Window=NULL;
	}

	if (draw_buffer_mem) {
		FreeVec(draw_buffer_mem);
		draw_buffer_mem = NULL;
	}
}

/******************************** UNINIT    ******************************************/
static void uninit(void)
{
	FreeGfx();

	if (EmptyPointer)
	{
	  FreeVec(EmptyPointer);
	  EmptyPointer=NULL;
	}

	Cgx_ReleaseArg();
}

/******************************** CONTROL ******************************************/
static int control(uint32_t request, void *data)
{
	switch (request) 
	{
		case VOCTRL_GUISUPPORT:
			return VO_TRUE;

		case VOCTRL_FULLSCREEN:
			FreeGfx();
			is_fullscreen = !is_fullscreen;
			if (config_internal() < 0) 
			    return VO_FALSE;

			return VO_TRUE;

		case VOCTRL_PAUSE:
			Cgx_Stop(My_Window);
			Cgx_ControlBlanker(My_Screen, TRUE);
			return VO_TRUE;					

		case VOCTRL_RESUME:
			Cgx_Start(My_Window);
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

/******************************** CHECK_EVENTS    ******************************************/
static void check_events(void)
{
	Cgx_CheckEvents(My_Screen, My_Window, &draw_buffer_height, &draw_buffer_width, &win_left, &win_top);
}

static int query_format(uint32_t format)
{
	switch( format) 
	{
		case IMGFMT_YV12:
		case IMGFMT_I420:
		case IMGFMT_IYUV:
			return VO_TRUE;
		default:
			return VO_FALSE;
	}
}
