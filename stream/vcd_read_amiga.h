#include <proto/exec.h>
#include <proto/alib.h>
#include <exec/types.h>

#include <devices/trackdisk.h>
#include <devices/scsidisk.h>

#include "amiga_scsi.h"

typedef struct mp_vcd_priv_st
{
	UBYTE vcd_buf[VCD_SECTOR_SIZE];
	ULONG track_start[MAX_TRACK];
	UBYTE Global_SCSISense[SENSE_LEN];

	ULONG current_sector;
	BYTE first_track=-1;
	BYTE last_track=-1;       // no set
} mp_vcd_priv_t;


static BOOL set_blocksize(struct IOStdReq *My_IOStdReq, ULONG block_size);
static BOOL DiskPresent(struct IOStdReq *My_IOStdReq);
static ULONG read_capacity(struct IOStdReq *My_IOStdReq);
static BOOL check_cd (struct IOStdReq *My_IOStdReq);
static APTR choice_read_sector ( struct IOStdReq *My_IOStdReq);
static void set_speed(struct IOStdReq *My_IOStdReq, UWORD speed);

static BOOL read_sector_mmc1 (struct IOStdReq *My_IOStdReq, ULONG start_block, UBYTE *Data);
static BOOL read_sector_scsi (struct IOStdReq *My_IOStdReq, ULONG start_block, UBYTE *Data);
static BOOL read_sector_msf (struct IOStdReq *My_IOStdReq, ULONG start_block, UBYTE *Data);

static BOOL (*read_sector) (struct IOStdReq *My_IOStdReq, ULONG start_block, UBYTE *Data);

/***************************************/
static inline void vcd_set_msf(mp_vcd_priv_t* vcd, unsigned int sect){
   vcd->current_sector = sect;
}

/****************************************/
static inline unsigned int vcd_get_msf(mp_vcd_priv_t* vcd, int track){
   return vcd->current_sector;
}

/***************************************/
int vcd_seek_to_track(struct IOStdReq *My_IOStdReq,int track){
   if ( (track > last_track) || (track < first_track) ) return -1;
   current_sector=track_start[track-1];
   return track_start[track-1] * VCD_SECTOR_DATA;
}

/***************************************/
int vcd_get_track_end(struct IOStdReq *My_IOStdReq,int track){
   return track_start[last_track] * VCD_SECTOR_DATA ; // -1 in not set or error, should be set by vcd_read_toc
}

/*************************************/
BOOL vcd_read_toc(struct IOStdReq *My_IOStdReq){
   UBYTE MyToc[MAX_TOC_LEN];
	struct SCSICmd MySCSICmd;

	SCSICMD10 MyCmd= {
		SCSI_CMD_READTOC,
		0,
    	PAD, PAD, PAD, PAD,
    	0,				/* starting track */
    	0x03, 0x24,			/* max. TOC data length on current CD-ROMs 804 bytes
				   or 100 TOC track descriptors */
    	PAD
 	 };

   if (!(DiskPresent(My_IOStdReq) ) ) return FALSE;

	My_IOStdReq->io_Command    = HD_SCSICMD;
   My_IOStdReq->io_Data       = &MySCSICmd;
   My_IOStdReq->io_Length     = sizeof(struct SCSICmd);;

	MySCSICmd.scsi_Data 			= (UWORD *) MyToc;
	MySCSICmd.scsi_Length		= MAX_TOC_LEN;
	MySCSICmd.scsi_SenseActual = 0;	
	MySCSICmd.scsi_SenseData	= Global_SCSISense;
	MySCSICmd.scsi_SenseLength	= SENSE_LEN;
	MySCSICmd.scsi_Command		= (UBYTE *) &MyCmd;
	MySCSICmd.scsi_CmdLength	= sizeof(SCSICMD10);
	MySCSICmd.scsi_Flags			= SCSIF_READ | SCSIF_AUTOSENSE;

   DoIO( (struct IORequest *)My_IOStdReq );

	if (My_IOStdReq->io_Error) return FALSE;
	{
		UBYTE current_track;
		UBYTE num_tracks;

		first_track = (UBYTE) MyToc[2];
		last_track = (UBYTE) MyToc[3];

      if (!check_cd(My_IOStdReq)) return FALSE;

		track_start[last_track] = read_capacity(My_IOStdReq); 
      if (!track_start[last_track]) return FALSE;

      num_tracks = last_track - first_track + 1;

      mp_msg(MSGT_OPEN,MSGL_INFO,"There are %d tracks on the CD from %d to %d.\n", num_tracks, first_track, last_track);

		for ( current_track = 0 ; current_track < last_track; current_track++) {
			// Parse all the track
			UBYTE *track_ptr = MyToc + 4 + ( (current_track) *8);
			ULONG track_sec_start = *(ULONG *)(&track_ptr[4]);
         
         track_start[current_track] = track_sec_start;
		}
		for ( current_track = 0 ; current_track < last_track; current_track++) {
			ULONG track_size = track_start[current_track+1] - track_start[current_track] -1;
			ULONG duration_minute = track_size / (75 * 60);
			ULONG duration_second = (ULONG) ( (float) duration_minute - (float) track_size / (75 * 60) ) * 60;
			mp_msg(MSGT_OPEN,MSGL_INFO,"Track %2d: %2ld:%2ld (start sector %ld)\n", current_track, duration_minute, duration_second, track_start[current_track]);			
		}
      
	}

	vcd_seek_to_track(My_IOStdReq, first_track);

   return ( read_sector = choice_read_sector(My_IOStdReq) ) ? TRUE : FALSE;
}

void vcd_close(struct IOStdReq  *My_IOStdReq) {
	// Reset the unit should restore the good cd spped 

	set_speed(My_IOStdReq, 0xffff); 
/*
	struct SCSICmd MySCSICmd;
	SCSICMD6 MyCmd= {
 		SCSI_CMD_RZU,
      0, 
		0, 0
 	 };

	My_IOStdReq->io_Command    = HD_SCSICMD;
   My_IOStdReq->io_Data       = &MySCSICmd;
   My_IOStdReq->io_Length     = sizeof(struct SCSICmd);

	MySCSICmd.scsi_Data 			= (UWORD *) NULL;
	MySCSICmd.scsi_Length		= 0;
	MySCSICmd.scsi_SenseActual = 0;	
	MySCSICmd.scsi_SenseData	= Global_SCSISense;
	MySCSICmd.scsi_SenseLength	= SENSE_LEN;
	MySCSICmd.scsi_Command		= (UBYTE *) &MyCmd;
	MySCSICmd.scsi_CmdLength	= sizeof(MyCmd);
	MySCSICmd.scsi_Flags			= SCSIF_WRITE | SCSIF_AUTOSENSE;

	DoIO( (struct IORequest *)My_IOStdReq );
*/
}

/***************************************/
static int vcd_read(struct IOStdReq *My_IOStdReq, char *mem){
	//kk(printf("current_sector for read= %ld\n", current_sector);)
   if (read_sector(My_IOStdReq, current_sector, vcd_buf) ) {
      memcpy( mem, vcd_buf + VCD_SECTOR_OFFS, VCD_SECTOR_DATA);
      current_sector++;
      return VCD_SECTOR_DATA;
   }
   else {
      return 0;
   }
}

/**************************************/
/**************************************/
/* Useful function for VCD            */
/**************************************/
/**************************************/

static APTR choice_read_sector ( struct IOStdReq *My_IOStdReq) {
	struct SCSICmd MySCSICmd;
   UBYTE buffer[36];
	SCSICMD6 MyCmd= {
 		SCSI_CMD_INQ,
		0,
    	PAD, PAD,  
		32,
    	PAD
 	 };

	My_IOStdReq->io_Command    = HD_SCSICMD;
   My_IOStdReq->io_Data       = &MySCSICmd;
   My_IOStdReq->io_Length     = sizeof(struct SCSICmd);

	MySCSICmd.scsi_Data 			= (UWORD *) buffer;
	MySCSICmd.scsi_Length		= 36;
	MySCSICmd.scsi_SenseActual = 0;	
	MySCSICmd.scsi_SenseData	= Global_SCSISense;
	MySCSICmd.scsi_SenseLength	= SENSE_LEN;
	MySCSICmd.scsi_Command		= (UBYTE *) &MyCmd;
	MySCSICmd.scsi_CmdLength	= sizeof(MyCmd);
	MySCSICmd.scsi_Flags			= SCSIF_READ | SCSIF_AUTOSENSE;

	DoIO( (struct IORequest *)My_IOStdReq );	

	if(My_IOStdReq->io_Error) return NULL;

	if  ( !memcmp("PIONEER", buffer + 8, strlen("PIONEER") ) 
	   || ( !memcmp("PLEXTOR", buffer + 8, strlen("PLEXTOR") ) && !memcmp("CD-ROM", buffer + 0x10, strlen("CD-ROM") )  )) {
		// Real CD driver (aka SCSI like)
		mp_msg(MSGT_OPEN,MSGL_V,"Using SCSI_CMD_RXT command (aka SCSI, Plextor, Yamaha ...).\n");

		//set_speed(My_IOStdReq, 600); // 600 KB/s should be far enough for VCD and less noisy !
		set_blocksize(My_IOStdReq, VCD_SECTOR_SIZE);
   	return  read_sector_scsi;
	}
	else if ( !memcmp("ATAPI", buffer + 8, strlen("ATAPI") ) 
         || !memcmp("SAMSUNG", buffer + 8, strlen("SAMSUNG") ) ) {
		// CDMSF stuff
		mp_msg(MSGT_OPEN,MSGL_V,"Using READ_CDMSF command (TraxData, Samsung ...).\n");
		return read_sector_msf;
	}
	
	mp_msg(MSGT_OPEN,MSGL_V,"Using READ_CD comand.\n");
	return read_sector_mmc1;
}




/**************************/
static BOOL set_blocksize(struct IOStdReq *My_IOStdReq, ULONG block_size) {
	struct SCSICmd MySCSICmd;
	struct ModeSel_Head My_ModeSel_Head;
	SCSICMD6 MyCmd= {
 		SCSI_CMD_MSL,
		1 << 4,
    	PAD, PAD,  
		12,
    	PAD
 	 };

	memset(&My_ModeSel_Head, 0x00, sizeof(struct ModeSel_Head));
	My_ModeSel_Head.block_desc_length 	= 0x08;
	My_ModeSel_Head.block_length_med		= (block_size >> 8) & 0xff;
	My_ModeSel_Head.block_length_lo		= block_size & 0xff;

	My_IOStdReq->io_Command    = HD_SCSICMD;
   My_IOStdReq->io_Data       = &MySCSICmd;
   My_IOStdReq->io_Length     = sizeof(struct SCSICmd);

	MySCSICmd.scsi_Data 			= (UWORD *) &My_ModeSel_Head;
	MySCSICmd.scsi_Length		= sizeof(struct ModeSel_Head);
	MySCSICmd.scsi_SenseActual = 0;	
	MySCSICmd.scsi_SenseData	= Global_SCSISense;
	MySCSICmd.scsi_SenseLength	= SENSE_LEN;
	MySCSICmd.scsi_Command		= (UBYTE *) &MyCmd;
	MySCSICmd.scsi_CmdLength	= sizeof(MyCmd);
	MySCSICmd.scsi_Flags			= SCSIF_WRITE | SCSIF_AUTOSENSE;

	DoIO( (struct IORequest *)My_IOStdReq );

	return My_IOStdReq->io_Error ? FALSE : TRUE;
}

/**************************/
static BOOL check_cd (struct IOStdReq *My_IOStdReq) {
	struct SCSICmd MySCSICmd;
   UBYTE buffer[36];
	SCSICMD6 MyCmd= {
 		SCSI_CMD_INQ,
		0,
    	PAD, PAD,  
		32,
    	PAD
 	 };

	My_IOStdReq->io_Command    = HD_SCSICMD;
   My_IOStdReq->io_Data       = &MySCSICmd;
   My_IOStdReq->io_Length     = sizeof(struct SCSICmd);

	MySCSICmd.scsi_Data 			= (UWORD *) buffer;
	MySCSICmd.scsi_Length		= 36;
	MySCSICmd.scsi_SenseActual = 0;	
	MySCSICmd.scsi_SenseData	= Global_SCSISense;
	MySCSICmd.scsi_SenseLength	= SENSE_LEN;
	MySCSICmd.scsi_Command		= (UBYTE *) &MyCmd;
	MySCSICmd.scsi_CmdLength	= sizeof(MyCmd);
	MySCSICmd.scsi_Flags			= SCSIF_READ | SCSIF_AUTOSENSE;

	DoIO( (struct IORequest *)My_IOStdReq );

	return My_IOStdReq->io_Error ? FALSE : ( ( (buffer[0] & 0x1F) == 0x05 ) ? TRUE : FALSE );
}

/************************/
// Return the size of the CD in sectors
static ULONG read_capacity(struct IOStdReq *My_IOStdReq) {
	struct SCSICmd MySCSICmd;
   ULONG capacity[2];
	SCSICMD10 MyCmd= {
 		SCSI_CMD_RCP,
      PAD,
		0, 0, 0, 0,
    	PAD, PAD, PAD, PAD
 	 };

	My_IOStdReq->io_Command    = HD_SCSICMD;
   My_IOStdReq->io_Data       = &MySCSICmd;
   My_IOStdReq->io_Length     = sizeof(struct SCSICmd);

	MySCSICmd.scsi_Data 			= (UWORD *) &capacity;
	MySCSICmd.scsi_Length		= 8;
	MySCSICmd.scsi_SenseActual = 0;	
	MySCSICmd.scsi_SenseData	= Global_SCSISense;
	MySCSICmd.scsi_SenseLength	= SENSE_LEN;
	MySCSICmd.scsi_Command		= (UBYTE *) &MyCmd;
	MySCSICmd.scsi_CmdLength	= sizeof(MyCmd);
	MySCSICmd.scsi_Flags			= SCSIF_READ | SCSIF_AUTOSENSE;

	DoIO( (struct IORequest *)My_IOStdReq );

	return My_IOStdReq->io_Error ? 0 : capacity[0];
}

/************************/
// Try to set the CD read speed
static void set_speed(struct IOStdReq *My_IOStdReq, UWORD speed) {
	struct SCSICmd MySCSICmd;
	SCSICMD12 MyCmd= {
 		SCSI_CMD_SET_CD_SPEED,
      PAD,
		(speed >> 8) & 0xff, speed & 0xff, 	// Here we put the speed wanted
		(speed >> 8) & 0xff, speed & 0xff, 	// Here we put the speed wanted
    	PAD, PAD, PAD, PAD, PAD
 	 };

	My_IOStdReq->io_Command    = HD_SCSICMD;
   My_IOStdReq->io_Data       = &MySCSICmd;
   My_IOStdReq->io_Length     = sizeof(struct SCSICmd);

	MySCSICmd.scsi_Data 			= (UWORD *) NULL;
	MySCSICmd.scsi_Length		= 0;
	MySCSICmd.scsi_SenseActual = 0;	
	MySCSICmd.scsi_SenseData	= Global_SCSISense;
	MySCSICmd.scsi_SenseLength	= SENSE_LEN;
	MySCSICmd.scsi_Command		= (UBYTE *) &MyCmd;
	MySCSICmd.scsi_CmdLength	= sizeof(MyCmd);
	MySCSICmd.scsi_Flags			= SCSIF_WRITE | SCSIF_AUTOSENSE;

	DoIO( (struct IORequest *)My_IOStdReq );
}

static  BOOL DiskPresent(struct IOStdReq *My_IOStdReq) {
    My_IOStdReq->io_Command    = TD_CHANGESTATE;
    My_IOStdReq->io_Flags      = 0;

    DoIO( (struct IORequest *)My_IOStdReq );

    return My_IOStdReq->io_Actual ? FALSE : TRUE;
}

static BOOL read_sector_scsi (struct IOStdReq *My_IOStdReq, ULONG start_block, UBYTE *Data) {
	ULONG block_count = 1;
	struct SCSICmd MySCSICmd;

	SCSICMD10 command = {
      SCSI_CMD_RXT,
      PAD,  
      start_block >> 24,
      (start_block >> 16) & 0xFF,
      (start_block >> 8) & 0xFF,
      (start_block) & 0xFF,

      (block_count >> 16) & 0xff,
      (block_count >> 8) & 0xFF,
      block_count & 0xFF,
      PAD
   };

	My_IOStdReq->io_Command    = HD_SCSICMD;
   My_IOStdReq->io_Data       = &MySCSICmd;
   My_IOStdReq->io_Length     = sizeof(struct SCSICmd);

	MySCSICmd.scsi_Data 			= (UWORD *) Data;
	MySCSICmd.scsi_Length		= VCD_SECTORSIZE;
	MySCSICmd.scsi_SenseActual = 0;	
	MySCSICmd.scsi_SenseData	= Global_SCSISense;
	MySCSICmd.scsi_SenseLength	= SENSE_LEN;
	MySCSICmd.scsi_Command		= (UBYTE *) &command;
	MySCSICmd.scsi_CmdLength	= sizeof(SCSICMD10);
	MySCSICmd.scsi_Flags			= SCSIF_READ | SCSIF_AUTOSENSE;

	DoIO( (struct IORequest *)My_IOStdReq );

	return My_IOStdReq->io_Error ? FALSE : TRUE;
}

static BOOL read_sector_msf (struct IOStdReq *My_IOStdReq, ULONG start_block, UBYTE *Data) {
	struct SCSICmd MySCSICmd;
   ULONG sector = current_sector;

   SCSICMD12 command = {
      SCSI_CMD_READ_CDMSF,
	   PAD,
      PAD,
      
      0,0,0,
      0,0,0,
	
      0xf8,
      PAD,
      PAD
   };

   command.b3 = sector / ( 60 * 75) ;
	sector -= command.b3 * ( 60 * 75);
   command.b4 = sector / 75;
	sector -= command.b4 * 75;
   command.b5 = sector;

   sector = current_sector+1;
	command.b6 = sector / ( 60 * 75) ;
	sector -= command.b6 * ( 60 * 75);
   command.b7 = sector / 75;
	sector -= command.b7 * 75;
   command.b8 = sector;

	My_IOStdReq->io_Command    = HD_SCSICMD;
   My_IOStdReq->io_Data       = &MySCSICmd;
   My_IOStdReq->io_Length     = sizeof(struct SCSICmd);

	MySCSICmd.scsi_Data 			= (UWORD *) Data;
	MySCSICmd.scsi_Length		= VCD_SECTORSIZE;
	MySCSICmd.scsi_SenseActual = 0;	
	MySCSICmd.scsi_SenseData	= Global_SCSISense;
	MySCSICmd.scsi_SenseLength	= SENSE_LEN;
	MySCSICmd.scsi_Command		= (UBYTE *) &command;
	MySCSICmd.scsi_CmdLength	= sizeof(SCSICMD12);
	MySCSICmd.scsi_Flags			= SCSIF_READ | SCSIF_AUTOSENSE;

	DoIO( (struct IORequest *)My_IOStdReq );

	return My_IOStdReq->io_Error ? FALSE : TRUE;
}

static BOOL read_sector_mmc1 (struct IOStdReq *My_IOStdReq, ULONG start_block, UBYTE *Data) {
	ULONG block_count = 1;
	struct SCSICmd MySCSICmd;

	SCSICMD12 command = {
      SCSI_CMD_READ_CD,
	   0,
      start_block >> 24,
      (start_block >> 16) & 0xFF,
      (start_block >> 8) & 0xFF,
      (start_block) & 0xFF,
   
      (block_count >> 16) & 0xFF,
      (block_count >> 8) & 0xFF,
      block_count & 0xFF,
	
      0xf8,
      PAD
   };

	My_IOStdReq->io_Command    = HD_SCSICMD;
   My_IOStdReq->io_Data       = &MySCSICmd;
   My_IOStdReq->io_Length     = sizeof(struct SCSICmd);

	MySCSICmd.scsi_Data 			= (UWORD *) Data;
	MySCSICmd.scsi_Length		= VCD_SECTORSIZE;
	MySCSICmd.scsi_SenseActual = 0;	
	MySCSICmd.scsi_SenseData	= Global_SCSISense;
	MySCSICmd.scsi_SenseLength	= SENSE_LEN;
	MySCSICmd.scsi_Command		= (UBYTE *) &command;
	MySCSICmd.scsi_CmdLength	= sizeof(SCSICMD12);
	MySCSICmd.scsi_Flags			= SCSIF_READ | SCSIF_AUTOSENSE;

	DoIO( (struct IORequest *)My_IOStdReq );

   return My_IOStdReq->io_Error ? FALSE : TRUE;
}

