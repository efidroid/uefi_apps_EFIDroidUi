/** @file

Copyright (c) 2004 - 2008, Intel Corporation. All rights reserved.<BR>
Copyright (c) 2014, ARM Ltd. All rights reserved.<BR>

This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "AndroidBoot.h"

STATIC UINT64 FileSize = 0;
STATIC EFI_FILE_PROTOCOL* FileHandle = NULL;

EFI_STATUS
EFIAPI
BIOReset (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN BOOLEAN                        ExtendedVerification
  )
{
  return EFI_SUCCESS;
}

STATIC EFI_STATUS
EFIAPI
BIOReadBlocks (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN UINT32                         MediaId,
  IN EFI_LBA                        Lba,
  IN UINTN                          BufferSize,
  OUT VOID                          *Buffer
  )
{
  EFI_BLOCK_IO_MEDIA         *Media;
  UINTN                      BlockSize;
  EFI_STATUS                 Status;

  Media     = This->Media;
  BlockSize = Media->BlockSize;

  if (MediaId != Media->MediaId) {
    return EFI_MEDIA_CHANGED;
  }

  if (Lba > Media->LastBlock) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Lba + (BufferSize / BlockSize) - 1) > Media->LastBlock) {
    return EFI_INVALID_PARAMETER;
  }

  if (BufferSize % BlockSize != 0) {
    return EFI_BAD_BUFFER_SIZE;
  }

  if (Buffer == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (BufferSize == 0) {
    return EFI_SUCCESS;
  }

  Status = FileHandleSetPosition(FileHandle, Lba*BlockSize);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  Status = FileHandleRead(FileHandle, &BufferSize, Buffer);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

STATIC EFI_STATUS
EFIAPI
BIOWriteBlocks (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN UINT32                         MediaId,
  IN EFI_LBA                        Lba,
  IN UINTN                          BufferSize,
  IN VOID                           *Buffer
  )
{
  return EFI_WRITE_PROTECTED;
}
STATIC EFI_STATUS
EFIAPI
BIOFlushBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
MultibootCallback (
  IN VOID *Private
)
{
  EFI_STATUS                Status;
  multiboot_handle_t        *mbhandle = Private;
  EFI_FILE_PROTOCOL         *BootFile;

  DEBUG((EFI_D_ERROR, "Booting %a ...\n", mbhandle->Name));

  // open ROM directory
  Status = mbhandle->ROMDirectory->Open (
                   mbhandle->ROMDirectory,
                   &BootFile,
                   mbhandle->PartitionBoot,
                   EFI_FILE_MODE_READ,
                   0
                   );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = FileHandleGetSize(BootFile, &FileSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  FileHandle = BootFile;

  EFI_BLOCK_IO_MEDIA Media = {
    .MediaId = SIGNATURE_32('f', 'i', 'l', 'e'),
    .RemovableMedia = FALSE,
    .MediaPresent = TRUE,
    .LogicalPartition = FALSE,
    .ReadOnly = TRUE,
    .WriteCaching = FALSE,
    .BlockSize = 512,
    .IoAlign = 4,
    .LastBlock = FileSize/512 - 1
  };

  EFI_BLOCK_IO_PROTOCOL BlockIo = {
    .Revision = EFI_BLOCK_IO_INTERFACE_REVISION,
    .Media = &Media,
    .Reset = BIOReset,
    .ReadBlocks = BIOReadBlocks,
    .WriteBlocks = BIOWriteBlocks,
    .FlushBlocks = BIOFlushBlocks,
  };

  return AndroidBootFromBlockIo(&BlockIo, mbhandle);
}
