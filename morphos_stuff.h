#ifndef __MORPHOS_STUFF_H__
#define __MORPHOS_STUFF_H__

#include <exec/types.h>

int MorphOS_Open(int argc, char *argv[]);
void MorphOS_Close(void);
void MorphOS_ParseArg(int argc, char *argv[], int *new_argc, char ***new_argv);
char *MorphOS_GetWindowTitle(void);
char *MorphOS_GetScreenTitle(void);
char *MorphOS_GetLastVisitedPath(void);
void MorphOS_SetLastVisitedPath(char *);
char *MorphOS_GetPattern(void);
void MorphOS_SetPattern(char *);
void MorphOS_RestorePath(void);
void MorphOS_SavePath(void);

int vo_init(void);

STRPTR stristr(CONST_STRPTR str1, CONST_STRPTR str2);

#define PUBLIC_SCREEN 0

#define __MPLAYER__

#ifdef __MPLAYER__
#define MPLAYER 1
#else
#define MPLAYER 0
#endif

#if MPLAYER
void gui_handle_events (void);
#endif

#endif
