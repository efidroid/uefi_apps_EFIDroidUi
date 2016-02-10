#include <PiDxe.h>
#include <Library/DevicePathLib.h>
#include <Library/DebugLib.h>
#include <Library/Cpio.h>
#include <Library/DxeServicesLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/LoadedImage.h>

#define LIBUTIL_NOAROMA
#include <Library/Util.h>

#define RD_FILENAME L"EFIDroidRamdisk.cpio"

STATIC CPIO_NEWC_HEADER *gRamdisk = NULL;
STATIC UINTN            gRamdiskSize = 0;

STATIC
VOID
StripFileName (
  CHAR16* Path
)
{
  INT32 LastSlash = -1;
  INT32 Index;

  for(Index=0; Path[Index]; Index++) {
    if(Path[Index] == '\\')
      LastSlash = Index;
  }

  if(LastSlash>=0)
    Path[LastSlash] = 0;
}


EFI_STATUS
EFIAPI
UEFIRamdiskGetFile (
  CONST CHAR8 *Path,
  VOID  **Ptr,
  UINTN *Size
  )
{
  if (gRamdisk==NULL)
    return EFI_UNSUPPORTED;

  CPIO_NEWC_HEADER* CpioFile = CpioGetByName(gRamdisk, Path);
  if (!CpioFile)
    return EFI_NOT_FOUND;
  
  return CpioGetData(CpioFile, Ptr, Size);
}

EFI_STATUS
EFIAPI
UEFIRamdiskLibConstructor (
  VOID
)
{
  EFI_STATUS                 Status;
  EFI_LOADED_IMAGE_PROTOCOL  *LoadedImage;
  EFI_DEVICE_PATH_PROTOCOL   *DevicePathNode;
  CHAR16                     *EfiFilePath;
  CHAR16                     *RamdiskFilePath;
  EFI_FILE_HANDLE            Root;
  EFI_FILE_HANDLE            File;
  UINT64                     FileSize = 0;
  CPIO_NEWC_HEADER           *Ramdisk;
  UINTN                      RamdiskSize;

  // get ramdisk
  Status = GetSectionFromAnyFv (PcdGetPtr(PcdUEFIRamdisk), EFI_SECTION_RAW, 0, (VOID **) &Ramdisk, &RamdiskSize);
  if (!EFI_ERROR (Status)) {
    // validate header
    if(!CpioIsValid (Ramdisk))
      return EFI_UNSUPPORTED;

    gRamdisk = Ramdisk;
    gRamdiskSize = RamdiskSize;

    return EFI_SUCCESS;
  }

  // get loaded image protocol
  Status = gBS->HandleProtocol (
             gImageHandle,
             &gEfiLoadedImageProtocolGuid,
             (VOID **)&LoadedImage
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // get file path
  EfiFilePath = NULL;
  DevicePathNode = LoadedImage->FilePath;
  while (!IsDevicePathEnd (DevicePathNode)) {
    if (DevicePathType (DevicePathNode) != MEDIA_DEVICE_PATH ||
        DevicePathSubType (DevicePathNode) != MEDIA_FILEPATH_DP) {
      goto NEXT;
    }

    EfiFilePath = ((FILEPATH_DEVICE_PATH *) DevicePathNode)->PathName;
    break;

NEXT:
    DevicePathNode = NextDevicePathNode (DevicePathNode);
  }

  if (EfiFilePath==NULL) {
    return EFI_UNSUPPORTED;
  }

  // convert to dirname
  StripFileName(EfiFilePath);

  // get root file
  Root = UtilOpenRoot(LoadedImage->DeviceHandle);
  if (Root==NULL) {
    return EFI_UNSUPPORTED;
  }

  // build ramdisk path
  UINTN RamdiskFilePathSize = StrSize(EfiFilePath) + 1 + StrSize(RD_FILENAME) + 1;
  RamdiskFilePath = AllocateZeroPool(RamdiskFilePathSize);
  if (RamdiskFilePath==NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  UnicodeSPrint(RamdiskFilePath, RamdiskFilePathSize, L"%s\\%s", EfiFilePath, RD_FILENAME);

  // open ramdisk file
  Status = Root->Open (
                Root,
                &File,
                RamdiskFilePath,
                EFI_FILE_MODE_READ,
                0
                );
  FreePool(RamdiskFilePath);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // get file size
  Status = FileHandleGetSize(File, &FileSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // allocate ramdisk memory
  Ramdisk = AllocatePool(FileSize);
  if (Ramdisk==NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  // read ramdisk data
  UINTN BytesRead = FileSize;
  Status = File->Read(File, &BytesRead, Ramdisk);
  if (EFI_ERROR (Status)) {
    FreePool(Ramdisk);
    return Status;
  }

  // validate header
  if(!CpioIsValid (Ramdisk))
    return EFI_UNSUPPORTED;

  gRamdisk = Ramdisk;
  gRamdiskSize = FileSize;

  return EFI_SUCCESS;
}
