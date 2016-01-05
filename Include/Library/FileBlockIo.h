#ifndef LIBRARY_FILEBLOCKIO_H
#define LIBRARY_FILEBLOCKIO_H 1

#include <PiDxe.h>
#include <Guid/FileInfo.h>

#include <Library/BaseLib.h>
#include <Library/FileHandleLib.h>

EFI_STATUS
FileBlockIoCreate (
  IN EFI_FILE_PROTOCOL *File,
  IN EFI_BLOCK_IO_PROTOCOL** BlockIo
  );

VOID
FileBlockIoFree (
  IN EFI_BLOCK_IO_PROTOCOL* BlockIo
  );

#endif /* ! LIBRARY_FILEBLOCKIO_H */
