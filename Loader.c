#include "EFIDroidUi.h"

#define SIDELOAD_FILENAME L"Sideload.efi"

typedef VOID (*LINUX_KERNEL)(UINT32 Zero, UINT32 Arch, UINTN ParametersBase);

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
    CONST CHAR8* CmdlineExt = mLKApi->boot_get_cmdline_extension(RecoveryMode);
    if(CmdlineExt) {
      libboot_cmdline_addall(&Context->cmdline, CmdlineExt, 1);
    }
  }

  // [EFI] Forces disk with valid GPT signature but
  // invalid Protective MBR to be treated as GPT. If the
  // primary GPT is corrupted, it enables the backup/alternate
  // GPT to be used instead.
  if(RecoveryMode)
    libboot_cmdline_add(&Context->cmdline, "gpt", NULL, 1);

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
      Len = 3+1+8+1+2+1 + AsciiStrLen(mbhandle->MultibootConfig) + 1;
      DevPathString = AllocatePool(Len);
      AsciiSPrint(DevPathString, Len, "MBR,%08x-%02u,%a", *((UINT32 *) (&(Hd->Signature[0]))), Hd->PartitionNumber, mbhandle->MultibootConfig);
      break;

    case SIGNATURE_TYPE_GUID:
      Len = 3+1+36+1 + AsciiStrLen(mbhandle->MultibootConfig) + 1;
      DevPathString = AllocatePool(Len);
      AsciiSPrint(DevPathString, Len, "GPT,%g,%a", (EFI_GUID *) &(Hd->Signature[0]), mbhandle->MultibootConfig);
      break;

    default:
      return EFI_INVALID_PARAMETER;
    }

    // add to cmdline
    libboot_cmdline_add(&Context->cmdline, "multibootpath", DevPathString, 1);  
    FreePool(DevPathString);

    // apply cmdline overrides
    if(!RecoveryMode && mbhandle->ReplacementCmdline) {
      libboot_cmdline_addall(&Context->cmdline, mbhandle->ReplacementCmdline, 1);
    }
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

  if (SettingBoolGet("boot-force-permissive"))
    libboot_cmdline_add(&Context->cmdline, "androidboot.selinux", "permissive", 1);

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

STATIC boot_intn_t internal_io_fn_blockio_read(boot_io_t* io, void* buf, boot_uintn_t blkoff, boot_uintn_t count) {
    EFI_BLOCK_IO_PROTOCOL* BlockIo = io->pdata;
    EFI_STATUS Status;

    Status = BlockIo->ReadBlocks(BlockIo, BlockIo->Media->MediaId, blkoff, count*BlockIo->Media->BlockSize, buf);
    if(EFI_ERROR(Status))
        return -1;

    return count;
}

INTN libboot_identify_blockio(EFI_BLOCK_IO_PROTOCOL* BlockIo, bootimg_context_t* context) {
    boot_io_t* io = libboot_alloc(sizeof(boot_io_t));
    if(!io) return -1;
    io->read = internal_io_fn_blockio_read;
    io->blksz = BlockIo->Media->BlockSize;
    io->numblocks = BlockIo->Media->LastBlock+1;
    io->pdata = BlockIo;
    io->pdata_is_allocated = 0;

    INTN rc = libboot_identify(io, context);
    if(rc) {
        libboot_free(io);
    }

    return rc;
}

STATIC boot_intn_t internal_io_fn_file_read(boot_io_t* io, void* buf, boot_uintn_t blkoff, boot_uintn_t count) {
    EFI_FILE_PROTOCOL  *File = io->pdata;
    EFI_STATUS         Status;
    UINTN              BufferSize = count*io->blksz;

    Status = FileHandleSetPosition(File, blkoff*io->blksz);
    if (EFI_ERROR (Status)) {
      return .1;
    }

    Status = FileHandleRead(File, &BufferSize, buf);
    if (EFI_ERROR (Status)) {
      return -1;
    }

    return BufferSize;
}

INTN libboot_identify_file(EFI_FILE_PROTOCOL* File, bootimg_context_t* context) {
    EFI_STATUS Status;
    UINT64     FileSize = 0;

    Status = FileHandleGetSize(File, &FileSize);
    if (EFI_ERROR (Status)) {
      return -1;
    }

    boot_io_t* io = libboot_alloc(sizeof(boot_io_t));
    if(!io) return -1;
    io->read = internal_io_fn_file_read;
    io->blksz = 1;
    io->numblocks = FileSize*io->blksz;
    io->pdata = File;
    io->pdata_is_allocated = 0;

    INTN rc = libboot_identify(io, context);
    if(rc) {
        libboot_free(io);
    }

    return rc;
}

STATIC VOID* lkapi_add_custom_atags(VOID *tags) {
  if(mLKApi) return mLKApi->boot_extend_atags(tags);
  return tags;
}

STATIC VOID lkapi_patch_fdt(VOID *fdt) {
  if(mLKApi) mLKApi->boot_extend_fdt(fdt);
}

VOID custom_init_context(bootimg_context_t* context) {
  libboot_init_context(context);
  context->add_custom_atags = lkapi_add_custom_atags;
  context->patch_fdt = lkapi_patch_fdt;
}

STATIC
EFI_STATUS
BootEfiContext (
  IN bootimg_context_t      *context
)
{
  EFI_STATUS                Status;
  EFI_RAM_DISK_PROTOCOL     *RamDiskProtocol;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath;
  EFI_GUID                  DiskGuid = gEfiVirtualDiskGuid;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL   *Volume;
  EFI_FILE_PROTOCOL                 *Root;
  EFI_FILE_PROTOCOL                 *EfiFile;
  VOID* RamDisk = NULL;
  BOOLEAN FreeRamdisk = FALSE;
  UINT64 RamDiskSize = 0;
  CONST CHAR16* EfiFileName = NULL;
  BOOLEAN FreeEfiFileName = FALSE;
  VOID* LoadOptions = NULL;
  UINTN LoadOptionsSize = 0;

  // get ramdisk protocol
  Status = gBS->LocateProtocol (&gEfiRamDiskProtocolGuid, NULL, (VOID **) &RamDiskProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }
  // provided ramdisk
  if (context->ramdisk_data && context->ramdisk_size>0) {
    RamDisk = context->ramdisk_data;
    RamDiskSize = context->ramdisk_size;
    FreeRamdisk = FALSE;
  }

  // kernel only
  // if we have a ramdisk too, the creator must make the ramdisk big enough
  else if(context->kernel_data && context->kernel_size>0) {
    RamDiskSize = MAX(context->kernel_size + context->ramdisk_size + 64*1024, SIZE_1MB);
    RamDisk = AllocatePool(RamDiskSize);
    if (RamDisk==NULL) {
      return EFI_OUT_OF_RESOURCES;
    }
    FreeRamdisk = TRUE;

    // mkfs.vfat
    Status = MakeDosFs(RamDisk, RamDiskSize);
    if (EFI_ERROR (Status)) {
      goto ERROR_FREE_RAMDISK;
    }
  }
  else {
    return EFI_INVALID_PARAMETER;
  }

  // now we have a ramdisk

  // register ramdisk
  Status = RamDiskProtocol->Register((UINTN)RamDisk, RamDiskSize, &DiskGuid, NULL, &DevicePath);
  if (EFI_ERROR (Status)) {
    goto ERROR_FREE_RAMDISK;
  }

  // get handle
  EFI_DEVICE_PATH_PROTOCOL* Protocol = DevicePath;
  EFI_HANDLE FSHandle;
  Status = gBS->LocateDevicePath (
                  &gEfiSimpleFileSystemProtocolGuid,
                  &Protocol,
                  &FSHandle
                  );
  if (EFI_ERROR (Status)) {
    goto ERROR_UNREGISTER_RAMDISK;
  }

  // get the SimpleFilesystem protocol on that handle
  Volume = NULL;
  Status = gBS->HandleProtocol (
                  FSHandle,
                  &gEfiSimpleFileSystemProtocolGuid,
                  (VOID **)&Volume
                  );
  if (EFI_ERROR (Status)) {
    goto ERROR_UNREGISTER_RAMDISK;
  }

  // Open the root directory of the volume
  Root = NULL;
  Status = Volume->OpenVolume (
                     Volume,
                     &Root
                     );
  if (EFI_ERROR (Status) || Root==NULL) {
    goto ERROR_UNREGISTER_RAMDISK;
  }

  // copy optional kernel to ramdisk
  if(context->kernel_data && context->kernel_size>0) {
    // Create EFI file
    EfiFile = NULL;
    Status = Root->Open (
                     Root,
                     &EfiFile,
                     SIDELOAD_FILENAME,
                     EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE|EFI_FILE_MODE_CREATE,
                     0
                     );
    if (EFI_ERROR (Status)) {
      goto ERROR_UNREGISTER_RAMDISK;
    }

    // write kernel to efi file
    UINTN WriteSize = context->kernel_size;
    Status = EfiFile->Write(EfiFile, &WriteSize, (VOID*)context->kernel_data);
    if (EFI_ERROR (Status)) {
      goto ERROR_UNREGISTER_RAMDISK;
    }

    // use internal filename
    EfiFileName = SIDELOAD_FILENAME;
  }

  else if(context->tags_data && context->tags_size>0) {
    // use filename stored in tags
    EfiFileName = Ascii2Unicode((CHAR8*)context->tags_data);
    if(!EfiFileName) goto ERROR_UNREGISTER_RAMDISK;
    PathToUefi((CHAR16*)EfiFileName);
    FreeEfiFileName = TRUE;
  }

  else {
    // no kernel and no ramdisk path
    goto ERROR_UNREGISTER_RAMDISK;
  }

  // build device path
  EFI_DEVICE_PATH_PROTOCOL *LoaderDevicePath;
  LoaderDevicePath = FileDevicePath(FSHandle, EfiFileName);
  if (LoaderDevicePath==NULL) {
    goto ERROR_UNREGISTER_RAMDISK;
  }

  // build arguments
  UINTN CmdlineLen = libboot_cmdline_length(&context->cmdline);
  if (CmdlineLen) {
    // generate ascii cmdline
    CHAR8* Cmdline8 = AllocatePool(CmdlineLen);
    if(!Cmdline8) goto ERROR_UNREGISTER_RAMDISK;
    libboot_cmdline_generate(&context->cmdline, Cmdline8, CmdlineLen);

    // convert to unicode
    CHAR16* Cmdline = Ascii2Unicode(Cmdline8);
    if(!Cmdline8) goto ERROR_UNREGISTER_RAMDISK;
    FreePool(Cmdline8);

    // set load options
    LoadOptions = Cmdline;
    LoadOptionsSize = StrSize(Cmdline);
  }

  else {
    CONST CHAR16* Args = L"";
    LoadOptionsSize = (UINT32)StrSize (Args);
    LoadOptions     = AllocatePool (LoadOptionsSize);
    StrCpy (LoadOptions, Args);
  }

  // shut down menu
  MenuPreBoot();

  // start efi application
  Status = UtilStartEfiApplication (LoaderDevicePath, LoadOptionsSize, LoadOptions);
  if(EFI_ERROR(Status)) {
    CHAR8 Buf[100];
    AsciiSPrint(Buf, 100, "Can't boot: %r", Status);
    MenuShowMessage("Error", Buf);
  }

  // restart menu
  MenuPostBoot();

ERROR_UNREGISTER_RAMDISK:
  // free load options
  if(LoadOptions)
    FreePool(LoadOptions);

  // free filename
  if(FreeEfiFileName)
    FreePool((VOID*)EfiFileName);

  // unregister ramdisk
  Status = RamDiskProtocol->Unregister(DevicePath);
  if (EFI_ERROR (Status)) {
    ASSERT(FALSE);
  }

ERROR_FREE_RAMDISK:
  // free ramdisk memory
  if(RamDisk && FreeRamdisk)
    FreePool(RamDisk);

  return Status;
}

EFI_STATUS
AutoBootContext (
  IN bootimg_context_t      *context,
  IN multiboot_handle_t     *mbhandle,
  IN BOOLEAN                DisablePatching,
  IN LAST_BOOT_ENTRY        *LastBootEntry
)
{
  EFI_STATUS                Status;
  EFI_STATUS                ReturnStatus = EFI_UNSUPPORTED;
  VOID                      *NewRamdisk = NULL;
  UINT32                    RamdiskUncompressedLen = 0;
  BOOLEAN                   RecoveryMode = FALSE;
  CHAR8                     Buf[100];
  INTN                      rc;
  UINT32                    i;
  CHAR8                     **error_stack;

  // load image
  rc = libboot_load(context);

  // libboot returns an error because it can't handle efi files
  // but it still set the correct inner type of the boot image
  // Also, if the kernel_size is 0, android couldn't boot anyway, so try to boot it as a EFI image
  if(context->type==BOOTIMG_TYPE_EFI || context->kernel_size==0) {
    ReturnStatus = BootEfiContext(context);
    goto CLEANUP;
  }

  // libboot returned an error, and this is not a EFI image
  if(rc) goto CLEANUP;

  // update addresses if necessary
  if(mLKApi)
    mLKApi->boot_update_addrs(&context->kernel_addr, &context->ramdisk_addr, &context->tags_addr);

  if(!DisablePatching && context->ramdisk_data) {
    // get decompressor
    CONST CHAR8 *DecompName;
    decompress_fn Decompressor = decompress_method(context->ramdisk_data, context->ramdisk_size, &DecompName);
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
    NewRamdisk = libboot_alloc(RamdiskUncompressedLen);
    if (!NewRamdisk) {
      AsciiSPrint(Buf, sizeof(Buf), "Can't allocate memory for decompressing ramdisk: %r", Status);
      MenuShowMessage("Error", Buf);
      goto CLEANUP;
    }

    // decompress ramdisk
    if(Decompressor(context->ramdisk_data, context->ramdisk_size, NULL, NULL, NewRamdisk, NULL, DecompError)) {
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

    // replace ramdisk data
    libboot_free(context->ramdisk_data);
    context->ramdisk_data = NewRamdisk;
    context->ramdisk_size = ((UINT32)cpiohd)-((UINT32)NewRamdisk);
    NewRamdisk = NULL;
  }

  // patch cmdline
  Status = AndroidPatchCmdline(context, mbhandle, RecoveryMode, DisablePatching);
  if (EFI_ERROR(Status)) {
    AsciiSPrint(Buf, sizeof(Buf), "Can't load cmdline: %r", Status);
    MenuShowMessage("Error", Buf);
    goto CLEANUP;
  }

  // prepare for boot
  rc = libboot_prepare(context);
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
  BootContext(context);

  // WE SHOULD NEVER GET HERE

CLEANUP:
  // print errors
  error_stack = libboot_error_stack_get();
  for(i=0; i<libboot_error_stack_count(); i++)
      MenuShowMessage("Error", error_stack[i]);
  libboot_error_stack_reset();

  // cleanup
  libboot_free(NewRamdisk);

  // unload
  libboot_unload(context);

  return ReturnStatus;
}

EFI_STATUS
AndroidGetDecompressedRamdisk (
  IN bootimg_context_t      *context,
  OUT CPIO_NEWC_HEADER      **DecompressedRamdiskOut
)
{
  EFI_STATUS                Status;
  CONST CHAR8               *DecompName;
  UINT32                    RamdiskUncompressedLen = 0;
  CPIO_NEWC_HEADER          *DecompressedRamdisk = NULL;

  Status = EFI_LOAD_ERROR;

  // load image
  INTN rc = libboot_load_partial(context, LIBBOOT_LOAD_TYPE_RAMDISK, 0);
  if(rc) goto ERROR;

  // check if we have a ramdisk
  if(!context->ramdisk_data) goto ERROR;

  // get decompressor
  decompress_fn Decompressor = decompress_method(context->ramdisk_data, context->ramdisk_size, &DecompName);
  if(Decompressor==NULL) goto ERROR;

  // get uncompressed size
  // since the Linux decompressor doesn't support predicting the length we hardcode this to 10MB
  RamdiskUncompressedLen = 10*1024*1024;
  DecompressedRamdisk = AllocatePool(RamdiskUncompressedLen);
  if(DecompressedRamdisk==NULL) goto ERROR;

  // decompress ramdisk
  if(Decompressor(context->ramdisk_data, context->ramdisk_size, NULL, NULL, (VOID*)DecompressedRamdisk, NULL, DecompError))
    goto ERROR;

  // return data
  *DecompressedRamdiskOut = DecompressedRamdisk;
  Status = EFI_SUCCESS;

  goto CLEANUP;

ERROR:
  if(DecompressedRamdisk)
    FreePool(DecompressedRamdisk);

CLEANUP:
  libboot_unload(context);

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
  EFI_STATUS Status = EFI_UNSUPPORTED;

  // setup context
  bootimg_context_t context;
  custom_init_context(&context);

  // identify
  INTN rc = libboot_identify_file(File, &context);
  if(rc) goto CLEANUP;

  Status = AutoBootContext(&context, mbhandle, DisablePatching, LastBootEntry);

CLEANUP:
  libboot_free_context(&context);

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
  EFI_STATUS Status = EFI_UNSUPPORTED;

  // setup context
  bootimg_context_t context;
  custom_init_context(&context);

  // identify
  INTN rc = libboot_identify_memory(Buffer, Size, &context);
  if(rc) goto CLEANUP;

  Status = AutoBootContext(&context, mbhandle, DisablePatching, LastBootEntry);

CLEANUP:
  libboot_free_context(&context);

  return Status;
}

EFI_STATUS
AndroidBootFromBlockIo (
  IN EFI_BLOCK_IO_PROTOCOL  *BlockIo,
  IN multiboot_handle_t     *mbhandle,
  IN BOOLEAN                DisablePatching,
  IN LAST_BOOT_ENTRY        *LastBootEntry
)
{
  EFI_STATUS Status = EFI_UNSUPPORTED;

  // setup context
  bootimg_context_t context;
  custom_init_context(&context);

  // identify
  INTN rc = libboot_identify_blockio(BlockIo, &context);
  if(rc) goto CLEANUP;

  Status = AutoBootContext(&context, mbhandle, DisablePatching, LastBootEntry);

CLEANUP:
  libboot_free_context(&context);

  return Status;
}
