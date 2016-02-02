#include "MemoryBlockIo.h"

STATIC MEMORYBLOCKIO_INSTANCE mTemplate = {
  MEMORYBLOCKIO_INSTANCE_SIGNATURE,
  { // BlockIo
    EFI_BLOCK_IO_INTERFACE_REVISION,   // Revision
    NULL,                              // *Media
    MemoryBlockIoReset,                  // Reset
    MemoryBlockIoReadBlocks,             // ReadBlocks
    MemoryBlockIoWriteBlocks,            // WriteBlocks
    MemoryBlockIoFlushBlocks             // FlushBlocks
  },
  { // BlockMedia
    MEMORYBLOCKIO_INSTANCE_SIGNATURE,           // MediaId
    FALSE,                                    // RemovableMedia
    TRUE,                                     // MediaPresent
    FALSE,                                    // LogicalPartition
    TRUE,                                     // ReadOnly
    FALSE,                                    // WriteCaching
    512,                                      // BlockSize
    4,                                        // IoAlign
    0,                                        // Pad
    0                                         // LastBlock
  },
  NULL // Buffer
};

STATIC
EFI_STATUS
EFIAPI
MemoryBlockIoReset (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN BOOLEAN                        ExtendedVerification
  )
{
  return EFI_SUCCESS;
}


STATIC
EFI_STATUS
EFIAPI
MemoryBlockIoReadBlocks (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN UINT32                         MediaId,
  IN EFI_LBA                        Lba,
  IN UINTN                          BufferSize,
  OUT VOID                          *Buffer
  )
{
  MEMORYBLOCKIO_INSTANCE    *Instance;
  EFI_BLOCK_IO_MEDIA        *Media;
  UINTN                     BlockSize;

  Instance = MEMORYBLOCKIO_INSTANCE_FROM_BLOCKIO_THIS(This);
  Media     = &Instance->BlockMedia;
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

  UINT8* Buffer8 = Instance->Buffer;
  CopyMem(Buffer, Buffer8 + Lba*BlockSize, BufferSize);

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
MemoryBlockIoWriteBlocks (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN UINT32                         MediaId,
  IN EFI_LBA                        Lba,
  IN UINTN                          BufferSize,
  IN VOID                           *Buffer
  )
{
  return EFI_WRITE_PROTECTED;
}

STATIC
EFI_STATUS
EFIAPI
MemoryBlockIoFlushBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This
  )
{
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
MemoryBlockIoInstanceContructor (
  OUT MEMORYBLOCKIO_INSTANCE** NewInstance
  )
{
  MEMORYBLOCKIO_INSTANCE* Instance;

  Instance = AllocateCopyPool (sizeof(MEMORYBLOCKIO_INSTANCE), &mTemplate);
  if (Instance == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Instance->BlockIo.Media     = &Instance->BlockMedia;

  *NewInstance = Instance;
  return EFI_SUCCESS;
}

EFI_STATUS
MemoryBlockIoCreate (
  IN VOID                    *Buffer,
  IN UINTN                   Size,
  IN EFI_BLOCK_IO_PROTOCOL** BlockIo
  )
{
  EFI_STATUS              Status;
  MEMORYBLOCKIO_INSTANCE  *Instance = NULL;
  UINT64                  FileSize = 0;

  if(!Buffer || !Size)
    return EFI_INVALID_PARAMETER;

  FileSize = Size;

  // allocate instance
  Status = MemoryBlockIoInstanceContructor (&Instance);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  // set data
  Instance->BlockMedia.LastBlock = FileSize/Instance->BlockMedia.BlockSize - 1;
  Instance->Buffer = Buffer;

  *BlockIo = &Instance->BlockIo;

  return EFI_SUCCESS;
}

VOID
MemoryBlockIoFree (
  IN EFI_BLOCK_IO_PROTOCOL* BlockIo
  )
{
  MEMORYBLOCKIO_INSTANCE    *Instance = MEMORYBLOCKIO_INSTANCE_FROM_BLOCKIO_THIS(BlockIo);
  FreePool(Instance);
}
