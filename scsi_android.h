typedef struct sg_io_hdr
{
  int interface_id;           /* [i] 'S' for SCSI generic (required) */
  int dxfer_direction;        /* [i] data transfer direction  */
  unsigned char cmd_len;      /* [i] SCSI command length ( <= 16 bytes) */
  unsigned char mx_sb_len;    /* [i] max length to write to sbp */
  unsigned short int iovec_count; /* [i] 0 implies no scatter gather */
  unsigned int dxfer_len;     /* [i] byte count of data transfer */
  void * dxferp;              /* [i], [*io] points to data transfer memory
                 or scatter gather list */
  unsigned char * cmdp;       /* [i], [*i] points to command to perform */
  unsigned char * sbp;        /* [i], [*o] points to sense_buffer memory */
  unsigned int timeout;       /* [i] MAX_UINT->no timeout (unit: millisec) */
  unsigned int flags;         /* [i] 0 -> default, see SG_FLAG... */
  int pack_id;                /* [i->o] unused internally (normally) */
  void * usr_ptr;             /* [i->o] unused internally */
  unsigned char status;       /* [o] scsi status */
  unsigned char masked_status;/* [o] shifted, masked scsi status */
  unsigned char msg_status;   /* [o] messaging level data (optional) */
  unsigned char sb_len_wr;    /* [o] byte count actually written to sbp */
  unsigned short int host_status; /* [o] errors from host adapter */
  unsigned short int driver_status;/* [o] errors from software driver */
  int resid;                  /* [o] dxfer_len - actual_transferred */
  unsigned int duration;      /* [o] time taken by cmd (unit: millisec) */
  unsigned int info;          /* [o] auxiliary information */
} sg_io_hdr_t;

#define SG_DXFER_TO_DEV     -2
#define SG_DXFER_FROM_DEV   -3

/* synchronous SCSI command ioctl, (only in version 3 interface) */
#define SG_IO 0x2285   /* similar effect as write() followed by read() */

/* The following 'info' values are "or"-ed together.  */
#define SG_INFO_OK_MASK	0x1
#define SG_INFO_OK      0x0	/* no sense, host nor driver "noise" */
#define SG_INFO_CHECK	0x1     /* something abnormal happened */
