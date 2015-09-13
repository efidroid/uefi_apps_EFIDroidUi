#include "Menu.h"

EFI_GRAPHICS_OUTPUT_PROTOCOL *mGop;
lkapi_t* mLKApi;

EFI_STATUS
EFIAPI
MenuLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  mLKApi = GetLKApi();
  return EFI_SUCCESS;
}

STATIC BOOT_MENU_ENTRY* mActiveMenu = NULL;
STATIC UINTN mActiveMenuSize = 0;
STATIC UINTN mActiveMenuPosition = 0;

BOOT_MENU_ENTRY*
MenuCreate (
  VOID
)
{
  return NULL;
}

BOOT_MENU_ENTRY*
MenuAddEntry (
  BOOT_MENU_ENTRY  **Menu,
  UINTN            *Size
)
{
  BOOT_MENU_ENTRY *NewMenu;

  NewMenu = ReallocatePool ((*Size)*sizeof(BOOT_MENU_ENTRY), ((*Size)+1)*sizeof(BOOT_MENU_ENTRY), *Menu);
  if(NewMenu==NULL)
    return NULL;
  *Menu = NewMenu;

  BOOT_MENU_ENTRY *NewEntry =  &NewMenu[(*Size)++];
  SetMem(NewEntry, sizeof(*NewEntry), 0);

  return NewEntry;
}

EFI_STATUS
MenuFinish (
  BOOT_MENU_ENTRY  **Menu,
  UINTN            *Size
)
{
  BOOT_MENU_ENTRY *Entry = MenuAddEntry(Menu, Size);
  if (Entry == NULL)
    return EFI_OUT_OF_RESOURCES;

  Entry->Description = NULL;
  Entry->Callback = NULL;

  return EFI_SUCCESS;
}


VOID
SetActiveMenu (
  BOOT_MENU_ENTRY* Menu
)
{
  BOOT_MENU_ENTRY* mEntry = Menu;

  // count entries
  UINTN NumEntries = 0;
  while(mEntry->Description) {
    NumEntries++;
    mEntry++;
  }

  mActiveMenu = Menu;
  mActiveMenuSize = NumEntries;
  mActiveMenuPosition = 0;
}

STATIC VOID
RenderActiveMenu(
  VOID
)
{
  UINTN LineHeight, TitleBottom;

  BOOT_MENU_ENTRY* mEntry = mActiveMenu;
  if(mEntry==NULL)
    return;

  ClearScreen();

  // draw title
  SetFontSize(56, 60);
  SetColor(0xff, 0xff, 0xff);
  LineHeight = TextLineHeight();
  CONST CHAR8 *Title = "EFIDroid";
  UINTN TitleWidth = TextLineWidth(Title);
  TextDrawAscii(Title, GetScreenWidth()/2 - TitleWidth/2, LineHeight);
  TitleBottom = LineHeight*2;

  // calculate item height
  SetFontSize(34, 40);
  LineHeight = TextLineHeight();

  // calculate vertical start position (put the center of the menu to the center of the screen)
  UINTN y = GetScreenHeight()/2 - (mActiveMenuSize*LineHeight)/2 + LineHeight;

  // move the active item to the screen's center
  if(mActiveMenuSize*LineHeight > GetScreenHeight()) {
    y += (mActiveMenuSize/2 - mActiveMenuPosition)*LineHeight;
  }

  // render all entries
  UINTN Count;
  for(Count=0; Count<mActiveMenuSize; Count++) {
    // get horizontal start position
    UINTN TextWidth = TextLineWidth(mActiveMenu[Count].Description);
    UINTN x = GetScreenWidth()/2 - TextWidth/2;

    if(Count==mActiveMenuPosition)
      SetColor(0xA5, 0xC5, 0x39);
    else
      SetColor(0xff, 0xff, 0xff);

    // draw text
    if(y>=TitleBottom && y<=GetScreenHeight())
      TextDrawAscii(mActiveMenu[Count].Description, x, y);

    y += LineHeight;
  }

  LCDFlush();
}

STATIC VOID
RenderBootScreen(
  BOOT_MENU_ENTRY *Entry
)
{
  UINTN LineHeight;

  ClearScreen();

  SetFontSize(56, 60);
  SetColor(0xff, 0xff, 0xff);
  LineHeight = TextLineHeight();
  CONST CHAR8 *Title = "EFIDroid";
  UINTN TitleWidth = TextLineWidth(Title);
  TextDrawAscii(Title, GetScreenWidth()/2 - TitleWidth/2, LineHeight);

  SetFontSize(34, 40);
  LineHeight = TextLineHeight();

  // calculate vertical start position
  UINTN y = GetScreenHeight()/2 - LineHeight*2/2 + LineHeight;

  CONST CHAR8* BootingStr = "Booting";
  UINTN TextWidth = TextLineWidth(BootingStr);
  UINTN x = GetScreenWidth()/2 - TextWidth/2;
  TextDrawAscii(BootingStr, x, y);
  y += LineHeight;

  SetColor(0xA5, 0xC5, 0x39);
  TextWidth = TextLineWidth(Entry->Description);
  x = GetScreenWidth()/2 - TextWidth/2;
  TextDrawAscii(Entry->Description, x, y);
  y += LineHeight;

  LCDFlush();
}

VOID
EFIDroidEnterFrontPage (
  IN UINT16                 TimeoutDefault,
  IN BOOLEAN                ConnectAllHappened
  )
{
  EFI_STATUS Status;

  // get graphics protocol
  Status = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **) &mGop);
  if (EFI_ERROR (Status)) {
    ASSERT(FALSE);
    return;
  }

  // set mode to initialize HW
  mGop->SetMode(mGop, 0);

  SetDensity(mLKApi->lcd_get_density());

  // initialize text engine
  TextInit();

  UINTN           WaitIndex;
  EFI_INPUT_KEY   Key;
  while(TRUE) {
    RenderActiveMenu();

    Status = gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &WaitIndex);
    ASSERT_EFI_ERROR (Status);

    Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);

    if(Key.ScanCode==SCAN_NULL) {
      switch(Key.UnicodeChar) {
        case CHAR_CARRIAGE_RETURN:
          RenderBootScreen(&mActiveMenu[mActiveMenuPosition]);

          if(mActiveMenu[mActiveMenuPosition].Callback)
            mActiveMenu[mActiveMenuPosition].Callback(mActiveMenu[mActiveMenuPosition].Private);
          break;
      }
    }
    else {
      switch(Key.ScanCode) {
        case SCAN_UP:
          if(mActiveMenuPosition>0)
            mActiveMenuPosition--;
          else mActiveMenuPosition = mActiveMenuSize-1;
          break;
        case SCAN_DOWN:
          if(mActiveMenuPosition+1<mActiveMenuSize)
            mActiveMenuPosition++;
          else mActiveMenuPosition = 0;
          break;
      }
    }
  }
}
