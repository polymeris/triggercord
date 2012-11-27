/*
    pkTriggerCord
    Copyright (C) 2011-2012 Andras Salamon <andras.salamon@melda.info>
    Remote control of Pentax DSLR cameras.

    based on:

    PK-Remote
    Remote control of Pentax DSLR cameras.
    Copyright (C) 2008 Pontus Lidman <pontus@lysator.liu.se>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdint.h>
#include <sys/ioctl.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>

#include "pslr_scsi.h"

#ifndef ANDROID
    #include <scsi/sg.h>
#else
    #include <android/log.h>
    #define DPRINT(...)	\
        __android_log_print(ANDROID_LOG_DEBUG, "Pentax", __VA_ARGS__)

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

#endif /* ANDROID */

void print_scsi_error(sg_io_hdr_t *pIo, uint8_t *sense_buffer) {
    int k;

    if (pIo->sb_len_wr > 0) {
        DPRINT("SCSI error: sense data: ");
        for (k = 0; k < pIo->sb_len_wr; ++k) {
            if ((k > 0) && (0 == (k % 10)))
                DPRINT("\n  ");
            DPRINT("0x%02x ", sense_buffer[k]);
        }
        DPRINT("\n");
    }
    if (pIo->masked_status)
        DPRINT("SCSI status=0x%x\n", pIo->status);
    if (pIo->host_status)
        DPRINT("host_status=0x%x\n", pIo->host_status);
    if (pIo->driver_status)
        DPRINT("driver_status=0x%x\n", pIo->driver_status);
}

char **get_drives(int *driveNum) {
    DIR *d;
    struct dirent *ent;
    char *tmp[64];
    char **ret;
    int j,jj;
    d = opendir("/sys/class/scsi_generic");

    if (!d) {
        DPRINT("Cannot open /sys/class/scsi_generic\n");
	d = opendir("/sys/block");
	if( !d ) {
	    *driveNum = 0;
            return NULL;
	}
    }
    j=0;
    while( (ent = readdir(d)) ) {
        if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
	    tmp[j] = malloc( sizeof(ent->d_name) );
	    strncpy(tmp[j], ent->d_name, sizeof(ent->d_name));
	    ++j;
	}
    }
    closedir(d);
    ret = malloc( j * sizeof(char*) );
    for( jj=0; jj<j; ++jj ) {
	ret[jj] = malloc( sizeof(tmp[jj]) );
	strncpy( ret[jj], tmp[jj], sizeof(tmp[jj]) );
    }
    *driveNum = j;
    return ret;
}

pslr_result get_drive_info(char* driveName, char* vendorId, int vendorIdSizeMax,
			   char* productId, int productIdSizeMax) {
    char nmbuf[256];
    int fd;

    vendorId[0] = '\0';
    productId[0] = '\0';
    snprintf(nmbuf, sizeof (nmbuf), "/sys/class/scsi_generic/%s/device/vendor", driveName);
    fd = open(nmbuf, O_RDONLY);
    if (fd == -1) {
      snprintf(nmbuf, sizeof (nmbuf), "/sys/block/%s/device/vendor", driveName);
      fd = open(nmbuf, O_RDONLY);
      if (fd == -1) {
        return PSLR_DEVICE_ERROR;
      }
    }
    int v_length = read(fd, vendorId, vendorIdSizeMax-1);
    vendorId[v_length]='\0';
    close(fd);

    snprintf(nmbuf, sizeof (nmbuf), "/sys/class/scsi_generic/%s/device/model", driveName);
    fd = open(nmbuf, O_RDONLY);
    if (fd == -1) {       
      snprintf(nmbuf, sizeof (nmbuf), "/sys/block/%s/device/model", driveName);
      fd = open(nmbuf, O_RDONLY);
      if (fd == -1) {
           return PSLR_DEVICE_ERROR;
      }
    }
    int p_length = read(fd, productId, productIdSizeMax-1);
    productId[p_length]='\0';
    close(fd);
    return PSLR_OK;
}

pslr_result open_drive(int* hDevice, char * driveName)
{
    char nmbuf[256];
    char sudocmd[256];
    snprintf(nmbuf, sizeof (nmbuf), "/dev/%s", driveName);
    
    *hDevice = open(nmbuf, O_RDWR);
    if( *hDevice == -1) {
        #ifdef ANDROID
            snprintf(sudocmd, sizeof (sudocmd), "su -c \"chmod 666 %s\"", nmbuf);
            DPRINT("Need root! Executing %s", sudocmd);
            system(&sudocmd);
            *hDevice = open(nmbuf, O_RDWR);
        #endif
        
        if( *hDevice == -1)
            return PSLR_DEVICE_ERROR;
    }
    return PSLR_OK;
}

void close_drive(int *hDevice) {
    close( *hDevice );
}

int scsi_read(int sg_fd, uint8_t *cmd, uint32_t cmdLen,
        uint8_t *buf, uint32_t bufLen) {
    sg_io_hdr_t io;
    uint8_t sense[32];
    int r;

    memset(&io, 0, sizeof (io));

    io.interface_id = 'S';
    io.cmd_len = cmdLen;
    /* io.iovec_count = 0; */ /* memset takes care of this */
    io.mx_sb_len = sizeof (sense);
    io.dxfer_direction = SG_DXFER_FROM_DEV;
    io.dxfer_len = bufLen;
    io.dxferp = buf;
    io.cmdp = cmd;
    io.sbp = sense;
    io.timeout = 20000; /* 20000 millisecs == 20 seconds */
    /* io.flags = 0; */ /* take defaults: indirect IO, etc */
    /* io.pack_id = 0; */
    /* io.usr_ptr = NULL; */

    r = ioctl(sg_fd, SG_IO, &io);
    if (r == -1) {
        perror("ioctl");
        return -PSLR_DEVICE_ERROR;
    }

    if ((io.info & SG_INFO_OK_MASK) != SG_INFO_OK) {
        print_scsi_error(&io, sense);
        return -PSLR_SCSI_ERROR;
    } else {
        /* Older Pentax DSLR will report all bytes remaining, so make
         * a special case for this (treat it as all bytes read). */
        if (io.resid == bufLen)
            return bufLen;
        else
            return bufLen - io.resid;
    }
}

int scsi_write(int sg_fd, uint8_t *cmd, uint32_t cmdLen,
        uint8_t *buf, uint32_t bufLen) {

    sg_io_hdr_t io;
    uint8_t sense[32];
    int r;

    memset(&io, 0, sizeof (io));

    io.interface_id = 'S';
    io.cmd_len = cmdLen;
    /* io.iovec_count = 0; */ /* memset takes care of this */
    io.mx_sb_len = sizeof (sense);
    io.dxfer_direction = SG_DXFER_TO_DEV;
    io.dxfer_len = bufLen;
    io.dxferp = buf;
    io.cmdp = cmd;
    io.sbp = sense;
    io.timeout = 20000; /* 20000 millisecs == 20 seconds */
    /* io.flags = 0; */ /* take defaults: indirect IO, etc */
    /* io.pack_id = 0; */
    /* io.usr_ptr = NULL; */

    r = ioctl(sg_fd, SG_IO, &io);

    if (r == -1) {
        perror("ioctl");
        return PSLR_DEVICE_ERROR;
    }

    if ((io.info & SG_INFO_OK_MASK) != SG_INFO_OK) {
        print_scsi_error(&io, sense);
        return PSLR_SCSI_ERROR;
    } else {
        return PSLR_OK;
    }
}
