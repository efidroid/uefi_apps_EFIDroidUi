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
#define ROUNDUP(a, b)   (((a) + ((b)-1)) & ~((b)-1))
#define ROUNDDOWN(a, b) ((a) & ~((b)-1))

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
  android_parsed_bootimg_t* Parsed
)
{
  boot_img_hdr_t* Hdr = Parsed->Hdr;

  // terminate cmdlines
  Hdr->cmdline[BOOT_ARGS_SIZE-1] = 0;
  Hdr->extra_cmdline[BOOT_EXTRA_ARGS_SIZE-1] = 0;

  // create cmdline
  UINTN len_cmdline = AsciiStrLen(Hdr->cmdline);
  UINTN len_cmdline_extra = AsciiStrLen(Hdr->extra_cmdline);
  Parsed->Cmdline = AllocatePool(len_cmdline + len_cmdline_extra + 1);
  if (Parsed->Cmdline == NULL)
    return EFI_OUT_OF_RESOURCES;

  AsciiStrnCpy(Parsed->Cmdline, Hdr->cmdline, len_cmdline+1);
  AsciiStrnCpy(Parsed->Cmdline + len_cmdline, Hdr->extra_cmdline, len_cmdline_extra+1);
  Parsed->Cmdline[len_cmdline + len_cmdline_extra] = 0;

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

  Status = gBS->AllocatePages (AllocateAddress, EfiBootServicesData, EFI_SIZE_TO_PAGES(AlignedSize), &AllocationAddress);
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

EFI_STATUS
AndroidBootFromBlockIo (
  IN VOID *Private
)
{
  EFI_STATUS                Status;
  EFI_BLOCK_IO_PROTOCOL     *BlockIo = Private;
  UINTN                     BufferSize;
  boot_img_hdr_t            *AndroidHdr;
  android_parsed_bootimg_t  Parsed;
  LINUX_KERNEL              LinuxKernel;
  UINTN                     TagsSize;
  lkapi_t                   *LKApi = GetLKApi();

  // initialize parsed data
  SetMem(&Parsed, sizeof(Parsed), 0);

  // allocate a buffer for the android header aligned on the block size
  BufferSize = ALIGN_VALUE(sizeof(boot_img_hdr_t), BlockIo->Media->BlockSize);
  AndroidHdr = AllocatePool(BufferSize);
  if (AndroidHdr == NULL)
    return EFI_OUT_OF_RESOURCES;

  // read and verify the android header
  BlockIo->ReadBlocks(BlockIo, BlockIo->Media->MediaId, 0, BufferSize, AndroidHdr);
  Status = AndroidVerify(AndroidHdr);
  if (EFI_ERROR(Status))
    goto FREEBUFFER;
  Parsed.Hdr = AndroidHdr;

  // this is not supported
  // actually I've never seen a device using this so it's not even clear how this would work
  if (AndroidHdr->second_size > 0) {
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

  // load images
  Status = AndroidLoadImage(BlockIo, off_kernel, AndroidHdr->kernel_size, &Parsed.Kernel, AndroidHdr->kernel_addr);
  if (EFI_ERROR(Status))
    goto FREEBUFFER;
  Status = AndroidLoadImage(BlockIo, off_ramdisk, AndroidHdr->ramdisk_size, &Parsed.Ramdisk, AndroidHdr->ramdisk_addr);
  if (EFI_ERROR(Status))
    goto FREEBUFFER;

  // compute tag size
  TagsSize = AndroidHdr->dt_size;
  if (AndroidHdr->dt_size==0) {
    TagsSize = ATAG_MAX_SIZE;
    off_tags = 0;
  }

  // load tags
  Status = AndroidLoadImage(BlockIo, off_tags, TagsSize, &Parsed.Tags, AndroidHdr->tags_addr);
  if (EFI_ERROR(Status))
    goto FREEBUFFER;

  // load cmdline
  AndroidLoadCmdline(&Parsed);

  // generate Atags
  if(LKApi->boot_create_tags(Parsed.Cmdline, AndroidHdr->ramdisk_addr, AndroidHdr->ramdisk_size, AndroidHdr->tags_addr, TagsSize))
    goto FREEBUFFER;

  // Shut down UEFI boot services. ExitBootServices() will notify every driver that created an event on
  // ExitBootServices event. Example the Interrupt DXE driver will disable the interrupts on this event.
  Status = ShutdownUefiBootServices ();
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "ERROR: Can not shutdown UEFI boot services. Status=0x%X\n", Status));
    return Status;
  }

  //
  // Switch off interrupts, caches, mmu, etc
  //
  PreparePlatformHardware ();

  // Outside BootServices, so can't use Print();
  DEBUG ((EFI_D_ERROR, "\nStarting the kernel:\n\n"));

  LinuxKernel = (LINUX_KERNEL)(UINTN)Parsed.Kernel;
  // Jump to kernel with register set
  LinuxKernel ((UINTN)0, LKApi->boot_machine_type(), (UINTN)AndroidHdr->tags_addr);

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
    FreeAlignedMemoryRange((UINT32)Parsed.Ramdisk, AndroidHdr->ramdisk_size);
  if(Parsed.Tags)
    FreeAlignedMemoryRange((UINT32)Parsed.Tags, AndroidHdr->dt_size);

  FreePool(AndroidHdr);

  return EFI_SUCCESS;
}
