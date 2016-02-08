/** @file
*
*  Copyright (c) 2011-2015, ARM Limited. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/

#ifndef __EFIDROIDUI_H__
#define __EFIDROIDUI_H__

#include <PiDxe.h>
#include <Protocol/BlockIo.h>
#include <Protocol/PartitionName.h>
#include <Protocol/EfiShellParameters.h>
#include <Protocol/EfiShell.h>
#include <Protocol/DevicePathFromText.h>
#include <Protocol/DevicePathToText.h>

#include <Library/ArmLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BdsLib.h>
#include <Library/DebugLib.h>
#include <Library/HiiLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/ShellLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/GenericBdsLib.h>
#include <Library/DevicePathLib.h>
#include <Library/Cpio.h>
#include <Library/Decompress.h>
#include <Library/Ini.h>
#include <Library/PcdLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/FileHandleLib.h>
#include <Library/UEFIRamdisk.h>
#include <Library/Util.h>
#include <Library/Menu.h>
#include <Library/FileBlockIo.h>
#include <Library/MemoryBlockIo.h>
#include <Library/MakeDosFs.h>

#include <LittleKernel.h>

#include "bootimg.h"

extern EFI_GUID gEFIDroidVariableGuid;

typedef struct {
  // ini values
  CHAR8  *Name;
  CHAR8  *Description;
  CHAR16 *PartitionBoot;

  // handles
  EFI_HANDLE DeviceHandle;
  EFI_FILE_PROTOCOL* ROMDirectory;

  // set by MultibootCallback
  CHAR8* MultibootConfig;
} multiboot_handle_t;

typedef struct {
  EFI_BLOCK_IO_PROTOCOL *BlockIo;
  multiboot_handle_t    *mbhandle;
  BOOLEAN               DisablePatching;
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

EFI_STATUS
AndroidBootFromBlockIo (
  IN EFI_BLOCK_IO_PROTOCOL  *BlockIo,
  IN multiboot_handle_t     *mbhandle,
  IN BOOLEAN                DisablePatching
);

EFI_STATUS
AndroidBootFromFile (
  IN EFI_FILE_PROTOCOL  *File,
  IN multiboot_handle_t *mbhandle,
  IN BOOLEAN            DisablePatching
);

EFI_STATUS
AndroidBootFromBuffer (
  IN VOID               *Buffer,
  IN UINTN              Size,
  IN multiboot_handle_t *mbhandle,
  IN BOOLEAN            DisablePatching
);

EFI_STATUS
AndroidVerify (
  IN VOID* Buffer
);

EFI_STATUS
AndroidGetDecompRamdiskFromBlockIo (
  IN EFI_BLOCK_IO_PROTOCOL  *BlockIo,
  OUT CPIO_NEWC_HEADER      **DecompressedRamdiskOut
);

EFI_STATUS
MultibootCallback (
  IN MENU_ENTRY *This
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

#endif /* __EFIDROIDUI_H__ */
