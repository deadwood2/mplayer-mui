#include <clib/macros.h>

#include "gui/interface.h"
#include "gui.h"

/******************************************************************
 * equalizergroupclass
 *****************************************************************/

struct EqualizerGroupData
{
	APTR BT_Apply;
	APTR BT_Reset;
	APTR BT_Close;
	APTR STR_Param;
};

DEFNEW(EqualizerGroup)
{
	APTR BT_Apply, BT_Reset, BT_Close, STR_Param;

	obj = (Object *)DoSuperNew(cl, obj,
		Child,
			HGroup,
				Child, STR_Param = StringObject,
					StringFrame,
					MUIA_String_Contents, "0:0:0:0:0:-2:-5:-7:-5:0",
					End,
				End,
		Child,
			HGroup,
				Child, BT_Apply = KeyButton("Apply", 'a'),
				Child, HSpace(0),
				Child, BT_Reset = KeyButton("Reset", 'r'),
				Child, HSpace(0),
				Child, BT_Close = KeyButton("Close", 'c'),
				End,

		TAG_DONE
	);

	if (obj)
	{
		struct EqualizerGroupData *data = INST_DATA(cl, obj);

		data->BT_Apply  = BT_Apply;
		data->BT_Reset  = BT_Reset;
		data->BT_Close  = BT_Close;
		data->STR_Param  = STR_Param;

		DoMethod(data->BT_Apply, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_EqualizerGroup_Apply);

		DoMethod(data->BT_Reset, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_EqualizerGroup_Reset);

		DoMethod(data->BT_Close, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_EqualizerGroup_Close);
	}

	return (IPTR)obj;
}

DEFTMETHOD(EqualizerGroup_Apply)
{
	struct EqualizerGroupData *data = INST_DATA(cl, obj);

	STRPTR param = (STRPTR)getv(data->STR_Param, MUIA_String_Contents);

	DoMethod(mygui->maingroup, MM_MPlayerGroup_Equalizer, param , TRUE);

	return 0;
}

DEFTMETHOD(EqualizerGroup_Reset)
{
	DoMethod(mygui->maingroup, MM_MPlayerGroup_Equalizer, NULL, FALSE);

	set(mygui->equalizerwindow, MUIA_Window_Open, FALSE);

	return 0;
}

DEFTMETHOD(EqualizerGroup_Close)
{
	set(mygui->equalizerwindow, MUIA_Window_Open, FALSE);

	return 0;
}

BEGINMTABLE2(equalizergroupclass)
DECNEW(EqualizerGroup)
DECTMETHOD(EqualizerGroup_Apply)
DECTMETHOD(EqualizerGroup_Reset)
DECTMETHOD(EqualizerGroup_Close)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Group, equalizergroupclass, EqualizerGroupData)
