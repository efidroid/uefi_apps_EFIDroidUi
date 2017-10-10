#include "EFIDroidUi.h"

#if defined (MDE_CPU_ARM)
#include <Library/ArmLib.h>
#endif

#define SIDELOAD_FILENAME L"Sideload.efi"

typedef VOID (*LINUX_KERNEL)(UINT32 Zero, UINT32 Arch, UINTN ParametersBase);

#define KERNEL64_HDR_MAGIC 0x644D5241 /* ARM64 */
#define IS_ARM64(ptr) (!!( ((KERNEL64_HDR*)(ptr))->magic_64 == KERNEL64_HDR_MAGIC ))
typedef struct
{
  UINT32 insn;
  UINT32 res1;
  UINT64 text_offset;
  UINT64 res2;
  UINT64 res3;
  UINT64 res4;
  UINT64 res5;
  UINT64 res6;
  UINT32 magic_64;
  UINT32 res7;
} KERNEL64_HDR;

STATIC EFI_STATUS
PatchCmdline (
  bootimg_context_t         *Context,
  IN multiboot_handle_t     *mbhandle,
  IN BOOLEAN                RecoveryMode,
  IN BOOLEAN                DisablePatching
)
{
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
    // apply cmdline overrides
    if(!RecoveryMode && mbhandle->ReplacementCmdline) {
      libboot_cmdline_addall(&Context->cmdline, mbhandle->ReplacementCmdline, 1);
    }
  }

  if(!DisablePatching) {
    libboot_cmdline_add(&Context->cmdline, "rdinit", "/multiboot_init", 1);

    // in recovery mode we ptrace the whole system. that doesn't work well with selinux
    if(RecoveryMode)
      libboot_cmdline_add(&Context->cmdline, "androidboot.selinux", "permissive", 1);
  }

  if (SettingBoolGet("boot-force-permissive"))
    libboot_cmdline_add(&Context->cmdline, "androidboot.selinux", "permissive", 1);

  return EFI_SUCCESS;
}

STATIC EFI_STATUS
GenerateMultibootCmdline (
  libboot_list_node_t       *mbcmdline,
  IN multiboot_handle_t     *mbhandle,
  IN BOOLEAN                DisablePatching
)
{
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
    libboot_cmdline_add(mbcmdline, "multibootpath", DevPathString, 1);
    FreePool(DevPathString);
  }

  CHAR8* DebugValue = UtilGetEFIDroidVariable("multiboot-debuglevel");
  if (DebugValue)
    libboot_cmdline_add(mbcmdline, "multiboot.debug", DebugValue, 1);

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
    mLKApi->boot_exec(IS_ARM64((VOID*)(UINTN)context->kernel_addr), (VOID*)(UINTN)context->kernel_addr, context->kernel_arguments[0], context->kernel_arguments[1], context->kernel_arguments[2]);

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

    return count*BlockIo->Media->BlockSize;
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
      return -1;
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
  EFI_FILE_PROTOCOL                 *Root = NULL;
  EFI_FILE_PROTOCOL                 *EfiFile = NULL;
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
    Status = FileHandleWrite(EfiFile, &WriteSize, (VOID*)context->kernel_data);
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

  FileHandleClose(EfiFile);
  FileHandleClose(Root);

ERROR_FREE_RAMDISK:
  // free ramdisk memory
  if(RamDisk && FreeRamdisk)
    FreePool(RamDisk);

  return Status;
}
#include <zlib.h>
int zlib_compress(void *in, void *out, size_t inlen, size_t outlen)
{
	int err, ret;
    z_stream stream;
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

	ret = -1;
	err = deflateInit2(&stream, Z_BEST_SPEED, Z_DEFLATED, MAX_WBITS|16, 8, Z_DEFAULT_STRATEGY);
	if (err != Z_OK)
		goto error;

	stream.next_in = in;
	stream.avail_in = inlen;
	stream.total_in = 0;
	stream.next_out = out;
	stream.avail_out = outlen;
	stream.total_out = 0;

	err = deflate(&stream, Z_FINISH);
	if (err != Z_STREAM_END)
		goto error;

	err = deflateEnd(&stream);
	if (err != Z_OK)
		goto error;

	if (stream.total_out >= stream.total_in)
		goto error;

	ret = stream.total_out;
error:
	return ret;
}

EFI_STATUS
LoaderBootContext (
  IN bootimg_context_t      *context,
  IN multiboot_handle_t     *mbhandle,
  IN BOOLEAN                DisablePatching,
  IN BOOLEAN                IsRecovery,
  IN LAST_BOOT_ENTRY        *LastBootEntry
)
{
  EFI_STATUS                Status;
  EFI_STATUS                Status2;
  EFI_STATUS                ReturnStatus = EFI_UNSUPPORTED;
  VOID                      *NewRamdisk = NULL;
  UINT32                    RamdiskUncompressedLen = 0;
  CHAR8                     Buf[100];
  INTN                      rc;
  UINT32                    i;
  CHAR8                     **error_stack;
  libboot_list_node_t       mbcmdline;
  BOOLEAN                   Is64BitKernel;
  CONST CHAR8               *MultibootInitUefiRdPath;

  libboot_list_initialize(&mbcmdline);

  // load image
  rc = libboot_load(context);

  // libboot returns an error because it can't handle efi files
  // but it still set the correct inner type of the boot image
  // Also, if the kernel_size is 0, android couldn't boot anyway, so try to boot it as a EFI image
  if(context->type==BOOTIMG_TYPE_EFI || context->kernel_size==0) {
    libboot_error_stack_reset();
    ReturnStatus = BootEfiContext(context);
    goto CLEANUP;
  }

  // libboot returned an error, and this is not a EFI image
  if(rc) goto CLEANUP;

  Is64BitKernel = IS_ARM64(context->kernel_data);
  if (Is64BitKernel) {
    MultibootInitUefiRdPath = "arm64/multiboot_init";
  }
  else {
    MultibootInitUefiRdPath = "arm/multiboot_init";
  }

  // update addresses if necessary
  if(mLKApi)
    mLKApi->boot_update_addrs(Is64BitKernel, &context->kernel_addr, &context->ramdisk_addr, &context->tags_addr);

  if(context->ramdisk_data) {
    // get decompressor
    CONST CHAR8 *DecompName;
    decompress_fn Decompressor = decompress_method(context->ramdisk_data, context->ramdisk_size, &DecompName);
    if(Decompressor==NULL) {
      MenuShowMessage("Error", "Can't find decompressor.");
      goto CLEANUP;
    }

    // get uncompressed size
    // since the Linux decompressor doesn't support predicting the length we hardcode this to 10MB
    RamdiskUncompressedLen = 50*1024*1024;

    // get multiboot_init from UEFIRamdisk
    UINT8 *MultibootBin;
    UINTN MultibootSize;
    Status = UEFIRamdiskGetFile (MultibootInitUefiRdPath, (VOID **) &MultibootBin, &MultibootSize);
    if (EFI_ERROR (Status)) {
      MenuShowMessage("Error", "Multiboot binary not found.");
      goto CLEANUP;
    }

    // get fstab.multiboot from UEFIRamdisk
    UINT8 *FstabBin;
    UINTN FstabSize;
    Status = UEFIRamdiskGetFile ("fstab.multiboot", (VOID **) &FstabBin, &FstabSize);
    if (EFI_ERROR (Status)) {
      MenuShowMessage("Error", "fstab.multiboot not found.");
      goto CLEANUP;
    }

    // generate multiboot cmdline
    Status = GenerateMultibootCmdline(&mbcmdline, mbhandle, DisablePatching);
    if (EFI_ERROR(Status)) {
      AsciiSPrint(Buf, sizeof(Buf), "Can't generate multiboot_cmdline: %r", Status);
      MenuShowMessage("Error", Buf);
      goto CLEANUP;
    }

    // add multiboot binary size to uncompressed ramdisk size
    CONST CHAR8 *cpio_name_mbinit = "/multiboot_init";
    RamdiskUncompressedLen += CpioPredictObjSize(AsciiStrLen(cpio_name_mbinit), MultibootSize);

    // add fstab file size to uncompressed ramdisk size
    CONST CHAR8 *cpio_name_mbfstab = "/multiboot_fstab";
    RamdiskUncompressedLen += CpioPredictObjSize(AsciiStrLen(cpio_name_mbfstab), FstabSize);

    // add cmdline file size to uncompressed ramdisk size
    CONST CHAR8 *cpio_name_mbcmdline = "/multiboot_cmdline";
    UINTN mbcmdline_len = libboot_cmdline_length(&mbcmdline);
    RamdiskUncompressedLen += CpioPredictObjSize(AsciiStrLen(cpio_name_mbcmdline), mbcmdline_len);

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

    // get CPIO ramdisk
    CPIO_NEWC_HEADER *cpiohd = (CPIO_NEWC_HEADER *) NewRamdisk;

    // check if this is a merged ramdisk
    CPIO_NEWC_HEADER* DualRamdiskAndroidHdr = CpioGetByName(cpiohd, "sbin/ramdisk.cpio");
    CPIO_NEWC_HEADER* DualRamdiskRecoveryHdr = CpioGetByName(cpiohd, "sbin/ramdisk-recovery.cpio");
    if (DualRamdiskAndroidHdr && DualRamdiskRecoveryHdr) {
      // get android ramdisk
      CPIO_NEWC_HEADER* DualRamdiskAndroid;
      UINTN DualRamdiskAndroidSize;
      Status = CpioGetData(DualRamdiskAndroidHdr, (VOID**)&DualRamdiskAndroid, &DualRamdiskAndroidSize);

      // get recovery ramdisk
      CPIO_NEWC_HEADER* DualRamdiskRecovery;
      UINTN DualRamdiskRecoverySize;
      Status2 = CpioGetData(DualRamdiskRecoveryHdr, (VOID**)&DualRamdiskRecovery, &DualRamdiskRecoverySize);

      if(!EFI_ERROR(Status) && !EFI_ERROR(Status2)) {
        if(IsRecovery)
          cpiohd = DualRamdiskRecovery;
        else
          cpiohd = DualRamdiskAndroid;

        NewRamdisk = cpiohd;
      }
    }

    // skip to last CPIO object
    cpiohd = CpioGetLast (cpiohd);

    // add multiboot files
    if(!DisablePatching) {
      // multiboot_init
      cpiohd = CpioCreateObj (cpiohd, cpio_name_mbinit, MultibootBin, MultibootSize, CPIO_MODE_REG|0700);

      // multiboot fstab
      cpiohd = CpioCreateObj (cpiohd, cpio_name_mbfstab, FstabBin, FstabSize, CPIO_MODE_REG|0400);

      // multiboot_cmdline
      CPIO_NEWC_HEADER *cpiohd_mbcmdline = cpiohd;
      cpiohd = CpioCreateObj (cpiohd, cpio_name_mbcmdline, NULL, mbcmdline_len, CPIO_MODE_REG|0444);

      // write cmdline data
      VOID *cpio_mbcmdline_data;
      Status = CpioGetData(cpiohd_mbcmdline, &cpio_mbcmdline_data, NULL);
      if (EFI_ERROR(Status)) {
        AsciiSPrint(Buf, sizeof(Buf), "Can't load cmdline: %r", Status);
        MenuShowMessage("Error", Buf);
        goto CLEANUP;
      }
      libboot_cmdline_generate(&mbcmdline, cpio_mbcmdline_data, mbcmdline_len);

      // cpio trailer
      cpiohd = CpioCreateObj (cpiohd, CPIO_TRAILER, NULL, 0, 0);
    }

    // verify that we didn't overflow the buffer
    ASSERT((UINT32)cpiohd <= ((UINT32)NewRamdisk)+RamdiskUncompressedLen);

    // check if this is a recovery ramdisk
    if (CpioGetByName((CPIO_NEWC_HEADER *)NewRamdisk, "sbin/recovery")) {
      IsRecovery = TRUE;
    }

    void *out = libboot_alloc(RamdiskUncompressedLen);
    rc = zlib_compress(NewRamdisk, out, ((UINT32)cpiohd)-((UINT32)NewRamdisk), RamdiskUncompressedLen);
    if (rc<0) {
      AsciiSPrint(Buf, sizeof(Buf), "Can't compress ramdisk: %d", rc);
      MenuShowMessage("Error", Buf);
      goto CLEANUP;
    }
    libboot_free(NewRamdisk);

    // replace ramdisk data
    libboot_free(context->ramdisk_data);
    context->ramdisk_data = out;
    context->ramdisk_size = rc;
    NewRamdisk = NULL;
  }

  // patch cmdline
  Status = PatchCmdline(context, mbhandle, IsRecovery, DisablePatching);
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
LoaderGetDecompressedRamdisk (
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
  RamdiskUncompressedLen = 50*1024*1024;
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
LoaderBootFromFile (
  IN EFI_FILE_PROTOCOL  *File,
  IN multiboot_handle_t *mbhandle,
  IN BOOLEAN            DisablePatching,
  IN BOOLEAN            IsRecovery,
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

  Status = LoaderBootContext(&context, mbhandle, DisablePatching, IsRecovery, LastBootEntry);

CLEANUP:
  libboot_free_context(&context);

  return Status;
}

EFI_STATUS
LoaderBootFromBuffer (
  IN VOID               *Buffer,
  IN UINTN              Size,
  IN multiboot_handle_t *mbhandle,
  IN BOOLEAN            DisablePatching,
  IN BOOLEAN            IsRecovery,
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

  Status = LoaderBootContext(&context, mbhandle, DisablePatching, IsRecovery, LastBootEntry);

CLEANUP:
  libboot_free_context(&context);

  return Status;
}

EFI_STATUS
LoaderBootFromBlockIo (
  IN EFI_BLOCK_IO_PROTOCOL  *BlockIo,
  IN multiboot_handle_t     *mbhandle,
  IN BOOLEAN                DisablePatching,
  IN BOOLEAN                IsRecovery,
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

  Status = LoaderBootContext(&context, mbhandle, DisablePatching, IsRecovery, LastBootEntry);

CLEANUP:
  libboot_free_context(&context);

  return Status;
}

VOID
LoaderAddPartitionItem (
  multiboot_handle_t     *mbhandle,
  CONST CHAR8            *Name,
  CONST CHAR8            *Value
)
{
  if (!Name || !Value)
    return;

  // allocate menu
  PARTITION_LIST_ITEM *Item = AllocateZeroPool (sizeof(PARTITION_LIST_ITEM));
  if (Item==NULL)
    return;

  Item->Signature = PARTITION_LIST_SIGNATURE;
  Item->Name = Ascii2Unicode(Name);
  Item->Value = Ascii2Unicode(Value);
  UINTN ValueLen = StrLen(Item->Value);
  if (ValueLen>=4) {
    CONST CHAR16 *Extension = Item->Value+ValueLen-4;

    if (!StrCmp(Extension, L".img"))
      Item->IsFile = TRUE;
  }

  InsertTailList (&mbhandle->Partitions, &Item->Link);
}

PARTITION_LIST_ITEM*
LoaderGetPartitionItem (
  multiboot_handle_t     *mbhandle,
  CONST CHAR16           *Name
)
{
  LIST_ENTRY* Link;
  PARTITION_LIST_ITEM* Entry;
  for (Link = GetFirstNode (&mbhandle->Partitions);
       !IsNull (&mbhandle->Partitions, Link);
       Link = GetNextNode (&mbhandle->Partitions, Link)
      ) {
    Entry = CR (Link, PARTITION_LIST_ITEM, Link, PARTITION_LIST_SIGNATURE);

    if (!StrCmp(Entry->Name, Name))
      return Entry;
  }

  return NULL;
}

VOID
LoaderFreePartitionItems (
  multiboot_handle_t     *mbhandle
)
{
  LIST_ENTRY* Link;
  PARTITION_LIST_ITEM* Entry;

  while(!IsListEmpty(&mbhandle->Partitions)) {
    Link = GetFirstNode (&mbhandle->Partitions);
    Entry = CR (Link, PARTITION_LIST_ITEM, Link, PARTITION_LIST_SIGNATURE);

    RemoveEntryList(Link);
    FreePool(Entry->Name);
    FreePool(Entry->Value);
    FreePool(Entry);
  }
}
