#ifndef _VIRTUAL_BLOCK_IO_H_
#define _VIRTUAL_BLOCK_IO_H_

#include <Uefi.h>
#include <PiDxe.h>

#include <Guid/FileInfo.h>
#include <Library/BaseLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/FileHandleLib.h>

#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>

typedef struct {
  UINT32                                Signature;
  EFI_BLOCK_IO_PROTOCOL                 BlockIo;
  EFI_BLOCK_IO_MEDIA                    BlockMedia;
  EFI_FILE_PROTOCOL                     *File;
} FILEBLOCKIO_INSTANCE;

#define FILEBLOCKIO_INSTANCE_SIGNATURE  SIGNATURE_32('f', 'b', 'l', 'k')
#define FILEBLOCKIO_INSTANCE_FROM_BLOCKIO_THIS(a)     CR (a, FILEBLOCKIO_INSTANCE, BlockIo, FILEBLOCKIO_INSTANCE_SIGNATURE)

STATIC
EFI_STATUS
EFIAPI
FileBlockIoReset (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN BOOLEAN                        ExtendedVerification
  );


STATIC
EFI_STATUS
EFIAPI
FileBlockIoReadBlocks (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN UINT32                         MediaId,
  IN EFI_LBA                        Lba,
  IN UINTN                          BufferSize,
  OUT VOID                          *Buffer
  );

STATIC
EFI_STATUS
EFIAPI
FileBlockIoWriteBlocks (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN UINT32                         MediaId,
  IN EFI_LBA                        Lba,
  IN UINTN                          BufferSize,
  IN VOID                           *Buffer
  );

STATIC
EFI_STATUS
EFIAPI
FileBlockIoFlushBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This
  );

STATIC
EFI_STATUS
FileBlockIoInstanceContructor (
  OUT FILEBLOCKIO_INSTANCE** FILEBLOCKIO_INSTANCE
  );

#endif
