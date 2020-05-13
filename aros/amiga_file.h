/*****************************************************/
/*****************************************************/
/*****************************************************/
/*            MorphOS/AmigaOS/AROS section           */
/*****************************************************/
/*****************************************************/
/*****************************************************/

#include "amiga_scsi.h"

struct MySCSICmd
{
		struct SCSICmd req;
		SCSICMD12 cmd;
};

static BOOL DiskPresent(struct IOStdReq *My_IOStdReq);
static BOOL read_sector (struct IOStdReq *My_IOStdReq, ULONG start_block, ULONG block_count, UBYTE *Data, struct MySCSICmd *MySCSI, BOOL sync);
static UBYTE Global_SCSISense[SENSE_LEN];

static int amiga_open  ( DVD_INPUT_STRUCT_T my_dvd_input_struct_ptr, char const * device_name)
{
    ULONG dvd_unit;
    TEXT dvd_device[64];
    BYTE My_Device=-1;
    struct MsgPort    *My_MsgPort = NULL;
    struct IOStdReq   *My_IOReq = NULL;
    UBYTE *My_Buffer = NULL;

    AMIGA_OPEN_ENTRY

    if ( ! (My_MsgPort = CreateMsgPort() ) ) goto fail;

    if ( ! (My_IOReq = (struct IOStdReq *) CreateIORequest(My_MsgPort, sizeof(struct IOStdReq) ) ) ) goto fail;

    if ( ! (My_Buffer = malloc(sizeof(struct MySCSICmd) + AMIGA_DVD_BLOCK_SIZE + 31) ) ) goto fail;
    {
	    char *temp = strchr(device_name, ':');

	    if (!temp) goto fail;
	    dvd_unit = atoi(temp+1);
	    stccpy(dvd_device, device_name, temp - device_name + 1);
    }

    if ( ( My_Device = OpenDevice(dvd_device, dvd_unit, (struct IORequest *) My_IOReq, 0L) ) ) goto fail;

    if ( !DiskPresent( My_IOReq ) ) {
	    goto fail;
    }

    my_dvd_input_struct_ptr->i_pos = 0;

    my_dvd_input_struct_ptr->DVD_IOReq    = My_IOReq;
    my_dvd_input_struct_ptr->i_fd			= (int) My_IOReq;
    my_dvd_input_struct_ptr->DVD_MsgPort  = My_MsgPort;

    my_dvd_input_struct_ptr->DVD_BufPtr = My_Buffer;
    my_dvd_input_struct_ptr->DVD_Buffer = (APTR)((((IPTR)My_Buffer) + sizeof(struct MySCSICmd) + 31) & ~31);

    read_sector(My_IOReq, 0, 1, my_dvd_input_struct_ptr->DVD_Buffer, my_dvd_input_struct_ptr->DVD_BufPtr, 0);

    AMIGA_OPEN_EXIT

    return 0;

fail:
    if (!My_Device)   CloseDevice( (struct IORequest *) My_IOReq);
    if (My_IOReq)     DeleteIORequest( (struct IORequest *) My_IOReq);
    if (My_MsgPort)   DeleteMsgPort(My_MsgPort);
    if (My_Buffer)    free(My_Buffer);

    my_dvd_input_struct_ptr->DVD_IOReq = NULL;
    my_dvd_input_struct_ptr->DVD_MsgPort = NULL;
    my_dvd_input_struct_ptr->DVD_BufPtr = NULL;
    my_dvd_input_struct_ptr->DVD_Buffer = NULL;

    return -1;
}

/*****************************************************/
static void amiga_close(DVD_INPUT_STRUCT_T my_dvd_input_struct_ptr)
{
    if(my_dvd_input_struct_ptr)
    {
        if (my_dvd_input_struct_ptr->DVD_BufPtr)
        {
	        WaitIO((struct IORequest *)my_dvd_input_struct_ptr->DVD_IOReq);
	        free(my_dvd_input_struct_ptr->DVD_BufPtr);
	        my_dvd_input_struct_ptr->DVD_BufPtr = NULL;
        }
        if (my_dvd_input_struct_ptr->DVD_IOReq)
        {
	        CloseDevice( (struct IORequest *) my_dvd_input_struct_ptr->DVD_IOReq);
	        DeleteIORequest( (struct IORequest *) my_dvd_input_struct_ptr->DVD_IOReq);
	        my_dvd_input_struct_ptr->DVD_IOReq = NULL;
        }
        if (my_dvd_input_struct_ptr->DVD_MsgPort)
        {
	        DeleteMsgPort(my_dvd_input_struct_ptr->DVD_MsgPort);
	        my_dvd_input_struct_ptr->DVD_MsgPort = NULL;
        }
    }
}
/*****************************************************/
static int amiga_seek(DVD_INPUT_STRUCT_T my_dvd_input_struct_ptr, int blocks)
{
    if (my_dvd_input_struct_ptr->i_pos != blocks)
    {
	    WaitIO((struct IORequest *)my_dvd_input_struct_ptr->DVD_IOReq);
	    my_dvd_input_struct_ptr->i_pos = blocks;
	    read_sector(my_dvd_input_struct_ptr->DVD_IOReq, blocks, 1, my_dvd_input_struct_ptr->DVD_Buffer, my_dvd_input_struct_ptr->DVD_BufPtr, 0);
    }
    return blocks;
}

/*****************************************************/
static int amiga_read  ( DVD_INPUT_STRUCT_T my_dvd_input_struct_ptr, void *p_buffer, int blocks)
{
    WaitIO((struct IORequest *)my_dvd_input_struct_ptr->DVD_IOReq);

    if (blocks == 1)
    {
        if (my_dvd_input_struct_ptr->DVD_IOReq->io_Error)
        {
	        my_dvd_input_struct_ptr->i_pos = -1;
	        my_dvd_input_struct_ptr->DVD_IOReq->io_Flags = IOF_QUICK;	/* Prevent WaitIO from doing bad things */
	        return -1;
        }

        CopyMemQuick(my_dvd_input_struct_ptr->DVD_Buffer, p_buffer, AMIGA_DVD_BLOCK_SIZE);

        my_dvd_input_struct_ptr->i_pos += 1;
    } else
    {
        if ( !read_sector(my_dvd_input_struct_ptr->DVD_IOReq, my_dvd_input_struct_ptr->i_pos, blocks, p_buffer, my_dvd_input_struct_ptr->DVD_BufPtr, 1) )
        {
	        my_dvd_input_struct_ptr->i_pos = -1;
	        return -1;
        }

        my_dvd_input_struct_ptr->i_pos += blocks;
    }

    read_sector(my_dvd_input_struct_ptr->DVD_IOReq, my_dvd_input_struct_ptr->i_pos, 1, my_dvd_input_struct_ptr->DVD_Buffer, my_dvd_input_struct_ptr->DVD_BufPtr, 0);

    return blocks;
}

/*****************************************************/
static BOOL read_sector (struct IOStdReq *My_IOStdReq, ULONG start_block, ULONG block_count, UBYTE *Data, struct MySCSICmd *MySCSI, BOOL sync)
{
    MySCSI->cmd.opcode = SCSI_CMD_READ_CD12;
    MySCSI->cmd.b1 = 0;
    MySCSI->cmd.b2 = start_block >> 24;
    MySCSI->cmd.b3 = (start_block >> 16) & 0xFF;
    MySCSI->cmd.b4 = (start_block >> 8) & 0xFF;
    MySCSI->cmd.b5 = start_block & 0xFF;

    MySCSI->cmd.b6 = block_count >> 24;
    MySCSI->cmd.b7 = (block_count >> 16) & 0xFF;
    MySCSI->cmd.b8 = (block_count >> 8) & 0xFF;
    MySCSI->cmd.b9 = block_count & 0xFF;

    MySCSI->cmd.b10 = 0;
    MySCSI->cmd.control = PAD;

    My_IOStdReq->io_Command    = HD_SCSICMD;
    My_IOStdReq->io_Data       = &MySCSI->req;
    My_IOStdReq->io_Length     = sizeof(struct SCSICmd);

    MySCSI->req.scsi_Data 			= (UWORD *) Data;
    MySCSI->req.scsi_Length		= block_count * AMIGA_DVD_BLOCK_SIZE;
    MySCSI->req.scsi_SenseActual = 0;
    MySCSI->req.scsi_SenseData	= Global_SCSISense;
    MySCSI->req.scsi_SenseLength	= SENSE_LEN;
    MySCSI->req.scsi_Command		= (UBYTE *) &MySCSI->cmd;
    MySCSI->req.scsi_CmdLength	= sizeof(SCSICMD12);
    MySCSI->req.scsi_Flags			= SCSIF_READ | SCSIF_AUTOSENSE;

    if (sync)
    {
	    DoIO((struct IORequest *)My_IOStdReq);
	    return My_IOStdReq->io_Error ? FALSE : TRUE;
    }
    else
    {
	    SendIO((struct IORequest *)My_IOStdReq);
	    return TRUE;
    }
}
/*****************************************************/
static BOOL DiskPresent(struct IOStdReq *My_IOStdReq)
{
    My_IOStdReq->io_Command    = TD_CHANGESTATE;
    My_IOStdReq->io_Flags      = 0;

    DoIO( (struct IORequest *)My_IOStdReq );

    return My_IOStdReq->io_Actual ? FALSE : TRUE;
}
