#include <proto/asl.h>
#include <clib/macros.h>

#include "gui/interface.h"
#include "gui.h"

/******************************************************************
 * urlgroupclass
 *****************************************************************/

struct URLGroupData
{
	APTR BT_OK;
	APTR BT_Cancel;
	APTR ST_URL;
	APTR TargetObj;
};

DEFNEW(URLGroup)
{
	APTR BT_OK, BT_Cancel, ST_URL;

	obj = (Object *)DoSuperNew(cl, obj,
		Child,
			HGroup,
				Child, MUI_NewObject(MUIC_Dtpic, MUIA_Dtpic_Name, "PROGDIR:images/url.png", End,
				Child, ST_URL = (Object *) NewObject(geturlpopstringclass(), NULL, TAG_DONE),
				End,
		Child,
			HGroup,
				Child, BT_OK = KeyButton("Ok", 'o'),
				Child, HSpace(0),
				Child, BT_Cancel = KeyButton("Cancel", 'c'),
				End,

        TAG_MORE, INITTAGS
	);

	if (obj)
	{
		struct URLGroupData *data = INST_DATA(cl, obj);

		data->BT_OK = BT_OK;
		data->BT_Cancel = BT_Cancel;
		data->ST_URL = ST_URL;
		data->TargetObj = (APTR) GetTagData(MA_URLGroup_Target, NULL, INITTAGS);

		set(data->ST_URL, MUIA_CycleChain, 1);

		DoMethod(data->BT_OK, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_URLGroup_Open);

		DoMethod(data->BT_Cancel, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 1, MM_URLGroup_Cancel);

		DoMethod(data->ST_URL, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime,
				 obj, 1, MM_URLGroup_Open);
	}

	return (IPTR)obj;
}

DEFTMETHOD(URLGroup_Open)
{
	struct URLGroupData *data = INST_DATA(cl, obj);

	STRPTR url = (STRPTR) getv(data->ST_URL, MUIA_String_Contents);

	DoMethod(data->ST_URL, MM_URLPopString_Insert, url);
	DoMethod(data->TargetObj, MM_MPlayer_OpenStream, url);

	return 0;
}

DEFTMETHOD(URLGroup_Cancel)
{
	struct URLGroupData *data = INST_DATA(cl, obj);

	DoMethod(data->TargetObj, MM_MPlayer_OpenStream, NULL);

	return 0;
}

DEFMMETHOD(Show)
{
	struct URLGroupData *data = INST_DATA(cl, obj);

	ULONG rc = DOSUPER;

	set(data->ST_URL, MA_URLPopString_ActivateString, TRUE);

	return rc;
}

#if defined(__AROS__)
DEFSET(URLGroup)
{
    struct URLGroupData *data = INST_DATA(cl, obj);

    FORTAG(INITTAGS)
    {
        case MA_URLGroup_Target:
            data->TargetObj = (APTR)tag->ti_Data;
            break;
    }
    NEXTTAG

    return DOSUPER;
}
#endif

BEGINMTABLE2(urlgroupclass)
DECNEW(URLGroup)
#if defined(__AROS__)
DECSET(URLGroup)
#endif
DECMMETHOD(Show)
DECTMETHOD(URLGroup_Open)
DECTMETHOD(URLGroup_Cancel)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Group, urlgroupclass, URLGroupData)
