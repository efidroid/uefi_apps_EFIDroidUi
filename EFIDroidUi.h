#ifndef __EFIDROIDUI_H__
#define __EFIDROIDUI_H__

#include <PiDxe.h>
#include <Protocol/FirmwareVolume2.h>
#include <Protocol/BlockIo.h>
#include <Protocol/PartitionName.h>
#include <Protocol/DevicePathFromText.h>
#include <Protocol/DevicePathToText.h>
#include <Protocol/RamDisk.h>
#include <Guid/FileInfo.h>

#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/DevicePathLib.h>
#include <Library/Cpio.h>
#include <Library/Decompress.h>
#include <Library/PcdLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/FileHandleLib.h>
#include <Library/UEFIRamdisk.h>
#include <Library/Util.h>
#include <Library/Menu.h>
#include <Library/FileBlockIo.h>
#include <Library/MemoryBlockIo.h>
#include <Library/MakeDosFs.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/Fstab.h>

#if defined (MDE_CPU_ARM)
#include <Library/ArmLib.h>
#endif

#include <LittleKernel.h>

#include <lib/boot.h>

#include <Internal/Fastboot.h>
#include <Internal/Loader.h>
#include <Internal/AndroidLocator.h>

extern EFI_GUID gEFIDroidVariableGuid;
extern EFI_GUID gEFIDroidVariableDataGuid;
extern MENU_OPTION *mBootMenuMain;
extern EFI_DEVICE_PATH_TO_TEXT_PROTOCOL   *gEfiDevicePathToTextProtocol;
extern EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL *gEfiDevicePathFromTextProtocol;
extern lkapi_t *mLKApi;

#define STRING_LIST_SIGNATURE             SIGNATURE_32 ('s', 't', 'r', 'l')
typedef struct {
  UINTN           Signature;
  LIST_ENTRY      Link;

  CHAR16          *VariableName;
} STRING_LIST_ITEM;

EFI_STATUS
MainMenuUpdateUi (
  VOID
);

EFI_STATUS
FileExplorerCallback (
  IN MENU_ENTRY* This
);

VOID
FileExplorerUpdate (
  IN MENU_ENTRY* This
);

EFI_STATUS
SettingsMenuShow (
  VOID
);

#endif /* __EFIDROIDUI_H__ */
