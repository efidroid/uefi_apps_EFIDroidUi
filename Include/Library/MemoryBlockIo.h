#ifndef LIBRARY_MEMORYBLOCKIO_H
#define LIBRARY_MEMORYBLOCKIO_H 1

#include <PiDxe.h>
#include <Guid/FileInfo.h>

#include <Library/BaseLib.h>

EFI_STATUS
MemoryBlockIoCreate (
  IN VOID                    *Buffer,
  IN UINTN                   Size,
  IN EFI_BLOCK_IO_PROTOCOL** BlockIo
  );

VOID
MemoryBlockIoFree (
  IN EFI_BLOCK_IO_PROTOCOL* BlockIo
  );

#endif /* ! LIBRARY_MEMORYBLOCKIO_H */
