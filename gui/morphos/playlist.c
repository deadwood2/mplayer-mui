/*
 * MPlayer GUI for Win32
 * Copyright (C) 2003 Sascha Sommer <saschasommer@freenet.de>
 * Copyright (C) 2006 Erik Augustson <erik_27can@yahoo.com>
 * Copyright (C) 2006 Gianluigi Tiesi <sherpya@netfarm.it>
 *
 * This file is part of MPlayer.
 *
 * MPlayer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * MPlayer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with MPlayer; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <dirent.h>

#include <proto/dos.h>
#include "gui.h"
#undef set
#include "stream/stream.h"
#include "mp_msg.h"
#include "playlist.h"
#include "morphos_stuff.h"
#include <clib/debug_protos.h>
#include <time.h>

/* TODO: implement sort_playlist */


BOOL adddirtoplaylist(playlist_t *playlist, const char *path, BOOL recursive, BOOL refresh_gui)
{
	struct stat st;
	DIR * dir;
	struct dirent * entry;
	char findpath[MAX_PATH], filename[MAX_PATH];

	stccpy(findpath, path, sizeof(findpath));

	dir = opendir(findpath);

	if(dir)
	{
		while((entry = readdir(dir)))
		{
			stccpy(filename, findpath, sizeof(filename));
			AddPart(filename, entry->d_name, sizeof(filename));

			if(stat(filename, &st) != -1)
			{
#if !defined(__AROS__)
				if(S_ISDIR(st.st_mode))
#else
                if((S_ISDIR(st.st_mode)) && (strcmp(entry->d_name, ".") != 0)
                        && (strcmp(entry->d_name, "..") != 0))
#endif
				{
					if(recursive)
					{
						adddirtoplaylist(playlist, filename, recursive, refresh_gui);
					}
				}
				else
				{
					if(name_match(filename, /*PATTERN_STREAM*/MorphOS_GetPattern()) && !name_match(filename, PATTERN_PLAYLIST))
					{
						playlist->add_track(playlist, filename, NULL, NULL, 0, STREAMTYPE_FILE);
						
						if(refresh_gui)
						{
							DoMethod(mygui->playlistgroup, MM_PlaylistGroup_Add, mygui->playlist->tracks[mygui->playlist->trackcount - 1]);
						}
					}
				}
			}		 
		}
		closedir(dir);
	}

    return TRUE;
}

static void add_track(playlist_t *playlist, const char *filename, const char *artist, const char *title, int duration, int type)
{
    (playlist->trackcount)++;
	playlist->tracks = realloc(playlist->tracks, playlist->trackcount * sizeof(pl_track_t *));
    playlist->tracks[playlist->trackcount - 1] = calloc(1, sizeof(pl_track_t));
	memset(playlist->tracks[playlist->trackcount - 1], 0, sizeof(pl_track_t));
    if(filename) playlist->tracks[playlist->trackcount - 1]->filename = strdup(filename);
    if(artist) playlist->tracks[playlist->trackcount - 1]->artist = strdup(artist);
    if(title) playlist->tracks[playlist->trackcount - 1]->title = strdup(title);
    if(duration) playlist->tracks[playlist->trackcount - 1]->duration = duration;
	playlist->tracks[playlist->trackcount - 1]->type = type;
}

static void remove_track(playlist_t *playlist, int number)
{
    pl_track_t **tmp = calloc(1, playlist->trackcount * sizeof(pl_track_t *));
    int i, p = 0;
    memcpy(tmp, playlist->tracks, playlist->trackcount * sizeof(pl_track_t *));
    (playlist->trackcount)--;

    playlist->tracks = realloc(playlist->tracks, playlist->trackcount * sizeof(pl_track_t *));
    for(i=0; i<playlist->trackcount + 1; i++)
    {
        if(i != (number - 1))
        {
            playlist->tracks[p] = tmp[i];
            p++;
        }
        else
        {
 			if(tmp[i]->filename) free(tmp[i]->filename);
            if(tmp[i]->artist) free(tmp[i]->artist);
            if(tmp[i]->title) free(tmp[i]->title);
            free(tmp[i]);
        }
    }
    free(tmp);

	if(playlist->trackcount == 0)
	{
	    playlist->tracks = NULL;
	    playlist->current = 0;
	}
	else
	{
		if(playlist->current > number - 1) playlist->current--;
		else if(playlist->current == (number - 1) && playlist->current > (playlist->trackcount - 1) ) playlist->current--;
	}
}

static void moveup_track(playlist_t *playlist, int number)
{
    pl_track_t *tmp;
    if(number == 1) return; /* already first */
    tmp = playlist->tracks[number - 2];
    playlist->tracks[number - 2] = playlist->tracks[number - 1];
    playlist->tracks[number - 1] = tmp;

	if(playlist->current == number - 2) playlist->current++;
	else if(playlist->current == number - 1) playlist->current--;
}

static void movedown_track(playlist_t *playlist, int number)
{
    pl_track_t *tmp;
    if(number == playlist->trackcount) return; /* already latest */
    tmp = playlist->tracks[number];
    playlist->tracks[number] = playlist->tracks[number - 1];
    playlist->tracks[number - 1] = tmp;

	if(playlist->current == number - 1) playlist->current++;
	else if(playlist->current == number) playlist->current--;
}

static void clear_playlist(playlist_t *playlist)
{
    while(playlist->trackcount) playlist->remove_track(playlist, 1);
    playlist->tracks = NULL;
    playlist->current = 0;
}

static void free_playlist(playlist_t *playlist)
{
    if(playlist->tracks) playlist->clear_playlist(playlist);
    free(playlist);
}

static void dump_playlist(playlist_t *playlist)
{
    int i;
    for (i=0; i<playlist->trackcount; i++)
    {
		mp_msg(MSGT_GPLAYER, MSGL_INFO, "track %i %s ", i + 1, playlist->tracks[i]->filename);
		if(playlist->tracks[i]->artist) mp_msg(MSGT_GPLAYER, MSGL_INFO, "%s ", playlist->tracks[i]->artist);
		if(playlist->tracks[i]->title) mp_msg(MSGT_GPLAYER, MSGL_INFO, "- %s ", playlist->tracks[i]->title);
		if(playlist->tracks[i]->duration) mp_msg(MSGT_GPLAYER, MSGL_INFO, "%i ", playlist->tracks[i]->duration);
		mp_msg(MSGT_GPLAYER, MSGL_INFO, "\n");
    }
}

void shuffle_playlist(playlist_t *playlist)
{
	int i, j;
	pl_track_t * tmp;

	srand(time(NULL));

	for(i = playlist->trackcount - 1; i > 0; i--)
	{
		j = (int) ((float) i * ((float) rand()/(RAND_MAX + 1.0)));

		if(i==j) break;

		if(i == playlist->current) playlist->current = j;
		else if(j == playlist->current) playlist->current = i;

		tmp = playlist->tracks[i];
		playlist->tracks[i] = playlist->tracks[j];
		playlist->tracks[j] = tmp;
	}
}

static int sort_direction = 1;

static int playlist_compare_title(void const *a, void const *b)
{
	pl_track_t * a1 = *(pl_track_t **) a;
	pl_track_t * b1 = *(pl_track_t **) b;
	char* str1, * str2;

	if(a1->title) str1 = a1->title; else str1 = FilePart(a1->filename);
	if(b1->title) str2 = b1->title; else str2 = FilePart(b1->filename);

	return sort_direction * stricmp(str1, str2);
}

static int playlist_compare_duration(void const *a, void const *b)
{
	pl_track_t * a1 = *(pl_track_t **) a;
	pl_track_t * b1 = *(pl_track_t **) b;

	if(a1->duration > b1->duration)
		return sort_direction * 1;
	else if(a1->duration < b1->duration)
		return sort_direction * -1;
	else
		return 0;
}

static int playlist_compare_artist(void const *a, void const *b)
{
	int rc = 0;
	pl_track_t * a1 = *(pl_track_t **) a;
	pl_track_t * b1 = *(pl_track_t **) b;

	if ( !a1->artist && !b1->artist  )
		rc = 0;
	else if ( !b1->artist )
		rc = 1;
	else if ( !a1->artist )
		rc = -1;
	else
		rc = stricmp( a1->artist, b1->artist );

	if(rc == 0)
	{
		return playlist_compare_title(a, b);
	}
	else
	{
		return sort_direction * rc;
	}
}

static int playlist_compare_filename(void const *a, void const *b)
{
	pl_track_t * a1 = *(pl_track_t **) a;
	pl_track_t * b1 = *(pl_track_t **) b;
	char* str1, * str2;

	str1 = a1->filename;
	str2 = b1->filename;

	return sort_direction * stricmp(str1, str2);
}

static void sort_playlist(playlist_t *playlist, int opt, int direction)
{
	int i;
	pl_track_t * current_entry;

	if(!playlist->tracks) return;

	sort_direction = direction;

	current_entry = playlist->tracks[playlist->current];

	if(opt == 1)
	{
		qsort(playlist->tracks, playlist->trackcount, sizeof(pl_track_t *), playlist_compare_title);
	}
	else if(opt == 2)
	{
		qsort(playlist->tracks, playlist->trackcount, sizeof(pl_track_t *), playlist_compare_duration);
	}
	else if(opt == 3)
	{
		qsort(playlist->tracks, playlist->trackcount, sizeof(pl_track_t *), playlist_compare_artist);
	}
	else if(opt == 4)
	{
		qsort(playlist->tracks, playlist->trackcount, sizeof(pl_track_t *), playlist_compare_filename);
	}

	for(i=0; i<playlist->trackcount; i++)
	{
		if(playlist->tracks[i] == current_entry)
		{
			playlist->current = i;
		}
	}
}

playlist_t *create_playlist(void)
{
    playlist_t *playlist = calloc(1, sizeof(playlist_t));
    playlist->add_track = add_track;
    playlist->remove_track = remove_track;
    playlist->moveup_track = moveup_track;
    playlist->movedown_track = movedown_track;
    playlist->dump_playlist = dump_playlist;
    playlist->sort_playlist = sort_playlist;
    playlist->clear_playlist = clear_playlist;
    playlist->free_playlist = free_playlist;
	playlist->shuffle_playlist = shuffle_playlist;
    return playlist;
}

