#include <clib/macros.h>
#include <proto/dos.h>

#include "gui/interface.h"
#include "gui.h"

/******************************************************************
 * popstringclass
 *****************************************************************/

struct PopStringData
{
};

DEFMMETHOD(Popstring_Open)
{
	set(mygui->maingroup, MA_MPlayerGroup_Update, FALSE);

	return DOSUPER;
}

DEFMMETHOD(Popstring_Close)
{
	set(mygui->maingroup, MA_MPlayerGroup_Update, TRUE);

	return DOSUPER;
}

BEGINMTABLE2(popstringclass)
DECMMETHOD(Popstring_Open)
DECMMETHOD(Popstring_Close)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Popobject, popstringclass, PopStringData)
