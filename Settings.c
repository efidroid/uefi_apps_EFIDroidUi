#include "EFIDroidUi.h"

STATIC
EFI_STATUS
SettingsMenuBackCallback (
  MENU_OPTION* This
)
{
  MenuStackPop();
  MenuFree(This);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
ShowFileExplorerCallback (
  MENU_ENTRY* This
)
{
  SettingBoolSet("ui-show-file-explorer", !This->ToggleEnabled);
  This->ToggleEnabled = SettingBoolGet("ui-show-file-explorer");
  InvalidateActiveMenu();
  MainMenuUpdateUi();
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
ShowUEFIOptionsCallback (
  MENU_ENTRY* This
)
{
  SettingBoolSet("ui-show-uefi-options", !This->ToggleEnabled);
  This->ToggleEnabled = SettingBoolGet("ui-show-uefi-options");
  InvalidateActiveMenu();
  MainMenuUpdateUi();
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
ShowFastbootCallback (
  MENU_ENTRY* This
)
{
  SettingBoolSet("ui-show-fastboot", !This->ToggleEnabled);
  This->ToggleEnabled = SettingBoolGet("ui-show-fastboot");
  InvalidateActiveMenu();
  MainMenuUpdateUi();
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
ShowPermissiveCallback (
  MENU_ENTRY* This
)
{
  SettingBoolSet("boot-force-permissive", !This->ToggleEnabled);
  This->ToggleEnabled = SettingBoolGet("boot-force-permissive");
  InvalidateActiveMenu();
  MainMenuUpdateUi();
  return EFI_SUCCESS;
}

EFI_STATUS
SettingsMenuShow (
  VOID
)
{
  MENU_ENTRY *Entry;
  MENU_OPTION* Menu;

  // create menu
  Menu = MenuCreate();
  Menu->Title = AsciiStrDup("Settings");
  Menu->BackCallback = SettingsMenuBackCallback;

  // UEFI options
  Entry = MenuCreateEntry();
  Entry->Name = AsciiStrDup("Show UEFI boot options");
  Entry->ShowToggle = TRUE;
  Entry->ToggleEnabled = SettingBoolGet("ui-show-uefi-options");
  Entry->HideBootMessage = TRUE;
  Entry->Callback = ShowUEFIOptionsCallback;
  MenuAddEntry(Menu, Entry);

  // file explorer
  Entry = MenuCreateEntry();
  Entry->Name = AsciiStrDup("Show File Explorer");
  Entry->ShowToggle = TRUE;
  Entry->ToggleEnabled = SettingBoolGet("ui-show-file-explorer");
  Entry->HideBootMessage = TRUE;
  Entry->Callback = ShowFileExplorerCallback;
  MenuAddEntry(Menu, Entry);

  // fastboot
  Entry = MenuCreateEntry();
  Entry->Name = AsciiStrDup("Show Fastboot");
  Entry->ShowToggle = TRUE;
  Entry->ToggleEnabled = SettingBoolGet("ui-show-fastboot");
  Entry->HideBootMessage = TRUE;
  Entry->Callback = ShowFastbootCallback;
  MenuAddEntry(Menu, Entry);

  // fastboot
  Entry = MenuCreateEntry();
  Entry->Name = AsciiStrDup("Force selinux to permissive");
  Entry->ShowToggle = TRUE;
  Entry->ToggleEnabled = SettingBoolGet("boot-force-permissive");
  Entry->HideBootMessage = TRUE;
  Entry->Callback = ShowPermissiveCallback;
  MenuAddEntry(Menu, Entry);

  MenuStackPush(Menu);
  return EFI_SUCCESS;
}
