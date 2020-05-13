#include <clib/macros.h>
#include <proto/dos.h>

#include "mp_fifo.h"
#include "osdep/keycodes.h"

#include "gui/interface.h"
#include "gui.h"

/******************************************************************
 * seeksliderclass
 *****************************************************************/

struct SeekSliderData
{
	TEXT buffer[64];
	
	struct MUI_EventHandlerNode ehnode;
};

DEFNEW(SeekSlider)
{
	obj = (Object *)DoSuperNew(cl, obj,
//		  MUIA_Numeric_Min,  0,
//		  MUIA_Numeric_Max,  100,
		MUIA_Slider_Min,   0,
		MUIA_Slider_Max,   100,
		MUIA_Slider_Level, 0,
		TAG_MORE, INITTAGS,
	End;

	return (IPTR)obj;
}

DEFMMETHOD(Setup)
{
	struct SeekSliderData *data = INST_DATA(cl, obj);

	if (!DOSUPER)
	{
		return (0);
	}

	data->ehnode.ehn_Object = obj;
	data->ehnode.ehn_Class = cl;
	data->ehnode.ehn_Events =  IDCMP_RAWKEY;
	data->ehnode.ehn_Priority = 3;
	data->ehnode.ehn_Flags = MUI_EHF_GUIMODE;
	DoMethod(_win(obj), MUIM_Window_AddEventHandler, (IPTR)&data->ehnode);

	return TRUE;
}

DEFMMETHOD(Cleanup)
{
	struct SeekSliderData *data = INST_DATA(cl, obj);

	DoMethod(_win(obj), MUIM_Window_RemEventHandler, (IPTR)&data->ehnode);

	return DOSUPER;
}

DEFMMETHOD(Numeric_Stringify)
{
	struct SeekSliderData *data = INST_DATA(cl, obj);

	int	h=0, m=0, s=0;
	int val = msg->value;

	h = val / 3600;
	m = val / 60 % 60;
	s = val % 60;

	snprintf(data->buffer, sizeof(data->buffer), "%02d:%02d:%02d", h, m, s);

	return ((IPTR)data->buffer);
}

DEFMMETHOD(AskMinMax)
{
	ULONG maxwidth, defwidth, minwidth;

	DOSUPER;

	maxwidth = 0;
	defwidth = 0;
	minwidth = 100;

	msg->MinMaxInfo->MinWidth  += minwidth;
	msg->MinMaxInfo->DefWidth  += defwidth;
	msg->MinMaxInfo->MaxWidth  += maxwidth;

	return 0;
}

DEFMMETHOD(HandleEvent)
{
	if (msg->imsg)
	{
		ULONG Class;
		UWORD Code;
		int MouseX, MouseY;

		Class     = msg->imsg->Class;
		Code      = msg->imsg->Code;
		MouseX    = msg->imsg->MouseX;
		MouseY    = msg->imsg->MouseY;

		if(_isinobject(obj, MouseX, MouseY))
		{
			switch( Class )
			{
				case IDCMP_RAWKEY:
					 switch ( Code )
					 {
						 case NM_WHEEL_UP:
	                        mplayer_put_key(KEY_RIGHT);
	                        return MUI_EventHandlerRC_Eat;
						 case NM_WHEEL_DOWN:
							mplayer_put_key(KEY_LEFT);
	                        return MUI_EventHandlerRC_Eat;
					}
		    }
		}
	}

	return DOSUPER;
}

BEGINMTABLE2(seeksliderclass)
DECNEW(SeekSlider)
DECMMETHOD(AskMinMax)
DECMMETHOD(Numeric_Stringify)
DECMMETHOD(Setup)
DECMMETHOD(Cleanup)
DECMMETHOD(HandleEvent)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Slider, seeksliderclass, SeekSliderData)

