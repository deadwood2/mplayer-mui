/*
 *	scsi_stuff.h
 */

#include <exec/types.h>

#define BYTES_PER_LINE	16
#define SENSE_LEN 252
#define MAX_DATA_LEN 252
#define MAX_TOC_LEN 804		/* max TOC size = 100 TOC track descriptors */
#define PAD 0
#define	LINE_BUF	(128)
#define NUM_OF_CDDAFRAMES 75	/* 75 frames per second audio */
/*#define CDDALEN 2352	*/	/* 1 frame has 2352 bytes */
#define CDDALEN 2448		/* 1 frame has max. 2448 bytes (subcode 2) */
#define MAX_CDDALEN CDDALEN * NUM_OF_CDDAFRAMES
#define MAX_TRACK 100

#define VCD_SECTORSIZE 2352

#define OFFS_KEY 2
#define OFFS_CODE 12

#define NDBLBUF 8

struct ModeSel_Head
{
	UBYTE	reserved1;
	UBYTE	medium;
	UBYTE	reserved2;
	UBYTE	block_desc_length;
	UBYTE	density;
	UBYTE	number_of_blocks_hi;
	UBYTE	number_of_blocks_med;
	UBYTE	number_of_blocks_lo;
	UBYTE	reserved3;
	UBYTE	block_length_hi;
	UBYTE	block_length_med;
	UBYTE	block_length_lo;
};

/* type used for a 6 byte SCSI command */
typedef struct
 {
   UBYTE  opcode;
   UBYTE  b1;
   UBYTE  b2;
   UBYTE  b3;
   UBYTE  b4;
   UBYTE  control;
 } SCSICMD6;

/* type used for a 10 byte SCSI command */
typedef struct
 {
   UBYTE  opcode;
   UBYTE  b1;
   UBYTE  b2;
   UBYTE  b3;
   UBYTE  b4;
   UBYTE  b5;
   UBYTE  b6;
   UBYTE  b7;
   UBYTE  b8;
   UBYTE  control;
 } SCSICMD10;

/* type used for a 12 byte SCSI command */
typedef struct
 {
   UBYTE  opcode;
   UBYTE  b1;
   UBYTE  b2;
   UBYTE  b3;
   UBYTE  b4;
   UBYTE  b5;
   UBYTE  b6;
   UBYTE  b7;
   UBYTE  b8;
   UBYTE  b9;
   UBYTE  b10;
   UBYTE  control;
 } SCSICMD12;

/* SCSI commands */
 

#define SCSI_CMD_READ_CD12        0xa8
#define SCSI_CMD_READ_CD10        0x28
#define SCSI_CMD_READ_CDMSF        0xb9

#define SCSI_CMD_READ_CD        0xBE
#define SCSI_CMD_READCDDA       0xD8 

#define SCSI_CMD_SET_CD_SPEED 	0xbb

#define	SCSI_CMD_TUR	0x00	/* Test Unit Ready	*/
#define	SCSI_CMD_RZU	0x01	/* Rezero Unit		*/
#define	SCSI_CMD_RQS	0x03	/* Request Sense	*/
#define	SCSI_CMD_FMU	0x04	/* Format unit		*/
#define	SCSI_CMD_RAB	0x07	/* Reassign Block	*/
#define	SCSI_CMD_RD	0x08	/* Read			*/
#define	SCSI_CMD_WR	0x0A	/* Write		*/
#define	SCSI_CMD_SK	0x0B	/* Seek			*/
#define	SCSI_CMD_INQ	0x12	/*  6B: Inquiry		*/
#define	SCSI_CMD_MSL	0x15	/* Mode Select		*/
#define	SCSI_CMD_RU	0x16	/* Reserve Unit		*/
#define	SCSI_CMD_RLU	0x17	/* Release Unit		*/
#define	SCSI_CMD_MSE	0x1A	/*  6B: Mode Sense	*/
#define	SCSI_CMD_SSU	0x1B	/*  6B: Start/Stop Unit	*/
#define	SCSI_CMD_RDI	0x1C	/* Receive Diagnostic	*/
#define	SCSI_CMD_SDI	0x1D	/* Send Diagnostic	*/
#define SCSI_CMD_PAMR	0x1E	/*  6B: Prevent Allow Medium Removal */
#define	SCSI_CMD_RCP	0x25	/* Read Capacity	*/
#define	SCSI_CMD_RXT	0x28	/* Read Extended	*/
#define	SCSI_CMD_WXT	0x2A	/* Write Extended	*/
#define	SCSI_CMD_SKX	0x2B	/* Seek Extended	*/
#define	SCSI_CMD_WVF	0x2E	/* Write & Verify	*/
#define	SCSI_CMD_VF	0x2F	/* Verify		*/
#define	SCSI_CMD_RDD	0x37	/* Read Defect Data	*/
#define	SCSI_CMD_WDB	0x3B	/* Write Data Buffer	*/
#define	SCSI_CMD_RDB	0x3C	/* Read Data Buffer	*/

#define SCSI_CMD_COPY		0x18	/*  6B: Copy */
#define SCSI_CMD_COMPARE	0x39	/* 10B: Compare */
#define SCSI_CMD_COPYANDVERIFY	0x3A	/* 10B: Copy and Verify */
#define SCSI_CMD_CHGEDEF	0x40	/* 10B: Change Definition */
#define SCSI_CMD_READSUBCHANNEL	0x42	/* 10B: Read Sub-Channel */
#define SCSI_CMD_READTOC	0x43	/* Read TOC from CD Audio */
#define SCSI_CMD_READHEADER	0x44	/* 10B: Read data block address header */
#define SCSI_CMD_PLAYAUDIO10	0x45	/* Play CD Audio */
#define SCSI_CMD_PLAYAUDIOTRACKINDEX	0x48	/* Play CD Audio Track */
