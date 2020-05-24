#include <stdio.h>

#include "include/macros/vapor.h"
#include "classes.h"

/* Classes management */

struct classdesc {
	char * name;
	APTR initfunc;
	APTR cleanupfunc;
};

#define CLASSENT(s) {#s, create_##s##class, delete_##s##class}

/* classes declaration */

static const struct classdesc cd[] = {
	CLASSENT(mplayerapp),
	CLASSENT(mplayerwindow),
	CLASSENT(mplayergroup),
	CLASSENT(playlistgroup),
	CLASSENT(urlgroup),
	CLASSENT(dvddirgroup),
	CLASSENT(propertiesgroup),
	CLASSENT(prefsgroup),
	CLASSENT(prefsgeneralgroup),
	CLASSENT(playlistlist),
	CLASSENT(prefslist),
	CLASSENT(seekslider),
	CLASSENT(volumeslider),
	CLASSENT(picturebutton),
	CLASSENT(spacer),
	CLASSENT(videoarea),
	CLASSENT(poplist),
	CLASSENT(popstring),
	CLASSENT(urlpopstring),
	CLASSENT(prefspopstring),
	CLASSENT(cropgroup),
	CLASSENT(scalegroup),
	CLASSENT(audiogaingroup),
	CLASSENT(consolegroup),
	CLASSENT(consolelist),
	CLASSENT(equalizergroup),
//	  CLASSENT(prefsgroup),
//	  CLASSENT(aboutgroup),
	{ 0, 0, 0 }
};

ULONG classes_init(void)
{
	ULONG i;

	for (i = 0; cd[i].name; i++)
	{
		if (!(*(int(*)(void))cd[i].initfunc)())
		{
			fprintf(stderr, "Couldn't create class %s.\n", cd[i].name);
			return (FALSE);
		}
	}
	return (TRUE);
}


void classes_cleanup(void)
{
	LONG i;

	for (i = sizeof(cd) / sizeof(struct classdesc) - 2; i >= 0; i--)
	{
//		  fprintf(stderr, "destroying %s", cd[i].name);
		(*(void(*)(void))cd[i].cleanupfunc)();
	}
}

