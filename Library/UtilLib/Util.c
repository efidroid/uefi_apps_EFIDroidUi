#include <Library/Util.h>
#include <Library/UEFIRamdisk.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>

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
  IN CONST CHAR8* Str
  )
{
  return AllocateCopyPool (AsciiStrSize (Str), Str);
}

CHAR16*
UnicodeStrDup (
  IN CONST CHAR16* Str
  )
{
  return AllocateCopyPool (StrSize (Str), Str);
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
RangeLenOverlaps (
  UINT32 x,
  UINT32 xl,
  UINT32 y,
  UINT32 yl
)
{
  return RangeOverlaps(x, x+xl, y, y+yl);
}

UINT32
AlignMemoryRange (
  IN UINT32 Addr,
  IN OUT UINTN *Size,
  OUT UINTN  *AddrOffset,
  IN UINTN Alignment
)
{
  // align range
  UINT32 AddrAligned = ROUNDDOWN(Addr, Alignment);

  // calculate offset
  UINTN Offset = Addr - AddrAligned;
  if (AddrOffset!=NULL)
    *AddrOffset = Offset;

  // round and return size
  *Size = ROUNDUP(Offset + (*Size), Alignment);

  return AddrAligned;
}

EFI_STATUS
FreeAlignedMemoryRange (
  IN UINT32 Address,
  IN OUT UINTN Size,
  IN UINTN Alignment
)
{
  UINTN      AlignedSize = Size;
  UINTN      AddrOffset = 0;

  EFI_PHYSICAL_ADDRESS AllocationAddress = AlignMemoryRange(Address, &AlignedSize, &AddrOffset, Alignment);

  return gBS->FreePages(AllocationAddress, EFI_SIZE_TO_PAGES(AlignedSize));
}

EFI_STATUS
VisitAllInstancesOfProtocol (
  IN EFI_GUID                    *Id,
  IN PROTOCOL_INSTANCE_CALLBACK  CallBackFunction,
  IN VOID                        *Context
  )
{
  EFI_STATUS                Status;
  UINTN                     HandleCount;
  EFI_HANDLE                *HandleBuffer;
  UINTN                     Index;
  VOID                      *Instance;

  //
  // Start to check all the PciIo to find all possible device
  //
  HandleCount = 0;
  HandleBuffer = NULL;
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  Id,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (HandleBuffer[Index], Id, &Instance);
    if (EFI_ERROR (Status)) {
      continue;
    }

    Status = (*CallBackFunction) (
               HandleBuffer[Index],
               Instance,
               Context
               );
  }

  gBS->FreePool (HandleBuffer);

  return EFI_SUCCESS;
}

/**

  Function opens and returns a file handle to the root directory of a volume.

  @param DeviceHandle    A handle for a device

  @return A valid file handle or NULL is returned

**/
EFI_FILE_HANDLE
UtilOpenRoot (
  IN EFI_HANDLE                   DeviceHandle
  )
{
  EFI_STATUS                      Status;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Volume;
  EFI_FILE_HANDLE                 File;

  File = NULL;

  //
  // File the file system interface to the device
  //
  Status = gBS->HandleProtocol (
                  DeviceHandle,
                  &gEfiSimpleFileSystemProtocolGuid,
                  (VOID *) &Volume
                  );

  //
  // Open the root directory of the volume
  //
  if (!EFI_ERROR (Status)) {
    Status = Volume->OpenVolume (
                      Volume,
                      &File
                      );
  }
  //
  // Done
  //
  return EFI_ERROR (Status) ? NULL : File;
}

/**

  Function gets the file information from an open file descriptor, and stores it
  in a buffer allocated from pool.

  @param FHand           File Handle.
  @param InfoType        Info type need to get.

  @retval                A pointer to a buffer with file information or NULL is returned

**/
VOID *
UtilFileInfo (
  IN EFI_FILE_HANDLE      FHand,
  IN EFI_GUID             *InfoType
  )
{
  EFI_STATUS    Status;
  EFI_FILE_INFO *Buffer;
  UINTN         BufferSize;

  Buffer      = NULL;
  BufferSize  = 0;

  Status = FHand->GetInfo (
                    FHand,
                    InfoType,
                    &BufferSize,
                    Buffer
                    );
  if (Status == EFI_BUFFER_TOO_SMALL) {
    Buffer = AllocatePool (BufferSize);
    ASSERT (Buffer != NULL);
  }

  Status = FHand->GetInfo (
                    FHand,
                    InfoType,
                    &BufferSize,
                    Buffer
                    );

  return Buffer;
}

/**

  Get file type base on the file name.
  Just cut the file name, from the ".". eg ".efi"

  @param FileName  File need to be checked.

  @retval the file type string.

**/
CHAR16*
UtilGetTypeFromName (
  IN CHAR16   *FileName
  )
{
  UINTN    Index;

  Index = StrLen (FileName) - 1;
  while ((FileName[Index] != L'.') && (Index != 0)) {
    Index--;
  }

  Index++;

  return Index == 0 ? NULL : &FileName[Index];
}

/**
  Converts the unicode character of the string from uppercase to lowercase.
  This is a internal function.

  @param ConfigString  String to be converted

**/
VOID
UtilToLowerString (
  IN CHAR16  *String
  )
{
  CHAR16      *TmpStr;

  for (TmpStr = String; *TmpStr != L'\0'; TmpStr++) {
    if (*TmpStr >= L'A' && *TmpStr <= L'Z') {
      *TmpStr = (CHAR16) (*TmpStr - L'A' + L'a');
    }
  }
}

/**

  Check whether current FileName point to a valid
  Efi Image File.

  @param FileName  File need to be checked.

  @retval TRUE  Is Efi Image
  @retval FALSE Not a valid Efi Image

**/
CHAR16*
UtilGetExtensionLower (
  IN UINT16  *FileName
  )
{
  CHAR16     *InputFileType;
  CHAR16     *TmpStr;

  InputFileType = UtilGetTypeFromName (FileName);
  //
  // If the file not has *.* style, always return TRUE.
  //
  if (InputFileType == NULL) {
    return NULL;
  }

  TmpStr = AllocateCopyPool (StrSize (InputFileType), InputFileType);
  UtilToLowerString(TmpStr);

  return TmpStr;
}
