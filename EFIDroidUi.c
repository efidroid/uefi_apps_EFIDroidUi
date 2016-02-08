/** @file
*
*  Copyright (c) 2011-2015, ARM Limited. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
**/

#include <Library/UefiApplicationEntryPoint.h>
#include <Library/BaseMemoryLib.h>
#include <Library/Fstab.h>

#include <Protocol/DevicePathFromText.h>

#include "EFIDroidUi.h"
#include "bootimg.h"

MENU_OPTION                 *mBootMenuMain = NULL;
MENU_OPTION                 *mPowerMenu = NULL;
LIST_ENTRY                  mRecoveries;
FSTAB                       *mFstab = NULL;

// ESP
CHAR16                      *mEspPartitionName = NULL;
CHAR16                      *mEspPartitionPath = NULL;
EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *mEspVolume = NULL;
EFI_FILE_PROTOCOL               *mEspDir    = NULL;

VOID
MenuBootEntryFreeCallback (
  MENU_ENTRY* Entry
)
{
  MENU_ENTRY_PDATA *PData = Entry->Private;

  if(PData==NULL)
    return;

  FreePool(PData);
}

EFI_STATUS
MenuBootEntryCloneCallback (
  MENU_ENTRY* BaseEntry,
  MENU_ENTRY* Entry
)
{
  MENU_ENTRY_PDATA *PDataBase = BaseEntry->Private;
  MENU_ENTRY_PDATA *PData = Entry->Private;

  if(PDataBase==NULL)
    return EFI_SUCCESS;

  PData = AllocateCopyPool (sizeof (*PData), PDataBase);
  if (PData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  Entry->Private = PData;

  return EFI_SUCCESS;
}

EFI_STATUS
CallbackBootAndroid (
  IN MENU_ENTRY* This
)
{
  MENU_ENTRY_PDATA *PData = This->Private;

  return AndroidBootFromBlockIo(PData->BlockIo, PData->mbhandle, PData->DisablePatching);
}

MENU_ENTRY*
MenuCreateBootEntry (
  VOID
)
{
  MENU_ENTRY *MenuEntry;
  MENU_ENTRY_PDATA *PData;

  MenuEntry = MenuCreateEntry();
  if (MenuEntry == NULL) {
    return NULL;
  }

  PData = AllocateZeroPool (sizeof (*PData));
  if (PData == NULL) {
    MenuFreeEntry (MenuEntry);
    return NULL;
  }

  MenuEntry->Private = PData;
  MenuEntry->Callback = CallbackBootAndroid;
  MenuEntry->FreeCallback = MenuBootEntryFreeCallback;
  MenuEntry->CloneCallback = MenuBootEntryCloneCallback;

  return MenuEntry;
}

EFI_STATUS
RecoveryCallback (
  IN MENU_ENTRY* This
)
{
  RECOVERY_MENU *Menu = This->Private;
  Menu->SubMenu->Selection = 0;
  SetActiveMenu(Menu->SubMenu);
  return EFI_SUCCESS;
}

EFI_STATUS
RecoveryLongPressCallback (
  IN MENU_ENTRY* This
)
{
  RECOVERY_MENU *Menu = This->Private;

  INT32 Selection = MenuShowDialog("Unpatched boot", "Do you want to boot without any ramdisk patching?", "OK", "CANCEL");
  if(Selection==0) {
    RenderBootScreen(Menu->NoPatchEntry);
    return Menu->NoPatchEntry->Callback(Menu->NoPatchEntry);
  }
  return EFI_SUCCESS;
}

EFI_STATUS
RecoveryBackCallback (
  MENU_OPTION* This
)
{
  SetActiveMenu(mBootMenuMain);
  return EFI_UNSUPPORTED;
}

RECOVERY_MENU*
CreateRecoveryMenu (
  VOID
)
{
  RECOVERY_MENU *Menu;

  // allocate menu
  Menu = AllocateZeroPool (sizeof(*Menu));
  if(Menu==NULL)
    return NULL;

  Menu->Signature = RECOVERY_MENU_SIGNATURE;
  Menu->SubMenu = MenuCreate();
  Menu->SubMenu->BackCallback = RecoveryBackCallback;

  MENU_ENTRY* Entry = MenuCreateEntry();
  Entry->Callback = RecoveryCallback;
  Entry->LongPressCallback = RecoveryLongPressCallback;
  Entry->HideBootMessage = TRUE;
  Entry->Private = Menu;
  Menu->RootEntry = Entry;

  InsertTailList (&mRecoveries, &Menu->Link);

  return Menu;
}

VOID
AddMultibootSystemToRecoveryMenu (
  multiboot_handle_t* mbhandle
)
{
  LIST_ENTRY* Link;
  RECOVERY_MENU* RecEntry;

  for (Link = GetFirstNode (&mRecoveries);
       !IsNull (&mRecoveries, Link);
       Link = GetNextNode (&mRecoveries, Link)
      ) {
    RecEntry = CR (Link, RECOVERY_MENU, Link, RECOVERY_MENU_SIGNATURE);

    MENU_ENTRY* NewEntry = MenuCloneEntry(RecEntry->BaseEntry);
    if(NewEntry==NULL)
      continue;

    MENU_ENTRY_PDATA* NewEntryPData = NewEntry->Private;
    NewEntryPData->mbhandle = mbhandle;
    if(NewEntry->Name)
      FreePool(NewEntry->Name);
    NewEntry->Name = AsciiStrDup(mbhandle->Name);
    if(mbhandle->Description)
      NewEntry->Description = AsciiStrDup(mbhandle->Description);

    MenuAddEntry(RecEntry->SubMenu, NewEntry);
  }
}

STATIC EFI_STATUS
EFIAPI
FindESP (
  IN EFI_HANDLE  Handle,
  IN VOID        *Instance,
  IN VOID        *Context
  )
{
  EFI_STATUS                        Status;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL   *Volume;
  EFI_FILE_PROTOCOL                 *Root;
  EFI_FILE_PROTOCOL                 *DirEsp;
  EFI_PARTITION_NAME_PROTOCOL       *PartitionName;

  //
  // Get the PartitionName protocol on that handle
  //
  PartitionName = NULL;
  Status = gBS->HandleProtocol (
                  Handle,
                  &gEfiPartitionNameProtocolGuid,
                  (VOID **)&PartitionName
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // return if it's not our ESP
  if (StrCmp(PartitionName->Name, mEspPartitionName))
    return Status;

  //
  // Get the SimpleFilesystem protocol on that handle
  //
  Volume = NULL;
  Status = gBS->HandleProtocol (
                  Handle,
                  &gEfiSimpleFileSystemProtocolGuid,
                  (VOID **)&Volume
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Open the root directory of the volume
  //
  Root = NULL;
  Status = Volume->OpenVolume (
                     Volume,
                     &Root

                     );
  if (EFI_ERROR (Status) || Root==NULL) {
    return Status;
  }

  //
  // Open multiboot dir
  //
  DirEsp = NULL;
  Status = Root->Open (
                   Root,
                   &DirEsp,
                   mEspPartitionPath,
                   EFI_FILE_MODE_READ,
                   0
                   );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  mEspVolume = Volume;
  mEspDir = DirEsp;

  return Status;
}

STATIC EFI_STATUS
GetAndroidImgInfo (
  IN boot_img_hdr_t     *AndroidHdr,
  IN CPIO_NEWC_HEADER   *Ramdisk,
  LIBAROMA_STREAMP      *Icon,
  CHAR8                 **ImgName,
  BOOLEAN               *IsRecovery
)
{
  // check if this is a recovery ramdisk
  if (CpioGetByName(Ramdisk, "sbin/recovery")) {
    *IsRecovery = TRUE;

    // set icon and description
    if (CpioGetByName(Ramdisk, "sbin/twrp")) {
      *Icon = libaroma_stream_ramdisk("icons/recovery_twrp.png");
      *ImgName = AsciiStrDup("TWRP");
    }
    else if (CpioGetByName(Ramdisk, "sbin/raw-backup.sh")) {
      *Icon = libaroma_stream_ramdisk("icons/recovery_clockwork.png");
      *ImgName = AsciiStrDup("PhilZ Touch");
    }
    else if (CpioGetByName(Ramdisk, "res/images/icon_clockwork.png")) {
      *Icon = libaroma_stream_ramdisk("icons/recovery_clockwork.png");
      *ImgName = AsciiStrDup("ClockworkMod Recovery");
    }
    else if (CpioGetByName(Ramdisk, "res/images/font_log.png")) {
      *Icon = libaroma_stream_ramdisk("icons/recovery_cyanogen.png");
      *ImgName = AsciiStrDup("Cyanogen Recovery");
    }
    else if (CpioGetByName(Ramdisk, "sbin/lafd")) {
      *Icon = libaroma_stream_ramdisk("icons/recovery_lglaf.png");
      *ImgName = AsciiStrDup("LG Laf Recovery");
    }
    else if (CpioGetByName(Ramdisk, "res/images/icon_smile.png")) {
      *Icon = libaroma_stream_ramdisk("icons/recovery_xiaomi.png");
      *ImgName = AsciiStrDup("Xiaomi Recovery");
    }
    else {
      *Icon = libaroma_stream_ramdisk("icons/android.png");
      *ImgName = AsciiStrDup("Recovery");
    }

    return EFI_SUCCESS;
  }

  return EFI_UNSUPPORTED;
}

STATIC EFI_STATUS
EFIAPI
FindAndroidBlockIo (
  IN EFI_HANDLE  Handle,
  IN VOID        *Instance,
  IN VOID        *Context
  )
{
  EFI_STATUS                Status;
  EFI_BLOCK_IO_PROTOCOL     *BlockIo;
  EFI_PARTITION_NAME_PROTOCOL *PartitionName;
  UINTN                     BufferSize;
  boot_img_hdr_t            *AndroidHdr;
  BOOLEAN                   IsRecovery = FALSE;
  LIBAROMA_STREAMP          Icon = NULL;
  CHAR8                     *Name = NULL;

  Status = EFI_SUCCESS;

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

  MENU_ENTRY* Entry = MenuCreateBootEntry();
  if(Entry == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto FREEBUFFER;
  }
  MENU_ENTRY_PDATA* EntryPData = Entry->Private;
  EntryPData->BlockIo = BlockIo;

  //
  // Get the PartitionName protocol on that handle
  //
  PartitionName = NULL;
  Status = gBS->HandleProtocol (
                  Handle,
                  &gEfiPartitionNameProtocolGuid,
                  (VOID **)&PartitionName
                  );
  if (!EFI_ERROR (Status) && PartitionName->Name[0]) {
    // get fstab rec
    CHAR8* PartitionNameAscii = Unicode2Ascii(PartitionName->Name);
    ASSERT(PartitionNameAscii);
    FSTAB_REC* Rec = FstabGetByPartitionName(mFstab, PartitionNameAscii);
    FreePool(PartitionNameAscii);

    if(Rec) {
      // this partition needs a ESP redirect
      if(FstabIsUEFI(Rec) && mEspDir) {
        // build filename
        UINTN PathBufSize = 100*sizeof(CHAR16);
        CHAR16 *PathBuf = AllocateZeroPool(PathBufSize);
        ASSERT(PathBuf);
        UnicodeSPrint(PathBuf, PathBufSize, L"partition_%s.img", PartitionName->Name);

        // open File
        EFI_FILE_PROTOCOL* BootFile = NULL;
        Status = mEspDir->Open (
                         mEspDir,
                         &BootFile,
                         PathBuf,
                         EFI_FILE_MODE_READ,
                         0
                         );
        FreePool(PathBuf);
        if (EFI_ERROR(Status)) {
          goto FREEBUFFER;
        }

        Status = FileBlockIoCreate(BootFile, &EntryPData->BlockIo);
        if (EFI_ERROR(Status)) {
          goto FREEBUFFER;
        }
      }

      // this is a recovery partition
      if(!AsciiStrCmp(Rec->mount_point, "/recovery")) {
        IsRecovery = TRUE;
      }
    }

    // set entry description
    if (!StrCmp(PartitionName->Name, L"boot"))
      Name = AsciiStrDup("Android (Internal)");
    else if(IsRecovery)
      Name = AsciiStrDup("Recovery (Internal)");
    else {
      Name = AllocateZeroPool(4096);
      if(Name) {
        AsciiSPrint(Name, 4096, "Android (%s)", PartitionName->Name);
      }
    }
  }
  else {
    Name = AsciiStrDup("Unknown");
  }

  CPIO_NEWC_HEADER *Ramdisk;
  Status = AndroidGetDecompRamdiskFromBlockIo (EntryPData->BlockIo, &Ramdisk);
  if(!EFI_ERROR(Status)) {
    CHAR8* ImgName = NULL;
    Status = GetAndroidImgInfo(AndroidHdr, Ramdisk, &Icon, &ImgName, &IsRecovery);
    if(!EFI_ERROR(Status) && ImgName) {
      FreePool(Name);
      Name = AllocateZeroPool(4096);
      if(Name) {
        AsciiSPrint(Name, 4096, "%a (Internal)", ImgName);
        FreePool(ImgName);
      }
      else {
        Name = ImgName;
      }
    }

    FreePool(Ramdisk);
  }
  if(Icon==NULL) {
    Icon = libaroma_stream_ramdisk("icons/android.png");
  }

  if(IsRecovery) {
    RECOVERY_MENU *RecMenu = CreateRecoveryMenu();
    RecMenu->RootEntry->Icon = Icon;
    RecMenu->RootEntry->Name = Name;

    Entry->Icon = libaroma_stream_ramdisk("icons/android.png");
    Entry->Name = AsciiStrDup("Android (Internal)");
    RecMenu->BaseEntry = Entry;

    // add nopatch entry
    RecMenu->NoPatchEntry = MenuCloneEntry(Entry);
    MENU_ENTRY_PDATA *PData = RecMenu->NoPatchEntry->Private;
    PData->DisablePatching = TRUE;
    FreePool(RecMenu->NoPatchEntry->Name);
    RecMenu->NoPatchEntry->Name = AsciiStrDup(RecMenu->RootEntry->Name);

    MenuAddEntry(RecMenu->SubMenu, Entry);
  }
  else {
    Entry->Icon = Icon;
    Entry->Name = Name;
    MenuAddEntry(mBootMenuMain, Entry);
  }

  Status = EFI_SUCCESS;

FREEBUFFER:
  FreePool(AndroidHdr);

  return Status;
}

STATIC INT32
IniHandler (
  VOID         *Private,
  CONST CHAR8  *Section,
  CONST CHAR8  *Name,
  CONST CHAR8  *Value
)
{
  multiboot_handle_t* mbhandle = (multiboot_handle_t*)Private;

  if(!AsciiStrCmp(Section, "config")) {
    if(!AsciiStrCmp(Name, "name")) {
      mbhandle->Name = AsciiStrDup(Value);
    }
    if(!AsciiStrCmp(Name, "description")) {
      mbhandle->Description = AsciiStrDup(Value);
    }
  }

  if(!AsciiStrCmp(Section, "partitions")) {
    if(!AsciiStrCmp(Name, "boot")) {
      mbhandle->PartitionBoot = Ascii2Unicode(Value);
    }
  }
  return 1;
} 

STATIC EFI_STATUS
EFIAPI
FindMultibootSFS (
  IN EFI_HANDLE  Handle,
  IN VOID        *Instance,
  IN VOID        *Context
  )
{
  EFI_STATUS                        Status;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL   *Volume;
  EFI_FILE_PROTOCOL                 *Root;
  EFI_FILE_PROTOCOL                 *DirMultiboot;
  EFI_FILE_PROTOCOL                 *FileMultibootIni;
  EFI_FILE_INFO                     *NodeInfo;
  CHAR16                            *FilenameBuf;
  BOOLEAN                           NoFile;
  multiboot_handle_t                *mbhandle;

  //
  // Get the SimpleFilesystem protocol on that handle
  //
  Volume = NULL;
  Status = gBS->HandleProtocol (
                  Handle,
                  &gEfiSimpleFileSystemProtocolGuid,
                  (VOID **)&Volume
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Open the root directory of the volume
  //
  Root = NULL;
  Status = Volume->OpenVolume (
                     Volume,
                     &Root
                     );
  if (EFI_ERROR (Status) || Root==NULL) {
    return Status;
  }

  //
  // Open multiboot dir
  //
  DirMultiboot = NULL;
  Status = Root->Open (
                   Root,
                   &DirMultiboot,
                   L"\\multiboot",
                   EFI_FILE_MODE_READ,
                   0
                   );
  if (!EFI_ERROR (Status)) goto ENUMERATE;

  DirMultiboot = NULL;
  Status = Root->Open (
                   Root,
                   &DirMultiboot,
                   L"\\media\\multiboot",
                   EFI_FILE_MODE_READ,
                   0
                   );
  if (!EFI_ERROR (Status)) goto ENUMERATE;


  DirMultiboot = NULL;
  Status = Root->Open (
                   Root,
                   &DirMultiboot,
                   L"\\media\\0\\multiboot",
                   EFI_FILE_MODE_READ,
                   0
                   );
  if (!EFI_ERROR (Status)) goto ENUMERATE;

  return Status;


ENUMERATE:
  // enumerate directories
  NoFile      = FALSE;
  NodeInfo    = NULL;
  FilenameBuf = NULL;
  mbhandle    = NULL;
  for ( Status = FileHandleFindFirstFile(DirMultiboot, &NodeInfo)
      ; !EFI_ERROR(Status) && !NoFile
      ; Status = FileHandleFindNextFile(DirMultiboot, NodeInfo, &NoFile)
     ){

    // ignore files
    if(!NodeIsDir(NodeInfo))
      continue;

    // build multiboot.ini path
    CONST CHAR16* PathMultibootIni = L"\\multiboot.ini";
    FilenameBuf = AllocateZeroPool(StrSize(NodeInfo->FileName)+StrSize(PathMultibootIni)-1);
    if (FilenameBuf == NULL) {
      continue;
    }
    StrCat(FilenameBuf, NodeInfo->FileName);
    StrCat(FilenameBuf, PathMultibootIni);
    
    // open multiboot.ini
    FileMultibootIni = NULL;
    Status = DirMultiboot->Open (
                     DirMultiboot,
                     &FileMultibootIni,
                     FilenameBuf,
                     EFI_FILE_MODE_READ,
                     0
                     );
    if (EFI_ERROR (Status)) {
      goto NEXT;
    }

    // allocate multiboot handle
    mbhandle = AllocateZeroPool(sizeof(multiboot_handle_t));
    if(mbhandle==NULL) {
      goto NEXT;
    }
    mbhandle->DeviceHandle = Handle;

    // open ROM directory
    Status = DirMultiboot->Open (
                     DirMultiboot,
                     &mbhandle->ROMDirectory,
                     NodeInfo->FileName,
                     EFI_FILE_MODE_READ,
                     0
                     );
    if (EFI_ERROR (Status)) {
      goto NEXT;
    }

    // parse ini
    ini_parse_file(FileMultibootIni, IniHandler, mbhandle);

    // get filename
    CHAR16* fname;
    Status = FileHandleGetFileName(FileMultibootIni, &fname);
    if (EFI_ERROR (Status)) {
      return Status;
    }

    // convert filename
    PathToUnix(fname);

    // store as ascii string
    mbhandle->MultibootConfig = Unicode2Ascii(fname);
    if (mbhandle->MultibootConfig == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    // cleanup
    FreePool(fname);

    // close multiboot.ini
    FileHandleClose(FileMultibootIni);

    // add menu entry
    if(mbhandle->Name && mbhandle->PartitionBoot) {
      // create new menu entry
      MENU_ENTRY *Entry = MenuCreateEntry();
      if(Entry == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }
      Entry->Icon = libaroma_stream_ramdisk("icons/android.png");
      Entry->Name = AsciiStrDup(mbhandle->Name);
      Entry->Description = AsciiStrDup(mbhandle->Description);
      Entry->Private = mbhandle;
      Entry->Callback = MultibootCallback;
      MenuAddEntry(mBootMenuMain, Entry);

      AddMultibootSystemToRecoveryMenu(mbhandle);
    }

NEXT:
    if(EFI_ERROR(Status) && mbhandle) {
      FreePool(mbhandle);
      mbhandle = NULL;
    }

    if(FilenameBuf) {
      FreePool(FilenameBuf);
      FilenameBuf = NULL;
    }
  }

  // close multiboot dir
  FileHandleClose(DirMultiboot);

  return Status;
}

EFI_STATUS
BootOptionEfiOption (
  IN MENU_ENTRY* This
)
{
  BDS_COMMON_OPTION *BootOption = (BDS_COMMON_OPTION*)This->Private;
  UINTN             ExitDataSize;
  CHAR16            *ExitData;
  EFI_STATUS        Status;

  //
  // Make sure the boot option device path connected,
  // but ignore the BBS device path
  //
  if (DevicePathType (BootOption->DevicePath) != BBS_DEVICE_PATH) {
    //
    // Notes: the internal shell can not been connected with device path
    // so we do not check the status here
    //
    BdsLibConnectDevicePath (BootOption->DevicePath);
  }

  //
  // All the driver options should have been processed since
  // now boot will be performed.
  //
  Status = BdsLibBootViaBootOption (BootOption, BootOption->DevicePath, &ExitDataSize, &ExitData);
  if(EFI_ERROR(Status)) {
    CHAR8 Buf[100];
    AsciiSPrint(Buf, 100, "%r", Status);
    MenuShowMessage("Error", Buf);
  }
  return Status;
}

EFI_STATUS
BootShell (
  IN MENU_ENTRY* This
  )
{
  EFI_STATUS       Status;
  EFI_DEVICE_PATH* EfiShellDevicePath;

  // Find the EFI Shell
  Status = LocateEfiApplicationInFvByName (L"Shell", &EfiShellDevicePath);
  if (Status == EFI_NOT_FOUND) {
    Print (L"Error: EFI Application not found.\n");
    return Status;
  } else if (EFI_ERROR (Status)) {
    Print (L"Error: Status Code: 0x%X\n", (UINT32)Status);
    return Status;
  } else {
    // Need to connect every drivers to ensure no dependencies are missing for the application
    Status = BdsConnectAllDrivers ();
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "FAIL to connect all drivers\n"));
      return Status;
    }

    CONST CHAR16* Args = L"";
    UINTN LoadOptionsSize = (UINT32)StrSize (Args);
    VOID *LoadOptions     = AllocatePool (LoadOptionsSize);
    StrCpy (LoadOptions, Args);

    return BdsStartEfiApplication (gImageHandle, EfiShellDevicePath, LoadOptionsSize, LoadOptions);
  }
}

EFI_STATUS
FastbootCallback (
  IN MENU_ENTRY* This
)
{
  FastbootInit();
  return EFI_SUCCESS;
}

EFI_STATUS
RebootCallback (
  IN MENU_ENTRY* This
)
{
  CHAR16* Reason = This->Private;
  UINTN Len = Reason?StrLen(Reason):0;

  gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, Len, Reason);

  return EFI_DEVICE_ERROR;
}

EFI_STATUS
PowerOffCallback (
  IN MENU_ENTRY* This
)
{
  gRT->ResetSystem (EfiResetShutdown, EFI_SUCCESS, 0, NULL);
  return EFI_DEVICE_ERROR;
}

EFI_STATUS
RebootLongPressCallback (
  IN MENU_ENTRY* This
)
{
  MenuShowSelectionDialog(mPowerMenu);
  return EFI_SUCCESS;
}


STATIC VOID
AddEfiBootOptions (
  VOID
)
{
  LIST_ENTRY        BootLists;
  LIST_ENTRY        *Link;
  BDS_COMMON_OPTION *Option;

  InitializeListHead (&BootLists);

  //
  // Parse the boot order to get boot option
  //
  BdsLibBuildOptionFromVar (&BootLists, L"BootOrder");

  //
  // When we didn't have chance to build boot option variables in the first 
  // full configuration boot (e.g.: Reset in the first page or in Device Manager),
  // we have no boot options in the following mini configuration boot.
  // Give the last chance to enumerate the boot options.
  //
  if (IsListEmpty (&BootLists)) {
    BdsLibEnumerateAllBootOption (&BootLists);
  }

  Link = BootLists.ForwardLink;

  //
  // Parameter check, make sure the loop will be valid
  //
  if (Link == NULL) {
    return;
  }
  //
  // Here we make the boot in a loop, every boot success will
  // return to the front page
  //
  for (Link = GetFirstNode (&BootLists); !IsNull (&BootLists, Link); Link = GetNextNode (&BootLists, Link)) {
    Option = CR (Link, BDS_COMMON_OPTION, Link, BDS_LOAD_OPTION_SIGNATURE);

    //
    // Don't display the hidden/inactive boot option
    //
    if (((Option->Attribute & LOAD_OPTION_HIDDEN) != 0) || ((Option->Attribute & LOAD_OPTION_ACTIVE) == 0)) {
      continue;
    }

    // Don't display the VNOR device
    if (DevicePathType(Option->DevicePath) == HARDWARE_DEVICE_PATH || DevicePathSubType(Option->DevicePath) == HW_VENDOR_DP) {
      VENDOR_DEVICE_PATH *Vendor = (VENDOR_DEVICE_PATH *) Option->DevicePath;
      if (CompareGuid (&Vendor->Guid, &gLKVNORGuid)) {
        continue;
      }
    }

    // skip shell
    if(!StrCmp(Option->Description, L"EFI Internal Shell"))
      continue;

    MENU_ENTRY *Entry = MenuCreateEntry();
    if(Entry == NULL) {
      break;
    }

    Entry->Icon = libaroma_stream_ramdisk("icons/uefi.png");
    Entry->Name = Unicode2Ascii(Option->Description);
    Entry->Callback = BootOptionEfiOption;
    Entry->Private = Option;
    Entry->ResetGop = TRUE;
    MenuAddEntry(mBootMenuMain, Entry);
  }
}

STATIC
EFI_STATUS
InitializeEspData (
  VOID
)
{
  // get ESP
  FSTAB_REC* EspRec = FstabGetESP(mFstab);
  if(!EspRec)
    return EFI_NOT_FOUND;

  // get ESP Partition name
  CHAR8* Tmp = FstabGetPartitionName(EspRec);
  if(!Tmp)
    return EFI_NOT_FOUND;
  mEspPartitionName = Ascii2Unicode(Tmp);
  ASSERT(mEspPartitionName);
  FreePool(Tmp);

  // build path for UEFIESP directory
  UINTN BufSize = 5000*sizeof(CHAR16);
  mEspPartitionPath = AllocateZeroPool(BufSize);
  ASSERT(mEspPartitionPath);
  if(EspRec->esp[0]=='/')
    UnicodeSPrint(mEspPartitionPath, BufSize, L"%s/UEFIESP", EspRec->esp);
  else if(!AsciiStrCmp(EspRec->esp, "datamedia"))
    UnicodeSPrint(mEspPartitionPath, BufSize, L"/media/UEFIESP");
  else {
    return EFI_INVALID_PARAMETER;
  }

  // convert path
  PathToUefi(mEspPartitionPath);

  // find ESP filesystem
  VisitAllInstancesOfProtocol (
    &gEfiSimpleFileSystemProtocolGuid,
    FindESP,
    NULL
    );

  return EFI_SUCCESS;
}

INT32
main (
  IN INT32  Argc,
  IN CHAR8  **Argv
  )
{
  UINTN                               Size;
  EFI_STATUS                          Status;
  MENU_ENTRY                          *Entry;
  CHAR8                               *FstabBin;
  UINTN                               FstabSize;

  // create menus
  mBootMenuMain = MenuCreate();
  mPowerMenu = MenuCreate();
  InitializeListHead(&mRecoveries);

  // get fstab data
  Status = UEFIRamdiskGetFile ("fstab.multiboot", (VOID **) &FstabBin, &FstabSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // parse fstab
  mFstab = FstabParse(FstabBin, FstabSize);
  if(!mFstab) {
    return EFI_UNSUPPORTED;
  }

  // init ESP variables
  InitializeEspData();

  // add Android options
  VisitAllInstancesOfProtocol (
    &gEfiBlockIoProtocolGuid,
    FindAndroidBlockIo,
    NULL
    );

  // add Multiboot options
  VisitAllInstancesOfProtocol (
    &gEfiSimpleFileSystemProtocolGuid,
    FindMultibootSFS,
    NULL
    );

  // add default EFI options
  AddEfiBootOptions();

  // add shell
  Entry = MenuCreateEntry();
  Entry->Icon = libaroma_stream_ramdisk("icons/efi_shell.png");
  Entry->Name = AsciiStrDup("EFI Internal Shell");
  Entry->Callback = BootShell;
  Entry->ResetGop = TRUE;
  MenuAddEntry(mBootMenuMain, Entry);

  // add recovery items
  LIST_ENTRY* Link;
  RECOVERY_MENU* RecEntry;
  for (Link = GetFirstNode (&mRecoveries);
       !IsNull (&mRecoveries, Link);
       Link = GetNextNode (&mRecoveries, Link)
      ) {
    RecEntry = CR (Link, RECOVERY_MENU, Link, RECOVERY_MENU_SIGNATURE);
    MenuAddEntry(mBootMenuMain, RecEntry->RootEntry);
  }

  // add fastboot option
  Entry = MenuCreateEntry();
  Entry->Icon = libaroma_stream_ramdisk("icons/fileexplorer.png");
  Entry->Name = AsciiStrDup("File Explorer");
  Entry->Callback = FileExplorerCallback;
  Entry->HideBootMessage = TRUE;
  MenuAddEntry(mBootMenuMain, Entry);

  // add fastboot option
  Entry = MenuCreateEntry();
  Entry->Icon = libaroma_stream_ramdisk("icons/android.png");
  Entry->Name = AsciiStrDup("Fastboot");
  Entry->Callback = FastbootCallback;
  Entry->HideBootMessage = TRUE;
  MenuAddEntry(mBootMenuMain, Entry);

  // add reboot option
  Entry = MenuCreateEntry();
  Entry->Icon = libaroma_stream_ramdisk("icons/reboot.png");
  Entry->Name = AsciiStrDup("Reboot");
  Entry->Callback = RebootCallback;
  Entry->Private = NULL;
  Entry->LongPressCallback = RebootLongPressCallback;
  MenuAddEntry(mBootMenuMain, Entry);

  Entry = MenuCreateEntry();
  Entry->Icon = libaroma_stream_ramdisk("icons/power_off.png");
  Entry->Name = AsciiStrDup("Power Off");
  Entry->Callback = PowerOffCallback;
  MenuAddEntry(mPowerMenu, Entry);

  Entry = MenuCreateEntry();
  Entry->Icon = libaroma_stream_ramdisk("icons/reboot.png");
  Entry->Name = AsciiStrDup("Reboot");
  Entry->Callback = RebootCallback;
  Entry->Private = NULL;
  MenuAddEntry(mPowerMenu, Entry);

  Entry = MenuCreateEntry();
  Entry->Icon = libaroma_stream_ramdisk("icons/reboot_recovery.png");
  Entry->Name = AsciiStrDup("Reboot to Recovery");
  Entry->Callback = RebootCallback;
  Entry->Private = UnicodeStrDup(L"recovery");
  MenuAddEntry(mPowerMenu, Entry);

  Entry = MenuCreateEntry();
  Entry->Icon = libaroma_stream_ramdisk("icons/reboot_bootloader.png");
  Entry->Name = AsciiStrDup("Reboot to Bootloader");
  Entry->Callback = RebootCallback;
  Entry->Private = UnicodeStrDup(L"bootloader");
  MenuAddEntry(mPowerMenu, Entry);

  Entry = MenuCreateEntry();
  Entry->Icon = libaroma_stream_ramdisk("icons/download_mode.png");
  Entry->Name = AsciiStrDup("Enter Download Mode");
  Entry->Callback = RebootCallback;
  Entry->Private = UnicodeStrDup(L"download");
  MenuAddEntry(mPowerMenu, Entry);

  MenuInit();

  // get size of 'EFIDroidErrorStr'
  Size = 0;
  Status = gRT->GetVariable (L"EFIDroidErrorStr", &gEFIDroidVariableGuid, NULL, &Size, NULL);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    // allocate memory
    CHAR8* EFIDroidErrorStr = AllocateZeroPool(Size);
    if (EFIDroidErrorStr) {
      // get actual variable value
      Status = gRT->GetVariable (L"EFIDroidErrorStr", &gEFIDroidVariableGuid, NULL, &Size, EFIDroidErrorStr);
      if (Status == EFI_SUCCESS) {
        MenuShowMessage("Previous boot failed", EFIDroidErrorStr);

        // delete variable
        Status = gRT->SetVariable (L"EFIDroidErrorStr", &gEFIDroidVariableGuid, 0, 0, NULL);
      }

      FreePool(EFIDroidErrorStr);
    }
  }

  FastbootCommandsAdd();

  // show main menu
  SetActiveMenu(mBootMenuMain);
  MenuEnter (0, TRUE);
  MenuDeInit();

  return EFI_SUCCESS;
}
