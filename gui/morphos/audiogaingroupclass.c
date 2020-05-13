#include <clib/macros.h>

#include "gui/interface.h"
#include "gui.h"

/******************************************************************
 * audiogaingroupclass
 *****************************************************************/

struct AudioGainGroupData
{
	APTR BT_OK;
	APTR BT_Reset;
	APTR BT_Cancel;
	APTR SL_Gain;
};

DEFNEW(AudioGainGroup)
{
	APTR BT_OK, BT_Reset, BT_Cancel, SL_Gain;

	obj = (Object *)DoSuperNew(cl, obj,
		Child,
			ColGroup(2),
				Child, TextObject, MUIA_Text_Contents, "Volume Gain:", End,
				Child, SL_Gain = SliderObject, MUIA_Slider_Min, -30,
											   MUIA_Slider_Max, 30,
											   MUIA_Slider_Level, 0,
								 End,
				End,
		Child,
			HGroup,
				Child, BT_OK = KeyButton("Apply", 'a'),
				Child, HSpace(0),
				Child, BT_Reset = KeyButton("Reset", 'r'),
				Child, HSpace(0),
				Child, BT_Cancel = KeyButton("Cancel", 'c'),
				End,

		TAG_DONE
	);

	if (obj)
	{
		struct AudioGainGroupData *data = INST_DATA(cl, obj);

		data->BT_OK     = BT_OK;
		data->BT_Reset  = BT_Reset;
		data->BT_Cancel = BT_Cancel;
		data->SL_Gain   = SL_Gain;

		DoMethod(data->BT_OK, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_AudioGainGroup_Apply);

		DoMethod(data->BT_Reset, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_AudioGainGroup_Reset);

		DoMethod(data->BT_Cancel, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_AudioGainGroup_Cancel);
	}

	return (IPTR)obj;
}

DEFTMETHOD(AudioGainGroup_Apply)
{
	struct AudioGainGroupData *data = INST_DATA(cl, obj);

	LONG gain = getv(data->SL_Gain, MUIA_Slider_Level);

	DoMethod(mygui->maingroup, MM_MPlayerGroup_AudioGain, gain, TRUE);

	set(mygui->audiogainwindow, MUIA_Window_Open, FALSE);

	return 0;
}

DEFTMETHOD(AudioGainGroup_Reset)
{
	DoMethod(mygui->maingroup, MM_MPlayerGroup_AudioGain, 0, FALSE);

	set(mygui->audiogainwindow, MUIA_Window_Open, FALSE);

	return 0;
}

DEFTMETHOD(AudioGainGroup_Cancel)
{
	set(mygui->audiogainwindow, MUIA_Window_Open, FALSE);

	return 0;
}

BEGINMTABLE2(audiogaingroupclass)
DECNEW(AudioGainGroup)
DECTMETHOD(AudioGainGroup_Apply)
DECTMETHOD(AudioGainGroup_Reset)
DECTMETHOD(AudioGainGroup_Cancel)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Group, audiogaingroupclass, AudioGainGroupData)
