/*
 *  vo_cgx_vmem.c
 *  VO module for MPlayer MorphOS
 *  using CGX/direct VMEM
 *  Writen by DET Nicolas
*/

#define SYSTEM_PRIVATE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "config.h"


#include "mp_msg.h"
#include "video_out.h"
#include "video_out_internal.h"
#include "cgx_common.h"
#include "version.h"
#include "sub/osd.h"
#include "sub/sub.h"


#include "libswscale/swscale.h"
#include "libswscale/swscale_internal.h" //FIXME
#include "libswscale/rgb2rgb.h"
#include "../libmpcodecs/vf_scale.h"

#include "../morphos_stuff.h"

#include <inttypes.h>		// Fix <postproc/rgb2rgb.h> bug

//Debug
#define kk(x)

char cgx_vmemscale = 0;

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

static vo_info_t info =
{
	"CyberGraphX video output (VMEM)",
	"cgx_vmem",
	"DET Nicolas",
	"MorphOS rules da world !"
};

LIBVO_EXTERN(cgx_vmem)

// To make gcc happy
extern void mplayer_put_key(int code);

// Some proto for our private func
static void (*vo_draw_alpha_func)(int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride);

static struct Window *         My_Window      = NULL;
static struct Screen *         My_Screen      = NULL;

static struct RastPort *       rp             = NULL;
static struct ViewPort *       vp             = NULL;

static struct MsgPort *        UserMsg        = NULL;

// For events
static struct BitMap **         BitmapTab       = NULL;

// not OS specific
static uint32_t   image_width;            // well no comment
static uint32_t   image_height;
static uint32_t   window_width;           // width and height on the window
static uint32_t   window_height;          // can be different from the image
static uint32_t   screen_width;           // Indicates the size of the screen in full screen
static uint32_t   screen_height;          // Only use for triple buffering

uint32_t	 is_fullscreen;

extern UWORD *EmptyPointer;
extern ULONG WantedModeID;

static uint32_t   image_bpp;            	// image bpp
static uint32_t   image_depth;            	// image depth in byte
static uint32_t   offset_x;               	// offset in the rp where we have to display the image
static uint32_t   offset_y;               	// ...
static uint32_t   internal_offset_x;        // Indicate where to render the picture inside the bitmap
static uint32_t   internal_offset_y;        // ...

static uint32_t		image_format;

static uint32_t   win_left;              
static uint32_t   win_top;                

static SwsContext *swsContext=NULL;

static void flip_page_3buffer(void);
static void flip_page_1buffer(void);

static void (*real_flip_page) (void);

// Buffer managemnt
static uint32_t   number_buffer;
static uint32_t   current_buffer;
static struct DBufInfo * MyDBuf = NULL;
static BOOL     SafeToWrite;    // Indicates that we can write into the render buffer
static BOOL     SafeToChange = FALSE;

static struct	MsgPort		*SafePort=NULL;
static struct	MsgPort		*DispPort=NULL;

#define GET_DISPLAY     ( current_buffer )
#define GET_READY       ( (current_buffer+1) % number_buffer )
#define GET_RENDER      ( (current_buffer+2) % number_buffer )
#define SWITCH_BUFFER   ( current_buffer = (current_buffer+1) % number_buffer )

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
	PREPARE_BACKFILL(window_width, window_height);

	BltBitMapRastPort( 
			BitmapTab[ GET_DISPLAY ], 
			BufferStartX,
			BufferStartY,
			&MyRP, 
			StartX, 
			StartY, 
			SizeX, 
			SizeY, 
			0xc0 );
}

/******************************** DRAW ALPHA ******************************************/
// Draw_alpha series !
static void draw_alpha_null (int x0,int y0, int w,int h, unsigned char* src, unsigned char *srca, int stride)
{
// Nothing
}

static void draw_alpha_rgb32 (int x0,int y0, int w,int h, unsigned char* src, unsigned char * srca, int stride)
{
ULONG *MyBitmap_addr;
ULONG MyBitmap_stride;
APTR lock_h=NULL;

if ( ! (lock_h = LockBitMapTags(BitmapTab[GET_RENDER] ,
				  LBMI_BASEADDRESS, &MyBitmap_addr,
				  LBMI_BYTESPERROW, &MyBitmap_stride,
			  TAG_DONE) ) ) return; // Unable to lock the BitMap -> do nothing
vo_draw_alpha_rgb32(w,h,src,srca,
			stride,
		 (UBYTE *) ( (ULONG) MyBitmap_addr + ((y0+internal_offset_y)*MyBitmap_stride)+(x0+internal_offset_x)*image_bpp), MyBitmap_stride);
UnLockBitMap ( lock_h );
}

static void draw_alpha_rgb24 (int x0,int y0, int w,int h, unsigned char* src, unsigned char * srca, int stride)
{
ULONG *MyBitmap_addr;
ULONG MyBitmap_stride;
APTR lock_h=NULL;

if ( ! (lock_h = LockBitMapTags( BitmapTab[GET_RENDER],
				  LBMI_BASEADDRESS, &MyBitmap_addr,
				  LBMI_BYTESPERROW, &MyBitmap_stride,
			  TAG_DONE) ) ) return; // Unable to lock the BitMap -> do nothing

vo_draw_alpha_rgb24(w,h,src,srca,
			stride,
	        (UBYTE *) ( (ULONG) MyBitmap_addr + ((y0+internal_offset_y)*MyBitmap_stride)+(x0+internal_offset_x)*image_bpp), MyBitmap_stride);

UnLockBitMap ( lock_h );
}

static void draw_alpha_rgb16 (int x0,int y0, int w,int h, unsigned char* src, unsigned char * srca, int stride)
{
ULONG *MyBitmap_addr;
ULONG MyBitmap_stride;
APTR lock_h=NULL;

if ( ! (lock_h = LockBitMapTags( BitmapTab[GET_RENDER],
				LBMI_BASEADDRESS, &MyBitmap_addr,
				LBMI_BYTESPERROW, &MyBitmap_stride,
			  TAG_DONE) ) ) return; // Unable to lock the BitMap -> do nothing

vo_draw_alpha_rgb16(w,h,src,srca,
			stride,
		 (UBYTE *) ( (ULONG) MyBitmap_addr + ((y0+internal_offset_y)*MyBitmap_stride)+(x0+internal_offset_x)*image_bpp), MyBitmap_stride);

UnLockBitMap ( lock_h );
}

static void draw_alpha_rgb16pc (int x0,int y0, int w,int h, unsigned char* src, unsigned char * srca, int stride)
{
#if 0
ULONG *MyBitmap_addr;
ULONG MyBitmap_stride;
APTR lock_h=NULL;

if ( ! (lock_h = LockBitMapTags( BitmapTab[GET_RENDER],
				LBMI_BASEADDRESS, &MyBitmap_addr,
				LBMI_BYTESPERROW, &MyBitmap_stride,
			  TAG_DONE) ) ) return; // Unable to lock the BitMap -> do nothing

vo_draw_alpha_rgb16pc(w,h,src,srca,
			stride,
		 (UBYTE *) ( (ULONG) MyBitmap_addr + ((y0+internal_offset_y)*MyBitmap_stride)+(x0+internal_offset_x)*image_bpp), MyBitmap_stride);

UnLockBitMap ( lock_h );
#endif
}

/******************************** PREINIT ******************************************/
static int preinit(const char *arg)
{
	mp_msg(MSGT_VO, MSGL_INFO, "VO: [cgx_vmem] Hallo !\n");

	if (!Cgx_GiveArg(arg))
	{
		return -1;	
	}

	return 0;
}

extern char * filename;

/**********************/
static ULONG Open_Window(void)
{		
	// Window
	ULONG ModeID = INVALID_ID;
	static BOOL FirstTime = TRUE;

	// No triple buffering in gindow mode
	number_buffer= 1;
	real_flip_page = flip_page_1buffer;

	My_Window = NULL;

		// Leo: passing NULL to LockPubScreen doesn't always
		//      returns a pointer to the Workbench screen
		//      (in MOS1.4 at least...)
	if ( ( My_Screen = LockPubScreen ( "Workbench") ) )
	{
		struct DrawInfo *dri;
		
		ModeID = GetVPModeID(&My_Screen->ViewPort);

		if ( (dri = GetScreenDrawInfo(My_Screen) ) ) 
		{
			ULONG bw, bh;

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

			if (FirstTime)
			{
				win_left = (My_Screen->Width - (window_width + bw)) / 2;
				win_top  = (My_Screen->Height - (window_height + bh)) / 2;
				FirstTime = FALSE;
			}
			
			switch(Cgx_BorderMode)
			{
				case NOBORDER:
					My_Window = OpenWindowTags( NULL,
						WA_CustomScreen,    (ULONG) My_Screen,
						WA_ScreenTitle,     (ULONG) "MPlayer " VERSION " for MorphOS",
						WA_Left,            win_left,
						WA_Top,             win_top,
						WA_InnerWidth,      window_width,
						WA_InnerHeight,     window_height,
						WA_SimpleRefresh,   TRUE,
						WA_CloseGadget,     FALSE,
						WA_DepthGadget,     FALSE,
						WA_DragBar,         FALSE,
						WA_Borderless,      TRUE,
						WA_SizeGadget,      FALSE,
						WA_Activate,        TRUE,
						WA_IDCMP,           IDCMP_MOUSEBUTTONS | IDCMP_INACTIVEWINDOW | IDCMP_ACTIVEWINDOW  | IDCMP_CHANGEWINDOW | IDCMP_MOUSEMOVE | IDCMP_REFRESHWINDOW | IDCMP_RAWKEY /*| IDCMP_VANILLAKEY*/ | IDCMP_CLOSEWINDOW | IDCMP_NEWSIZE,
						WA_Flags,           WFLG_REPORTMOUSE,
						WA_SkinInfo,        NULL,
					TAG_DONE);
				break;

				case TINYBORDER:
					My_Window = OpenWindowTags( NULL,
						WA_CustomScreen,    (ULONG) My_Screen,
						WA_ScreenTitle,     (ULONG) "MPlayer " VERSION " for MorphOS",
						WA_Left,            win_left,
						WA_Top,             win_top,
						WA_InnerWidth,      window_width,
						WA_InnerHeight,     window_height,
						WA_SimpleRefresh,   TRUE,
						WA_CloseGadget,     FALSE,
						WA_DepthGadget,     FALSE,
						WA_DragBar,         FALSE,
						WA_Borderless,      FALSE,
						WA_SizeGadget,      FALSE,
						WA_Activate,        TRUE,
						WA_IDCMP,           IDCMP_MOUSEBUTTONS | IDCMP_INACTIVEWINDOW | IDCMP_ACTIVEWINDOW  | IDCMP_CHANGEWINDOW | IDCMP_MOUSEMOVE | IDCMP_REFRESHWINDOW | IDCMP_RAWKEY /*| IDCMP_VANILLAKEY*/ | IDCMP_CLOSEWINDOW | IDCMP_NEWSIZE,
						WA_Flags,           WFLG_REPORTMOUSE,
						WA_SkinInfo,        NULL,
					TAG_DONE);
				break;

				default:
					My_Window = OpenWindowTags( NULL,
						WA_CustomScreen,    (ULONG) My_Screen,
						WA_Title,         (ULONG) filename ? MorphOS_GetWindowTitle() : "MPlayer for MorphOS",
						WA_ScreenTitle,     (ULONG) "MPlayer " VERSION " for MorphOS",
						WA_Left,            win_left,
						WA_Top,             win_top,
						WA_InnerWidth,      window_width,
						WA_InnerHeight,     window_height,
						WA_SimpleRefresh,   TRUE,
						WA_CloseGadget,     TRUE,
						WA_DepthGadget,     TRUE,
						WA_DragBar,         TRUE,
						WA_Borderless,      FALSE,
						WA_SizeGadget,      FALSE,
						WA_Activate,        TRUE,
						WA_IDCMP,           IDCMP_MOUSEBUTTONS | IDCMP_INACTIVEWINDOW | IDCMP_ACTIVEWINDOW  | IDCMP_CHANGEWINDOW | IDCMP_MOUSEMOVE | IDCMP_REFRESHWINDOW | IDCMP_RAWKEY /*| IDCMP_VANILLAKEY*/ | IDCMP_CLOSEWINDOW | IDCMP_NEWSIZE,
						WA_Flags,           WFLG_REPORTMOUSE,
						WA_SkinInfo,        NULL,
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
	offset_y = (Cgx_BorderMode == NOBORDER ) ? 0 : My_Window->BorderTop;

	internal_offset_x = 0;
	internal_offset_y = 0;

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

	number_buffer = 3;
	real_flip_page = flip_page_3buffer;

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

	depth = ( TRUE ) ? 16 : GetCyberIDAttr( CYBRIDATTR_DEPTH , ModeID);

	screen_width=window_width;
	screen_height=window_height;

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
				     buffer_Dimmension.Nominal.MaxX - buffer_Dimmension.Nominal.MinX + 1 >= window_width &&
					 	buffer_Dimmension.Nominal.MaxY - buffer_Dimmension.Nominal.MinY + 1 >= window_height ) 
			{
				break;
			}

			screen_width  += 10;
			screen_height += 10;

		}
		screen_width  = buffer_Dimmension.Nominal.MaxX - buffer_Dimmension.Nominal.MinX + 1;
		screen_height = buffer_Dimmension.Nominal.MaxY - buffer_Dimmension.Nominal.MinY + 1;
	}

	mp_msg(MSGT_VO, MSGL_INFO, "VO: [cgx_vmem] Full screen (ModeID=0x%8.8x).\n", (int) ModeID);
	if (cgx_monitor) mp_msg(MSGT_VO, MSGL_INFO, "VO: [cgx_vmem] Prefered screen is : %s\n", cgx_monitor);

	My_Screen = OpenScreenTags ( NULL,
		SA_DisplayID,  		ModeID,
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

	vp = &(My_Screen->ViewPort);

	if (number_buffer ==1) 
	{		// Only one buffer,
		mp_msg(MSGT_VO, MSGL_INFO, "[cgx(vmem)] Single buffering\n");
		offset_x = (screen_width - window_width)/2;
		offset_y = (screen_height - window_height)/2;
		internal_offset_x = 0;
		internal_offset_y = 0;
	}
	else 
	{	// Triple buffering
		mp_msg(MSGT_VO, MSGL_INFO, "[cgx(vmem)] Triple buffering\n");
	internal_offset_x = (screen_width - window_width)/2;
	  internal_offset_y = (screen_height - window_height)/2;
	offset_x = 0;
	offset_y = 0;

	if ( ! ( MyDBuf = AllocDBufInfo(vp) ) )
		{
		mp_msg(MSGT_VO, MSGL_ERR, "Unable to init buffering structure \n");
	  uninit();
	  return -1;
		}

	SafePort = CreateMsgPort();
	DispPort = CreateMsgPort();

	if ( (!SafePort) || (!DispPort) ) 
		{
		uninit();
	  return INVALID_ID;
 		}

	SafeToWrite = TRUE;

	MyDBuf->dbi_SafeMessage.mn_ReplyPort=SafePort;
	MyDBuf->dbi_DispMessage.mn_ReplyPort=DispPort;
	}

	My_Window = OpenWindowTags( NULL,
#if PUBLIC_SCREEN
			WA_PubScreen,  (ULONG) My_Screen,
#else
			WA_CustomScreen,  (ULONG) My_Screen,
#endif
		    WA_Top,             0,
		    WA_Left,            0,
		    WA_Height,          screen_height,
		    WA_Width,           screen_width,
		    WA_SimpleRefresh,   TRUE,
		    WA_CloseGadget,     FALSE,
		    WA_DragBar,         TRUE,
		    WA_Borderless,      FALSE,
		    WA_Activate,        TRUE,
			WA_IDCMP,           IDCMP_MOUSEMOVE | IDCMP_MOUSEBUTTONS | IDCMP_RAWKEY /*| IDCMP_VANILLAKEY*/ | IDCMP_REFRESHWINDOW,
	            WA_Flags,         WFLG_REPORTMOUSE,
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
	vo_dwidth = vo_screenwidth - 2*offset_x;
	vo_dheight = vo_screenheight - 2*offset_y;
	vo_fs = 1;

	Cgx_ControlBlanker(My_Screen, FALSE);

	return ModeID;
} 

/**************************/
static int PrepareVideo(uint32_t in_format, uint32_t out_format)
{
	int i;

	if (  ! (BitmapTab = (struct BitMap **) AllocVec( sizeof(struct BitMap *) * number_buffer, MEMF_CLEAR ) ) ) 
	{
		mp_msg(MSGT_VO, MSGL_ERR, "Not enough Memory (Bitmap Table)\n");
	uninit();
	return -1;
	}

  if (number_buffer == 1 ) 
	{
  	if ( ! ( BitmapTab[0] = AllocBitMap(window_width, window_height, image_depth, BMF_CLEAR | BMF_MINPLANES | BMF_DISPLAYABLE , rp->BitMap ) ) ) 
		{
			mp_msg(MSGT_VO, MSGL_ERR, "Not enough Memory (for 1 video buffer)\n");
	  uninit();
	  return -1;
		}
	}
  else
	{
		for (i=0; i < number_buffer; i++) 
		{
		if ( ! ( BitmapTab[i] = AllocBitMap(screen_width, screen_height, image_depth, BMF_CLEAR | BMF_MINPLANES | BMF_DISPLAYABLE , NULL ) ) ) 
			{
				mp_msg(MSGT_VO, MSGL_ERR, "Not enough Memory (for 3 video buffer)\n");
		uninit();
				return -1;
			}
		}
	}

#if 1
	swsContext= sws_getContextFromCmdLine(image_width, image_height, in_format, window_width, window_height, out_format );
  if (!swsContext)
	{
		uninit();
	return -1;
	}

#else
  yuv2rgb_init( image_depth, pixel_format ); // if the pixel format is unknow, mplayer will select just a bad one !
#endif


	return 0;
}


/******************************** CONFIG ******************************************/
static int config(uint32_t width, uint32_t height, uint32_t d_width,
		     uint32_t d_height, uint32_t fullscreen, char *title,
		     uint32_t in_format)
{
	uint32_t out_format;
	ULONG pixel_format;
	ULONG ModeID = INVALID_ID;

	if (My_Window) return 0;

	Cgx_Message();	

	is_fullscreen = fullscreen;

	image_format = in_format;

	number_buffer= 1;
	real_flip_page = flip_page_1buffer;

	// backup info
	image_width = (width - width % 8);
	image_height = (height - height % 8);

	if (cgx_vmemscale)
	{
		window_width = (d_width - d_width % 8);
		window_height = (d_height - d_height % 8);
	}
	else
	{
		window_width = image_width;
		window_height = image_height;
	}

	// Default value
  SafeToWrite = TRUE;
  SafeToChange = TRUE;

  // Default value
  current_buffer=0;

  if ( fullscreen ) ModeID = Open_FullScreen();
	else ModeID = Open_Window();

	rp = My_Window->RPort;
	UserMsg = My_Window->UserPort;

	// CyberIDAttr only works with CGX ID, however on MorphOS there are only CGX Screens
	// Anyway, it's easy to check, so lets do it... - Piru
	if ( ! IsCyberModeID(ModeID) ) 
	{
		uninit();
		return -1;
	}

	if ( (pixel_format = GetCyberIDAttr(CYBRIDATTR_PIXFMT , ModeID) ) == -1) 
	{
		uninit();
		return -1;
	}

	switch (pixel_format) 
	{
		case PIXFMT_RGB15PC:
						image_bpp=2;
						image_depth=15;
						out_format = IMGFMT_RGB15;
						vo_draw_alpha_func = draw_alpha_null;
			break;		
		case PIXFMT_BGR15PC:
						image_bpp=2;
						image_depth=15;
						out_format = IMGFMT_BGR15;
						vo_draw_alpha_func = draw_alpha_null;
			break;	

		case PIXFMT_RGB16:	// Ok
						image_bpp=2;
						image_depth=16;
						out_format = IMGFMT_RGB16;
						vo_draw_alpha_func = draw_alpha_rgb16;
						break;
		case PIXFMT_BGR16:
						//printf("BGR16 !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
						image_bpp=2;
						image_depth=16;
						out_format = IMGFMT_BGR16;
						vo_draw_alpha_func = draw_alpha_rgb16;
						break;

// FIX !!!!!!!!!

		case PIXFMT_RGB16PC:
						//printf("RGB16PC !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
						image_bpp=2;
						image_depth=16;
						out_format = IMGFMT_BGR16;
						vo_draw_alpha_func = draw_alpha_rgb16pc;
			break;
	
		case PIXFMT_BGR16PC:
						//printf("BGR16PC !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
						image_bpp=2;
						image_depth=16;
						out_format = IMGFMT_BGR16;
						vo_draw_alpha_func = draw_alpha_rgb16pc;
			break;

// END FIX

		case PIXFMT_RGB24:
						image_bpp=3;
						image_depth=24;
						out_format = IMGFMT_RGB24;
						vo_draw_alpha_func = draw_alpha_rgb24;
			break;

		case PIXFMT_BGR24:			// OK
						image_bpp=3;
						image_depth=24;
						out_format = IMGFMT_BGR24;
						vo_draw_alpha_func = draw_alpha_rgb24;
			break;

		case PIXFMT_ARGB32:			// OK
						image_bpp=4;
						image_depth=32;
						out_format = IMGFMT_RGB32;	// Strange but it works
						vo_draw_alpha_func = draw_alpha_rgb32;
			break;

	case PIXFMT_BGRA32:
						image_bpp=4;
						image_depth=32;
						out_format = IMGFMT_BGR32;
						vo_draw_alpha_func = draw_alpha_rgb32;
			break;	

	case PIXFMT_RGBA32:
						image_bpp=4;
						image_depth=32;
						out_format = IMGFMT_RGB32;
						vo_draw_alpha_func = draw_alpha_rgb32;
			break;

		default:
			vo_draw_alpha_func = draw_alpha_null;
						mp_msg(MSGT_VO, MSGL_ERR, "[cgx_mem] No YUV -> RGB conversion\n");
			uninit();
			return -1;
	}

 	if (PrepareVideo(in_format, out_format) < 0) 
	{
		uninit();
		return -1;
	}

	Cgx_Start(My_Window);

  return 0; // -> Ok
}

/******************************** DRAW_SLICE ******************************************/
static int draw_slice(uint8_t *image[], int stride[], int w,int h,int x,int y)
{
	ULONG *MyBitmap_addr;
	ULONG MyBitmap_stride;
	APTR lock_h=NULL;

	uint8_t *dst[3];
  int dstStride[3];

	w -= (w%8);

  if (!SafeToWrite) 
	{
  	while(! GetMsg(DispPort)) Wait(1l<<(DispPort->mp_SigBit));
	SafeToWrite= TRUE;
	}

  if ( ! (lock_h = LockBitMapTags( BitmapTab[GET_RENDER],
				  LBMI_BASEADDRESS, &MyBitmap_addr,
				  LBMI_BYTESPERROW, &MyBitmap_stride,
				  TAG_DONE) ) ) return -1; // Unable to lock the BitMap -> do nothing

#if 1
	dstStride[0] = MyBitmap_stride;
	dstStride[1] = 0;
	dstStride[2] = 0;

	dst[0] = (uint8_t *) ( (LONG) MyBitmap_addr + (x+internal_offset_x)*image_bpp );
	dst[1] = NULL;
	dst[2] = NULL; 

	sws_scale(swsContext, 
			image,
			stride,
			y + internal_offset_y,
			h,
			dst, 
			dstStride);
#else
	yuv2rgb( (BYTE *) ( (LONG) MyBitmap_addr + MyBitmap_stride*(y+internal_offset_y) + (x+internal_offset_x)*image_bpp ) , image[0], image[1], image[2],\
								  w, h, MyBitmap_stride,  \
							stride[0], stride[1] );
#endif
  UnLockBitMap ( lock_h );

	return 0;
}
/******************************** DRAW_OSD ******************************************/

static void draw_osd(void)
{
  if (!SafeToWrite) 
	{
		while(! GetMsg(DispPort)) Wait(1l<<(DispPort->mp_SigBit));
	SafeToWrite= TRUE;
	}

	vo_draw_text(window_width,window_height,vo_draw_alpha_func);
}

/******************************** FLIP_PAGE ******************************************/
static void flip_page_1buffer(void)
{
	BltBitMapRastPort   (BitmapTab[ GET_RENDER ], 0, 0, rp, offset_x, offset_y, window_width, window_height, 0xc0 );
}

static void flip_page_3buffer(void)
{
	// Triple buffering !!!!
		if ( ! SafeToChange ) while(! GetMsg(SafePort)) Wait(1l<<(SafePort->mp_SigBit));
	SafeToChange = TRUE;

	WaitBlit();

	ChangeVPBitMap(vp, BitmapTab[ GET_READY ], MyDBuf);

	SafeToChange = FALSE;
	SafeToWrite = FALSE;

	SWITCH_BUFFER;
}

static void flip_page(void)
{
	// Func pointer to the real func use
	real_flip_page();
}
/******************************** DRAW_FRAME ******************************************/
static int draw_frame(uint8_t *src[])
{
	// Nothing
	return -1;
}

/***********************/
// Just a litle func to help uninit and control
// it close screen (if fullscreen), the windows, and free all gfx related stuff
// but do not clsoe any libs

static void FreeGfx(void)
{
	int i=0;
  if (MyDBuf) 
	{
		if (SafePort) 
		{
  		if ( ! SafeToChange ) while(! GetMsg(SafePort)) Wait(1l<<(SafePort->mp_SigBit));
		DeleteMsgPort(SafePort);
		SafePort = NULL;
		}

  	if (DispPort) 
		{
  		if (!SafeToWrite) while(! GetMsg(DispPort)) Wait(1l<<(DispPort->mp_SigBit));
		DeleteMsgPort(DispPort);
		DispPort = NULL;
		}

  	FreeDBufInfo(MyDBuf);
  	MyDBuf = NULL;
  }

	if (swsContext) 
	{
		sws_freeContext(swsContext);
		swsContext = NULL;
	}

  Cgx_ControlBlanker(My_Screen, TRUE);

  Cgx_Stop(My_Window);

  // if screen : close Window then screen
  if (My_Screen) 
	{
		CloseWindow(My_Window);
	  My_Window=NULL;
	  CloseScreen(My_Screen);
		My_Screen = NULL;
  }

	if (My_Window) 
	{
		Cgx_StopWindow(My_Window);
		CloseWindow(My_Window);
		My_Window=NULL;
	}

  if (BitmapTab) 
	{
		for(i=0; i < number_buffer; i++)  if( BitmapTab[i] ) FreeBitMap(BitmapTab[i]);
	FreeVec(BitmapTab);
	BitmapTab = NULL;
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
			return VO_FALSE;

		case VOCTRL_FULLSCREEN:

			is_fullscreen = !is_fullscreen;

			FreeGfx(); // Free/Close all gfx stuff (screen windows, buffer...);
			if ( config(image_width, image_height, window_width, window_height, is_fullscreen, NULL, image_format) < 0) return VO_FALSE;

			return VO_TRUE;

		case VOCTRL_PAUSE:
			Cgx_Stop(My_Window);
			InstallLayerHook(rp->Layer, &BackFill_Hook);
			Cgx_ControlBlanker(My_Screen, TRUE);
			return VO_TRUE;					

		case VOCTRL_RESUME:
			Cgx_Start(My_Window);
			InstallLayerHook(rp->Layer, NULL);
			Cgx_ControlBlanker(My_Screen, FALSE);
			return VO_TRUE;

		case VOCTRL_QUERY_FORMAT:
			return query_format(*(ULONG *)data);
		
  }
  
	return VO_NOTIMPL;
}

/******************************** CHECK_EVENTS    ******************************************/
static void check_events(void)
{
	Cgx_CheckEvents(My_Screen, My_Window, &window_height, &window_width, &win_left, &win_top);
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
