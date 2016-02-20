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
ShowEFIShellCallback (
  MENU_ENTRY* This
)
{
  SettingBoolSet("ui-show-efi-shell", !This->ToggleEnabled);
  This->ToggleEnabled = SettingBoolGet("ui-show-efi-shell");
  InvalidateActiveMenu();
  MainMenuUpdateUi();
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

  // efi shell
  Entry = MenuCreateEntry();
  Entry->Name = AsciiStrDup("Show EFI Shell");
  Entry->ShowToggle = TRUE;
  Entry->ToggleEnabled = SettingBoolGet("ui-show-efi-shell");
  Entry->HideBootMessage = TRUE;
  Entry->Callback = ShowEFIShellCallback;
  MenuAddEntry(Menu, Entry);

  // file explorer
  Entry = MenuCreateEntry();
  Entry->Name = AsciiStrDup("Show File Explorer");
  Entry->ShowToggle = TRUE;
  Entry->ToggleEnabled = SettingBoolGet("ui-show-file-explorer");
  Entry->HideBootMessage = TRUE;
  Entry->Callback = ShowFileExplorerCallback;
  MenuAddEntry(Menu, Entry);

  MenuStackPush(Menu);
  return EFI_SUCCESS;
}
