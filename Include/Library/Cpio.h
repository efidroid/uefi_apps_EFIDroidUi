#ifndef CPIO_H
#define CPIO_H 1

#define	CPIO_NEWC_MAGIC "070701"
#define CPIO_TRAILER "TRAILER!!!"

typedef struct
{
  CHAR8 c_magic[6];
  CHAR8 c_ino[8];
  CHAR8 c_mode[8];
  CHAR8 c_uid[8];
  CHAR8 c_gid[8];
  CHAR8 c_nlink[8];
  CHAR8 c_mtime[8];
  CHAR8 c_filesize[8];
  CHAR8 c_devmajor[8];
  CHAR8 c_devminor[8];
  CHAR8 c_rdevmajor[8];
  CHAR8 c_rdevminor[8];
  CHAR8 c_namesize[8];
  CHAR8 c_check[8];
} PACKED CPIO_NEWC_HEADER;

UINT32 CpioStrToUl (CHAR8 *in);
VOID CpioUlToStr (CHAR8 *buf, UINT32 in);
INT32 CpioIsValid (VOID *ptr);
INT32 CpioHasNext (CPIO_NEWC_HEADER * hdr);
UINTN CpioPredictObjSize (UINT32 namesize,
				   UINT32 filesize);
UINTN CpioGetObjSize (CPIO_NEWC_HEADER * hdr);
CPIO_NEWC_HEADER *CpioGetLast (CPIO_NEWC_HEADER * hdr);

CPIO_NEWC_HEADER *CpioCreateObj (CPIO_NEWC_HEADER * hdr,
				     CONST CHAR8 *name, CONST VOID *data,
				     UINTN data_size);

#endif /* ! CPIO_H */
