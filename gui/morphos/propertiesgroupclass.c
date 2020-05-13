#include <proto/asl.h>
#include <clib/macros.h>

#include "command.h"

#include "gui/interface.h"
#include "gui.h"

#define PROPERTY(attribute) property_expand_string(guiInfo.mpcontext, attribute)

#define TITLE(label) \
			HGroup,\
				Child, MUI_MakeObject(MUIO_HBar, NULL),\
				Child,\
					TextObject,\
						MUIA_Text_Contents, label,\
						MUIA_Text_SetMax, TRUE,\
						MUIA_Font, MUIV_Font_Big,\
						MUIA_Text_PreParse, "\33b\33c",\
						End,\
				Child, MUI_MakeObject(MUIO_HBar, NULL),\
				End

#define LINE(label, value) \
				Child,\
					TextObject,\
						MUIA_Text_Contents, "\033r\033b" label,\
						End,\
				Child,\
					TextObject,\
						MUIA_Text_PreParse, "\33l",\
						MUIA_Text_Contents, (value && *value) ? value : "n/a",\
						MUIA_Text_SetMax, FALSE,\
						MUIA_Text_SetMin, FALSE,\
						End

/******************************************************************
 * propertiesgroupclass
 *****************************************************************/

struct PropertiesGroupData
{
	APTR GR_Root;
	APTR GR_Button;
};

DEFNEW(PropertiesGroup)
{
	obj = (Object *)DoSuperNew(cl, obj,
		Child,
			TITLE("General"),
		TAG_DONE
	);

	if (obj)
	{
		APTR o;
		struct PropertiesGroupData *data = INST_DATA(cl, obj);

		DoMethod(data->GR_Root, MUIM_Group_InitChange);

		o =	data->GR_Root = VGroup,
								Child, VSpace(0),
								Child, TextObject, MUIA_Text_Contents, "\033c\033bNo information available", End,
								Child, VSpace(0),
								End;

		DoMethod(obj, OM_ADDMEMBER, o);

		DoMethod(data->GR_Root, MUIM_Group_ExitChange);
	}

	return (IPTR)obj;
}

DEFTMETHOD(PropertiesGroup_Update)
{
	struct PropertiesGroupData *data = INST_DATA(cl, obj);
	APTR o;
	STRPTR ptr;

	DoMethod(obj, MUIM_Group_InitChange);

	if(data->GR_Root) 
	{
		DoMethod(obj, OM_REMMEMBER, data->GR_Root);
		MUI_DisposeObject(data->GR_Root);
		data->GR_Root = NULL;
	}

	o =	data->GR_Root = VGroup, End;
	DoMethod(obj, OM_ADDMEMBER, o);

	/* General information */
	o = ColGroup(2),
			LINE("Filename:", PROPERTY("${filename}")),
			LINE("Length:",   PROPERTY("${length}")),
			LINE("Format:",   PROPERTY("${demuxer}")),
			End;

	DoMethod(data->GR_Root, OM_ADDMEMBER, o);

#warning "Check if this VideoWindow check works"
	if(guiInfo.VideoWindow)
	//if(!guiInfo.AudioOnly)
	{
		o = TITLE("Video");
			
		DoMethod(data->GR_Root, OM_ADDMEMBER, o);

		o = ColGroup(2),
				LINE("Video Codec:",       PROPERTY("${video_codec}")),
				LINE("Video Bitrate:",     PROPERTY("${video_bitrate}")),
				LINE("Resolution:",        PROPERTY("${width} x ${height}")),
				LINE("Frames per Second:", PROPERTY("${fps}")),
				LINE("Aspect:",            PROPERTY("${aspect}")),
				End;

		DoMethod(data->GR_Root, OM_ADDMEMBER, o);
	}

	o = TITLE("Audio");

	DoMethod(data->GR_Root, OM_ADDMEMBER, o);

	o = ColGroup(2),
			LINE("Audio Codec:",   PROPERTY("${audio_codec}")),
			LINE("Audio Bitrate:", PROPERTY("${audio_bitrate}")),
			LINE("Audio Samples:", PROPERTY("${samplerate}, ${channels}")),
			End;

	DoMethod(data->GR_Root, OM_ADDMEMBER, o);

	o = TITLE("Clip Info"),

	DoMethod(data->GR_Root, OM_ADDMEMBER, o);

	o = ColGroup(2),
			LINE("Title:",    PROPERTY("${metadata/Title}")),
			LINE("Artist:",   PROPERTY("${metadata/Artist}")),
			LINE("Album:",    PROPERTY("${metadata/Album}")),
			LINE("Year:",     PROPERTY("${metadata/Year}")),
			LINE("Comment:",  PROPERTY("${metadata/Comment}")),
			LINE("Track:",    PROPERTY("${metadata/Track}")),
			LINE("Genre:",    PROPERTY("${metadata/Genre}")),
			LINE("Software:", PROPERTY("${metadata/Software}")),
			End;

	DoMethod(data->GR_Root, OM_ADDMEMBER, o);
	
	DoMethod(obj, MUIM_Group_ExitChange);

	return 0;
}

BEGINMTABLE2(propertiesgroupclass)
DECNEW(PropertiesGroup)
DECTMETHOD(PropertiesGroup_Update)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Group, propertiesgroupclass, PropertiesGroupData)
