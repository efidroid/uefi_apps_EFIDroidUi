#include "EFIDroidUi.h"
#include <Protocol/DevicePath.h>
#include <Protocol/RamDisk.h>
#include <IndustryStandard/PeImage.h>
#include <Library/DevicePathLib.h>

STATIC VOID
CommandReboot (
  CHAR8 *Arg,
  VOID *Data,
  UINT32 Size
)
{
  FastbootOkay("");
  gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
}

STATIC
BOOLEAN
IsEfiFile (
  VOID* Buffer
)
{
  EFI_IMAGE_DOS_HEADER                *DosHdr;
  EFI_IMAGE_OPTIONAL_HEADER_PTR_UNION Hdr;

  DosHdr = (EFI_IMAGE_DOS_HEADER *)Buffer;

  if (DosHdr->e_magic == EFI_IMAGE_DOS_SIGNATURE) {
    //
    // Valid DOS header so get address of PE header
    //
    Hdr.Pe32 = (EFI_IMAGE_NT_HEADERS32 *)(((CHAR8 *)DosHdr) + DosHdr->e_lfanew);
  } else {
    //
    // No Dos header so assume image starts with PE header.
    //
    Hdr.Pe32 = (EFI_IMAGE_NT_HEADERS32 *)Buffer;
  }

  if (Hdr.Pe32->Signature != EFI_IMAGE_NT_SIGNATURE) {
    //
    // Not a valid PE image so Exit
    //
    return FALSE;
  }

  return TRUE;
}

STATIC VOID
CommandBoot (
  CHAR8 *Arg,
  VOID *Data,
  UINT32 Size
)
{
  EFI_RAM_DISK_PROTOCOL     *RamDiskProtocol;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  boot_img_hdr_t            *AndroidHdr;
  UINT64                    DataAddr;
  EFI_STATUS                Status;
  EFI_GUID                  DiskGuid = gEfiVirtualDiskGuid;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL   *Volume;
  EFI_FILE_PROTOCOL                 *Root;
  EFI_FILE_PROTOCOL                 *EfiFile;

  AndroidHdr = Data;
  DataAddr = (UINT64)(UINTN)Data;
  DevicePath = NULL;

  // verify Android header
  Status = AndroidVerify(AndroidHdr);
  if (EFI_ERROR(Status)) {
    FastbootFail("Not a boot image");
    return;
  }

  // calculate offsets
  UINTN KernelOffset  = AndroidHdr->page_size;

  // EFI file without a ramdisk
  if(IsEfiFile((VOID*)(UINTN)(DataAddr+KernelOffset))) {
    // get ramdisk protocol
    Status = gBS->LocateProtocol (&gEfiRamDiskProtocolGuid, NULL, (VOID **) &RamDiskProtocol);
    if (EFI_ERROR (Status)) {
      FastbootFail("Can't locate ramdisk protocol");
      return;
    }

    // Allocate ramdisk
    UINT64 RamDiskSize = MAX(AndroidHdr->kernel_size + SIZE_1MB, SIZE_1MB);
    VOID* RamDisk = AllocatePool(RamDiskSize);
    if (RamDisk==NULL) {
      FastbootFail("Can't allocate ramdisk memory");
      return;
    }

    // mkfs.vfat
    Status = MakeDosFs(RamDisk, RamDiskSize);
    if (EFI_ERROR (Status)) {
      FastbootFail("Can't format ramdisk to FAT");
      goto ERROR_FREE_RAMDISK;
    }

    // register ramdisk
    Status = RamDiskProtocol->Register((UINTN)RamDisk, RamDiskSize, &DiskGuid, NULL, &DevicePath);
    if (EFI_ERROR (Status)) {
      FastbootFail("Can't register ramdisk");
      goto ERROR_FREE_RAMDISK;
    }

    // get handle
    EFI_DEVICE_PATH_PROTOCOL* Protocol = DevicePath;
    EFI_HANDLE FSHandle;
    Status = gBS->LocateDevicePath (
                    &gEfiSimpleFileSystemProtocolGuid,
                    &Protocol,
                    &FSHandle
                    );
    if (EFI_ERROR (Status)) {
      FastbootFail("Can't get FS handle");
      goto ERROR_UNREGISTER_RAMDISK;
    }

    // get the SimpleFilesystem protocol on that handle
    Volume = NULL;
    Status = gBS->HandleProtocol (
                    FSHandle,
                    &gEfiSimpleFileSystemProtocolGuid,
                    (VOID **)&Volume
                    );
    if (EFI_ERROR (Status)) {
      FastbootFail("Can't get FS protocol");
      goto ERROR_UNREGISTER_RAMDISK;
    }

    // Open the root directory of the volume
    Root = NULL;
    Status = Volume->OpenVolume (
                       Volume,
                       &Root
                       );
    if (EFI_ERROR (Status) || Root==NULL) {
      FastbootFail("Can't open volume");
      goto ERROR_UNREGISTER_RAMDISK;
    }

    // Create EFI file
    EfiFile = NULL;
    Status = Root->Open (
                     Root,
                     &EfiFile,
                     L"Sideload.efi",
                     EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,
                     0
                     );
    if (EFI_ERROR (Status)) {
      FastbootFail("Can't create file");
      goto ERROR_UNREGISTER_RAMDISK;
    }

    // write kernel to efi file
    UINTN WriteSize = AndroidHdr->kernel_size;
    Status = EfiFile->Write(EfiFile, &WriteSize, (VOID*)(UINTN)(DataAddr+KernelOffset));
    if (EFI_ERROR (Status)) {
      FastbootFail("Can't write to file");
      goto ERROR_UNREGISTER_RAMDISK;
    }

    // build device path
    EFI_DEVICE_PATH_PROTOCOL *LoaderDevicePath;
    LoaderDevicePath = FileDevicePath(FSHandle, L"Sideload.efi");
    if (LoaderDevicePath==NULL) {
      FastbootFail("Can't build file device path");
      goto ERROR_UNREGISTER_RAMDISK;
    }

    // Need to connect every drivers to ensure no dependencies are missing for the application
    Status = BdsConnectAllDrivers ();
    if (EFI_ERROR (Status)) {
      FastbootFail("failed to connect all drivers");
      goto ERROR_UNREGISTER_RAMDISK;
    }

    // build arguments
    CONST CHAR16* Args = L"";
    UINTN LoadOptionsSize = (UINT32)StrSize (Args);
    VOID *LoadOptions     = AllocatePool (LoadOptionsSize);
    StrCpy (LoadOptions, Args);

    // send OKAY
    FastbootOkay("");

    // shut down menu
    MenuPreBoot();

    // start efi application
    Status = BdsStartEfiApplication (gImageHandle, LoaderDevicePath, LoadOptionsSize, LoadOptions);

    // restart menu
    MenuPostBoot();

ERROR_UNREGISTER_RAMDISK:
    // unregister ramdisk
    Status = RamDiskProtocol->Unregister(DevicePath);
    if (EFI_ERROR (Status)) {
      ASSERT(FALSE);
    }

ERROR_FREE_RAMDISK:
    // free ramdisk memory
    FreePool(RamDisk);
  }

  // Android
  else {
    // send OKAY
    FastbootOkay("");

    // boot Android
    AndroidBootFromBuffer(Data, Size, NULL, TRUE);
  }
}

VOID
FastbootCommandsAdd (
  VOID
)
{
  FastbootRegister("reboot", CommandReboot);
  FastbootRegister("boot", CommandBoot);
}
