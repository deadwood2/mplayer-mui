#include <clib/macros.h>
#include <proto/dos.h>

#include "gui/interface.h"
#include "gui.h"

/******************************************************************
 * prefspopstringclass
 *****************************************************************/

struct PrefsPopStringData
{
};


DEFMMETHOD(Popstring_Close)
{
	DoMethod(mygui->prefsgroup, MM_PrefsGroup_RefreshDrivers);

	return DOSUPER;
}

BEGINMTABLE2(prefspopstringclass)
DECMMETHOD(Popstring_Close)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Popobject, prefspopstringclass, PrefsPopStringData)
