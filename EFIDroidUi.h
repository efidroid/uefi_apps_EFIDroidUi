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

extern EFI_GUID gEFIDroidVariableGuid;
extern EFI_GUID gEFIDroidVariableDataGuid;
extern MENU_OPTION *mBootMenuMain;
extern EFI_DEVICE_PATH_TO_TEXT_PROTOCOL   *gEfiDevicePathToTextProtocol;
extern EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL *gEfiDevicePathFromTextProtocol;
extern lkapi_t *mLKApi;

typedef struct {
  // ini values
  CHAR8  *Name;
  CHAR8  *Description;
  CHAR16 *PartitionBoot;
  CHAR8  *ReplacementCmdline;

  // handles
  EFI_HANDLE DeviceHandle;
  EFI_FILE_PROTOCOL* ROMDirectory;

  // set by MultibootCallback
  CHAR8* MultibootConfig;
} multiboot_handle_t;

typedef enum {
  LAST_BOOT_TYPE_BLOCKIO = 0,
  LAST_BOOT_TYPE_FILE,
  LAST_BOOT_TYPE_MULTIBOOT,
} LAST_BOOT_TYPE;

typedef struct {
  // common
  LAST_BOOT_TYPE Type;
  CHAR8 TextDevicePath[200];

  // blockio: unused
  // file: boot image file path
  // multiboot: multiboot.ini file path
  CHAR8 FilePathName[1024];
} LAST_BOOT_ENTRY;

#define MENU_ANDROID_BOOT_ENTRY_SIGNATURE   SIGNATURE_32 ('m', 'a', 'b', 'e')

typedef struct {
  UINTN                 Signature;
  bootimg_context_t     *context;
  multiboot_handle_t    *mbhandle;
  BOOLEAN               DisablePatching;
  LAST_BOOT_ENTRY       LastBootEntry;
} MENU_ENTRY_PDATA;

#define RECOVERY_MENU_SIGNATURE             SIGNATURE_32 ('r', 'e', 'c', 'm')

typedef struct {
  UINTN           Signature;
  LIST_ENTRY      Link;

  MENU_OPTION     *SubMenu;
  MENU_ENTRY      *RootEntry;
  MENU_ENTRY      *BaseEntry;
  MENU_ENTRY      *NoPatchEntry;
} RECOVERY_MENU;

typedef struct {
  CHAR8 Name[30];
  CHAR8 IconPath[30];
  BOOLEAN IsRecovery;
} IMGINFO_CACHE;

#define STRING_LIST_SIGNATURE             SIGNATURE_32 ('s', 't', 'r', 'l')
typedef struct {
  UINTN           Signature;
  LIST_ENTRY      Link;

  CHAR16          *VariableName;
} STRING_LIST_ITEM;

EFI_STATUS
AndroidLocatorInit (
  VOID
);

EFI_STATUS
AndroidLocatorAddItems (
  VOID
);

EFI_STATUS
AndroidLocatorHandleRecoveryMode (
  LAST_BOOT_ENTRY *LastBootEntry
);

EFI_STATUS
LoaderBootFromBlockIo (
  IN EFI_BLOCK_IO_PROTOCOL  *BlockIo,
  IN multiboot_handle_t     *mbhandle,
  IN BOOLEAN                DisablePatching,
  IN LAST_BOOT_ENTRY        *LastBootEntry
);

EFI_STATUS
LoaderBootFromFile (
  IN EFI_FILE_PROTOCOL  *File,
  IN multiboot_handle_t *mbhandle,
  IN BOOLEAN            DisablePatching,
  IN LAST_BOOT_ENTRY    *LastBootEntry
);

EFI_STATUS
LoaderBootFromBuffer (
  IN VOID               *Buffer,
  IN UINTN              Size,
  IN multiboot_handle_t *mbhandle,
  IN BOOLEAN            DisablePatching,
  IN LAST_BOOT_ENTRY    *LastBootEntry
);

EFI_STATUS
LoaderBootContext (
  IN bootimg_context_t      *context,
  IN multiboot_handle_t     *mbhandle,
  IN BOOLEAN                DisablePatching,
  IN LAST_BOOT_ENTRY        *LastBootEntry
);

VOID custom_init_context(bootimg_context_t* context);
INTN libboot_identify_blockio(EFI_BLOCK_IO_PROTOCOL* BlockIo, bootimg_context_t* context);
INTN libboot_identify_file(EFI_FILE_PROTOCOL* File, bootimg_context_t* context);

EFI_STATUS
LoaderGetDecompressedRamdisk (
  IN bootimg_context_t      *context,
  OUT CPIO_NEWC_HEADER      **DecompressedRamdiskOut
);

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

#define FASTBOOT_COMMAND_MAX_LENGTH 64

VOID
FastbootInit (
  VOID
);

VOID
FastbootInfo (
  CONST CHAR8 *Reason
);

VOID
FastbootFail (
  CONST CHAR8 *Reason
);

VOID
FastbootOkay (
  CONST CHAR8 *Info
);

VOID
FastbootRegister (
  CHAR8 *Prefix,
  VOID (*Handle)(CHAR8 *Arg, VOID *Data, UINT32 Size)
);

VOID
FastbootPublish (
  CONST CHAR8 *Name,
  CONST CHAR8 *Value
);

VOID
FastbootCommandsAdd (
  VOID
);

VOID
FastbootRequestStop (
  VOID
);

VOID
FastbootStopNow (
  VOID
);

VOID
FastbootSendString (
  IN CONST CHAR8 *Data,
  IN UINTN Size
);

VOID
FastbootSendBuf (
  IN CONST VOID *Data,
  IN UINTN Size
);

#endif /* __EFIDROIDUI_H__ */
