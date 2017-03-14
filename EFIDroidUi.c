#include "EFIDroidUi.h"

lkapi_t *mLKApi = NULL;

MENU_OPTION                 *mBootMenuMain = NULL;
MENU_OPTION                 *mPowerMenu = NULL;
MENU_OPTION                 *mFastbootMenu = NULL;
EFI_DEVICE_PATH_TO_TEXT_PROTOCOL   *gEfiDevicePathToTextProtocol = NULL;
EFI_DEVICE_PATH_FROM_TEXT_PROTOCOL *gEfiDevicePathFromTextProtocol = NULL;
multiboot_handle_t *gFastbootMBHandle = NULL;

STATIC EFI_GUID mUefiShellFileGuid = {0x7C04A583, 0x9E3E, 0x4f1c, {0xAD, 0x65, 0xE0, 0x52, 0x68, 0xD0, 0xB4, 0xD1 }};

STATIC
EFI_STATUS
BootOptionEfiOption (
  IN MENU_ENTRY* This
)
{
  EFI_BOOT_MANAGER_LOAD_OPTION  *BootOption = This->Private;

  EfiBootManagerBoot(BootOption);
  if(EFI_ERROR(BootOption->Status)) {
    CHAR8 Buf[100];
    AsciiSPrint(Buf, 100, "%r", BootOption->Status);
    MenuShowMessage("Error", Buf);
  }

  return BootOption->Status;
}

STATIC
VOID
UefiMenuEntryUpdate (
  IN MENU_ENTRY* This
)
{
  This->Hidden = !SettingBoolGet("ui-show-uefi-options");
}

STATIC VOID
AddEfiBootOptions (
  VOID
)
{
  UINTN                         Index;
  EFI_BOOT_MANAGER_LOAD_OPTION  *BootOption;
  UINTN                         BootOptionCount;
  BOOLEAN                       First = TRUE;
  MENU_ENTRY                    *Entry;
  EFI_DEVICE_PATH*              DevicePathNode;

  BootOption = EfiBootManagerGetLoadOptions (&BootOptionCount, LoadOptionTypeBoot);

  for (Index = 0; Index < BootOptionCount; Index++) {
    //
    // Don't display the hidden/inactive boot option
    //
    if (((BootOption[Index].Attributes & LOAD_OPTION_HIDDEN) != 0) || ((BootOption[Index].Attributes & LOAD_OPTION_ACTIVE) == 0)) {
      continue;
    }

    if (First) {
      // GROUP: UEFI
      Entry = MenuCreateGroupEntry();
      Entry->Name = AsciiStrDup("UEFI");
      Entry->Update = UefiMenuEntryUpdate;
      MenuAddEntry(mBootMenuMain, Entry);
      First = FALSE;
    }

    Entry = MenuCreateEntry();
    if(Entry == NULL) {
      break;
    }

    CONST CHAR8* IconPath = "icons/uefi.png";

    DevicePathNode = BootOption[Index].FilePath;
    while ((DevicePathNode != NULL) && !IsDevicePathEnd (DevicePathNode)) {

      // detect shell
      if (IS_DEVICE_PATH_NODE (DevicePathNode, MEDIA_DEVICE_PATH, MEDIA_PIWG_FW_FILE_DP)) {
        CONST MEDIA_FW_VOL_FILEPATH_DEVICE_PATH* FvDevicePathNode =  ((CONST MEDIA_FW_VOL_FILEPATH_DEVICE_PATH *)DevicePathNode);
        if (FvDevicePathNode != NULL && CompareGuid (&FvDevicePathNode->FvFileName, &mUefiShellFileGuid)) {
          IconPath = "icons/efi_shell.png";
          break;
        }
      }

      // next
      DevicePathNode     = NextDevicePathNode (DevicePathNode);
    }

    Entry->Icon = libaroma_stream_ramdisk(IconPath);
    Entry->Name = Unicode2Ascii(BootOption[Index].Description);
    Entry->Callback = BootOptionEfiOption;
    Entry->Private = &BootOption[Index];
    Entry->ResetGop = TRUE;
    Entry->Update = UefiMenuEntryUpdate;
    MenuAddEntry(mBootMenuMain, Entry);
  }
}

#if defined (MDE_CPU_ARM)
STATIC
EFI_STATUS
FastbootCallback (
  IN MENU_ENTRY* This
)
{
  mFastbootMenu->Selection = 0;
  MenuStackPush(mFastbootMenu);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
FastbootStartCallback (
  IN MENU_ENTRY* This
)
{
  multiboot_handle_t *mbhandle = This->Private;
  gFastbootMBHandle = mbhandle;
  FastbootInit();
  return EFI_SUCCESS;
}


STATIC
EFI_STATUS
FastbootBackCallback (
  MENU_OPTION* This
)
{
  MenuStackPop();
  return EFI_SUCCESS;
}

STATIC
VOID
FastbootMenuEntryUpdate (
  IN MENU_ENTRY* This
)
{
  This->Hidden = !SettingBoolGet("ui-show-fastboot");
}

VOID
FastbootAddInternalROM (
  VOID
)
{
  LIBAROMA_STREAMP      Icon;
  CONST CHAR8           *InternalROMIconPath;

  // get icon
  InternalROMIconPath = AndroidLocatorGetInternalROMIconPath();
  if(InternalROMIconPath)
    Icon = libaroma_stream_ramdisk(InternalROMIconPath);
  else
    Icon = libaroma_stream_ramdisk("icons/android.png");

  // create entry
  MENU_ENTRY* NewEntry = MenuCreateEntry();
  if(NewEntry==NULL)
    return;
  NewEntry->Name = AllocateZeroPool(4096);
  if(NewEntry->Name)
    AsciiSPrint(NewEntry->Name, 4096, "%a (Internal)", AndroidLocatorGetInternalROMName()?:"Android");
  NewEntry->Icon = Icon;
  NewEntry->Callback = FastbootStartCallback;
  NewEntry->HideBootMessage = TRUE;

  MenuAddEntry(mFastbootMenu, NewEntry);
}

VOID
AddSystemToFastbootMenu (
  MENU_ENTRY         *Entry,
  multiboot_handle_t *mbhandle
)
{

  MENU_ENTRY* NewEntry = MenuCreateEntry();
  if(NewEntry==NULL)
    return;

  NewEntry->Private = mbhandle;
  NewEntry->Name = AsciiStrDup(mbhandle->Name);
  if(mbhandle->Description)
    NewEntry->Description = AsciiStrDup(mbhandle->Description);
  if(Entry->Icon)
    NewEntry->Icon = Entry->Icon;
  NewEntry->Callback = FastbootStartCallback;
  NewEntry->HideBootMessage = TRUE;

  MenuAddEntry(mFastbootMenu, NewEntry);
}
#endif

STATIC
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

STATIC
EFI_STATUS
PowerOffCallback (
  IN MENU_ENTRY* This
)
{
  gRT->ResetSystem (EfiResetShutdown, EFI_SUCCESS, 0, NULL);
  return EFI_DEVICE_ERROR;
}

STATIC
EFI_STATUS
RebootLongPressCallback (
  IN MENU_ENTRY* This
)
{
  MenuShowSelectionDialog(mPowerMenu);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
PowerMenuBackCallback (
  MENU_OPTION* This
)
{
  return EFI_ABORTED;
}

STATIC
EFI_STATUS
MainMenuActionCallback (
  MENU_OPTION* This
)
{
  return SettingsMenuShow();
}

EFI_STATUS
MainMenuUpdateUi (
  VOID
)
{
  InvalidateMenu(mBootMenuMain);

  return EFI_SUCCESS;
}

INT32
main (
  IN INT32  Argc,
  IN CHAR8  **Argv
  )
{
  EFI_STATUS                          Status;
  MENU_ENTRY                          *Entry;

  // get LKAPi
  mLKApi = GetLKApi();

  // init fastboot
#if defined (MDE_CPU_ARM)
  FastbootCommandsAdd();
#endif

  // init libboot
  libboot_init();

  Status = gBS->LocateProtocol (
                  &gEfiDevicePathToTextProtocolGuid,
                  NULL,
                  (VOID **)&gEfiDevicePathToTextProtocol
                  );
  if (EFI_ERROR (Status)) {
    return -1;
  }

  Status = gBS->LocateProtocol (
                  &gEfiDevicePathFromTextProtocolGuid,
                  NULL,
                  (VOID **)&gEfiDevicePathFromTextProtocol
                  );
  if (EFI_ERROR (Status)) {
    return -1;
  }

  // set default values
  if(!UtilVariableExists(L"multiboot-debuglevel", &gEFIDroidVariableGuid))
    UtilSetEFIDroidVariable("multiboot-debuglevel", "4");
  if(!UtilVariableExists(L"fastboot-enable-boot-patch", &gEFIDroidVariableGuid))
    UtilSetEFIDroidVariable("fastboot-enable-boot-patch", "1");
  if(!UtilVariableExists(L"ui-show-file-explorer", &gEFIDroidVariableGuid))
    SettingBoolSet("ui-show-file-explorer", FALSE);
  if(!UtilVariableExists(L"ui-show-uefi-options", &gEFIDroidVariableGuid))
    SettingBoolSet("ui-show-uefi-options", FALSE);
  if(!UtilVariableExists(L"ui-show-fastboot", &gEFIDroidVariableGuid))
    SettingBoolSet("ui-show-fastboot", TRUE);
  if(!UtilVariableExists(L"ui-autoselect-last-boot", &gEFIDroidVariableGuid))
    SettingBoolSet("ui-autoselect-last-boot", FALSE);
  if(!UtilVariableExists(L"boot-force-permissive", &gEFIDroidVariableGuid))
    SettingBoolSet("boot-force-permissive", FALSE);

  // init UI
  Status = MenuInit();
  if (EFI_ERROR (Status)) {
#if defined (MDE_CPU_ARM)
    FastbootInit();
#endif
    return -1;
  }

  // create menus
  mBootMenuMain = MenuCreate();

  mPowerMenu = MenuCreate();
  mPowerMenu->BackCallback = PowerMenuBackCallback;
  mPowerMenu->HideBackIcon = TRUE;

#if defined (MDE_CPU_ARM)
  mFastbootMenu = MenuCreate();
  mFastbootMenu->Title = AsciiStrDup("Please Select OS");
  mFastbootMenu->BackCallback = FastbootBackCallback;
#endif

  mBootMenuMain->Title = AsciiStrDup("Please Select OS");
  mBootMenuMain->ActionIcon = libaroma_stream_ramdisk("icons/ic_settings_black_24dp.png");
  mBootMenuMain->ActionCallback = MainMenuActionCallback;
  mBootMenuMain->ItemFlags = MENU_ITEM_FLAG_SEPARATOR_ALIGN_TEXT;

#if defined (MDE_CPU_ARM)
  // add android options
  AndroidLocatorInit();
  AndroidLocatorAddItems();
#endif

  // add default EFI options
  AddEfiBootOptions();

  // GROUP: Tools
  Entry = MenuCreateGroupEntry();
  Entry->Name = AsciiStrDup("Tools");
  MenuAddEntry(mBootMenuMain, Entry);

  // add file explorer option
  Entry = MenuCreateEntry();
  Entry->Icon = libaroma_stream_ramdisk("icons/fileexplorer.png");
  Entry->Name = AsciiStrDup("File Explorer");
  Entry->Callback = FileExplorerCallback;
  Entry->HideBootMessage = TRUE;
  Entry->Update = FileExplorerUpdate;
  MenuAddEntry(mBootMenuMain, Entry);

#if defined (MDE_CPU_ARM)
  // add fastboot option
  Entry = MenuCreateEntry();
  Entry->Icon = libaroma_stream_ramdisk("icons/android.png");
  Entry->Name = AsciiStrDup("Fastboot");
  Entry->Callback = FastbootCallback;
  Entry->HideBootMessage = TRUE;
  Entry->Update = FastbootMenuEntryUpdate;
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

  // get last boot entry
  LAST_BOOT_ENTRY* LastBootEntry = UtilGetEFIDroidDataVariable(L"LastBootEntry");
  if(LastBootEntry)
   UtilSetEFIDroidDataVariable(L"LastBootEntry", NULL, 0);

#if defined (MDE_CPU_ARM)
  // run recovery mode handler
  if (mLKApi) {
    if(!AsciiStrCmp(mLKApi->platform_get_uefi_bootpart(), "recovery") || mLKApi->platform_get_uefi_bootmode()==LKAPI_UEFI_BM_RECOVERY) {
      AndroidLocatorHandleRecoveryMode(LastBootEntry);
    }
  }
#endif

  // select last booted entry
  if (SettingBoolGet("ui-autoselect-last-boot")) {
    mBootMenuMain->Selection = GetMenuIdFromLastBootEntry(mBootMenuMain, LastBootEntry);
  }

  // free last boot entry
  if(LastBootEntry)
    FreePool(LastBootEntry);

  // clear the watchdog timer
  gBS->SetWatchdogTimer (0, 0, 0, NULL);

  // first boot?
  UINTN* LastBootVersion = UtilGetEFIDroidDataVariable(L"last-boot-version");
  if(LastBootVersion==NULL) {
    UINTN NewValue = 1;
    UtilSetEFIDroidDataVariable(L"last-boot-version", &NewValue, sizeof(NewValue));
    MenuShowTutorial();
  }

  // show main menu
  MenuStackPush(mBootMenuMain);
  MenuEnter (0, TRUE);
  MenuDeInit();

  return 0;
}
