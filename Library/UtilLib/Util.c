#include <Library/Util.h>
#include <Library/UEFIRamdisk.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>

CHAR8*
Unicode2Ascii (
  CONST CHAR16* UnicodeStr
)
{
  CHAR8* AsciiStr = AllocatePool((StrLen (UnicodeStr) + 1) * sizeof (CHAR8));
  if (AsciiStr == NULL) {
    return NULL;
  }

  UnicodeStrToAsciiStr(UnicodeStr, AsciiStr);

  return AsciiStr;
}

CHAR16*
Ascii2Unicode (
  CONST CHAR8* AsciiStr
)
{
  CHAR16* UnicodeStr = AllocatePool((AsciiStrLen (AsciiStr) + 1) * sizeof (CHAR16));
  if (UnicodeStr == NULL) {
    return NULL;
  }

  AsciiStrToUnicodeStr(AsciiStr, UnicodeStr);

  return UnicodeStr;
}

CHAR8*
AsciiStrDup (
  CONST CHAR8* SrcStr
)
{
  UINTN Len = (AsciiStrLen (SrcStr) + 1) * sizeof (CHAR8);
  CHAR8* NewStr = AllocatePool(Len);
  if (NewStr == NULL) {
    return NULL;
  }

  CopyMem(NewStr, SrcStr, Len);

  return NewStr;
}

VOID
PathToUnix(
  CHAR16* fname
)
{
  CHAR16 *Tmp = fname;
  for(Tmp = fname; *Tmp != 0; Tmp++) {
    if(*Tmp=='\\')
      *Tmp = '/';
  }
}

VOID
PathToUefi(
  CHAR16* fname
)
{
  CHAR16 *Tmp = fname;
  for(Tmp = fname; *Tmp != 0; Tmp++) {
    if(*Tmp=='/')
      *Tmp = '\\';
  }
}

BOOLEAN
NodeIsDir (
  IN EFI_FILE_INFO      *NodeInfo
  )
{
  return ((NodeInfo->Attribute & EFI_FILE_DIRECTORY) == EFI_FILE_DIRECTORY);
}

LIBAROMA_STREAMP
libaroma_stream_ramdisk(
  CONST CHAR8* Path
)
{
  bytep Data;
  UINTN Size;
  EFI_STATUS Status;

  // get font data
  Status = UEFIRamdiskGetFile (Path, (VOID **) &Data, &Size);
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  return libaroma_stream_mem(Data, Size);
}

UINT32
RangeOverlaps (
  UINT32 x1,
  UINT32 x2,
  UINT32 y1,
  UINT32 y2
)
{
  return MAX(x1,y1) <= MIN(x2,y2);
}

UINT32
AlignMemoryRange (
  IN UINT32 Addr,
  IN OUT UINTN *Size,
  OUT UINTN  *AddrOffset
)
{
  // align range
  UINT32 AddrAligned = ROUNDDOWN(Addr, EFI_PAGE_SIZE);

  // calculate offset
  UINTN Offset = Addr - AddrAligned;
  if (AddrOffset!=NULL)
    *AddrOffset = Offset;

  // round and return size
  *Size = ROUNDUP(Offset + (*Size), EFI_PAGE_SIZE);

  return AddrAligned;
}

EFI_STATUS
FreeAlignedMemoryRange (
  IN UINT32 Address,
  IN OUT UINTN Size
)
{
  UINTN      AlignedSize = Size;
  UINTN      AddrOffset = 0;

  EFI_PHYSICAL_ADDRESS AllocationAddress = AlignMemoryRange(Address, &AlignedSize, &AddrOffset);

  return gBS->FreePages(AllocationAddress, EFI_SIZE_TO_PAGES(AlignedSize));
}
