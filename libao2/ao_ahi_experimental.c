/*
 * Copyright (C) 2005 Mark Olsen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

// OS headers
#include <exec/exec.h>
#include <devices/ahi.h>
#include <proto/exec.h>
#define USE_INLINE_STDARG
#include <proto/ahi.h>

#include <hardware/byteswap.h>

#include <stdio.h>

// MPlayer interface
#include "mp_msg.h"
#include "../libaf/af_format.h"
#include "audio_out.h"
#include "audio_out_internal.h"



typedef struct {
	int			channels;
	int			samples;				// mono samples in buffer
	int			submission_chunk;		// don't mix less than this #
	int			samplebits;
	int			speed;
	char		*buffer;
} dma_t;

struct AHIChannelInfo
{
	struct AHIEffChannelInfo aeci;
	ULONG offset;
};

struct AHIdata
{
	struct MsgPort *msgport;
	struct AHIRequest *ahireq;
	int ahiopen;
	struct AHIAudioCtrl *audioctrl;
	void *samplebuffer;
	struct Hook EffectHook;
	struct AHIChannelInfo aci;
	unsigned int readpos;
	unsigned int write_offset;
	unsigned int volume;
};

dma_t dma;
struct AHIdata *ad;
struct Library * AHIBase = 0;
int min_free_space = 0;

ULONG EffectFunc()
{
	struct Hook *hook = (struct Hook *)REG_A0;
	struct AHIEffChannelInfo *aeci = (struct AHIEffChannelInfo *)REG_A1;

	struct AHIdata *ad;

	ad = hook->h_Data;

	ad->readpos = aeci->ahieci_Offset[0];

	return 0;
}

static struct EmulLibEntry EffectFunc_Gate =
{
	TRAP_LIB, 0, (void (*)(void))EffectFunc
};


// Mplayer AO driver interface
typedef enum
{
  CO_NONE,
  CO_8_U2S,
  CO_16_U2S,
  CO_16_LE2BE,
  CO_16_LE2BE_U2S,
  CO_24_BE2BE,
  CO_24_LE2BE,
  CO_32_LE2BE
} convop_t;
convop_t convop;

static ao_info_t info =
{
#if defined(__MORPHOS__) || defined(__AROS__)
	"AHI audio output using low-level API (MorphOS)",
#else
   "AHI audio output using low-level API (AmigaOS)",
#endif
	"ahi",
	"Fabien Coeurjoly",
#if defined(__MORPHOS__) || defined(__AROS__)
	  "MorphOS Rulez :-)"
#else
	  "Amiga Survivor !",
#endif
};

// define the standard functions that mplayer will use.
// see define in audio_aout_internal.h
LIBAO_EXTERN(ahi)

// Interface implementation

/***************************** CONTROL *******************************/
// to set/get/query special features/parameters
static int control(int cmd, void *arg)
{
	switch (cmd)
	{
		case AOCONTROL_GET_VOLUME:
		{
			ao_control_vol_t* vol = (ao_control_vol_t*)arg;
			vol->left = vol->right =  ( (float) (ad->volume >> 8 ) ) / 2.56 ;
			return CONTROL_OK;
		}

		case AOCONTROL_SET_VOLUME:
		{
			float diff;
			ao_control_vol_t* vol = (ao_control_vol_t*)arg;
			diff = (vol->left+vol->right) / 2;
			ad->volume =  ( (int) (diff * 2.56) ) << 8 ;
			AHI_SetVol(0, ad->volume, 0x8000L, ad->audioctrl, AHISF_IMM);
			return CONTROL_OK;
		}
	}
	return CONTROL_UNKNOWN;
}

/***************************** INIT **********************************/
// open & setup audio device
// return: 1=success 0=fail
static int init(int rate,int channels,int format,int flags)
{
	int AHIType = 0;
	ULONG bits;
	ULONG r;
	struct AHISampleInfo sample;
	int playsamples;
	char modename[64];

	// Init ao.data to make mplayer happy :-)
	ao_data.channels   = channels;
	ao_data.samplerate = rate;
	ao_data.format     = format;
	ao_data.bps        = channels * rate;

	if (format != AF_FORMAT_U8 && format != AF_FORMAT_S8)
	{	
		ao_data.bps *= 2;
		if (format == AF_FORMAT_S24_BE || format == AF_FORMAT_S24_LE || format == AF_FORMAT_S32_BE || format == AF_FORMAT_S32_LE)
		{
			ao_data.bps *= 2;
		}
	}

	switch ( format )
	{
		case AF_FORMAT_U8:
		   convop  = CO_8_U2S;
		   AHIType = channels > 1 ? AHIST_S8S : AHIST_M8S;
		   break;
		case AF_FORMAT_S8:
		   convop  = CO_NONE;
		   AHIType = channels > 1 ? AHIST_S8S : AHIST_M8S;
		   break;
		case AF_FORMAT_U16_BE:
		   convop  = CO_16_U2S;
		   AHIType = channels > 1 ? AHIST_S16S : AHIST_M16S;
		   break;
		case AF_FORMAT_S16_BE:
		   convop  = CO_NONE;
		   AHIType = channels > 1 ? AHIST_S16S : AHIST_M16S;
		   break;
		case AF_FORMAT_U16_LE:
		   convop  = CO_16_LE2BE_U2S;
		   AHIType = channels > 1 ? AHIST_S16S : AHIST_M16S;
		   break;
		case AF_FORMAT_S16_LE:
		   convop  = CO_16_LE2BE;
		   AHIType = channels > 1 ? AHIST_S16S : AHIST_M16S;
		   break;
		case AF_FORMAT_S24_BE:
		   convop  = CO_24_BE2BE;
		   AHIType = channels > 1 ? AHIST_S32S : AHIST_M32S;
		   break;
		case AF_FORMAT_S24_LE:
		   convop  = CO_24_LE2BE;
		   AHIType = channels > 1 ? AHIST_S32S : AHIST_M32S;
		   break;
		case AF_FORMAT_S32_BE:
		   convop  = CO_NONE;
		   AHIType = channels > 1 ? AHIST_S32S : AHIST_M32S;
		   break;
		case AF_FORMAT_S32_LE:
		   convop  = CO_32_LE2BE;
		   AHIType = channels > 1 ? AHIST_S32S : AHIST_M32S;
		   break;
		case AF_FORMAT_AC3:
		   convop  = CO_NONE;
		   AHIType = AHIST_S16S;
		   rate = 48000;
		   break;
		default:
		   mp_msg(MSGT_AO, MSGL_WARN, "AHI: Sound format not supported by this driver.\n");
		   return 0; // return fail !
	}

	if (ad)
		return 0;

	if (rate == 0)
		rate = 44100;

	ad = AllocVec(sizeof(*ad), MEMF_ANY);
	if (ad)
	{
		ad->msgport = CreateMsgPort();
		if (ad->msgport)
		{
			ad->ahireq = (struct AHIRequest *)CreateIORequest(ad->msgport, sizeof(struct AHIRequest));
			if (ad->ahireq)
			{
				ad->ahiopen = !OpenDevice("ahi.device", AHI_NO_UNIT, (struct IORequest *)ad->ahireq, 0);
				if (ad->ahiopen)
				{
					AHIBase = (struct Library *)ad->ahireq->ahir_Std.io_Device;

					ad->audioctrl = AHI_AllocAudio(AHIA_AudioID, AHI_DEFAULT_ID,
												   AHIA_MixFreq, rate,
					                               AHIA_Channels, 1,
					                               AHIA_Sounds, 1,
					                               TAG_END);

					if (ad->audioctrl)
					{
						bits = AHI_SampleFrameSize(AHIType)*8/channels;

						AHI_GetAudioAttrs(AHI_INVALID_ID, ad->audioctrl,
						                  AHIDB_BufferLen, sizeof(modename),
						                  AHIDB_Name, (ULONG)modename,
						                  AHIDB_MaxChannels, (ULONG)&channels,
						                  AHIDB_Bits, (ULONG)&bits,
										  AHIDB_MaxPlaySamples, (ULONG) &playsamples,
						                  TAG_END);

						AHI_ControlAudio(ad->audioctrl,
										 AHIC_MixFreq_Query, (ULONG)&rate,
						                 TAG_END);

						if (bits == 8 || bits == 16)
						{
							if (channels > 2)
								channels = 2;

							dma.speed = rate;
							dma.samplebits = bits;
							dma.channels = channels;
							dma.samples = ao_data.bps/AHI_SampleFrameSize(AHIType);
							dma.submission_chunk = 1;

							ad->volume = 0x10000;
							ad->readpos = 0;
							ad->write_offset = 0;

							ad->samplebuffer = AllocVec(ao_data.bps, MEMF_ANY|MEMF_CLEAR);
							if (ad->samplebuffer)
							{
								dma.buffer = ad->samplebuffer;

								sample.ahisi_Type    = AHIType;
								sample.ahisi_Address = ad->samplebuffer;
								sample.ahisi_Length  = ao_data.bps/AHI_SampleFrameSize(AHIType);

								r = AHI_LoadSound(0, AHIST_DYNAMICSAMPLE, &sample, ad->audioctrl);
								if (r == 0)
								{
									r = AHI_ControlAudio(ad->audioctrl,
									                     AHIC_Play, TRUE,
									                     TAG_END);

									if (r == 0)
									{
										AHI_Play(ad->audioctrl,
										         AHIP_BeginChannel, 0,
												 AHIP_Freq, rate,
												 AHIP_Vol, ad->volume,
										         AHIP_Pan, 0x8000,
										         AHIP_Sound, 0,
										         AHIP_EndChannel, NULL,
										         TAG_END);

										ad->aci.aeci.ahie_Effect = AHIET_CHANNELINFO;
										ad->aci.aeci.ahieci_Func = &ad->EffectHook;
										ad->aci.aeci.ahieci_Channels = 1;

										ad->EffectHook.h_Entry = (void *)&EffectFunc_Gate;
										ad->EffectHook.h_Data = ad;

										AHI_SetEffect(&ad->aci, ad->audioctrl);

										printf("Using AHI mode \"%s\" for audio output\n", modename);
										printf("Channels: %d bits: %d frequency: %d\n", channels, (int) bits, rate);

										ao_data.outburst = playsamples * AHI_SampleFrameSize( AHIType );
										ao_data.buffersize = ao_data.bps;

										printf("outburst = %d buffersize = %d\n", ao_data.outburst, ao_data.buffersize);

										return 1;
									}
								}
							}
							FreeVec(ad->samplebuffer);
						}
						AHI_FreeAudio(ad->audioctrl);
					}
					else
					{
						printf("Failed to allocate AHI audio\n");
					}

					CloseDevice((struct IORequest *)ad->ahireq);
				}
				DeleteIORequest((struct IORequest *)ad->ahireq);
			}
			DeleteMsgPort(ad->msgport);
		}
		FreeVec(ad);
	}

	return 0;
}

/***************************** UNINIT ********************************/
// close audio device
static void uninit(int immed)
{
	ad->aci.aeci.ahie_Effect = AHIET_CHANNELINFO|AHIET_CANCEL;
	AHI_SetEffect(&ad->aci.aeci, ad->audioctrl);
	AHI_ControlAudio(ad->audioctrl,
	                 AHIC_Play, FALSE,
	                 TAG_END);

	AHI_FreeAudio(ad->audioctrl);
	FreeVec(ad->samplebuffer);
	CloseDevice((struct IORequest *)ad->ahireq);
	DeleteIORequest((struct IORequest *)ad->ahireq);
	DeleteMsgPort(ad->msgport);
	FreeVec(ad);

	ad = 0;	   
	AHIBase = 0;
}

/***************************** RESET **********************************/
// stop playing and empty buffers (for seeking/pause)
static void reset()
{
	memset(ad->samplebuffer, 0, ao_data.buffersize);
	ad->write_offset = 0;
	AHI_SetSound(0, 0, 0, 0, ad->audioctrl, NULL);
}

/***************************** PAUSE **********************************/
// stop playing, keep buffers (for pause)
static void audio_pause()
{
	AHI_ControlAudio(ad->audioctrl,	AHIC_Play, FALSE, TAG_END);
}

/***************************** RESUME **********************************/
// resume playing, after audio_pause()
static void audio_resume()
{
	AHI_ControlAudio(ad->audioctrl, AHIC_Play, TRUE, TAG_END);
}

/***************************** GET_SPACE *******************************/
// return: how many bytes can be played without blocking
static int get_space()
{
	int space;
	printf("**********GetSpace***********\n");
	printf("write_offset = %d play pos = %d\n", ad->write_offset, ad->readpos*dma.channels*(dma.samplebits/8));
	space = ao_data.buffersize - (ad->write_offset - ad->readpos*dma.channels*(dma.samplebits/8));
	if(space > ao_data.buffersize) space -= ao_data.buffersize;
	printf("get_space = %d min free space = %d\n", space, min_free_space);
	if(space < min_free_space) return 0;
	return space - min_free_space;
}

/***************************** GET_DELAY **********************************/
// return: delay in seconds between first and last sample in buffer
static float get_delay()
{
	int space = ad->readpos*dma.channels*(dma.samplebits/8) - ad->write_offset;
	printf("write_offset = %d play pos = %d\n", ad->write_offset, ad->readpos*dma.channels*(dma.samplebits/8));
	if(space <= 0) space+=ao_data.buffersize;
	printf("**********GetDelay***********\n");
	printf("get_delay = %f\n", (float)(ao_data.buffersize - space)/(float)ao_data.bps);
	return (float)(ao_data.buffersize - space)/(float)ao_data.bps;
}

/***************************** PLAY ***********************************/
// plays 'len' bytes of 'data'
// it should round it down to outburst*n
// return: number of bytes played
static int play(void* data,int len,int flags)
{
	// This function just fill a buffer which is played by another task
	int data_to_put, data_put;
	int play_pos;
	int space;
	int i, imax;
	char * buffer = ad->samplebuffer;

	printf("***************Play**************\n");
	printf("len = %d\n", len);

	play_pos = ad->readpos*dma.channels*(dma.samplebits/8);
	space = ao_data.buffersize - (ad->write_offset - play_pos);
	if(space > ao_data.buffersize) space -= ao_data.buffersize;
	if(space < len) len = space;

	if (!(flags & AOPLAY_FINAL_CHUNK))
		len = (len/ao_data.outburst)*ao_data.outburst;

	printf("rounded len = %d\n", len);
	printf("outburst = %d buffersize = %d write_offset = %d\n", ao_data.outburst, ao_data.buffersize, ad->write_offset);

	data_put = data_to_put = len;

	printf("data_put = %d\n", data_put);

	#if 0

	switch (convop)
	{
		case CO_NONE:
		CopyMem( data, buffer + ad->write_offset, data_to_put);
		break;

		case CO_8_U2S:
		imax = data_to_put;
		for (i = 0; i < imax ; i++)
		{
			((BYTE *) (buffer + ad->write_offset))[i] = ((UBYTE *)data)[i] - 128;
		}
		break;

		case CO_16_U2S:
		imax = data_to_put / 2;
		for (i = 0; i < imax; i++)
		{
			((WORD *) (buffer + ad->write_offset))[i] = ((UWORD *)data)[i] - 32768;
		}
		break;

		case CO_16_LE2BE_U2S:
		imax = data_to_put / 2;
		for (i = 0; i < imax; i++)
		{
			((WORD *) (buffer + ad->write_offset))[i] = SWAPWORD(((UWORD *)data)[i]) - 32768;
		}
		break;

		case CO_16_LE2BE:
		imax = data_to_put / 2;
		for (i = 0; i < imax; i++)
		{
			((WORD *) (buffer + ad->write_offset))[i] = SWAPWORD(((WORD *)data)[i]);
		}
		break;

		case CO_24_BE2BE:
		imax = data_to_put / 4;
		data_to_put = imax * 4;
		data_put = imax * 3;
		for (i = 0; i < imax; i++)
		{
			((LONG *) (buffer + ad->write_offset))[i] = (LONG)(*((ULONG *)(((UBYTE *)data)+((i*3)-1))) << 8);
		}
		break;

		case CO_24_LE2BE:
		imax = data_to_put / 4;
		data_to_put = imax * 4;
		data_put = imax * 3;
		for (i = 0; i < imax; i++)
		{
			((LONG *) (buffer + ad->write_offset))[i] = (LONG)(SWAPLONG(*((ULONG *)(((UBYTE *)data)+(i*3)))) << 8);
		}
		break;

		case CO_32_LE2BE:
		imax = data_to_put / 4;
		for (i = 0; i < imax; i++)
		{
			((LONG *) (buffer + ad->write_offset))[i] = SWAPLONG(((LONG *)data)[i]);
		}
		break;
	}
	#endif
	// update buffer_put
	ad->write_offset += data_to_put;
	ad->write_offset %= ao_data.buffersize;

	printf("new write_offset = %d\n", ad->write_offset);

	return data_put;
}

 
