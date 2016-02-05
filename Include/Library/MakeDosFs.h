#ifndef MAKEDOSFS_H
#define MAKEDOSFS_H 1

#include <PiDxe.h>
#include <Library/BaseLib.h>

EFI_STATUS
MakeDosFs (
  VOID* Buffer,
  UINT64 BufferSize
);

#endif /* ! MAKEDOSFS_H */
