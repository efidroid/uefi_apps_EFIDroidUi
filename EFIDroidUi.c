#include "EFIDroidUi.h"

MENU_OPTION                 *mBootMenuMain = NULL;
MENU_OPTION                 *mPowerMenu = NULL;
EFI_DEVICE_PATH_TO_TEXT_PROTOCOL   *gEfiDevicePathToTextProtocol = NULL;
EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL *gEfiDevicePathFromTextProtocol = NULL;

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

#if defined (MDE_CPU_ARM)
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
    BdsLibConnectAll ();

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
#endif

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

EFI_STATUS
PowerMenuBackCallback (
  MENU_OPTION* This
)
{
  return EFI_ABORTED;
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
  lkapi_t                             *LKApi;

  Status = gBS->LocateProtocol (
                  &gEfiDevicePathToTextProtocolGuid,
                  NULL,
                  (VOID **)&gEfiDevicePathToTextProtocol
                  );
  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }

  Status = gBS->LocateProtocol (
                  &gEfiDevicePathFromTextProtocolGuid,
                  NULL,
                  (VOID **)&gEfiDevicePathFromTextProtocol
                  );
  if (EFI_ERROR (Status)) {
    return EFI_NOT_FOUND;
  }

  // create menus
  mBootMenuMain = MenuCreate();

  mPowerMenu = MenuCreate();
  mPowerMenu->BackCallback = PowerMenuBackCallback;

  mBootMenuMain->Title = AsciiStrDup("Please Select OS");

#if defined (MDE_CPU_ARM)
  // add android options
  AndroidLocatorInit();
  AndroidLocatorAddItems();
#endif

  // add default EFI options
  AddEfiBootOptions();

#if defined (MDE_CPU_ARM)
  // add shell
  Entry = MenuCreateEntry();
  Entry->Icon = libaroma_stream_ramdisk("icons/efi_shell.png");
  Entry->Name = AsciiStrDup("EFI Internal Shell");
  Entry->Callback = BootShell;
  Entry->ResetGop = TRUE;
  MenuAddEntry(mBootMenuMain, Entry);
#endif

  // add file explorer option
  Entry = MenuCreateEntry();
  Entry->Icon = libaroma_stream_ramdisk("icons/fileexplorer.png");
  Entry->Name = AsciiStrDup("File Explorer");
  Entry->Callback = FileExplorerCallback;
  Entry->HideBootMessage = TRUE;
  MenuAddEntry(mBootMenuMain, Entry);

#if defined (MDE_CPU_ARM)
  FastbootCommandsAdd();

  // add fastboot option
  Entry = MenuCreateEntry();
  Entry->Icon = libaroma_stream_ramdisk("icons/android.png");
  Entry->Name = AsciiStrDup("Fastboot");
  Entry->Callback = FastbootCallback;
  Entry->HideBootMessage = TRUE;
  MenuAddEntry(mBootMenuMain, Entry);
#endif

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

  // show previous boot error
  CHAR8* EFIDroidErrorStr = UtilGetEFIDroidVariable("EFIDroidErrorStr");
  if (EFIDroidErrorStr != NULL) {
    MenuShowMessage("Previous boot failed", EFIDroidErrorStr);

    // delete variable
    UtilSetEFIDroidVariable("EFIDroidErrorStr", NULL);

    // backup variable
    UtilSetEFIDroidVariable("EFIDroidErrorStrPrev", EFIDroidErrorStr);

    // free pool
    FreePool(EFIDroidErrorStr);
  }

  // set default value for multiboot-debuglevel
  Size = 0;
  Status = gRT->GetVariable (L"multiboot-debuglevel", &gEFIDroidVariableGuid, NULL, &Size, NULL);
  if (Status == EFI_NOT_FOUND) {
    UtilSetEFIDroidVariable("multiboot-debuglevel", "4");
  }

  // set default value for fastboot-enable-boot-patch
  Size = 0;
  Status = gRT->GetVariable (L"fastboot-enable-boot-patch", &gEFIDroidVariableGuid, NULL, &Size, NULL);
  if (Status == EFI_NOT_FOUND) {
    UtilSetEFIDroidVariable("fastboot-enable-boot-patch", "0");
  }

  // get last boot entry
  LAST_BOOT_ENTRY* LastBootEntry = UtilGetEFIDroidDataVariable(L"LastBootEntry");
  if(LastBootEntry)
   UtilSetEFIDroidDataVariable(L"LastBootEntry", NULL, 0);

  // run recovery mode handler
  LKApi = GetLKApi();
  if (LKApi && LKApi->platform_get_uefi_bootmode()==LKAPI_UEFI_BM_RECOVERY) {
    AndroidLocatorHandleRecoveryMode(LastBootEntry);
  }

  // free last boot entry
  if(LastBootEntry)
    FreePool(LastBootEntry);

  // show main menu
  MenuStackPush(mBootMenuMain);
  MenuEnter (0, TRUE);
  MenuDeInit();

  return EFI_SUCCESS;
}
