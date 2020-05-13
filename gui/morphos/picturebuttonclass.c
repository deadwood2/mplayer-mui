#include "gui/interface.h"
#include "gui.h"

struct PictureButtonData
{
	Object		*Image;
	Object		*Text;
	ULONG		Hit;
	ULONG		UserData;
	char		Path[256];
};

#ifndef MUIA_Dtpic_Alpha
#define MUIA_Dtpic_Alpha                    0x8042b4db /* V20 isg LONG              */ /* private */
#endif
#ifndef MUIA_Dtpic_Fade
#define MUIA_Dtpic_Fade                     0x80420429 /* V20 isg LONG              */ /* private */
#endif
#ifndef MUIA_Dtpic_DarkenSelState
#define MUIA_Dtpic_DarkenSelState           0x80423247 /* V20 i.g BOOL              */
#endif
#ifndef MUIA_Dtpic_LightenOnMouse
#define MUIA_Dtpic_LightenOnMouse           0x8042966a /* V20 i.g BOOL              */
#endif

DEFNEW(PictureButton)
{
	obj = (Object *)DoSuperNew(cl, obj,
			MUIA_Dtpic_DarkenSelState,TRUE,
			MUIA_Dtpic_LightenOnMouse,TRUE,
			MUIA_ShowSelState, FALSE,
			MUIA_InputMode , MUIV_InputMode_RelVerify,
			//MUIA_Dtpic_Fade,1,
			//MUIA_Dtpic_Alpha,0xff,
		    TAG_MORE, INITTAGS);

	if (obj)
	{
		struct PictureButtonData *data = INST_DATA(cl,obj);
		char *ptr;

		if ((ptr = (char*)GetTagData(MA_PictureButton_Path, NULL, INITTAGS)))
		{
			stccpy(data->Path, ptr, sizeof(data->Path));
			set(obj, MUIA_Dtpic_Name, data->Path);
		}

		data->UserData = GetTagData(MA_PictureButton_UserData, NULL, INITTAGS);

		DoMethod(obj, MUIM_Notify, MUIA_Pressed, FALSE,
				 obj, 3, MUIM_Set, MA_PictureButton_Hit, data->UserData);

		return (IPTR)obj;
	}

	return(0);
}

static void doset(APTR obj, struct PictureButtonData *data, struct TagItem *tags)
{
	FORTAG(tags)
	{
		case MA_PictureButton_Hit:
		{
			data->Hit = tag->ti_Data;
		}
		break;

		case MA_PictureButton_UserData:
		{
			data->UserData = tag->ti_Data;
		}
		break;
	}
	NEXTTAG
}

DEFGET(PictureButton)
{
	struct PictureButtonData *data = INST_DATA(cl,obj);

	switch (msg->opg_AttrID)
	{
		case MA_PictureButton_Hit:
		{
			*msg->opg_Storage = data->Hit;
			return (TRUE);
		}
		break;

		case MA_PictureButton_UserData:
		{
			*msg->opg_Storage = data->UserData;
			return (TRUE);
		}
		break;
	}

	return (DOSUPER);
}

DEFSET(PictureButton)
{
	struct PictureButtonData *data = INST_DATA(cl,obj);

	doset(obj, data, INITTAGS);

	return DOSUPER;
}

BEGINMTABLE2(picturebuttonclass)
DECNEW(PictureButton)
DECGET(PictureButton)
DECSET(PictureButton)
ENDMTABLE

DECSUBCLASS_NC(MUIC_Dtpic, picturebuttonclass, PictureButtonData)
