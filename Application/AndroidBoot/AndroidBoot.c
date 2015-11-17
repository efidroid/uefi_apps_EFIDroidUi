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

#include <Protocol/DevicePathFromText.h>

#include "AndroidBoot.h"
#include "bootimg.h"

BOOT_MENU_ENTRY             *mBootMenuMain = NULL;
UINTN                       mBootMenuMainCount = 0;
BOOT_MENU_ENTRY             *mBootMenuRecovery = NULL;
UINTN                       mBootMenuRecoveryCount = 0;
CONST CHAR8                 *gErrorStr = NULL;
INT32                       gEntryRecoveryIndex = -1;

// Type definitions
//

typedef
EFI_STATUS
(EFIAPI *PROTOCOL_INSTANCE_CALLBACK)(
  IN EFI_HANDLE           Handle,
  IN VOID                 *Instance,
  IN VOID                 *Context
  );

CHAR8*
Unicode2Ascii (
  CONST CHAR16* UnicodeStr
)
{
  CHAR8* AsciiStr = AllocatePool((StrLen (UnicodeStr) + 1) * sizeof (CHAR8));
  if (AsciiStr == NULL) {
    return NULL;
  }

  UnicodeStrToAsciiStr(UnicodeStr, AsciiStr);

  return AsciiStr;
}

CHAR16*
Ascii2Unicode (
  CONST CHAR8* AsciiStr
)
{
  CHAR16* UnicodeStr = AllocatePool((AsciiStrLen (AsciiStr) + 1) * sizeof (CHAR16));
  if (UnicodeStr == NULL) {
    return NULL;
  }

  AsciiStrToUnicodeStr(AsciiStr, UnicodeStr);

  return UnicodeStr;
}

CHAR8*
AsciiStrDup (
  CONST CHAR8* SrcStr
)
{
  UINTN Len = (AsciiStrLen (SrcStr) + 1) * sizeof (CHAR8);
  CHAR8* NewStr = AllocatePool(Len);
  if (NewStr == NULL) {
    return NULL;
  }

  CopyMem(NewStr, SrcStr, Len);

  return NewStr;
}

STATIC EFI_STATUS
AndroidCallback (
  IN VOID *Private
)
{
  return AndroidBootFromBlockIo((EFI_BLOCK_IO_PROTOCOL*)Private, NULL);
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
  VOID                      *AndroidHdr;
  CONST CHAR8               *EntryDescription;
  BOOLEAN                   IsRecovery = FALSE;

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

  // read android header
  Status = AndroidVerify(AndroidHdr);
  if(EFI_ERROR(Status)) {
    goto FREEBUFFER;
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
  if (!EFI_ERROR (Status) && PartitionName->Name[0]) {
    if (!StrCmp(PartitionName->Name, L"boot"))
      EntryDescription = "Internal Android";
    else if (!StrCmp(PartitionName->Name, L"recovery")) {
      EntryDescription = "Internal Android";
      IsRecovery = TRUE;
    }
    else {
      EntryDescription = Unicode2Ascii(PartitionName->Name);
    }
  }
  else {
    EntryDescription = "Unknown";
  }

  // create new menu entry
  BOOT_MENU_ENTRY *Entry = NULL;
  if(IsRecovery) {
    Entry = MenuAddEntry(&mBootMenuRecovery, &mBootMenuRecoveryCount);
    gEntryRecoveryIndex = mBootMenuRecoveryCount-1;
  }
  else {
    Entry = MenuAddEntry(&mBootMenuMain, &mBootMenuMainCount);
  }
  if(Entry == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }
  Entry->Description = EntryDescription;
  Entry->Callback = AndroidCallback;
  Entry->Private = BlockIo;

  Status = EFI_SUCCESS;

FREEBUFFER:
  FreePool(AndroidHdr);

  return Status;
}

STATIC BOOLEAN
NodeIsDir (
  IN EFI_FILE_INFO      *NodeInfo
  )
{
  return ((NodeInfo->Attribute & EFI_FILE_DIRECTORY) == EFI_FILE_DIRECTORY);
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
  }

  if(!AsciiStrCmp(Section, "partitions")) {
    if(!AsciiStrCmp(Name, "boot")) {
      mbhandle->PartitionBoot = Ascii2Unicode(Value);
    }
  }
  return 1;
} 

STATIC EFI_STATUS
MultibootRecoveryCallback (
  IN VOID *Private
)
{
  multiboot_handle_t        *mbhandle = Private;
  return AndroidBootFromBlockIo(mBootMenuRecovery[gEntryRecoveryIndex].Private, mbhandle);
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
    CHAR16 *Tmp = fname;
    for(Tmp = fname; *Tmp != 0; Tmp++) {
      if(*Tmp=='\\')
        *Tmp = '/';
    }

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
      BOOT_MENU_ENTRY *Entry = MenuAddEntry(&mBootMenuMain, &mBootMenuMainCount);
      if(Entry == NULL) {
        return EFI_OUT_OF_RESOURCES;
      }
      Entry->Description = mbhandle->Name;
      Entry->Private = mbhandle;
      Entry->Callback = MultibootCallback;

      if (gEntryRecoveryIndex >= 0) {
        // create new recovery menu entry
        BOOT_MENU_ENTRY *EntryRecovery = MenuAddEntry(&mBootMenuRecovery, &mBootMenuRecoveryCount);
        if(EntryRecovery == NULL) {
          return EFI_OUT_OF_RESOURCES;
        }
        CopyMem(EntryRecovery, Entry, sizeof(BOOT_MENU_ENTRY));
        EntryRecovery->Callback = MultibootRecoveryCallback;
      }
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
  IN VOID* Private
)
{
  BDS_COMMON_OPTION *BootOption = (BDS_COMMON_OPTION*)Private;
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
  return Status;
}

EFI_STATUS
RecoveryCallback (
  IN VOID* Private
)
{
  SetActiveMenu(mBootMenuRecovery);
  return EFI_UNSUPPORTED;
}

EFI_STATUS
RecoveryBackCallback (
  IN VOID* Private
)
{
  SetActiveMenu(mBootMenuMain);
  return EFI_UNSUPPORTED;
}

EFI_STATUS
RebootCallback (
  IN VOID* Private
)
{
  gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
  return EFI_UNSUPPORTED;
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

    BOOT_MENU_ENTRY *Entry = MenuAddEntry(&mBootMenuMain, &mBootMenuMainCount);
    if(Entry == NULL) {
      break;
    }

    Entry->Description = Unicode2Ascii(Option->Description);
    Entry->Callback = BootOptionEfiOption;
    Entry->Private = Option;
    Entry->ResetGop = TRUE;
  }
}

STATIC EFI_STATUS
VisitAllInstancesOfProtocol (
  IN EFI_GUID                    *Id,
  IN PROTOCOL_INSTANCE_CALLBACK  CallBackFunction,
  IN VOID                        *Context
  )
{
  EFI_STATUS                Status;
  UINTN                     HandleCount;
  EFI_HANDLE                *HandleBuffer;
  UINTN                     Index;
  VOID                      *Instance;

  //
  // Start to check all the PciIo to find all possible device
  //
  HandleCount = 0;
  HandleBuffer = NULL;
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  Id,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (HandleBuffer[Index], Id, &Instance);
    if (EFI_ERROR (Status)) {
      continue;
    }

    Status = (*CallBackFunction) (
               HandleBuffer[Index],
               Instance,
               Context
               );
  }

  gBS->FreePool (HandleBuffer);

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
AndroidBootEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  UINTN                               Size;
  EFI_STATUS                          Status;
  BOOT_MENU_ENTRY                     *Entry;

  // create menus
  mBootMenuMain = MenuCreate();
  mBootMenuRecovery = MenuCreate();

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

  if (gEntryRecoveryIndex >= 0) {
    // add recovery option
    Entry = MenuAddEntry(&mBootMenuMain, &mBootMenuMainCount);
    Entry->Description = "Recovery";
    Entry->Callback = RecoveryCallback;
    Entry->HideBootMessage = TRUE;
  }

  // add reboot option
  Entry = MenuAddEntry(&mBootMenuMain, &mBootMenuMainCount);
  Entry->Description = "Reboot";
  Entry->Callback = RebootCallback;

  // add back option
  Entry = MenuAddEntry(&mBootMenuRecovery, &mBootMenuRecoveryCount);
  Entry->Description = "< Back <";
  Entry->Callback = RecoveryBackCallback;
  Entry->HideBootMessage = TRUE;

  // get size of 'EFIDroidErrorStr'
  Size = 0;
  Status = gRT->GetVariable (L"EFIDroidErrorStr", &gEFIDroidVariableGuid, NULL, &Size, NULL);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    // allocate memory (XXX: this is a memleak)
    CHAR8* EFIDroidErrorStr = AllocateZeroPool(Size);
    if (EFIDroidErrorStr) {
      // get actual variable value and set 'gErrorStr'
      Status = gRT->GetVariable (L"EFIDroidErrorStr", &gEFIDroidVariableGuid, NULL, &Size, EFIDroidErrorStr);
      if (Status == EFI_SUCCESS) {
        gErrorStr = EFIDroidErrorStr;

        // delete variable
        Status = gRT->SetVariable (L"EFIDroidErrorStr", &gEFIDroidVariableGuid, 0, 0, NULL);
      }
    }
  }

  // finish menus
  MenuFinish(&mBootMenuMain, &mBootMenuMainCount);
  MenuFinish(&mBootMenuRecovery, &mBootMenuRecoveryCount);

  // show main menu
  SetActiveMenu(mBootMenuMain);
  EFIDroidEnterFrontPage (0, TRUE);

  return EFI_SUCCESS;
}
