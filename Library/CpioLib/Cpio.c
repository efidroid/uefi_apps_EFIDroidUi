#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Library/Cpio.h>

STATIC CONST CHAR8 *cpio_mode_executable = "000081e8";

UINT32
CpioStrToUl (
  CHAR8 *in
)
{
  CHAR8 buf[9];
  CopyMem (buf, in, 8);
  buf[8] = 0;

  return AsciiStrHexToUintn (buf);
}

VOID
CpioUlToStr (
  CHAR8  *buf,
  UINT32 in
)
{
  AsciiSPrint (buf, 9, "%08X", in);
}

INT32
CpioIsValid (
  VOID *ptr
)
{
  return !AsciiStrnCmp (ptr, CPIO_NEWC_MAGIC, 6);
}

INT32
CpioHasNext (
  CPIO_NEWC_HEADER *hdr
)
{
  return !!AsciiStrCmp ((CONST CHAR8 *) (hdr + 1), CPIO_TRAILER);
}

UINTN
CpioPredictObjSize (
  UINT32 namesize,
  UINT32 filesize
)
{
  return ALIGN_VALUE (sizeof (CPIO_NEWC_HEADER) + namesize, 4) + ALIGN_VALUE (filesize, 4);
}

UINTN
CpioGetObjSize (
  CPIO_NEWC_HEADER *hdr
)
{
  UINT32 namesize = CpioStrToUl (hdr->c_namesize);
  UINT32 filesize = CpioStrToUl (hdr->c_filesize);

  return CpioPredictObjSize (namesize, filesize);
}

CPIO_NEWC_HEADER*
CpioGetLast (
  CPIO_NEWC_HEADER *hdr
)
{
  while (CpioIsValid (hdr) && CpioHasNext (hdr)) {
    hdr = (CPIO_NEWC_HEADER *) (((CHAR8 *) hdr) + CpioGetObjSize (hdr));
  }

  return hdr;
}

CPIO_NEWC_HEADER*
CpioGetByName (
  CPIO_NEWC_HEADER *hdr,
  CONST CHAR8        *name
)
{
  while (CpioIsValid (hdr) && CpioHasNext (hdr)) {
    hdr = (CPIO_NEWC_HEADER *) (((CHAR8 *) hdr) + CpioGetObjSize (hdr));
    CONST CHAR8  *nameptr = (CONST CHAR8 *) (hdr + 1);
    if (!AsciiStrCmp(nameptr, name))
      return hdr;
  }

  return NULL;
}

CPIO_NEWC_HEADER*
CpioCreateObj (
  CPIO_NEWC_HEADER   *hdr,
  CONST CHAR8        *name,
  CONST VOID         *data,
  UINTN              data_size
)
{
  UINT32 namesize = AsciiStrLen (name) + 1;
  UINT32 namepad = ALIGN_VALUE (sizeof (*hdr) + namesize, 4) - (sizeof (*hdr) + namesize);
  CHAR8  *nameptr = (CHAR8 *) (hdr + 1);
  CHAR8  *dataptr = (CHAR8 *) (nameptr + namesize + namepad);
  CHAR8  intbuf[9];

  // clear
  SetMem (hdr, sizeof (*hdr), '0');

  // magic
  CopyMem (hdr->c_magic, CPIO_NEWC_MAGIC, 8);

  // namesize
  CpioUlToStr (intbuf, namesize);
  CopyMem (hdr->c_namesize, intbuf, 8);
  // name
  CopyMem (nameptr, name, namesize);

  // filesize
  CpioUlToStr (intbuf, data_size);
  CopyMem (hdr->c_filesize, intbuf, 8);
  // data
  if (data) {
    CopyMem (dataptr, data, data_size);
  }

  // mode: -rwxr-x---
  CopyMem (hdr->c_mode, cpio_mode_executable, AsciiStrLen (cpio_mode_executable));

  return (CPIO_NEWC_HEADER *) (dataptr + ALIGN_VALUE (data_size, 4));
}
