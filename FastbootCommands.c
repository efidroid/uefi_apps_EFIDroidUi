#include "EFIDroidUi.h"

#define SIDELOAD_FILENAME L"Sideload.efi"

extern UINT64 gRenderTime;
extern UINT64 gSyncTime;
extern UINT64 gRenderFrames;
extern UINT64 gSyncFrames;

STATIC VOID
CommandRebootInternal (
  CONST CHAR16 *Reason
)
{
  FastbootOkay("");
  gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, Reason?StrLen(Reason):0, (CHAR16*)Reason);
}

STATIC VOID
CommandReboot (
  CHAR8 *Arg,
  VOID *Data,
  UINT32 Size
)
{
  CommandRebootInternal(NULL);
}

STATIC VOID
CommandRebootRecovery (
  CHAR8 *Arg,
  VOID *Data,
  UINT32 Size
)
{
  CommandRebootInternal(L"recovery");
}

STATIC VOID
CommandRebootBootloader (
  CHAR8 *Arg,
  VOID *Data,
  UINT32 Size
)
{
  CommandRebootInternal(L"bootloader");
}

STATIC VOID
CommandRebootDownload (
  CHAR8 *Arg,
  VOID *Data,
  UINT32 Size
)
{
  CommandRebootInternal(L"download");
}

STATIC VOID
CommandPowerOff (
  CHAR8 *Arg,
  VOID *Data,
  UINT32 Size
)
{
  FastbootInfo("You have 5s to unplug your USB cable :)");
  FastbootOkay("");
  gBS->Stall(5*1000000);
  gRT->ResetSystem (EfiResetShutdown, EFI_SUCCESS, 0, NULL);
}

STATIC VOID
CommandBoot (
  CHAR8 *Arg,
  VOID *Data,
  UINT32 Size
)
{
  if (gFastbootMBHandle) {
    FastbootInfo("INFO: redirect boot to Multiboot ROM");
  }

  // stop fastboot
  FastbootOkay("");
  FastbootStopNow();

  // check if we need to enable patching
  BOOLEAN DisablePatching = TRUE;
  CHAR8* Var = UtilGetEFIDroidVariable("fastboot-enable-boot-patch");
  if (Var && !AsciiStrCmp(Var, "1")) {
    DisablePatching = FALSE;
    FreePool(Var);
  }

  // force patching for multiboot ROM's
  if (gFastbootMBHandle) {
    DisablePatching = FALSE;
  }

  // boot Android
  LoaderBootFromBuffer(Data, Size, gFastbootMBHandle, DisablePatching, FALSE, NULL);

  // start fastboot
  FastbootInit();
}

#if 0
STATIC EFI_TEXT_STRING mOutputStringOrig = NULL;
STATIC EFI_TEXT_STRING mErrOutputStringOrig = NULL;
STATIC CHAR8 mOutputBuffer[FASTBOOT_COMMAND_MAX_LENGTH];
STATIC UINTN mOutputBufferPos = 0;

STATIC
EFI_STATUS
EFIAPI
OutputStringHook (
  IN EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *This,
  IN CHAR16                          *String
  )
{
  CHAR8* AsciiString = Unicode2Ascii(String);
  if(AsciiString) {
    UINTN i;
    for(i=0; i<AsciiStrLen(AsciiString); i++) {
      CHAR8 c = AsciiString[i];

      mOutputBuffer[mOutputBufferPos++] = c;

      if(mOutputBufferPos==sizeof(mOutputBuffer)-1-4 || c=='\n' || c=='\r') {
        mOutputBuffer[mOutputBufferPos] = 0;
        FastbootInfo(mOutputBuffer);
        mOutputBufferPos = 0;
      }
    }

    FreePool(AsciiString);
  }

  return EFI_SUCCESS;
}

STATIC VOID
CommandShell (
  CHAR8 *Arg,
  VOID *Data,
  UINT32 Size
)
{
  EFI_STATUS Status;
  EFI_STATUS CommandStatus;
  CHAR8 Buffer[59];

  // convert to unicode
  CHAR16* UnicodeCommand = Ascii2Unicode(Arg);
  if(UnicodeCommand==NULL) {
    FastbootFail("Memory error");
    return;
  }

  // initialize console hook
  mOutputBufferPos = 0;
  mOutputStringOrig = gST->ConOut->OutputString;
  mErrOutputStringOrig = gST->StdErr->OutputString;
  gST->ConOut->OutputString = OutputStringHook;
  gST->StdErr->OutputString = OutputStringHook;

  // run shell command
  Status = ShellExecute (&gImageHandle, UnicodeCommand, FALSE, NULL, &CommandStatus);

  // flush output buffer
  if(mOutputBufferPos>0)
      FastbootInfo(mOutputBuffer);

  // restore console
  gST->StdErr->OutputString = mErrOutputStringOrig;
  gST->ConOut->OutputString = mOutputStringOrig;

  // print status
  if(EFI_ERROR(Status)) {
    AsciiSPrint(Buffer, 59, "Error: %r", Status);
    FastbootFail(Buffer);
  }
  else {
    FastbootOkay("");
  }
}
#endif

STATIC VOID
CommandDisplayInfo (
  CHAR8 *Arg,
  VOID *Data,
  UINT32 Size
)
{
  CHAR8                                Buffer[59];
  EFI_GRAPHICS_OUTPUT_PROTOCOL         *Gop;
  EFI_LK_DISPLAY_PROTOCOL              *LKDisplay;
  UINT32                               ModeIndex;
  UINTN                                MaxMode;
  UINTN                                SizeOfInfo;
  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
  EFI_STATUS                           Status;

  AsciiSPrint(Buffer, 59, "render:%llu sync:%llu", gRenderFrames==0?0ULL:gRenderTime/gRenderFrames, gSyncFrames==0?0ULL:gSyncTime/gSyncFrames);
  FastbootInfo(Buffer);

  // get graphics protocol
  Status = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **) &Gop);
  if (EFI_ERROR (Status)) {
    AsciiSPrint(Buffer, 59, "Error: %r", Status);
    FastbootFail(Buffer);
    return;
  }

  MaxMode = Gop->Mode->MaxMode;

  AsciiSPrint(Buffer, 59, "%u mode%a", MaxMode, MaxMode>1?"s":"");
  FastbootInfo(Buffer);

  AsciiSPrint(Buffer, 59, "buffer: 0x%08x-0x%08x",
    (UINTN)Gop->Mode->FrameBufferBase,
    (UINTN)Gop->Mode->FrameBufferBase+Gop->Mode->FrameBufferSize
  );
  FastbootInfo(Buffer);

  // get LKDisplay protocol
  Status = gBS->LocateProtocol (&gEfiLKDisplayProtocolGuid, NULL, (VOID **) &LKDisplay);
  if (!EFI_ERROR (Status)) {
    AsciiSPrint(Buffer, 59, "density: %u", LKDisplay->GetDensity(LKDisplay));
    FastbootInfo(Buffer);

    LK_DISPLAY_FLUSH_MODE FlushMode = LKDisplay->GetFlushMode(LKDisplay);
    if (FlushMode==LK_DISPLAY_FLUSH_MODE_AUTO)
      AsciiSPrint(Buffer, 59, "flush_mode: auto");
    else if (FlushMode==LK_DISPLAY_FLUSH_MODE_MANUAL)
      AsciiSPrint(Buffer, 59, "flush_mode: manual");
    else
      AsciiSPrint(Buffer, 59, "flush_mode: invalid");
    FastbootInfo(Buffer);

    AsciiSPrint(Buffer, 59, "portrait mode: %u", LKDisplay->GetPortraitMode());
    FastbootInfo(Buffer);

    AsciiSPrint(Buffer, 59, "landscape mode: %u", LKDisplay->GetLandscapeMode());
    FastbootInfo(Buffer);
  }

  for (ModeIndex = 0; ModeIndex < MaxMode; ModeIndex++) {
    Status = Gop->QueryMode (
                       Gop,
                       ModeIndex,
                       &SizeOfInfo,
                       &Info
                       );
    if (!EFI_ERROR (Status)) {

      AsciiSPrint(Buffer, 59, "mode %u:%a", ModeIndex, ModeIndex==Gop->Mode->Mode?" (active)":"");
      FastbootInfo(Buffer);

      AsciiSPrint(Buffer, 59, "  res: %ux%u", Info->HorizontalResolution, Info->VerticalResolution);
      FastbootInfo(Buffer);

      AsciiSPrint(Buffer, 59, "  scanline: %upx", Info->PixelsPerScanLine);
      FastbootInfo(Buffer);

      if (Info->PixelFormat==PixelRedGreenBlueReserved8BitPerColor)
        AsciiSPrint(Buffer, 59, "  format: RGB888");
      else if (Info->PixelFormat==PixelBlueGreenRedReserved8BitPerColor)
        AsciiSPrint(Buffer, 59, "  format: BGR888");
      else if (Info->PixelFormat==PixelBitMask) {
        AsciiSPrint(Buffer, 59, "  format: mask %08x/%08x/%08x",
          Info->PixelInformation.RedMask,
          Info->PixelInformation.GreenMask,
          Info->PixelInformation.BlueMask
        );
      }
      else if (Info->PixelFormat==PixelBltOnly)
        AsciiSPrint(Buffer, 59, "  format: unknown");
      else
        AsciiSPrint(Buffer, 59, "  format: invalid");


      FastbootInfo(Buffer);

      FreePool (Info);
    }
  }

  FastbootOkay("");
}

STATIC VOID
CommandExit (
  CHAR8 *Arg,
  VOID *Data,
  UINT32 Size
)
{
  FastbootOkay("");
  FastbootRequestStop();
}

STATIC
VOID
CommandScreenShot (
  CHAR8 *Arg,
  VOID *Data,
  UINT32 Size
)
{
  SCREENSHOT *ScreenShot;
  CHAR8 Response[128];
  UINTN Index;

  // show list of screenshots
  if(Arg[0]==0) {
    FastbootInfo("available screenshots:");

    for (ScreenShot = gScreenShotList,Index=0; ScreenShot; ScreenShot = ScreenShot->Next,Index++) {
      AsciiSPrint(Response, sizeof(Response), "\t%d: %u bytes", Index, ScreenShot->Len);
      FastbootInfo(Response);
    }

    FastbootOkay("");
  }

  else {
    UINT8 ScreenShotIndex = atoi (Arg);
    for (ScreenShot = gScreenShotList,Index=0; ScreenShot; ScreenShot = ScreenShot->Next,Index++) {
      if(Index!=ScreenShotIndex)
        continue;

      FastbootSendBuf(ScreenShot->Data, ScreenShot->Len);
      FastbootOkay("");

      return;
    }

    FastbootFail("screenshot not found");
  }
}

STATIC
RETURN_STATUS
EFIAPI
IterateVariablesCallbackPrint (
  IN  VOID                         *Context,
  IN  CHAR16                       *VariableName,
  IN  EFI_GUID                     *VendorGuid,
  IN  UINT32                       Attributes,
  IN  UINTN                        DataSize,
  IN  VOID                         *Data
  )
{
  EFI_STATUS          Status;
  CONST CHAR16        *Arg;

  Status = EFI_SUCCESS;
  Arg = Context;

  // show our variables only
  if (!CompareGuid(VendorGuid, &gEFIDroidVariableGuid))
    return Status;

  // show specific variable only
  if (Arg && StrCmp(VariableName, Arg))
    return Status;

  UINTN BufSize = StrLen(VariableName) + DataSize + 10 + 1;
  CHAR8 *Buf = AllocatePool(BufSize);
  if (Buf == NULL)
    return EFI_OUT_OF_RESOURCES;

  AsciiSPrint(Buf, BufSize, "%s: %a\n", VariableName, (CONST CHAR8*)Data);
  FastbootSendString(Buf, AsciiStrLen(Buf));

  FreePool(Buf);

  return Status;
}

STATIC
VOID
CommandGetNvVar (
  CHAR8 *Arg,
  VOID *Data,
  UINT32 Size
)
{
  EFI_STATUS Status;
  CHAR8      Buf[100];
  CHAR16     *Arg16;

  Arg16 = NULL;
  if(AsciiStrLen(Arg)>0) {
    Arg16 = Ascii2Unicode(Arg);
    ASSERT(Arg16);
  }

  Status = UtilIterateVariables(IterateVariablesCallbackPrint, Arg16);

  if (Arg16)
    FreePool(Arg16);

  if(EFI_ERROR(Status)) {
    AsciiSPrint(Buf, sizeof(Buf), "%r", Status);
    FastbootFail(Buf);
  }
  else {
    FastbootOkay("");
  }
}

STATIC
VOID
CommandSetNvVar (
  CHAR8 *Arg,
  VOID *Data,
  UINT32 Size
)
{
  EFI_STATUS  Status;
  CHAR8       Buf[100];
  CONST CHAR8 *Name;
  CONST CHAR8 *Value;
  CHAR8       *Ptr;

  Status = EFI_SUCCESS;

  Name = Arg;
  Value = NULL;
  for (Ptr=Arg; *Ptr; Ptr++) {
    if(Ptr[0]==' ') {
      Ptr[0] = '\0';
      Value = &Ptr[1];
      break;
    }
  }

  Status = UtilSetEFIDroidVariable(Name, Value);
  if(EFI_ERROR(Status)) {
    AsciiSPrint(Buf, sizeof(Buf), "%r", Status);
    FastbootFail(Buf);
  }
  else {
    FastbootOkay("");
  }
}

typedef struct {
  CHAR16       *PartitionName;
  VOID         *Data;
  UINTN        DataSize;

  BOOLEAN      Done;
} FLASH_CONTEXT;

STATIC
EFI_STATUS
HandleBlockIoFlash (
  IN EFI_HANDLE  Handle,
  IN VOID        *Instance,
  IN VOID        *VoidContext
  )
{
  EFI_STATUS                        Status;
  EFI_BLOCK_IO_PROTOCOL             *BlockIo = NULL;
  EFI_PARTITION_NAME_PROTOCOL       *PartitionNameProtocol = NULL;
  FLASH_CONTEXT                     *Context;
  CHAR8                             Buf[100];

  Context = VoidContext;
  if(Context->Done)
    return EFI_SUCCESS;

  //
  // Get the BlockIO protocol on that handle
  //
  BlockIo = NULL;
  Status = gBS->HandleProtocol (
                  Handle,
                  &gEfiBlockIoProtocolGuid,
                  (VOID **)&BlockIo
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Get the PartitionName protocol on that handle
  //
  Status = gBS->HandleProtocol (
                  Handle,
                  &gEfiPartitionNameProtocolGuid,
                  (VOID **)&PartitionNameProtocol
                  );
  if (EFI_ERROR (Status) || PartitionNameProtocol->Name[0]==0) {
    return Status;
  }

  if (StrCmp(PartitionNameProtocol->Name, Context->PartitionName)) {
    return EFI_NOT_FOUND;
  }

  Context->Done = TRUE;

  UINTN SizeAligned = ROUNDDOWN(Context->DataSize, BlockIo->Media->BlockSize);
  UINTN SizeLeft = Context->DataSize - SizeAligned;

  if (SizeAligned>0) {
    Status = BlockIo->WriteBlocks(BlockIo, BlockIo->Media->MediaId, 0, SizeAligned, Context->Data);
    if (EFI_ERROR(Status)) {
      AsciiSPrint(Buf, sizeof(Buf), "can't write blocks %r", Status);
      FastbootFail(Buf);
      return Status;
    }
  }

  if (SizeLeft>0) {
    UINTN NumTmpBufPages = ROUNDUP(BlockIo->Media->BlockSize, EFI_PAGE_SIZE)/EFI_PAGE_SIZE;
    VOID *TmpBuf = AllocateAlignedPages(NumTmpBufPages, BlockIo->Media->BlockSize);
    if (TmpBuf == NULL) {
      FastbootFail("can't allocate memory");
      return EFI_OUT_OF_RESOURCES;
    }

    ZeroMem(TmpBuf, BlockIo->Media->BlockSize);
    CopyMem(TmpBuf, Context->Data + SizeAligned, SizeLeft);

    Status = BlockIo->WriteBlocks(BlockIo, BlockIo->Media->MediaId, SizeAligned, BlockIo->Media->BlockSize, TmpBuf);
    FreeAlignedPages(TmpBuf, NumTmpBufPages);
    if (EFI_ERROR(Status)) {
      AsciiSPrint(Buf, sizeof(Buf), "can't write blocks %r", Status);
      FastbootFail(Buf);
      return Status;
    }
  }

  FastbootOkay("");
  return EFI_SUCCESS;
}

VOID
HandleFileFlash (
  EFI_FILE_PROTOCOL *File,
  VOID              *Data,
  UINTN             DataSize
)
{
  UINT64     FileSize = 0;
  EFI_STATUS Status;
  CHAR8      Buf[100];

  // get file size
  Status = FileHandleGetSize(File, &FileSize);
  if (EFI_ERROR (Status)) {
    AsciiSPrint(Buf, sizeof(Buf), "can't get file size: %r", Status);
    FastbootFail(Buf);
    goto Done;
  }

  // validate size
  if (DataSize>FileSize) {
    FastbootFail("data size exceeds partition size");
    goto Done;
  }

  // write data
  UINTN WriteSize = DataSize;
  Status = FileHandleWrite(File, &WriteSize, Data);
  if (EFI_ERROR (Status)) {
    AsciiSPrint(Buf, sizeof(Buf), "can't write: %r", Status);
    FastbootFail(Buf);
    goto Done;
  }

  // validate return value
  if (WriteSize!=DataSize) {
    AsciiSPrint(Buf, sizeof(Buf), "short write: %u/%u bytes written", WriteSize, DataSize);
    FastbootFail(Buf);
    goto Done;
  }

  FastbootOkay("");

Done:
  return;
}

STATIC
VOID
CommandFlash (
  CHAR8 *Arg,
  VOID *Data,
  UINT32 Size
)
{
  FSTAB              *FsTab;
  EFI_FILE_PROTOCOL  *EspDir;
  FLASH_CONTEXT      Context;
  EFI_STATUS         Status;
  CHAR8              Buf[100];

  if (gFastbootMBHandle) {
    // get partition
    CHAR16 *NameUnicode = Ascii2Unicode(Arg);
    if (!NameUnicode) {
      FastbootFail("can't allocate memory");
      return;
    }
    PARTITION_LIST_ITEM *Item = LoaderGetPartitionItem(gFastbootMBHandle, NameUnicode);
    FreePool(NameUnicode);
    if (!Item) {
      FastbootFail("partition not found");
      return;
    }
    if (!Item->IsFile) {
      FastbootFail("partition is not a file");
      return;
    }

    // open File
    EFI_FILE_PROTOCOL *PartitionFile = NULL;
    Status = gFastbootMBHandle->ROMDirectory->Open (
                     gFastbootMBHandle->ROMDirectory,
                     &PartitionFile,
                     Item->Value,
                     EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE,
                     0
                     );
    if (EFI_ERROR(Status)) {
      AsciiSPrint(Buf, sizeof(Buf), "can't open replacement partition: %r", Status);
      FastbootFail(Buf);
      return;
    }

    // flash
    FastbootInfo("INFO: redirect flash to Multiboot ROM");
    HandleFileFlash (PartitionFile, Data, (UINTN)Size);

    // close
    FileHandleClose(PartitionFile);
    return;
  }

  FsTab = AndroidLocatorGetMultibootFsTab ();
  EspDir = AndroidLocatorGetEspDir ();
  if (FsTab && EspDir) {
    // handle the special uefi prefix
    BOOLEAN IsUefiFlash = FALSE;
    if (!AsciiStrnCmp(Arg, "uefi_", 5)) {
      Arg += 5;
      IsUefiFlash = TRUE;
    }

    FSTAB_REC* Rec = FstabGetByPartitionName(FsTab, Arg);
    if(Rec && FstabIsUEFI(Rec)) {
      EFI_FILE_PROTOCOL *PartitionFile = NULL;

      // this is a uefi partition flash
      if (IsUefiFlash) {
        FastbootInfo("INFO: flash to UEFI partition");
        goto DO_BLOCKIO_FLASH;
      }

      // build filename
      UINTN PathBufSize = 100*sizeof(CHAR16);
      CHAR16 *PathBuf = AllocateZeroPool(PathBufSize);
      ASSERT(PathBuf);
      UnicodeSPrint(PathBuf, PathBufSize, L"partition_%a.img", Rec->mount_point+1);

      // open File
      Status = EspDir->Open (
                       EspDir,
                       &PartitionFile,
                       PathBuf,
                       EFI_FILE_MODE_READ|EFI_FILE_MODE_WRITE,
                       0
                       );
      FreePool(PathBuf);
      if (EFI_ERROR(Status)) {
        AsciiSPrint(Buf, sizeof(Buf), "can't open replacement partition: %r", Status);
        FastbootFail(Buf);
        return;
      }

      // flash
      FastbootInfo("INFO: redirect flash to ESP");
      HandleFileFlash (PartitionFile, Data, (UINTN)Size);

      // close
      FileHandleClose(PartitionFile);
      return;
    }
  }

DO_BLOCKIO_FLASH:
  Context.PartitionName = Ascii2Unicode(Arg);
  Context.Data          = Data;
  Context.DataSize      = (UINTN)Size;
  Context.Done          = FALSE;

  // flash
  VisitAllInstancesOfProtocol (
    &gEfiBlockIoProtocolGuid,
    HandleBlockIoFlash,
    &Context
    );

  if(Context.PartitionName)
    FreePool(Context.PartitionName);

  if(!Context.Done)
    FastbootFail("partition not found");
}

STATIC
VOID
CommandErase (
  CHAR8 *Arg,
  VOID *Data,
  UINT32 Size
)
{
  FastbootInfo("WARNING: erase is not supported");
  FastbootOkay("");
}

VOID
FastbootCommandsAdd (
  VOID
)
{
  FastbootRegister("flash:", CommandFlash);
  FastbootRegister("erase:", CommandErase);

  FastbootRegister("reboot", CommandReboot);
  FastbootRegister("reboot-bootloader", CommandRebootBootloader);
  FastbootRegister("oem reboot-recovery", CommandRebootRecovery);
  FastbootRegister("oem reboot-download", CommandRebootDownload);
  FastbootRegister("oem poweroff", CommandPowerOff);
  FastbootRegister("boot", CommandBoot);

  //FastbootRegister("oem shell", CommandShell);
  FastbootRegister("oem displayinfo", CommandDisplayInfo);
  FastbootRegister("oem exit", CommandExit);
  FastbootRegister("oem screenshot", CommandScreenShot);
  FastbootRegister("oem getnvvar", CommandGetNvVar);
  FastbootRegister("oem setnvvar", CommandSetNvVar);
}
