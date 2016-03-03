#include "EFIDroidUi.h"

STATIC LIST_ENTRY                  mRecoveries;
STATIC FSTAB                       *mFstab = NULL;

// ESP
STATIC CHAR16                          *mEspPartitionName = NULL;
STATIC CHAR16                          *mEspPartitionPath = NULL;
STATIC EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *mEspVolume = NULL;
STATIC EFI_FILE_PROTOCOL               *mEspDir    = NULL;
STATIC EFI_DEVICE_PATH_PROTOCOL        *mEspDevicePath = NULL;

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
  MENU_ENTRY_PDATA *PData;

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

  return AndroidBootFromBlockIo(PData->BlockIo, PData->mbhandle, PData->DisablePatching, &PData->LastBootEntry);
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

  PData->Signature = MENU_ANDROID_BOOT_ENTRY_SIGNATURE;

  return MenuEntry;
}

VOID
RecoveryEntryFreeCallback (
  MENU_ENTRY* Entry
)
{
  RECOVERY_MENU *PData = Entry->Private;

  if(PData==NULL)
    return;

  if(PData->SubMenu)
    MenuFree(PData->SubMenu);
  if(PData->BaseEntry)
    MenuFreeEntry(PData->BaseEntry);
  if(PData->NoPatchEntry)
    MenuFreeEntry(PData->NoPatchEntry);

  FreePool(PData);
}

EFI_STATUS
RecoveryEntryCloneCallback (
  MENU_ENTRY* BaseEntry,
  MENU_ENTRY* Entry
)
{
  RECOVERY_MENU *PDataBase = BaseEntry->Private;
  RECOVERY_MENU *PData;

  if(PDataBase==NULL)
    return EFI_SUCCESS;

  PData = AllocateZeroPool (sizeof (*PData));
  if (PData == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  PData->Signature = RECOVERY_MENU_SIGNATURE;
  PData->RootEntry = Entry;

  if(PDataBase->SubMenu)
    PData->SubMenu = MenuClone(PDataBase->SubMenu);
  if(PDataBase->BaseEntry)
    PData->BaseEntry = MenuCloneEntry(PDataBase->BaseEntry);
  if(PDataBase->NoPatchEntry)
    PData->NoPatchEntry = MenuCloneEntry(PDataBase->NoPatchEntry);

  Entry->Private = PData;

  return EFI_SUCCESS;
}

EFI_STATUS
RecoveryCallback (
  IN MENU_ENTRY* This
)
{
  RECOVERY_MENU *Menu = This->Private;
  Menu->SubMenu->Selection = 0;
  MenuStackPush(Menu->SubMenu);
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
  MenuStackPop();
  return EFI_SUCCESS;
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
  Menu->SubMenu->Title = AsciiStrDup("Please Select OS");

  MENU_ENTRY* Entry = MenuCreateEntry();
  Entry->Callback = RecoveryCallback;
  Entry->CloneCallback = RecoveryEntryCloneCallback;
  Entry->FreeCallback = RecoveryEntryFreeCallback;
  Entry->LongPressCallback = RecoveryLongPressCallback;
  Entry->HideBootMessage = TRUE;
  Entry->Private = Menu;
  Menu->RootEntry = Entry;

  InsertTailList (&mRecoveries, &Menu->Link);

  return Menu;
}

VOID
AddMultibootSystemToRecoveryMenu (
  multiboot_handle_t *mbhandle,
  MENU_ENTRY_PDATA   *EntryPData
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
    NewEntryPData->LastBootEntry = EntryPData->LastBootEntry;
    if(NewEntry->Name)
      FreePool(NewEntry->Name);
    NewEntry->Name = AsciiStrDup(mbhandle->Name);
    if(mbhandle->Description)
      NewEntry->Description = AsciiStrDup(mbhandle->Description);

    MenuAddEntry(RecEntry->SubMenu, NewEntry);
  }
}

STATIC
MENU_ENTRY*
GetMenuEntryFromLastBootEntryInternal (
  MENU_OPTION     *Menu,
  LAST_BOOT_ENTRY *LastBootEntry
)
{
  LIST_ENTRY   *Link;
  MENU_ENTRY   *Entry;
  UINTN        Index;

  Link = Menu->Head.ForwardLink;
  Index = 0;
  while (Link != NULL && Link != &Menu->Head) {
    Entry = CR (Link, MENU_ENTRY, Link, MENU_ENTRY_SIGNATURE);
    MENU_ENTRY_PDATA* EntryPData = Entry->Private;

    if (EntryPData == NULL)
      goto NEXT;
    if (EntryPData->Signature != MENU_ANDROID_BOOT_ENTRY_SIGNATURE)
      goto NEXT;

    LAST_BOOT_ENTRY* LocalLastBootEntry = &EntryPData->LastBootEntry;
    if (LocalLastBootEntry->Type != LastBootEntry->Type)
      goto NEXT;

    switch (LocalLastBootEntry->Type) {
      case LAST_BOOT_TYPE_BLOCKIO:
        if (!AsciiStrCmp(LocalLastBootEntry->TextDevicePath, LastBootEntry->TextDevicePath))
          return Entry;
        break;

      case LAST_BOOT_TYPE_FILE:
      case LAST_BOOT_TYPE_MULTIBOOT:
        if (
            !AsciiStrCmp(LocalLastBootEntry->TextDevicePath, LastBootEntry->TextDevicePath) &&
            !AsciiStrCmp(LocalLastBootEntry->FilePathName, LastBootEntry->FilePathName))
          return Entry;

        break;
    }

NEXT:
    Link = Link->ForwardLink;
    Index++;
  }

  return NULL;
}

MENU_ENTRY*
GetMenuEntryFromLastBootEntry (
  LAST_BOOT_ENTRY *LastBootEntry
)
{
  MENU_ENTRY* Entry;
  LIST_ENTRY* Link;
  RECOVERY_MENU* RecEntry;

  Entry = GetMenuEntryFromLastBootEntryInternal(mBootMenuMain, LastBootEntry);
  if (Entry) return Entry;

  for (Link = GetFirstNode (&mRecoveries);
       !IsNull (&mRecoveries, Link);
       Link = GetNextNode (&mRecoveries, Link)
      ) {
    RecEntry = CR (Link, RECOVERY_MENU, Link, RECOVERY_MENU_SIGNATURE);

    Entry = GetMenuEntryFromLastBootEntryInternal(RecEntry->SubMenu, LastBootEntry);
    if (Entry) return Entry;
  }

  return NULL;
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

  mEspDevicePath = DevicePathFromHandle(Handle);
  if (mEspDevicePath==NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

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
  IN CPIO_NEWC_HEADER   *Ramdisk,
  CONST CHAR8           **IconPath,
  CHAR8                 **ImgName,
  BOOLEAN               *IsRecovery
)
{
  // check if this is a recovery ramdisk
  if (CpioGetByName(Ramdisk, "sbin/recovery")) {
    *IsRecovery = TRUE;

    // set icon and description
    if (CpioGetByName(Ramdisk, "sbin/twrp")) {
      *IconPath = "icons/recovery_twrp.png";
      *ImgName = AsciiStrDup("TWRP");
    }
    else if (CpioGetByName(Ramdisk, "sbin/raw-backup.sh")) {
      *IconPath = "icons/recovery_clockwork.png";
      *ImgName = AsciiStrDup("PhilZ Touch");
    }
    else if (CpioGetByName(Ramdisk, "res/images/icon_clockwork.png")) {
      *IconPath = "icons/recovery_clockwork.png";
      *ImgName = AsciiStrDup("ClockworkMod Recovery");
    }
    else if (CpioGetByName(Ramdisk, "res/images/font_log.png")) {
      *IconPath = "icons/recovery_cyanogen.png";
      *ImgName = AsciiStrDup("Cyanogen Recovery");
    }
    else if (CpioGetByName(Ramdisk, "sbin/lafd")) {
      *IconPath = "icons/recovery_lglaf.png";
      *ImgName = AsciiStrDup("LG Laf Recovery");
    }
    else if (CpioGetByName(Ramdisk, "res/images/icon_smile.png")) {
      *IconPath = "icons/recovery_xiaomi.png";
      *ImgName = AsciiStrDup("Xiaomi Recovery");
    }
    else {
      *IconPath = "icons/android.png";
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
  CONST CHAR8               *IconPath = NULL;
  LIBAROMA_STREAMP          Icon = NULL;
  CHAR8                     *Name = NULL;
  MENU_ENTRY                *Entry = NULL;
  LAST_BOOT_ENTRY           LastBootEntry = {0};
  CHAR16                    *TmpStr;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath = NULL;

  Status = EFI_SUCCESS;

  DevicePath = DevicePathFromHandle(Handle);
  if (DevicePath==NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

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

  // build lastbootentry info
  LastBootEntry.Type = LAST_BOOT_TYPE_BLOCKIO;
  TmpStr = gEfiDevicePathToTextProtocol->ConvertDevicePathToText(DevicePath, FALSE, FALSE);
  AsciiSPrint(LastBootEntry.TextDevicePath, sizeof(LastBootEntry.TextDevicePath), "%s", TmpStr);
  FreePool(TmpStr);

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
      if(FstabIsUEFI(Rec)) {
        if (mEspDir == NULL)
          goto FREEBUFFER;

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

        Status = FileBlockIoCreate(BootFile, &BlockIo);
        if (EFI_ERROR(Status)) {
          goto FREEBUFFER;
        }

        // build lastbootentry info
        LastBootEntry.Type = LAST_BOOT_TYPE_FILE;
        TmpStr = gEfiDevicePathToTextProtocol->ConvertDevicePathToText(mEspDevicePath, FALSE, FALSE);
        AsciiSPrint(LastBootEntry.TextDevicePath, sizeof(LastBootEntry.TextDevicePath), "%s", TmpStr);
        FreePool(TmpStr);

        TmpStr = NULL;
        Status = FileHandleGetFileName(BootFile, &TmpStr);
        if (EFI_ERROR (Status)) {
          goto FREEBUFFER;
        }
        AsciiSPrint(LastBootEntry.FilePathName, sizeof(LastBootEntry.FilePathName), "%s", TmpStr);
        FreePool(TmpStr);

        // read android header
        SetMem(AndroidHdr, BufferSize, 0);
        Status = BlockIo->ReadBlocks(BlockIo, BlockIo->Media->MediaId, 0, BufferSize, AndroidHdr);
        if(EFI_ERROR(Status)) {
          goto FREEBUFFER;
        }

        // verify android header
        Status = AndroidVerify(AndroidHdr);
        if(EFI_ERROR(Status)) {
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

  UINT32 Crc32 = 0;
  Status = gBS->CalculateCrc32(AndroidHdr, sizeof(*AndroidHdr), &Crc32);
  if (!EFI_ERROR(Status)) {
    CHAR16 Buf[50];
    UnicodeSPrint(Buf, sizeof(Buf), L"RdInfoCache-%08x", Crc32);
    IMGINFO_CACHE* Cache = UtilGetEFIDroidDataVariable(Buf);
    if (Cache) {
      Icon = libaroma_stream_ramdisk(Cache->IconPath);
      IsRecovery = Cache->IsRecovery;

      FreePool(Name);
      Name = AllocateZeroPool(4096);
      if(Name) {
        AsciiSPrint(Name, 4096, "%a (Internal)", Cache->Name);
      }
      else {
        Name = AsciiStrDup(Cache->Name);
      }

      FreePool(Cache);
      goto SKIP;
    }
  }

  CPIO_NEWC_HEADER *Ramdisk;
  Status = AndroidGetDecompRamdiskFromBlockIo (BlockIo, &Ramdisk);
  if(!EFI_ERROR(Status)) {
    CHAR8* ImgName = NULL;
    Status = GetAndroidImgInfo(Ramdisk, &IconPath, &ImgName, &IsRecovery);
    if(!EFI_ERROR(Status) && ImgName) {
      // write to cache
      if (Crc32) {
        IMGINFO_CACHE Cache;
        AsciiSPrint(Cache.Name, sizeof(Cache.Name), "%a", ImgName);
        AsciiSPrint(Cache.IconPath, sizeof(Cache.IconPath), "%a", IconPath);
        Cache.IsRecovery = IsRecovery;

        CHAR16 Buf[50];
        UnicodeSPrint(Buf, sizeof(Buf), L"RdInfoCache-%08x", Crc32);
        UtilSetEFIDroidDataVariable(Buf, &Cache, sizeof(Cache));
      }

      Icon = libaroma_stream_ramdisk(IconPath);
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

SKIP:
  Entry = MenuCreateBootEntry();
  if(Entry == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto FREEBUFFER;
  }
  MENU_ENTRY_PDATA* EntryPData = Entry->Private;
  EntryPData->BlockIo = BlockIo;
  EntryPData->LastBootEntry = LastBootEntry;

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
  EFI_DEVICE_PATH_PROTOCOL          *DevicePath = NULL;
  LAST_BOOT_ENTRY                   LastBootEntry = {0};

  DevicePath = DevicePathFromHandle(Handle);
  if (DevicePath==NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

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
    IniParseEfiFile(FileMultibootIni, IniHandler, mbhandle);

    // get filename
    CHAR16* fname;
    Status = FileHandleGetFileName(FileMultibootIni, &fname);
    if (EFI_ERROR (Status)) {
      goto NEXT;
    }

    // build lastbootentry info
    LastBootEntry.Type = LAST_BOOT_TYPE_MULTIBOOT;
    CHAR16 *TmpStr = gEfiDevicePathToTextProtocol->ConvertDevicePathToText(DevicePath, FALSE, FALSE);
    AsciiSPrint(LastBootEntry.TextDevicePath, sizeof(LastBootEntry.TextDevicePath), "%s", TmpStr);
    FreePool(TmpStr);
    AsciiSPrint(LastBootEntry.FilePathName, sizeof(LastBootEntry.FilePathName), "%s", fname);

    // convert filename
    PathToUnix(fname);

    // store as ascii string
    mbhandle->MultibootConfig = Unicode2Ascii(fname);
    if (mbhandle->MultibootConfig == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto NEXT;
    }

    // cleanup
    FreePool(fname);

    // close multiboot.ini
    FileHandleClose(FileMultibootIni);

    // add menu entry
    if(mbhandle->Name && mbhandle->PartitionBoot) {
      EFI_FILE_PROTOCOL     *BootFile;
      EFI_BLOCK_IO_PROTOCOL *BlockIo;

      // open boot file
      Status = mbhandle->ROMDirectory->Open (
                       mbhandle->ROMDirectory,
                       &BootFile,
                       mbhandle->PartitionBoot,
                       EFI_FILE_MODE_READ,
                       0
                       );
      if (EFI_ERROR (Status)) {
        goto NEXT;
      }

      // create block IO
      Status = FileBlockIoCreate(BootFile, &BlockIo);
      if (EFI_ERROR(Status)) {
        goto NEXT;
      }

      // create new menu entry
      MENU_ENTRY *Entry = MenuCreateBootEntry();
      if(Entry == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto NEXT;
      }
      MENU_ENTRY_PDATA* EntryPData = Entry->Private;
      Entry->Icon = libaroma_stream_ramdisk("icons/android.png");
      Entry->Name = AsciiStrDup(mbhandle->Name);
      Entry->Description = mbhandle->Description?AsciiStrDup(mbhandle->Description):NULL;
      EntryPData->BlockIo = BlockIo;
      EntryPData->LastBootEntry = LastBootEntry;
      EntryPData->mbhandle = mbhandle;
      MenuAddEntry(mBootMenuMain, Entry);

      AddMultibootSystemToRecoveryMenu(mbhandle, EntryPData);
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

STATIC
EFI_STATUS
InitializeEspData (
  VOID
)
{
  EFI_STATUS Status;
  UINT32     Retry = 0;

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
  BOOLEAN IsDataMedia = FALSE;
  mEspPartitionPath = AllocateZeroPool(BufSize);
  ASSERT(mEspPartitionPath);
  if(EspRec->esp[0]=='/')
    UnicodeSPrint(mEspPartitionPath, BufSize, L"%s/UEFIESP", EspRec->esp);
  else if(!AsciiStrCmp(EspRec->esp, "datamedia")) {
    UnicodeSPrint(mEspPartitionPath, BufSize, L"/media/UEFIESP");
    IsDataMedia = TRUE;
  }
  else {
    return EFI_INVALID_PARAMETER;
  }

AGAIN:
  // convert path
  PathToUefi(mEspPartitionPath);

  // find ESP filesystem
  VisitAllInstancesOfProtocol (
    &gEfiSimpleFileSystemProtocolGuid,
    FindESP,
    NULL
    );

  if (Retry==0 && !mEspDir && IsDataMedia) {
    UnicodeSPrint(mEspPartitionPath, BufSize, L"/media/0/UEFIESP");
    Retry = 1;
    goto AGAIN;
  }

  // publish filename to fastboot
  if (mEspDir) {
    CHAR16* FileName = NULL;
    Status = FileHandleGetFileName(mEspDir, &FileName);
    if (!EFI_ERROR (Status)) {
      FastbootPublish("esp-dir", Unicode2Ascii(FileName));
      FreePool(FileName);
    }
    else {
      FastbootPublish("esp-dir", "<name-error>");
    }
  }
  else {
    FastbootPublish("esp-dir", "<null>");
  }

  return EFI_SUCCESS;
}

EFI_STATUS
AndroidLocatorInit (
  VOID
)
{
  EFI_STATUS                          Status;
  CHAR8                               *FstabBin;
  UINTN                               FstabSize;

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

  return EFI_SUCCESS;
}

EFI_STATUS
AndroidLocatorAddItems (
  VOID
)
{
  MENU_ENTRY                          *Entry;

  // GROUP: Android
  Entry = MenuCreateGroupEntry();
  Entry->Name = AsciiStrDup("Android");
  MenuAddEntry(mBootMenuMain, Entry);

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

  // GROUP: Recovery
  Entry = MenuCreateGroupEntry();
  Entry->Name = AsciiStrDup("Recovery");
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

  return EFI_SUCCESS;
}

STATIC
MENU_ENTRY*
AndroidLocatorGetMatchingRecoveryEntry (
  MENU_OPTION   *Menu,
  MENU_ENTRY    *LastEntry
)
{
  LIST_ENTRY   *Link;
  MENU_ENTRY   *Entry;
  UINTN        Index;

  MENU_ENTRY_PDATA* LastEntryPData = LastEntry->Private;

  Link = Menu->Head.ForwardLink;
  Index = 0;
  while (Link != NULL && Link != &Menu->Head) {
    Entry = CR (Link, MENU_ENTRY, Link, MENU_ENTRY_SIGNATURE);
    MENU_ENTRY_PDATA* EntryPData = Entry->Private;

    if (EntryPData == NULL)
      goto NEXT;
    if (EntryPData->Signature != MENU_ANDROID_BOOT_ENTRY_SIGNATURE)
      goto NEXT;

    if (EntryPData->mbhandle == LastEntryPData->mbhandle)
      return Entry;

NEXT:
    Link = Link->ForwardLink;
    Index++;
  }

  return NULL;
}

STATIC
EFI_STATUS
PureRecoveryBackCallback (
  MENU_OPTION* This
)
{
  MenuStackPop();
  return EFI_SUCCESS;
}

EFI_STATUS
AndroidLocatorHandleRecoveryMode (
  LAST_BOOT_ENTRY *LastBootEntry
)
{
  MENU_ENTRY    *LastEntry;
  MENU_OPTION   *Menu = NULL;
  LIST_ENTRY    *Link;
  RECOVERY_MENU *RecEntry;
  CHAR8         Buf[100];

  Menu = MenuCreate();
  Menu->BackCallback = PureRecoveryBackCallback;

  LastEntry = GetMenuEntryFromLastBootEntry(LastBootEntry);
  if (LastEntry==NULL) {
    Menu->Title = AsciiStrDup("Select Recovery Tool");

    // add recovery items
    for (Link = GetFirstNode (&mRecoveries);
         !IsNull (&mRecoveries, Link);
         Link = GetNextNode (&mRecoveries, Link)
        ) {
      RecEntry = CR (Link, RECOVERY_MENU, Link, RECOVERY_MENU_SIGNATURE);

      MENU_ENTRY* CloneEntry = MenuCloneEntry(RecEntry->RootEntry);
      if (CloneEntry == NULL)
        continue;

      MenuAddEntry(Menu, CloneEntry);
    }
  }

  else {
    AsciiSPrint(Buf, sizeof(Buf), "Recovery: %a", LastEntry->Name);
    Menu->Title = AsciiStrDup(Buf);

    // add recovery items
    for (Link = GetFirstNode (&mRecoveries);
         !IsNull (&mRecoveries, Link);
         Link = GetNextNode (&mRecoveries, Link)
        ) {
      RecEntry = CR (Link, RECOVERY_MENU, Link, RECOVERY_MENU_SIGNATURE);
      MENU_ENTRY* MatchingRecEntry = AndroidLocatorGetMatchingRecoveryEntry(RecEntry->SubMenu, LastEntry);
      if (MatchingRecEntry == NULL)
        continue;

      MENU_ENTRY* CloneEntry = MenuCloneEntry(MatchingRecEntry);
      if (CloneEntry == NULL)
        continue;

      CloneEntry->Icon = RecEntry->RootEntry->Icon;
      if (CloneEntry->Name) {
        FreePool(CloneEntry->Name);
        CloneEntry->Name = NULL;
      }
      if (CloneEntry->Description) {
        FreePool(CloneEntry->Description);
        CloneEntry->Description = NULL;
      }

      if (RecEntry->RootEntry->Name)
        CloneEntry->Name = AsciiStrDup(RecEntry->RootEntry->Name);
      if (RecEntry->RootEntry->Description)
        CloneEntry->Description = AsciiStrDup(RecEntry->RootEntry->Description);

      MenuAddEntry(Menu, CloneEntry);
    }
  }

  // show main menu
  MenuStackPush(Menu);
  MenuEnter (0, TRUE);

  return EFI_SUCCESS;
}
