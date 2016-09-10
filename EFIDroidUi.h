#ifndef __EFIDROIDUI_H__
#define __EFIDROIDUI_H__

#include <PiDxe.h>
#include <LittleKernel.h>

#include <Protocol/BlockIo.h>
#include <Protocol/RamDisk.h>
#include <Protocol/PartitionName.h>
#include <Protocol/DevicePathFromText.h>
#include <Protocol/DevicePathToText.h>
#include <Protocol/LKDisplay.h>

#include <Library/PrintLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DevicePathLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

#include <Guid/FileSystemVolumeLabelInfo.h>

#include <Library/Cpio.h>
#include <Library/Decompress.h>
#include <Library/Fstab.h>
#include <Library/MakeDosFs.h>
#include <Library/Menu.h>
#include <Library/UEFIRamdisk.h>
#include <Library/Util.h>
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
