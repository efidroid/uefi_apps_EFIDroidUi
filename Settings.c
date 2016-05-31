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
