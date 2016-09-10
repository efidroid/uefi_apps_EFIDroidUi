#ifndef __INTERNAL_LOADER_H__
#define __INTERNAL_LOADER_H__

typedef enum {
  LAST_BOOT_TYPE_BLOCKIO = 0,
  LAST_BOOT_TYPE_FILE,
  LAST_BOOT_TYPE_MULTIBOOT,
} LAST_BOOT_TYPE;

typedef struct {
  // common
  LAST_BOOT_TYPE Type;
  CHAR8 TextDevicePath[200];

  // blockio: unused
  // file: boot image file path
  // multiboot: multiboot.ini file path
  CHAR8 FilePathName[1024];
} LAST_BOOT_ENTRY;

typedef struct {
  // ini values
  CHAR8  *Name;
  CHAR8  *Description;
  CHAR16 *PartitionBoot;
  CHAR8  *ReplacementCmdline;

  // handles
  EFI_HANDLE DeviceHandle;
  EFI_FILE_PROTOCOL* ROMDirectory;

  // set by MultibootCallback
  CHAR8* MultibootConfig;
} multiboot_handle_t;

EFI_STATUS
LoaderBootFromBlockIo (
  IN EFI_BLOCK_IO_PROTOCOL  *BlockIo,
  IN multiboot_handle_t     *mbhandle,
  IN BOOLEAN                DisablePatching,
  IN LAST_BOOT_ENTRY        *LastBootEntry
);

EFI_STATUS
LoaderBootFromFile (
  IN EFI_FILE_PROTOCOL  *File,
  IN multiboot_handle_t *mbhandle,
  IN BOOLEAN            DisablePatching,
  IN LAST_BOOT_ENTRY    *LastBootEntry
);

EFI_STATUS
LoaderBootFromBuffer (
  IN VOID               *Buffer,
  IN UINTN              Size,
  IN multiboot_handle_t *mbhandle,
  IN BOOLEAN            DisablePatching,
  IN LAST_BOOT_ENTRY    *LastBootEntry
);

EFI_STATUS
LoaderBootContext (
  IN bootimg_context_t      *context,
  IN multiboot_handle_t     *mbhandle,
  IN BOOLEAN                DisablePatching,
  IN LAST_BOOT_ENTRY        *LastBootEntry
);

EFI_STATUS
LoaderGetDecompressedRamdisk (
  IN bootimg_context_t      *context,
  OUT CPIO_NEWC_HEADER      **DecompressedRamdiskOut
);

VOID
custom_init_context (
  IN bootimg_context_t *context
);

INTN
libboot_identify_blockio (
  IN EFI_BLOCK_IO_PROTOCOL *BlockIo,
  IN bootimg_context_t     *context
);

INTN
libboot_identify_file (
  IN EFI_FILE_PROTOCOL *File,
  IN bootimg_context_t *context
);

#endif /* __INTERNAL_LOADER_H__ */
