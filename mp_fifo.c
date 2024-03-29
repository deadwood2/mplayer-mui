/*
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
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <stdlib.h>
#include "osdep/timer.h"
#include "input/input.h"
#include "input/mouse.h"
#include "mp_fifo.h"

#if defined(__MORPHOS__) || defined(__AROS__)
#include <proto/intuition.h>
#endif

int key_fifo_size = 7;
static int *key_fifo_data;
static unsigned key_fifo_read;
static unsigned key_fifo_write;
static int previous_down_key;

static void mplayer_put_key_internal(int code){
  int fifo_free = key_fifo_read + key_fifo_size - key_fifo_write;
  if (key_fifo_data == NULL)
    key_fifo_data = malloc(key_fifo_size * sizeof(int));
  if(!fifo_free) return; // FIFO FULL!!
  // reserve some space for key release events to avoid stuck keys
  // Make sure we do not reset key state because of a down event
  if((code & MP_KEY_DOWN) && fifo_free <= (key_fifo_size >> 1))
    return;
  // in the worst case, just reset key state
  if (fifo_free == 1) {
    // ensure we do not only create MP_KEY_RELEASE_ALL events
    if (previous_down_key & MP_KEY_RELEASE_ALL)
      return;
    // HACK: this ensures that a fifo size of 2 does
    // not queue any key presses while still allowing
    // the mouse wheel to work (which sends down and up
    // at nearly the same time
    if (code != previous_down_key)
      code = 0;
    code |= MP_KEY_RELEASE_ALL;
  }
  key_fifo_data[key_fifo_write % key_fifo_size]=code;
  key_fifo_write++;
  if (code & MP_KEY_DOWN)
    previous_down_key = code & ~MP_KEY_DOWN;
  else
    previous_down_key = code & MP_KEY_RELEASE_ALL;
}

int mplayer_get_key(int fd){
  int key;
  if (key_fifo_data == NULL)
    return MP_INPUT_NOTHING;
  if(key_fifo_write==key_fifo_read) return MP_INPUT_NOTHING;
  key=key_fifo_data[key_fifo_read % key_fifo_size];
  key_fifo_read++;
  return key;
}


unsigned doubleclick_time = 300;

static void put_double(int code) {
  if (code >= MOUSE_BTN0 && code <= MOUSE_BTN_LAST)
    mplayer_put_key_internal(code - MOUSE_BTN0 + MOUSE_BTN0_DBL);
}

void mplayer_put_key(int code) {
  static unsigned last_key_time[2];
  static int last_key[2];
  unsigned now = GetTimerMS();
  // ignore system-doubleclick if we generate these events ourselves
  if (doubleclick_time &&
      (code & ~MP_KEY_DOWN) >= MOUSE_BTN0_DBL &&
      (code & ~MP_KEY_DOWN) <= MOUSE_BTN_LAST_DBL)
    return;
  mplayer_put_key_internal(code);
  if (code & MP_KEY_DOWN) {
    code &= ~MP_KEY_DOWN;
    last_key[1] = last_key[0];
    last_key[0] = code;
    last_key_time[1] = last_key_time[0];
    last_key_time[0] = now;
/*
    if (last_key[1] == code &&
        now - last_key_time[1] < doubleclick_time)
      put_double(code);
    return;
  }
  if (last_key[0] == code && last_key[1] == code &&
      now - last_key_time[1] < doubleclick_time)
    put_double(code);
*/
/* __MORPHOS */
	if (last_key[1] == code &&
		DoubleClick(last_key_time[1]/1000, (last_key_time[1]%1000)*1000, now/1000, (now%1000)*1000)  )
	  put_double(code);
	return;
  }

  if (last_key[0] == code && last_key[1] == code &&
	  DoubleClick(last_key_time[1]/1000, (last_key_time[1]%1000)*1000, now/1000, (now%1000)*1000)  )
	put_double(code);
}
