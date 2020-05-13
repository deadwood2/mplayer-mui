#include <proto/dos.h>
#include <clib/macros.h>

#include "input/input.h"

#include "gui/interface.h"
#include "gui.h"

struct MPlayerWindowData
{
	ULONG opened;
};

DEFNEW(MPlayerWindow)
{
	obj = (Object *)DoSuperNew(cl, obj,
		    TAG_MORE, INITTAGS);

	if (obj)
	{
		struct MPlayerWindowData *data = INST_DATA(cl,obj);

		data->opened = FALSE;

		return (IPTR)obj;
	}

	return(0);
}

DEFSET(MPlayerWindow)
{
	struct MPlayerWindowData *data = INST_DATA(cl,obj);
	IPTR rc = 0;

	if(mygui->embedded)
	{
		FORTAG(((struct opSet *)msg)->ops_AttrList)
		{
			case MUIA_Window_Open:
			{
				if(tag->ti_Data == FALSE && data->opened)
				{
					//Cgx_StopWindow(mygui->videowindow);

					if(guiInfo.Playing)
					{
						if(mygui->vo_opened)
						{
							mygui->gui_ready = FALSE;

							mp_input_queue_cmd(mp_input_parse_cmd("gui_hide"));

							// wait for vo to be released before closing window
							while(mygui->vo_opened)
							{
								Delay(1);
							}
						}
					}

					rc = DOSUPER;

					data->opened = tag->ti_Data;

					return rc;
				}
				else if(tag->ti_Data == TRUE && !data->opened)
				{
					rc = DOSUPER;

					data->opened = tag->ti_Data;

					mygui->videowindow = (struct Window *) getv(obj, MUIA_Window);
					mygui->gui_ready = TRUE;

					//Cgx_StartWindow(mygui->videowindow);

					return rc;
				}
			}
			break;
		}
		NEXTTAG
	}

	rc = DOSUPER;

	return rc;
}

BEGINMTABLE2(mplayerwindowclass)
DECNEW(MPlayerWindow)
DECSET(MPlayerWindow)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Window, mplayerwindowclass, MPlayerWindowData)
