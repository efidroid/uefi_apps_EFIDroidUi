#include "FileBlockIo.h"

STATIC FILEBLOCKIO_INSTANCE mTemplate = {
  FILEBLOCKIO_INSTANCE_SIGNATURE,
  { // BlockIo
    EFI_BLOCK_IO_INTERFACE_REVISION,   // Revision
    NULL,                              // *Media
    FileBlockIoReset,                  // Reset
    FileBlockIoReadBlocks,             // ReadBlocks
    FileBlockIoWriteBlocks,            // WriteBlocks
    FileBlockIoFlushBlocks             // FlushBlocks
  },
  { // BlockMedia
    FILEBLOCKIO_INSTANCE_SIGNATURE,           // MediaId
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
  NULL // File
};

STATIC
EFI_STATUS
EFIAPI
FileBlockIoReset (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN BOOLEAN                        ExtendedVerification
  )
{
  return EFI_SUCCESS;
}


STATIC
EFI_STATUS
EFIAPI
FileBlockIoReadBlocks (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN UINT32                         MediaId,
  IN EFI_LBA                        Lba,
  IN UINTN                          BufferSize,
  OUT VOID                          *Buffer
  )
{
  FILEBLOCKIO_INSTANCE      *Instance;
  EFI_BLOCK_IO_MEDIA        *Media;
  UINTN                     BlockSize;
  EFI_STATUS                Status;

  Instance = FILEBLOCKIO_INSTANCE_FROM_BLOCKIO_THIS(This);
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

  Status = FileHandleSetPosition(Instance->File, Lba*BlockSize);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  Status = FileHandleRead(Instance->File, &BufferSize, Buffer);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
FileBlockIoWriteBlocks (
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
FileBlockIoFlushBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This
  )
{
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
FileBlockIoInstanceContructor (
  OUT FILEBLOCKIO_INSTANCE** NewInstance
  )
{
  FILEBLOCKIO_INSTANCE* Instance;

  Instance = AllocateCopyPool (sizeof(FILEBLOCKIO_INSTANCE), &mTemplate);
  if (Instance == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Instance->BlockIo.Media     = &Instance->BlockMedia;

  *NewInstance = Instance;
  return EFI_SUCCESS;
}

EFI_STATUS
FileBlockIoCreate (
  IN EFI_FILE_PROTOCOL *File,
  IN EFI_BLOCK_IO_PROTOCOL** BlockIo
  )
{
  EFI_STATUS              Status;
  FILEBLOCKIO_INSTANCE    *Instance = NULL;
  UINT64                  FileSize = 0;

  if(!File || !BlockIo)
    return EFI_INVALID_PARAMETER;

  Status = FileHandleGetSize(File, &FileSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // allocate instance
  Status = FileBlockIoInstanceContructor (&Instance);
  if (EFI_ERROR(Status)) {
    return Status;
  }

  // set data
  Instance->BlockMedia.LastBlock = FileSize/Instance->BlockMedia.BlockSize - 1;
  Instance->File = File;

  *BlockIo = &Instance->BlockIo;

  return EFI_SUCCESS;
}

VOID
FileBlockIoFree (
  IN EFI_BLOCK_IO_PROTOCOL* BlockIo
  )
{
  FILEBLOCKIO_INSTANCE    *Instance = FILEBLOCKIO_INSTANCE_FROM_BLOCKIO_THIS(BlockIo);
  FreePool(Instance);
}
