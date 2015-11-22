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

#define ATAG_MAX_SIZE   0x3000
#define DTB_PAD_SIZE    0x1000
#define ROUNDUP(a, b)   (((a) + ((b)-1)) & ~((b)-1))
#define ROUNDDOWN(a, b) ((a) & ~((b)-1))

STATIC EFI_FILE_PROTOCOL* FileHandle = NULL;
CONST CHAR8* CMDLINE_MULTIBOOTPATH = " multibootpath=";
CONST CHAR8* CMDLINE_RDINIT        = " rdinit=/init.multiboot";

typedef VOID (*LINUX_KERNEL)(UINT32 Zero, UINT32 Arch, UINTN ParametersBase);

typedef struct {
  boot_img_hdr_t  *Hdr;
  UINT32          MachType;

  VOID   *Kernel;
  VOID   *Ramdisk;
  VOID   *Second;
  VOID   *Tags;
  CHAR8  *Cmdline;

  VOID   *kernel_loaded;
  VOID   *ramdisk_loaded;
  VOID   *second_loaded;
  VOID   *tags_loaded;
} android_parsed_bootimg_t;

STATIC inline
UINT32
RangeOverlaps (
  UINT32 x1,
  UINT32 x2,
  UINT32 y1,
  UINT32 y2
)
{
  return MAX(x1,y1) <= MIN(x2,y2);
}

#define MAXVAL_FUNCTION(name, type) \
int name(type n, ...) { \
  type i, val, largest; \
  VA_LIST vl; \
 \
  VA_START(vl, n); \
  largest = VA_ARG(vl, type); \
  for(i=1; i<n; i++) { \
    val = VA_ARG(vl,type); \
    largest = (largest>val) ? largest : val; \
  } \
  VA_END(vl); \
 \
  return largest; \
}

MAXVAL_FUNCTION(MAXUINT, UINT32);


EFI_STATUS
AndroidVerify (
  IN VOID* Buffer
)
{
  boot_img_hdr_t *AndroidHdr = Buffer;

  if(CompareMem(AndroidHdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE)!=0) {
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

STATIC EFI_STATUS
AndroidLoadCmdline (
  android_parsed_bootimg_t  *Parsed,
  IN multiboot_handle_t     *mbhandle
)
{
  boot_img_hdr_t* Hdr = Parsed->Hdr;
  EFI_DEVICE_PATH_PROTOCOL  *DevPath;
  CHAR8 *DevPathString = NULL;
  UINTN Len;

  // check mbhandle
  if(mbhandle) {
    // get devpath
    DevPath = DevicePathFromHandle(mbhandle->DeviceHandle);
    if (DevPath == NULL)
      return EFI_INVALID_PARAMETER;

    // get HD devpath
    EFI_DEVICE_PATH_PROTOCOL *Node = DevPath;
    BOOLEAN Found = FALSE;
    while (!IsDevicePathEnd (Node)) {
      if (DevicePathType (Node) == MEDIA_DEVICE_PATH &&
          DevicePathSubType (Node) == MEDIA_HARDDRIVE_DP
          ) {
        Found = TRUE;
        break;
      }
      Node = NextDevicePathNode (Node);
    }
    if(Found == FALSE)
      return EFI_INVALID_PARAMETER;

    // build guid part
    HARDDRIVE_DEVICE_PATH *Hd = (HARDDRIVE_DEVICE_PATH*) Node;
    switch (Hd->SignatureType) {
    case SIGNATURE_TYPE_MBR:
      Len = 3+1+8+1+1;
      DevPathString = AllocatePool(Len);
      AsciiSPrint(DevPathString, Len, "MBR,%08x,", *((UINT32 *) (&(Hd->Signature[0]))));
      break;

    case SIGNATURE_TYPE_GUID:
      Len = 3+1+36+1+1;
      DevPathString = AllocatePool(Len);
      AsciiSPrint(DevPathString, Len, "GPT,%g,", (EFI_GUID *) &(Hd->Signature[0]));
      break;

    default:
      return EFI_INVALID_PARAMETER;
    }
  }

  // terminate cmdlines
  Hdr->cmdline[BOOT_ARGS_SIZE-1] = 0;
  Hdr->extra_cmdline[BOOT_EXTRA_ARGS_SIZE-1] = 0;

  // create cmdline
  UINTN len_cmdline = AsciiStrLen(Hdr->cmdline);
  UINTN len_cmdline_extra = AsciiStrLen(Hdr->extra_cmdline);
  UINTN len_cmdline_rdinit = AsciiStrLen(CMDLINE_RDINIT);
  UINTN len_cmdline_mbpath = 0;
  if(mbhandle) {
    len_cmdline_mbpath += AsciiStrLen(CMDLINE_MULTIBOOTPATH);
    len_cmdline_mbpath += AsciiStrLen(DevPathString);
    len_cmdline_mbpath += AsciiStrLen(mbhandle->MultibootConfig);
  }

  UINTN CmdlineLenMax = len_cmdline + len_cmdline_extra + len_cmdline_rdinit + len_cmdline_mbpath + 1;
  Parsed->Cmdline = AllocateZeroPool(CmdlineLenMax);
  if (Parsed->Cmdline == NULL)
    return EFI_OUT_OF_RESOURCES;

  AsciiStrCatS(Parsed->Cmdline, CmdlineLenMax, Hdr->cmdline);
  AsciiStrCatS(Parsed->Cmdline, CmdlineLenMax, Hdr->extra_cmdline);
  AsciiStrCatS(Parsed->Cmdline, CmdlineLenMax, CMDLINE_RDINIT);

  if(mbhandle) {
    AsciiStrCatS(Parsed->Cmdline, CmdlineLenMax, CMDLINE_MULTIBOOTPATH);
    AsciiStrCatS(Parsed->Cmdline, CmdlineLenMax, DevPathString);
    AsciiStrCatS(Parsed->Cmdline, CmdlineLenMax, mbhandle->MultibootConfig);
  }

  return EFI_SUCCESS;
}

STATIC UINT32
AlignMemoryRange (
  IN UINT32 Addr,
  IN OUT UINTN *Size,
  OUT UINTN  *AddrOffset
)
{
  // align range
  UINT32 AddrAligned = ROUNDDOWN(Addr, EFI_PAGE_SIZE);

  // calculate offset
  UINTN Offset = Addr - AddrAligned;
  if (AddrOffset!=NULL)
    *AddrOffset = Offset;

  // round and return size
  *Size = ROUNDUP(Offset + (*Size), EFI_PAGE_SIZE);

  return AddrAligned;
}

STATIC EFI_STATUS
FreeAlignedMemoryRange (
  IN UINT32 Address,
  IN OUT UINTN Size
)
{
  UINTN      AlignedSize = Size;
  UINTN      AddrOffset = 0;

  EFI_PHYSICAL_ADDRESS AllocationAddress = AlignMemoryRange(Address, &AlignedSize, &AddrOffset);

  return gBS->FreePages(AllocationAddress, EFI_SIZE_TO_PAGES(AlignedSize));
}

STATIC EFI_STATUS
AndroidLoadImage (
  EFI_BLOCK_IO_PROTOCOL *BlockIo,
  UINTN                 Offset,
  UINTN                 Size,
  VOID                  **Buffer,
  UINT32                Address
)
{
  EFI_STATUS Status;
  UINTN      AlignedSize = Size;
  UINTN      AddrOffset = 0;
  EFI_PHYSICAL_ADDRESS AllocationAddress = AlignMemoryRange(Address, &AlignedSize, &AddrOffset);

  if ((Offset % BlockIo->Media->BlockSize) != 0)
    return EFI_INVALID_PARAMETER;

  if (Size == 0) {
    return EFI_INVALID_PARAMETER;
  }

  Status = gBS->AllocatePages (Address?AllocateAddress:AllocateAnyPages, EfiBootServicesData, EFI_SIZE_TO_PAGES(AlignedSize), &AllocationAddress);
  if (EFI_ERROR(Status)) {
    return Status;
  }
  *Buffer = (VOID*)((UINTN)AllocationAddress)+AddrOffset;

  if (Offset!=0) {
    // read data
    Status = BlockIo->ReadBlocks(BlockIo, BlockIo->Media->MediaId, Offset/BlockIo->Media->BlockSize, AlignedSize, *Buffer);
    if (EFI_ERROR(Status)) {
      gBS->FreePages(AllocationAddress, EFI_SIZE_TO_PAGES(Size));
      *Buffer = NULL;
      return Status;
    }
  }

  return EFI_SUCCESS;
}

STATIC
VOID
PreparePlatformHardware (
  VOID
  )
{
  //Note: Interrupts will be disabled by the GIC driver when ExitBootServices() will be called.

  // Clean before Disable else the Stack gets corrupted with old data.
  ArmCleanDataCache ();
  ArmDisableDataCache ();
  // Invalidate all the entries that might have snuck in.
  ArmInvalidateDataCache ();

  // Invalidate and disable the Instruction cache
  ArmDisableInstructionCache ();
  ArmInvalidateInstructionCache ();

  // Turn off MMU
  ArmDisableMmu ();
}

VOID DecompError(CHAR8* Str)
{
  DEBUG((EFI_D_ERROR, "%a\n", Str));
}

EFI_STATUS
AndroidBootFromBlockIo (
  IN EFI_BLOCK_IO_PROTOCOL  *BlockIo,
  IN multiboot_handle_t     *mbhandle
)
{
  EFI_STATUS                Status;
  UINTN                     BufferSize;
  boot_img_hdr_t            *AndroidHdr;
  android_parsed_bootimg_t  Parsed;
  LINUX_KERNEL              LinuxKernel;
  UINTN                     TagsSize;
  lkapi_t                   *LKApi = GetLKApi();
  VOID                      *OriginalRamdisk = NULL;
  UINT32                    RamdiskUncompressedLen = 0;

  // initialize parsed data
  SetMem(&Parsed, sizeof(Parsed), 0);

  // allocate a buffer for the android header aligned on the block size
  BufferSize = ALIGN_VALUE(sizeof(boot_img_hdr_t), BlockIo->Media->BlockSize);
  AndroidHdr = AllocatePool(BufferSize);
  if (AndroidHdr == NULL) {
    gErrorStr = "Error allocating bootimg header";
    return EFI_OUT_OF_RESOURCES;
  }

  // read and verify the android header
  Status = BlockIo->ReadBlocks(BlockIo, BlockIo->Media->MediaId, 0, BufferSize, AndroidHdr);
  if (EFI_ERROR(Status)) {
    gErrorStr = "Can't read boot image header";
    goto FREEBUFFER;
  }

  Status = AndroidVerify(AndroidHdr);
  if (EFI_ERROR(Status)) {
    gErrorStr = "Not a boot image";
    goto FREEBUFFER;
  }
  Parsed.Hdr = AndroidHdr;

  // this is not supported
  // actually I've never seen a device using this so it's not even clear how this would work
  if (AndroidHdr->second_size > 0) {
    gErrorStr = "second_size > 0";
    Status = EFI_UNSUPPORTED;
    goto FREEBUFFER;
  }

  // load cmdline
  Status = AndroidLoadCmdline(&Parsed, mbhandle);
  if (EFI_ERROR(Status)) {
    gErrorStr = "Error loading cmdline";
    goto FREEBUFFER;
  }

  // update addresses if necessary
  LKApi->boot_update_addrs(&AndroidHdr->kernel_addr, &AndroidHdr->ramdisk_addr, &AndroidHdr->tags_addr);

  // calculate offsets
  UINTN off_kernel  = AndroidHdr->page_size;
  UINTN off_ramdisk = off_kernel  + ALIGN_VALUE(AndroidHdr->kernel_size,  AndroidHdr->page_size);
  UINTN off_second  = off_ramdisk + ALIGN_VALUE(AndroidHdr->ramdisk_size, AndroidHdr->page_size);
  UINTN off_tags    = off_second  + ALIGN_VALUE(AndroidHdr->second_size,  AndroidHdr->page_size);

  // load kernel
  Status = AndroidLoadImage(BlockIo, off_kernel, AndroidHdr->kernel_size, &Parsed.Kernel, AndroidHdr->kernel_addr);
  if (EFI_ERROR(Status)) {
    gErrorStr = "Error loading kernel";
    goto FREEBUFFER;
  }

  // compute tag size
  TagsSize = AndroidHdr->dt_size;
  if (AndroidHdr->dt_size==0) {
    TagsSize = ATAG_MAX_SIZE;
    off_tags = 0;
  }
  else {
    // the DTB may get expanded
    TagsSize += DTB_PAD_SIZE;
  }


  // allocate tag memory and load dtb if available
  Status = AndroidLoadImage(BlockIo, off_tags, TagsSize, &Parsed.Tags, AndroidHdr->tags_addr);
  if (EFI_ERROR(Status)) {
    gErrorStr = "Error loading tags";
    goto FREEBUFFER;
  }

  // load ramdisk into UEFI memory
  Status = AndroidLoadImage(BlockIo, off_ramdisk, AndroidHdr->ramdisk_size, &OriginalRamdisk, 0);
  if (EFI_ERROR(Status)) {
    gErrorStr = "Error loading ramdisk";
    goto FREEBUFFER;
  }

  // get decompressor
  CONST CHAR8 *DecompName;
  decompress_fn Decompressor = decompress_method(OriginalRamdisk, AndroidHdr->ramdisk_size, &DecompName);
  if(Decompressor==NULL) {
    gErrorStr = "no decompressor found";
    goto FREEBUFFER;
  }
  else {
    DEBUG((EFI_D_INFO, "decompressor: %a\n", DecompName));
  }

  // get uncompressed size
  // since the Linux decompressor doesn't support predicting the length we hardcode this to 10MB
  RamdiskUncompressedLen = 10*1024*1024;

  UINT8 *MultibootBin;
  UINTN MultibootSize;
  Status = GetSectionFromAnyFv (PcdGetPtr(PcdMultibootBin), EFI_SECTION_RAW, 0, (VOID **) &MultibootBin, &MultibootSize);
  if (EFI_ERROR (Status)) {
    gErrorStr = "Multiboot binary not found";
    goto FREEBUFFER;
  }

  // add multiboot binary size to uncompressed ramdisk size
  CONST CHAR8 *cpio_name_mbinit = "/init.multiboot";
  UINTN objsize = CpioPredictObjSize(AsciiStrLen(cpio_name_mbinit), MultibootSize);
  RamdiskUncompressedLen += objsize;

  if (   RangeOverlaps(AndroidHdr->ramdisk_addr, RamdiskUncompressedLen, (UINT32)Parsed.Kernel, AndroidHdr->kernel_size)
      || RangeOverlaps(AndroidHdr->ramdisk_addr, RamdiskUncompressedLen, (UINT32)Parsed.Tags, TagsSize)
     )
  {
    AndroidHdr->ramdisk_addr = MAXUINT((UINT32)Parsed.Kernel + AndroidHdr->kernel_size, Parsed.Tags + TagsSize);
    DEBUG((EFI_D_INFO, "Ramdisk overlaps - move it to 0x%08x.\n", AndroidHdr->ramdisk_addr));
  }

  // allocate uncompressed ramdisk memory in boot memory
  Status = AndroidLoadImage(BlockIo, 0, RamdiskUncompressedLen, &Parsed.Ramdisk, AndroidHdr->ramdisk_addr);
  if (EFI_ERROR(Status)) {
    gErrorStr = "Error allocating decompressed ramdisk memory";
    goto FREEBUFFER;
  }

  // decompress ramdisk
  if(Decompressor(OriginalRamdisk, AndroidHdr->ramdisk_size, NULL, NULL, Parsed.Ramdisk, NULL, DecompError)) {
    gErrorStr = "Error decompressing ramdisk";
    goto FREEBUFFER;
  }

  // add multiboot binary
  CPIO_NEWC_HEADER *cpiohd = (CPIO_NEWC_HEADER *) Parsed.Ramdisk;
  cpiohd = CpioGetLast (cpiohd);
  cpiohd = CpioCreateObj (cpiohd, cpio_name_mbinit, MultibootBin, MultibootSize);
  cpiohd = CpioCreateObj (cpiohd, CPIO_TRAILER, NULL, 0);
  ASSERT((UINT32)cpiohd <= (UINT32)Parsed.Ramdisk+RamdiskUncompressedLen);

  // generate Atags
  if(LKApi->boot_create_tags(Parsed.Cmdline, (UINT32)Parsed.Ramdisk, ((UINT32)cpiohd)-((UINT32)Parsed.Ramdisk), AndroidHdr->tags_addr, TagsSize-DTB_PAD_SIZE)) {
    gErrorStr = "Error creating tags";
    goto FREEBUFFER;
  }

  // Shut down UEFI boot services. ExitBootServices() will notify every driver that created an event on
  // ExitBootServices event. Example the Interrupt DXE driver will disable the interrupts on this event.
  Status = ShutdownUefiBootServices ();
  if (EFI_ERROR (Status)) {
    gErrorStr = "Error shutting down boot services";
    DEBUG ((EFI_D_ERROR, "ERROR: Can not shutdown UEFI boot services. Status=0x%X\n", Status));
    goto FREEBUFFER;
  }

  //
  // Switch off interrupts, caches, mmu, etc
  //
  LKApi->platform_uninit();
  PreparePlatformHardware ();

  // Outside BootServices, so can't use Print();
  DEBUG ((EFI_D_ERROR, "\nStarting the kernel:\n\n"));

  LinuxKernel = (LINUX_KERNEL)(UINTN)Parsed.Kernel;
  // Jump to kernel with register set
  LKApi->boot_exec(LinuxKernel, (UINTN)0, LKApi->boot_machine_type(), (UINTN)AndroidHdr->tags_addr);

  // Kernel should never exit
  // After Life services are not provided
  ASSERT (FALSE);
  // We cannot recover the execution at this stage
  while (1);

FREEBUFFER:
  if(Parsed.Cmdline)
    FreePool(Parsed.Cmdline);
  if(Parsed.Kernel)
    FreeAlignedMemoryRange((UINT32)Parsed.Kernel, AndroidHdr->kernel_size);
  if(Parsed.Ramdisk)
    FreeAlignedMemoryRange((UINT32)Parsed.Ramdisk, RamdiskUncompressedLen);
  if(Parsed.Tags)
    FreeAlignedMemoryRange((UINT32)Parsed.Tags, AndroidHdr->dt_size);
  if(OriginalRamdisk)
    FreeAlignedMemoryRange((UINT32)OriginalRamdisk, AndroidHdr->ramdisk_size);

  FreePool(AndroidHdr);

  return EFI_SUCCESS;
}

STATIC EFI_STATUS
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
AndroidBootFromFile (
  IN EFI_FILE_PROTOCOL  *File,
  IN multiboot_handle_t *mbhandle
)
{
  EFI_STATUS                Status;
  UINT64                    FileSize = 0;

  Status = FileHandleGetSize(File, &FileSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  FileHandle = File;

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
