#include "EFIDroidUi.h"

#define ATAG_MAX_SIZE   0x3000
#define DTB_PAD_SIZE    0x1000

CONST CHAR8* CMDLINE_MULTIBOOTPATH = " multibootpath=";
CONST CHAR8* CMDLINE_MULTIBOOTDEBUG= " multiboot.debug=";
CONST CHAR8* CMDLINE_RDINIT        = " rdinit=/init.multiboot";
CONST CHAR8* CMDLINE_PERMISSIVE    = " androidboot.selinux=permissive";

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
  IN multiboot_handle_t     *mbhandle,
  IN BOOLEAN                RecoveryMode,
  IN BOOLEAN                DisablePatching
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
  UINTN len_cmdline_permissive = AsciiStrLen(CMDLINE_PERMISSIVE);
  UINTN len_cmdline_mbpath = 0;
  if(mbhandle) {
    len_cmdline_mbpath += AsciiStrLen(CMDLINE_MULTIBOOTPATH);
    len_cmdline_mbpath += AsciiStrLen(DevPathString);
    len_cmdline_mbpath += AsciiStrLen(mbhandle->MultibootConfig);
  }

  UINTN len_cmdline_multibootdebug = 0;
  UINTN len_cmdline_multibootdebug_val = 0;

  CHAR8* DebugValue = UtilGetEFIDroidVariable("multiboot-debuglevel");
  if (DebugValue) {
    len_cmdline_multibootdebug = AsciiStrLen(CMDLINE_MULTIBOOTDEBUG);
    len_cmdline_multibootdebug_val = AsciiStrLen(DebugValue);
  }

  UINTN CmdlineLenMax = len_cmdline + len_cmdline_extra + len_cmdline_rdinit + 
                        len_cmdline_permissive + len_cmdline_mbpath +
                        len_cmdline_multibootdebug + len_cmdline_multibootdebug_val + 1;
  Parsed->Cmdline = AllocateZeroPool(CmdlineLenMax);
  if (Parsed->Cmdline == NULL)
    return EFI_OUT_OF_RESOURCES;

  AsciiStrCatS(Parsed->Cmdline, CmdlineLenMax, Hdr->cmdline);
  AsciiStrCatS(Parsed->Cmdline, CmdlineLenMax, Hdr->extra_cmdline);
  if(!DisablePatching)
    AsciiStrCatS(Parsed->Cmdline, CmdlineLenMax, CMDLINE_RDINIT);

  // in recovery mode we ptrace the whole system. that doesn't work well with selinux
  if (RecoveryMode)
    AsciiStrCatS(Parsed->Cmdline, CmdlineLenMax, CMDLINE_PERMISSIVE);

  if(mbhandle) {
    AsciiStrCatS(Parsed->Cmdline, CmdlineLenMax, CMDLINE_MULTIBOOTPATH);
    AsciiStrCatS(Parsed->Cmdline, CmdlineLenMax, DevPathString);
    AsciiStrCatS(Parsed->Cmdline, CmdlineLenMax, mbhandle->MultibootConfig);
  }

  if (DebugValue) {
    AsciiStrCatS(Parsed->Cmdline, CmdlineLenMax, CMDLINE_MULTIBOOTDEBUG);
    AsciiStrCatS(Parsed->Cmdline, CmdlineLenMax, DebugValue);
  }

  return EFI_SUCCESS;
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
  EFI_PHYSICAL_ADDRESS AllocationAddress = AlignMemoryRange(Address, &AlignedSize, &AddrOffset, BlockIo->Media->BlockSize);

  if ((Offset % BlockIo->Media->BlockSize) != 0)
    return EFI_INVALID_PARAMETER;

  if (AlignedSize == 0) {
    AlignedSize = BlockIo->Media->BlockSize;
  }

  if((*Buffer) == NULL) {
    Status = gBS->AllocatePages (Address?AllocateAddress:AllocateAnyPages, EfiBootServicesData, EFI_SIZE_TO_PAGES(AlignedSize), &AllocationAddress);
    if (EFI_ERROR(Status)) {
      return Status;
    }
    *Buffer = (VOID*)((UINTN)AllocationAddress)+AddrOffset;
  }

  if (Offset!=0 && Size!=0) {
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
  MenuShowMessage("Decompression Error", Str);
}

EFI_STATUS
AndroidBootFromBlockIo (
  IN EFI_BLOCK_IO_PROTOCOL  *BlockIo,
  IN multiboot_handle_t     *mbhandle,
  IN BOOLEAN                DisablePatching,
  IN LAST_BOOT_ENTRY        *LastBootEntry
)
{
  EFI_STATUS                Status;
  UINTN                     BufferSize;
  boot_img_hdr_t            *AndroidHdr;
  android_parsed_bootimg_t  Parsed = {0};
  LINUX_KERNEL              LinuxKernel;
  UINTN                     TagsSize = 0;
  lkapi_t                   *LKApi = GetLKApi();
  VOID                      *OriginalRamdisk = NULL;
  UINT32                    RamdiskUncompressedLen = 0;
  BOOLEAN                   RecoveryMode = FALSE;
  CHAR8                     Buf[100];

  // initialize parsed data
  SetMem(&Parsed, sizeof(Parsed), 0);

  // allocate a buffer for the android header aligned on the block size
  BufferSize = ALIGN_VALUE(sizeof(boot_img_hdr_t), BlockIo->Media->BlockSize);
  AndroidHdr = AllocatePool(BufferSize);
  if (AndroidHdr == NULL) {
    MenuShowMessage("Error", "Can't allocate boot image header.");
    return EFI_OUT_OF_RESOURCES;
  }

  // read and verify the android header
  Status = BlockIo->ReadBlocks(BlockIo, BlockIo->Media->MediaId, 0, BufferSize, AndroidHdr);
  if (EFI_ERROR(Status)) {
    AsciiSPrint(Buf, sizeof(Buf), "Can't read boot image header: %r", Status);
    MenuShowMessage("Error", Buf);
    goto FREEBUFFER;
  }

  Status = AndroidVerify(AndroidHdr);
  if (EFI_ERROR(Status)) {
    MenuShowMessage("Error", "Not a boot image.");
    goto FREEBUFFER;
  }
  Parsed.Hdr = AndroidHdr;

  // this is not supported
  // actually I've never seen a device using this so it's not even clear how this would work
  if (AndroidHdr->second_size > 0) {
    MenuShowMessage("Error", "Secondary loaders are not supported.");
    Status = EFI_UNSUPPORTED;
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
    AsciiSPrint(Buf, sizeof(Buf), "Can't load kernel: %r", Status);
    MenuShowMessage("Error", Buf);
    goto FREEBUFFER;
  }

  // compute tag size
  TagsSize = AndroidHdr->dt_size;
  if (AndroidHdr->dt_size==0) {
    TagsSize = ATAG_MAX_SIZE;
  }
  else {
    // the DTB may get expanded
    TagsSize += DTB_PAD_SIZE;
  }


  // allocate tag memory
  Status = AndroidLoadImage(BlockIo, 0, TagsSize, &Parsed.Tags, AndroidHdr->tags_addr);
  if (EFI_ERROR(Status)) {
    AsciiSPrint(Buf, sizeof(Buf), "Can't allocate tags: %r", Status);
    MenuShowMessage("Error", Buf);
    goto FREEBUFFER;
  }

  // load DT
  if (AndroidHdr->dt_size) {
    Status = AndroidLoadImage(BlockIo, off_tags, AndroidHdr->dt_size, &Parsed.Tags, AndroidHdr->tags_addr);
    if (EFI_ERROR(Status)) {
      AsciiSPrint(Buf, sizeof(Buf), "Can't load tags: %r", Status);
      MenuShowMessage("Error", Buf);
      goto FREEBUFFER;
    }
  }

  if(DisablePatching) {
    // load ramdisk
    Status = AndroidLoadImage(BlockIo, off_ramdisk, AndroidHdr->ramdisk_size, &Parsed.Ramdisk, AndroidHdr->ramdisk_addr);
    if (EFI_ERROR(Status)) {
      AsciiSPrint(Buf, sizeof(Buf), "Can't load ramdisk: %r", Status);
      MenuShowMessage("Error", Buf);
      goto FREEBUFFER;
    }
  }

  else {
    // load ramdisk into UEFI memory
    Status = AndroidLoadImage(BlockIo, off_ramdisk, AndroidHdr->ramdisk_size, &OriginalRamdisk, 0);
    if (EFI_ERROR(Status)) {
      AsciiSPrint(Buf, sizeof(Buf), "Can't load ramdisk: %r", Status);
      MenuShowMessage("Error", Buf);
      goto FREEBUFFER;
    }

    // get decompressor
    CONST CHAR8 *DecompName;
    decompress_fn Decompressor = decompress_method(OriginalRamdisk, AndroidHdr->ramdisk_size, &DecompName);
    if(Decompressor==NULL) {
      MenuShowMessage("Error", "Can't find decompressor.");
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
    Status = UEFIRamdiskGetFile ("init.multiboot", (VOID **) &MultibootBin, &MultibootSize);
    if (EFI_ERROR (Status)) {
      MenuShowMessage("Error", "Multiboot binary not found.");
      goto FREEBUFFER;
    }

    // add multiboot binary size to uncompressed ramdisk size
    CONST CHAR8 *cpio_name_mbinit = "/init.multiboot";
    UINTN objsize = CpioPredictObjSize(AsciiStrLen(cpio_name_mbinit), MultibootSize);
    RamdiskUncompressedLen += objsize;

    if (   RangeLenOverlaps(AndroidHdr->ramdisk_addr, RamdiskUncompressedLen, (UINT32)Parsed.Kernel, AndroidHdr->kernel_size)
        || RangeLenOverlaps(AndroidHdr->ramdisk_addr, RamdiskUncompressedLen, (UINT32)Parsed.Tags, TagsSize)
       )
    {
      AndroidHdr->ramdisk_addr = MAXUINT((UINT32)Parsed.Kernel + AndroidHdr->kernel_size, Parsed.Tags + TagsSize);
      DEBUG((EFI_D_INFO, "Ramdisk overlaps - move it to 0x%08x.\n", AndroidHdr->ramdisk_addr));
    }

    // allocate uncompressed ramdisk memory in boot memory
    Status = AndroidLoadImage(BlockIo, 0, RamdiskUncompressedLen, &Parsed.Ramdisk, AndroidHdr->ramdisk_addr);
    if (EFI_ERROR(Status)) {
      AsciiSPrint(Buf, sizeof(Buf), "Can't allocate memory for decompressing ramdisk: %r", Status);
      MenuShowMessage("Error", Buf);
      goto FREEBUFFER;
    }

    // decompress ramdisk
    if(Decompressor(OriginalRamdisk, AndroidHdr->ramdisk_size, NULL, NULL, Parsed.Ramdisk, NULL, DecompError)) {
      goto FREEBUFFER;
    }

    // add multiboot binary
    CPIO_NEWC_HEADER *cpiohd = (CPIO_NEWC_HEADER *) Parsed.Ramdisk;
    cpiohd = CpioGetLast (cpiohd);
    cpiohd = CpioCreateObj (cpiohd, cpio_name_mbinit, MultibootBin, MultibootSize);
    cpiohd = CpioCreateObj (cpiohd, CPIO_TRAILER, NULL, 0);
    ASSERT((UINT32)cpiohd <= (UINT32)Parsed.Ramdisk+RamdiskUncompressedLen);

    // check if this is a recovery ramdisk
    if (CpioGetByName((CPIO_NEWC_HEADER *)Parsed.Ramdisk, "sbin/recovery")) {
      RecoveryMode = TRUE;
    }

    AndroidHdr->ramdisk_size = ((UINT32)cpiohd)-((UINT32)Parsed.Ramdisk);
  }

  // load cmdline
  Status = AndroidLoadCmdline(&Parsed, mbhandle, RecoveryMode, DisablePatching);
  if (EFI_ERROR(Status)) {
    AsciiSPrint(Buf, sizeof(Buf), "Can't load cmdline: %r", Status);
    MenuShowMessage("Error", Buf);
    goto FREEBUFFER;
  }

  // generate Atags
  if(LKApi->boot_create_tags(Parsed.Cmdline, (UINT32)Parsed.Ramdisk, AndroidHdr->ramdisk_size, AndroidHdr->tags_addr, TagsSize-DTB_PAD_SIZE)) {
    MenuShowMessage("Error", "Can't generate tags.");
    goto FREEBUFFER;
  }

  // set LastBootEntry variable
  Status = UtilSetEFIDroidDataVariable(L"LastBootEntry", LastBootEntry, LastBootEntry?sizeof(*LastBootEntry):0);
  if (EFI_ERROR (Status)) {
    AsciiSPrint(Buf, sizeof(Buf), "Can't set variable 'LastBootEntry': %r", Status);
    MenuShowMessage("Error", Buf);
    goto FREEBUFFER;
  }

  // Shut down UEFI boot services. ExitBootServices() will notify every driver that created an event on
  // ExitBootServices event. Example the Interrupt DXE driver will disable the interrupts on this event.
  Status = ShutdownUefiBootServices ();
  if (EFI_ERROR (Status)) {
    MenuShowMessage("Error", "Can't shut down UEFI boot services.");
    DEBUG ((EFI_D_ERROR, "ERROR: Can not shutdown UEFI boot services. Status=0x%X\n", Status));
    goto FREEBUFFER;
  }

  //
  // Switch off interrupts, caches, mmu, etc
  //
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
    FreeAlignedMemoryRange((UINT32)Parsed.Kernel, AndroidHdr->kernel_size, BlockIo->Media->BlockSize);
  if(Parsed.Ramdisk)
    FreeAlignedMemoryRange((UINT32)Parsed.Ramdisk, RamdiskUncompressedLen, BlockIo->Media->BlockSize);
  if(Parsed.Tags)
    FreeAlignedMemoryRange((UINT32)Parsed.Tags, TagsSize, BlockIo->Media->BlockSize);
  if(OriginalRamdisk)
    FreeAlignedMemoryRange((UINT32)OriginalRamdisk, AndroidHdr->ramdisk_size, BlockIo->Media->BlockSize);

  FreePool(AndroidHdr);

  return EFI_SUCCESS;
}

EFI_STATUS
AndroidGetDecompRamdiskFromBlockIo (
  IN EFI_BLOCK_IO_PROTOCOL  *BlockIo,
  OUT CPIO_NEWC_HEADER      **DecompressedRamdiskOut
)
{
  VOID                      *OriginalRamdisk = NULL;
  UINT32                    RamdiskUncompressedLen = 0;
  CPIO_NEWC_HEADER          *DecompressedRamdisk = NULL;
  UINTN                     BufferSize;
  EFI_STATUS                Status;
  IN boot_img_hdr_t         *AndroidHdr;

  // allocate a buffer for the android header aligned on the block size
  BufferSize = ALIGN_VALUE(sizeof(boot_img_hdr_t), BlockIo->Media->BlockSize);
  AndroidHdr = AllocatePool(BufferSize);
  if(AndroidHdr == NULL)
    return EFI_OUT_OF_RESOURCES;

  // read android header
  Status = BlockIo->ReadBlocks(BlockIo, BlockIo->Media->MediaId, 0, BufferSize, AndroidHdr);
  if(EFI_ERROR(Status)) {
    goto FREEBUFFER;
  }

  // verify android header
  Status = AndroidVerify(AndroidHdr);
  if(EFI_ERROR(Status)) {
    goto FREEBUFFER;
  }

  // calculate offsets
  UINTN off_kernel  = AndroidHdr->page_size;
  UINTN off_ramdisk = off_kernel  + ALIGN_VALUE(AndroidHdr->kernel_size,  AndroidHdr->page_size);

  // load ramdisk into UEFI memory
  Status = AndroidLoadImage(BlockIo, off_ramdisk, AndroidHdr->ramdisk_size, &OriginalRamdisk, 0);
  if (!EFI_ERROR(Status) && OriginalRamdisk) {
    CONST CHAR8 *DecompName;

    // get decompressor
    decompress_fn Decompressor = decompress_method(OriginalRamdisk, AndroidHdr->ramdisk_size, &DecompName);
    if(Decompressor==NULL) {
      goto FreeOriginalRd;
    }

    // get uncompressed size
    // since the Linux decompressor doesn't support predicting the length we hardcode this to 10MB
    RamdiskUncompressedLen = 10*1024*1024;
    DecompressedRamdisk = AllocatePool(RamdiskUncompressedLen);
    if(DecompressedRamdisk==NULL) {
      goto FreeOriginalRd;
    }

    // decompress ramdisk
    if(Decompressor(OriginalRamdisk, AndroidHdr->ramdisk_size, NULL, NULL, (VOID*)DecompressedRamdisk, NULL, DecompError)) {
      goto FreeDecompRd;
    }

    goto FreeOriginalRd;

  FreeDecompRd:
    if(DecompressedRamdisk) {
      FreePool(DecompressedRamdisk);
      DecompressedRamdisk = NULL;
    }

  FreeOriginalRd:
    FreeAlignedMemoryRange((UINT32)OriginalRamdisk, AndroidHdr->ramdisk_size, BlockIo->Media->BlockSize);
  }

FREEBUFFER:
  FreePool(AndroidHdr);

  *DecompressedRamdiskOut = DecompressedRamdisk;
  return Status;
}

EFI_STATUS
AndroidBootFromFile (
  IN EFI_FILE_PROTOCOL  *File,
  IN multiboot_handle_t *mbhandle,
  IN BOOLEAN            DisablePatching,
  IN LAST_BOOT_ENTRY    *LastBootEntry
)
{
  EFI_STATUS                Status;
  EFI_BLOCK_IO_PROTOCOL     *BlockIo;

  Status = FileBlockIoCreate(File, &BlockIo);

  Status = AndroidBootFromBlockIo(BlockIo, mbhandle, DisablePatching, LastBootEntry);
  FileBlockIoFree(BlockIo);

  return Status;
}

EFI_STATUS
AndroidBootFromBuffer (
  IN VOID               *Buffer,
  IN UINTN              Size,
  IN multiboot_handle_t *mbhandle,
  IN BOOLEAN            DisablePatching,
  IN LAST_BOOT_ENTRY    *LastBootEntry
)
{
  EFI_STATUS                Status;
  EFI_BLOCK_IO_PROTOCOL     *BlockIo;

  Status = MemoryBlockIoCreate(Buffer, Size, &BlockIo);

  Status = AndroidBootFromBlockIo(BlockIo, mbhandle, DisablePatching, LastBootEntry);
  MemoryBlockIoFree(BlockIo);

  return Status;
}
