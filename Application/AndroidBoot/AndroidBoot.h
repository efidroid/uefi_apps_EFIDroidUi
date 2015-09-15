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

#ifndef __LINUX_LOADER_H__
#define __LINUX_LOADER_H__

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
#include <Library/EFIDroid.h>
#include <Library/Cpio.h>
#include <Library/Decompress.h>
#include <Library/PcdLib.h>
#include <Library/DxeServicesLib.h>

#include <LittleKernel.h>

#include "bootimg.h"

EFI_STATUS
AndroidBootFromBlockIo (
  IN VOID *Private
);

EFI_STATUS
AndroidVerify (
  IN VOID* Buffer
);

#endif /* __LINUX_LOADER_H__ */
