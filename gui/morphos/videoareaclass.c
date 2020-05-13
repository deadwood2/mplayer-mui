#include <cybergraphx/cybergraphics.h>
#include <proto/cybergraphics.h>
#include <proto/graphics.h>

#include "gui/interface.h"
#include "gui.h"

#ifndef __AROS__ /* AROS:CHANGED */
#define USE_BACKFILL 1
#else
#define USE_BACKFILL 0
#endif


#if USE_BACKFILL
#ifndef MUIA_CustomBackfill
#define MUIA_CustomBackfill 0x80420a63
#endif

#ifndef MUIM_Backfill
#define MUIM_Backfill 0x80428d73
struct  MUIP_Backfill { ULONG MethodID; LONG left; LONG top; LONG right; LONG bottom; LONG xoffset; LONG yoffset; };
#endif

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

#endif

struct VideoAreaData
{
};

DEFNEW(VideoArea)
{
	obj = (Object *)DoSuperNew(cl, obj,
		MUIA_CycleChain, FALSE,
		InnerSpacing(0,0),
#if USE_BACKFILL
		MUIA_FillArea, TRUE,
		MUIA_CustomBackfill, TRUE,
#endif
		TAG_MORE, INITTAGS
	);

	return ((IPTR)obj);
}

DEFMMETHOD(AskMinMax)
{
	DOSUPER;

	msg->MinMaxInfo->MinHeight = 120;
	msg->MinMaxInfo->DefHeight = 480;

	return 0;
}

#if USE_BACKFILL
DEFMMETHOD(Backfill)
{
	LONG left = msg->left, top = msg->top, right = msg->right, bottom = msg->bottom;
	struct Rectangle b1, b2, k;
	struct Rectangle bounds = { left, top, right, bottom };

//	  kprintf("backfill %d %d %d %d x_offset %d y_offset %d\n", left, top, right, bottom, mygui->x_offset, mygui->y_offset);

	/* key rect */
	k.MinX = left + mygui->x_offset;
	k.MinY = top + mygui->y_offset;
	k.MaxX = right - mygui->x_offset;
	k.MaxY = bottom - mygui->y_offset;

	AndRectRect(&k, &bounds);

	if (mygui->x_offset || mygui->y_offset)
	{
		if (mygui->x_offset)
		{
			/* left rect */
			b1.MinX = left;
			b1.MinY = top;
			b1.MaxX = left + mygui->x_offset;
			b1.MaxY = bottom;

			/* right rect */
			b2.MinX = right - mygui->x_offset;
			b2.MinY = top;
			b2.MaxX = right;
			b2.MaxY = bottom;
		}
		else if (mygui->y_offset)
		{
			/* top rect */
			b1.MinX = left;
			b1.MinY = top;
			b1.MaxX = right;
			b1.MaxY = top + mygui->y_offset;

			/* bottom rect */
			b2.MinX = left;
			b2.MinY = bottom - mygui->y_offset;
			b2.MaxX = right;
			b2.MaxY = bottom;
		}

		AndRectRect(&b1, &bounds);
		AndRectRect(&b2, &bounds);
	}

	/* draw rects, if visible */

	if (mygui->x_offset || mygui->y_offset)
	{
		if (IsValidRect(&b1))
		{
			//kprintf("b1 (%d %d %d %d)\n", b1.MinX, b1.MinY, b1.MaxX, b1.MaxY);
			FillPixelArray(_rp(obj), b1.MinX, b1.MinY,
			               b1.MaxX - b1.MinX + 1, b1.MaxY - b1.MinY + 1,
			               0x00000000);
		}

		if (IsValidRect(&b2))
		{
			//kprintf("b2 (%d %d %d %d)\n", b2.MinX, b2.MinY, b2.MaxX, b2.MaxY);
			FillPixelArray(_rp(obj), b2.MinX, b2.MinY,
			               b2.MaxX - b2.MinX + 1, b2.MaxY - b2.MinY + 1,
			               0x00000000);
		}
	}

	if (IsValidRect(&k))
	{
		//kprintf("k (%d %d %d %d)\n", k.MinX, k.MinY, k.MaxX, k.MaxY);
		FillPixelArray(_rp(obj), k.MinX, k.MinY,
		               k.MaxX - k.MinX + 1, k.MaxY - k.MinY + 1,
					   mygui->colorkey);
	}

	return (TRUE);
}
#endif

DEFMMETHOD(Draw)
{
	DOSUPER;

	if(mygui->embedded)
	{
		if (msg->flags & MADF_DRAWOBJECT)
		{
			struct RastPort *rp = _rp(obj);

			mygui->video_top = _top(obj);
			mygui->video_bottom = _bottom(obj);
			mygui->video_left = _left(obj);
			mygui->video_right = _right(obj);

//			  kprintf("video area: %d %d %d %d\n", mygui->video_top, mygui->video_bottom, mygui->video_left, mygui->video_right);

#if !USE_BACKFILL
			FillPixelArray(rp,
						   mygui->video_left,
						   mygui->video_top,
						   mygui->video_right - mygui->video_left + 1,
						   mygui->video_bottom - mygui->video_top + 1,
						   0x00000000);

			FillPixelArray(rp,
						   mygui->video_left + mygui->x_offset,
						   mygui->video_top + mygui->y_offset,
						   mygui->video_right - mygui->video_left + 1 - 2*mygui->x_offset,
						   mygui->video_bottom - mygui->video_top + 1 - 2*mygui->y_offset,
						   mygui->colorkey);
#endif
		}
	}

	return 0;
}

BEGINMTABLE2(videoareaclass)
DECNEW(VideoArea)
DECMMETHOD(AskMinMax)
DECMMETHOD(Draw)
#if USE_BACKFILL
DECMMETHOD(Backfill)
#endif
ENDMTABLE

DECSUBCLASS_NC(MUIC_Rectangle, videoareaclass, VideoAreaData)

