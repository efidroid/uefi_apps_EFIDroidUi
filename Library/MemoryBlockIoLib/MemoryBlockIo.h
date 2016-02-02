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

#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>

typedef struct {
  UINT32                                Signature;
  EFI_BLOCK_IO_PROTOCOL                 BlockIo;
  EFI_BLOCK_IO_MEDIA                    BlockMedia;
  VOID                                  *Buffer;
} MEMORYBLOCKIO_INSTANCE;

#define MEMORYBLOCKIO_INSTANCE_SIGNATURE  SIGNATURE_32('m', 'b', 'l', 'k')
#define MEMORYBLOCKIO_INSTANCE_FROM_BLOCKIO_THIS(a)     CR (a, MEMORYBLOCKIO_INSTANCE, BlockIo, MEMORYBLOCKIO_INSTANCE_SIGNATURE)

STATIC
EFI_STATUS
EFIAPI
MemoryBlockIoReset (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN BOOLEAN                        ExtendedVerification
  );


STATIC
EFI_STATUS
EFIAPI
MemoryBlockIoReadBlocks (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN UINT32                         MediaId,
  IN EFI_LBA                        Lba,
  IN UINTN                          BufferSize,
  OUT VOID                          *Buffer
  );

STATIC
EFI_STATUS
EFIAPI
MemoryBlockIoWriteBlocks (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN UINT32                         MediaId,
  IN EFI_LBA                        Lba,
  IN UINTN                          BufferSize,
  IN VOID                           *Buffer
  );

STATIC
EFI_STATUS
EFIAPI
MemoryBlockIoFlushBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This
  );

STATIC
EFI_STATUS
MemoryBlockIoInstanceContructor (
  OUT MEMORYBLOCKIO_INSTANCE** MEMORYBLOCKIO_INSTANCE
  );

#endif
