#include "EFIDroidUi.h"

typedef VOID (*LINUX_KERNEL)(UINT32 Zero, UINT32 Arch, UINTN ParametersBase);

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
AndroidPatchCmdline (
  bootimg_context_t         *Context,
  IN multiboot_handle_t     *mbhandle,
  IN BOOLEAN                RecoveryMode,
  IN BOOLEAN                DisablePatching
)
{
  EFI_DEVICE_PATH_PROTOCOL  *DevPath;
  CHAR8 *DevPathString = NULL;
  UINTN Len;

  if(mLKApi) {
    CONST CHAR8* CmdlineExt = mLKApi->boot_get_cmdline_extension();
    if(CmdlineExt) {
      CHAR8* CmdlineExtCopy = AllocateCopyPool(AsciiStrSize(CmdlineExt), CmdlineExt);
      libboot_cmdline_addall(&Context->cmdline, CmdlineExtCopy, 1);
      FreePool(CmdlineExtCopy);
    }
  }

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

    // add to cmdline
    libboot_cmdline_add(&Context->cmdline, "multibootpath", DevPathString, 1);  
    FreePool(DevPathString);  
  }

  if(!DisablePatching) {
    libboot_cmdline_add(&Context->cmdline, "rdinit", "/init.multiboot", 1);

    // in recovery mode we ptrace the whole system. that doesn't work well with selinux
    if(RecoveryMode)
      libboot_cmdline_add(&Context->cmdline, "androidboot.selinux", "permissive", 1);
  }

  CHAR8* DebugValue = UtilGetEFIDroidVariable("multiboot-debuglevel");
  if (DebugValue)
    libboot_cmdline_add(&Context->cmdline, "multiboot.debug", DebugValue, 1);

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

STATIC
VOID
BootContext (
  bootimg_context_t* context
)
{
  EFI_STATUS Status;

  // Shut down UEFI boot services. ExitBootServices() will notify every driver that created an event on
  // ExitBootServices event. Example the Interrupt DXE driver will disable the interrupts on this event.
  Status = UtilShutdownUefiBootServices ();
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "ERROR: Can not shutdown UEFI boot services. Status=0x%X\n", Status));
    return;
  }

  //
  // Switch off interrupts, caches, mmu, etc
  //
  PreparePlatformHardware ();

  if(mLKApi)
    mLKApi->boot_exec((VOID*)(UINTN)context->kernel_addr, context->kernel_arguments[0], context->kernel_arguments[1], context->kernel_arguments[2]);

  LINUX_KERNEL LinuxKernel = (LINUX_KERNEL)(UINTN)context->kernel_addr;
  LinuxKernel (context->kernel_arguments[0], context->kernel_arguments[1], context->kernel_arguments[2]);

  // Kernel should never exit
  // After Life services are not provided
  ASSERT (FALSE);
  // We cannot recover the execution at this stage
  while (1);
}

STATIC VOID DecompError(CHAR8* Str) {
  MenuShowMessage("Decompression Error", Str);
}

static boot_intn_t internal_io_fn_blockio_read(boot_io_t* io, void* buf, boot_uintn_t blkoff, boot_uintn_t count) {
    EFI_BLOCK_IO_PROTOCOL* BlockIo = io->pdata;
    EFI_STATUS Status;

    Status = BlockIo->ReadBlocks(BlockIo, BlockIo->Media->MediaId, blkoff, count*BlockIo->Media->BlockSize, buf);
    if(EFI_ERROR(Status))
        return -1;

    return count;
}

static int libboot_identify_blockio(EFI_BLOCK_IO_PROTOCOL* BlockIo, bootimg_context_t* context) {
    boot_io_t* io = libboot_platform_alloc(sizeof(boot_io_t));
    if(!io) return -1;
    io->read = internal_io_fn_blockio_read;
    io->blksz = BlockIo->Media->BlockSize;
    io->numblocks = BlockIo->Media->LastBlock+1;
    io->pdata = BlockIo;
    io->pdata_is_allocated = 0;

    int rc = libboot_identify(io, context);
    if(rc) {
        libboot_platform_free(io);
    }

    return rc;
}

static void* lkapi_add_custom_atags(void *tags) {
  if(mLKApi) return mLKApi->boot_extend_atags(tags);
  return tags;
}

static void lkapi_patch_fdt(void *fdt) {
  if(mLKApi) mLKApi->boot_extend_fdt(fdt);
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
  VOID                      *NewRamdisk = NULL;
  UINT32                    RamdiskUncompressedLen = 0;
  BOOLEAN                   RecoveryMode = FALSE;
  CHAR8                     Buf[100];
  UINT32                    i;
  CHAR8                     **error_stack;

  // setup context
  bootimg_context_t context;
  libboot_init_context(&context);
  context.add_custom_atags = lkapi_add_custom_atags;
  context.patch_fdt = lkapi_patch_fdt;

  // identify
  int rc = libboot_identify_blockio(BlockIo, &context);
  if(rc) goto CLEANUP;

  // load image
  rc = libboot_load(&context);
  if(rc) goto CLEANUP;

  // update addresses if necessary
  if(mLKApi)
    mLKApi->boot_update_addrs(&context.kernel_addr, &context.ramdisk_addr, &context.tags_addr);

  if(!DisablePatching) {
    // get decompressor
    CONST CHAR8 *DecompName;
    decompress_fn Decompressor = decompress_method(context.ramdisk_data, context.ramdisk_size, &DecompName);
    if(Decompressor==NULL) {
      MenuShowMessage("Error", "Can't find decompressor.");
      goto CLEANUP;
    }

    // get uncompressed size
    // since the Linux decompressor doesn't support predicting the length we hardcode this to 10MB
    RamdiskUncompressedLen = 10*1024*1024;

    // get init.multiboot from UEFIRamdisk
    UINT8 *MultibootBin;
    UINTN MultibootSize;
    Status = UEFIRamdiskGetFile ("init.multiboot", (VOID **) &MultibootBin, &MultibootSize);
    if (EFI_ERROR (Status)) {
      MenuShowMessage("Error", "Multiboot binary not found.");
      goto CLEANUP;
    }

    // add multiboot binary size to uncompressed ramdisk size
    CONST CHAR8 *cpio_name_mbinit = "/init.multiboot";
    UINTN objsize = CpioPredictObjSize(AsciiStrLen(cpio_name_mbinit), MultibootSize);
    RamdiskUncompressedLen += objsize;

    // allocate uncompressed ramdisk memory
    NewRamdisk = libboot_platform_bigalloc(RamdiskUncompressedLen);
    if (!NewRamdisk) {
      AsciiSPrint(Buf, sizeof(Buf), "Can't allocate memory for decompressing ramdisk: %r", Status);
      MenuShowMessage("Error", Buf);
      goto CLEANUP;
    }

    // decompress ramdisk
    if(Decompressor(context.ramdisk_data, context.ramdisk_size, NULL, NULL, NewRamdisk, NULL, DecompError)) {
      goto CLEANUP;
    }

    // add multiboot binary
    CPIO_NEWC_HEADER *cpiohd = (CPIO_NEWC_HEADER *) NewRamdisk;
    cpiohd = CpioGetLast (cpiohd);
    cpiohd = CpioCreateObj (cpiohd, cpio_name_mbinit, MultibootBin, MultibootSize);
    cpiohd = CpioCreateObj (cpiohd, CPIO_TRAILER, NULL, 0);
    ASSERT((UINT32)cpiohd <= ((UINT32)NewRamdisk)+RamdiskUncompressedLen);

    // check if this is a recovery ramdisk
    if (CpioGetByName((CPIO_NEWC_HEADER *)NewRamdisk, "sbin/recovery")) {
      RecoveryMode = TRUE;
    }

    // free old data
    libboot_platform_bigfree(context.ramdisk_data);

    // set new data
    context.ramdisk_data = NewRamdisk;
    context.ramdisk_size = ((UINT32)cpiohd)-((UINT32)NewRamdisk);
    NewRamdisk = NULL;
  }

  // patch cmdline
  Status = AndroidPatchCmdline(&context, mbhandle, RecoveryMode, DisablePatching);
  if (EFI_ERROR(Status)) {
    AsciiSPrint(Buf, sizeof(Buf), "Can't load cmdline: %r", Status);
    MenuShowMessage("Error", Buf);
    goto CLEANUP;
  }

  // prepare for boot
  rc = libboot_prepare(&context);
  if(rc) goto CLEANUP;

  // set LastBootEntry variable
  Status = UtilSetEFIDroidDataVariable(L"LastBootEntry", LastBootEntry, LastBootEntry?sizeof(*LastBootEntry):0);
  if (EFI_ERROR (Status)) {
    if (!(LastBootEntry==NULL && Status==EFI_NOT_FOUND)) {
      AsciiSPrint(Buf, sizeof(Buf), "Can't set variable 'LastBootEntry': %r", Status);
      MenuShowMessage("Error", Buf);
      goto CLEANUP;
    }
  }

  // BOOT
  BootContext(&context);

  // WE SHOULD NEVER GET HERE

CLEANUP:
  // print errors
  error_stack = libboot_error_stack_get();
  for(i=0; i<libboot_error_stack_count(); i++)
      MenuShowMessage("Error", error_stack[i]);
  libboot_error_stack_reset();

  // cleanup
  libboot_platform_bigfree(NewRamdisk);
  libboot_free_context(&context);

  return EFI_SUCCESS;
}

EFI_STATUS
AndroidGetDecompRamdiskFromBlockIo (
  IN EFI_BLOCK_IO_PROTOCOL  *BlockIo,
  OUT CPIO_NEWC_HEADER      **DecompressedRamdiskOut
)
{
  EFI_STATUS                Status;
  CONST CHAR8               *DecompName;
  UINT32                    RamdiskUncompressedLen = 0;
  CPIO_NEWC_HEADER          *DecompressedRamdisk = NULL;

  Status = EFI_LOAD_ERROR;

  // setup context
  bootimg_context_t context;
  libboot_init_context(&context);

  // identify
  int rc = libboot_identify_blockio(BlockIo, &context);
  if(rc) goto ERROR;

  // load image
  rc = libboot_load(&context);
  if(rc) goto ERROR;

  // check if we have a ramdisk
  if(!context.ramdisk_data) goto ERROR;

  // get decompressor
  decompress_fn Decompressor = decompress_method(context.ramdisk_data, context.ramdisk_size, &DecompName);
  if(Decompressor==NULL) goto ERROR;

  // get uncompressed size
  // since the Linux decompressor doesn't support predicting the length we hardcode this to 10MB
  RamdiskUncompressedLen = 10*1024*1024;
  DecompressedRamdisk = AllocatePool(RamdiskUncompressedLen);
  if(DecompressedRamdisk==NULL) goto ERROR;

  // decompress ramdisk
  if(Decompressor(context.ramdisk_data, context.ramdisk_size, NULL, NULL, (VOID*)DecompressedRamdisk, NULL, DecompError))
    goto ERROR;

  // return data
  *DecompressedRamdiskOut = DecompressedRamdisk;
  Status = EFI_SUCCESS;

  goto CLEANUP;

ERROR:
  FreePool(DecompressedRamdisk);

CLEANUP:
  // cleanup
  libboot_free_context(&context);

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
