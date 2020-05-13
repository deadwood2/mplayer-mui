#include <proto/dos.h>
#include <clib/macros.h>

#include "input/input.h"

#include "gui/interface.h"
#include "gui.h"

struct MPlayerAppData
{
};

DEFNEW(MPlayerApp)
{
	obj = (Object *)DoSuperNew(cl, obj,
		    TAG_MORE, INITTAGS);

	return (IPTR)obj;
}

DEFSET(MPlayerApp)
{
	ULONG rc = 0;

	if(mygui->embedded)
	{
		FORTAG(((struct opSet *)msg)->ops_AttrList)
		{
			case MUIA_Application_Iconified:
			{
				if(tag->ti_Data == TRUE)
				{
					// prevent iconification in fullscreen.
					if(mygui->fullscreen) return 0;
				}
			}
			break;
		}
		NEXTTAG
	}

	rc = DOSUPER;
	return rc;
}

DEFSMETHOD(MPlayerApp_DisposeWindow)
{
	if(msg->obj)
	{
		DoMethod(obj, OM_REMMEMBER, msg->obj);
		MUI_DisposeObject((Object *)msg->obj);
	}

	return (TRUE);
}

BEGINMTABLE2(mplayerappclass)
DECNEW(MPlayerApp)
DECSET(MPlayerApp)
DECSMETHOD(MPlayerApp_DisposeWindow)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Application, mplayerappclass, MPlayerAppData)
