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

#include <stdlib.h>
#include <string.h>

#include "help_mp.h"
#include "m_config.h"
#include "m_option.h"
#include "mixer.h"
#include "mplayer.h"
#include "mp_msg.h"
#include "mpcommon.h"
#include "path.h"
#include "libass/ass.h"
#include "libvo/video_out.h"
#include "sub/sub.h"

#include "gui/interface.h"
#include "gui.h"

/* params */
int   gtkAONorm = 0;
int   gtkAOExtraStereo = 0;
float gtkAOExtraStereoMul = 1.0;
int   gtkCacheOn = 0;
int   gtkCacheSizeDVD = 8192;
int   gtkCacheSizeNet = 2048;
int   gtkCacheSizeFile = 0;
int   gtkAutoSyncOn = 0;
int   gtkAutoSync = 0;

//int sub_window = 0;
//int console = 0;

//int gui_save_pos = 1;
//int gui_main_pos_x = -2;
//int gui_main_pos_y = -2;
//int gui_sub_pos_x = -1;
//int gui_sub_pos_y = -1;

int gui_window_dimensions = 0;
int gui_show_gui = 1;
int gui_show_status = 1;
int gui_show_toolbar = 1;
int gui_show_control = 1;
int gui_use_dvdnav = 0;
int gui_black_status = 0;

int gui_repeat_mode = 0;

int remember_path = 1;
int remember_playlist = 0;

#ifdef CONFIG_ASS
gtkASS_t gtkASS;
#endif

/* External functions */
extern char *stream_dump_name;
extern char *proc_priority;
extern int m_config_parse_config_file(m_config_t *config, char *conffile);

m_config_t *gui_conf;
static const m_option_t gui_opts[] =
{
    {   "vo_driver", &video_driver_list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL },
	{   "v_framedrop", &frame_dropping, CONF_TYPE_INT, CONF_RANGE, 0, 2, NULL },
    {   "ao_driver", &audio_driver_list, CONF_TYPE_STRING_LIST, 0, 0, 0, NULL },
//	  {   "ao_volnorm", &gtkAONorm, CONF_TYPE_FLAG, 0, 0, 1, NULL },
//	  {   "softvol", &soft_vol, CONF_TYPE_FLAG, 0, 0, 1, NULL },
//	  {   "ao_extra_stereo", &gtkAOExtraStereo, CONF_TYPE_FLAG, 0, 0, 1, NULL },
//	  {   "ao_extra_stereo_coefficient", &gtkAOExtraStereoMul, CONF_TYPE_FLOAT, CONF_RANGE, -10, 10, NULL },
//	  {   "delay", &audio_delay, CONF_TYPE_FLOAT, CONF_RANGE, -100.0, 100.0, NULL},
    {   "dvd_device", &dvd_device, CONF_TYPE_STRING, 0, 0, 0, NULL },
//	  {   "cdrom_device", &cdrom_device, CONF_TYPE_STRING, 0, 0, 0, NULL },
//	  {   "osd_level", &osd_level, CONF_TYPE_INT, CONF_RANGE, 0, 3, NULL },

//	  {   "sub_auto_load",&sub_auto,CONF_TYPE_FLAG,0,0,1,NULL },
//	  {   "sub_unicode",&sub_unicode,CONF_TYPE_FLAG,0,0,1,NULL },
   #ifdef CONFIG_ASS
	{   "ass_enabled",&ass_enabled,CONF_TYPE_FLAG,0,0,1,NULL },
//	  {   "ass-color", &ass_color, CONF_TYPE_STRING, 0, 0, 0, NULL},
//	  {   "ass-border-color", &ass_border_color, CONF_TYPE_STRING, 0, 0, 0, NULL},
//	  {   "ass_use_margins",&ass_use_margins,CONF_TYPE_FLAG,0,0,1,NULL },
//	  {   "ass_top_margin",&ass_top_margin,CONF_TYPE_INT,CONF_RANGE,0,512,NULL },
//	  {   "ass_bottom_margin",&ass_bottom_margin,CONF_TYPE_INT,CONF_RANGE,0,512,NULL },
   #endif
//	  {   "sub_pos",&sub_pos,CONF_TYPE_INT,CONF_RANGE,0,200,NULL },
//	  {   "sub_overlap",&suboverlap_enabled,CONF_TYPE_FLAG,0,0,0,NULL },
   #ifdef CONFIG_ICONV
//	  {   "sub_cp",&sub_cp,CONF_TYPE_STRING,0,0,0,NULL },
   #endif
//	  {   "font_factor",&font_factor,CONF_TYPE_FLOAT,CONF_RANGE,0.0,10.0,NULL },
//	  {   "font_name",&font_name,CONF_TYPE_STRING,0,0,0,NULL },
   #ifdef CONFIG_FREETYPE
//	  {   "font_encoding",&subtitle_font_encoding,CONF_TYPE_STRING,0,0,0,NULL },
//	  {   "font_text_scale",&text_font_scale_factor,CONF_TYPE_FLOAT,CONF_RANGE,0,100,NULL },
//	  {   "font_osd_scale",&osd_font_scale_factor,CONF_TYPE_FLOAT,CONF_RANGE,0,100,NULL },
//	  {   "font_blur",&subtitle_font_radius,CONF_TYPE_FLOAT,CONF_RANGE,0,8,NULL },
//	  {   "font_outline",&subtitle_font_thickness,CONF_TYPE_FLOAT,CONF_RANGE,0,8,NULL },
//	  {   "font_autoscale",&subtitle_autoscale,CONF_TYPE_INT,CONF_RANGE,0,3,NULL },
   #endif

//	  {   "cache", &gtkCacheOn, CONF_TYPE_FLAG, 0, 0, 1, NULL },
	{   "cache_size_dvd", &gtkCacheSizeDVD, CONF_TYPE_INT, CONF_RANGE, -1, 65535, NULL },
	{   "cache_size_net", &gtkCacheSizeNet, CONF_TYPE_INT, CONF_RANGE, -1, 65535, NULL },
	{   "cache_size_file", &gtkCacheSizeFile, CONF_TYPE_INT, CONF_RANGE, -1, 65535, NULL },
    {   "autosync", &gtkAutoSyncOn, CONF_TYPE_FLAG, 0, 0, 1, NULL },
    {   "autosync_size", &gtkAutoSync, CONF_TYPE_INT, CONF_RANGE, 0, 10000, NULL },

	{   "dumpfile", &stream_dump_name, CONF_TYPE_STRING, 0, 0, 0, NULL },
	{   "remember_path", &remember_path, CONF_TYPE_FLAG, 0, 0, 1, NULL },
	{   "remember_playlist", &remember_playlist, CONF_TYPE_FLAG, 0, 0, 1, NULL },

	{   "loop", &gui_repeat_mode, CONF_TYPE_INT, CONF_RANGE, 0, 3, NULL },

	{   "gui_window_dimensions", &gui_window_dimensions, CONF_TYPE_INT, CONF_RANGE, 0, 3, NULL },
	{   "gui_show_gui", &gui_show_gui, CONF_TYPE_FLAG, 0, 0, 1, NULL },
	{   "gui_show_toolbar", &gui_show_toolbar, CONF_TYPE_FLAG, 0, 0, 1, NULL },
	{   "gui_show_control", &gui_show_control, CONF_TYPE_FLAG, 0, 0, 1, NULL },
	{   "gui_show_status", &gui_show_status, CONF_TYPE_FLAG, 0, 0, 1, NULL },
	{  	"gui_use_dvdnav", &gui_use_dvdnav, CONF_TYPE_FLAG, 0, 0, 1, NULL },
	{  	"gui_black_status", &gui_black_status, CONF_TYPE_FLAG, 0, 0, 1, NULL },
    {   NULL, NULL, 0, 0, 0, 0, NULL }
};

char *gfgets(char *str, int size, FILE *f)
{
    char *s = fgets(str, size, f);
    char c;
    if(s)
    {
        c = s[strlen(s) - 1];
        if ((c == '\n') || (c == '\r'))
            s[strlen(s) - 1]=0;
        c = s[strlen(s) - 1];
        if ((c == '\n') || (c == '\r'))
            s[strlen(s) - 1]=0;
    }
    return s;
}

int cfg_gui_include(m_option_t *conf, const char *filename)
{
    (void)conf;

    return m_config_parse_config_file(gui_conf, filename);
}

void cfg_read(void)
{
    char *cfg = get_path("gui.conf");

    /* read configuration */
    mp_msg(MSGT_GPLAYER, MSGL_V, "[GUI] [cfg] reading config file: %s\n", cfg);
    gui_conf = m_config_new();
    m_config_register_options(gui_conf, gui_opts);
    if (m_config_parse_config_file(gui_conf, cfg) < 0)
        mp_msg(MSGT_GPLAYER, MSGL_FATAL, MSGTR_ConfigFileError);
    free(cfg);
}

void cfg_write(void)
{
    char *cfg = get_path("gui.conf");
    FILE *f;
    int i;

    /* save configuration */
    if ((f = fopen(cfg, "wt+")))
    {
        for (i=0; gui_opts[i].name; i++)
        {
            char *v = m_option_print(&gui_opts[i], gui_opts[i].p);
            if(v == (char *)-1) {
                mp_msg(MSGT_GPLAYER, MSGL_WARN, MSGTR_UnableToSaveOption, gui_opts[i].name);
                v = NULL;
            }
            if(v)
            {
                fprintf(f, "%s = \"%s\"\n", gui_opts[i].name, v);
                free(v);
            }
        }
        fclose(f);
    }
    free(cfg);
}
