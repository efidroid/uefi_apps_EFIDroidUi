#include "Menu.h"

EFI_GRAPHICS_OUTPUT_PROTOCOL *mGop;
EFI_LK_DISPLAY_PROTOCOL *gLKDisplay;
STATIC MENU_OPTION* mActiveMenu = NULL;

EFI_STATUS
EFIAPI
MenuLibConstructor (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  return EFI_SUCCESS;
}


MENU_OPTION*
MenuCreate (
  VOID
)
{
  MENU_OPTION *Menu;

  // allocate menu
  Menu = AllocateZeroPool (sizeof(*Menu));
  if(Menu==NULL)
    return NULL;
  
  // initialize entry list
  InitializeListHead (&Menu->Head);

  Menu->Signature = MENU_SIGNATURE;

  return Menu;
}

VOID
MenuFree (
  MENU_OPTION  *Menu
)
{
  MENU_ENTRY *MenuEntry;
  while (!IsListEmpty (&Menu->Head)) {
    MenuEntry = CR (
                  Menu->Head.ForwardLink,
                  MENU_ENTRY,
                  Link,
                  MENU_ENTRY_SIGNATURE
                  );
    RemoveEntryList (&MenuEntry->Link);
    MenuFreeEntry (MenuEntry);
  }
  Menu->OptionNumber = 0;
  Menu->Selection = 0;
}

MENU_ENTRY*
MenuCreateEntry (
  VOID
)
{
  MENU_ENTRY *Entry;

  // allocate menu
  Entry = AllocateZeroPool (sizeof(*Entry));
  if(Entry==NULL)
    return NULL;

  Entry->Signature = MENU_ENTRY_SIGNATURE;

  return Entry;
}

VOID
MenuFreeEntry (
  MENU_ENTRY* Entry
)
{
  FreePool(Entry);
}

VOID
MenuAddEntry (
  MENU_OPTION  *Menu,
  MENU_ENTRY   *Entry
)
{
  Menu->OptionNumber++;
  InsertTailList (&Menu->Head, &Entry->Link);
}

VOID
MenuRemoveEntry (
  MENU_OPTION  *Menu,
  MENU_ENTRY   *Entry
)
{
  RemoveEntryList (&Entry->Link);
  Menu->OptionNumber--;
}

MENU_ENTRY *
MenuGetEntryById (
  MENU_OPTION         *MenuOption,
  UINTN               MenuNumber
  )
{
  MENU_ENTRY      *NewMenuEntry;
  UINTN           Index;
  LIST_ENTRY      *List;

  ASSERT (MenuNumber < MenuOption->OptionNumber);

  List = MenuOption->Head.ForwardLink;
  for (Index = 0; Index < MenuNumber; Index++) {
    List = List->ForwardLink;
  }

  NewMenuEntry = CR (List, MENU_ENTRY, Link, MENU_ENTRY_SIGNATURE);

  return NewMenuEntry;
}

VOID
SetActiveMenu (
  MENU_OPTION* Menu
)
{
  mActiveMenu = Menu;
}

MENU_OPTION*
GetActiveMenu(
  VOID
)
{
  return mActiveMenu;
}

STATIC VOID
RenderActiveMenu(
  VOID
)
{
  UINTN LineHeight, TitleBottom;
  EFI_TPL      OldTpl;
  LIST_ENTRY   *Link;
  MENU_ENTRY   *Entry;
  UINTN        Index;

  if(mActiveMenu==NULL)
    return;

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);

  ClearScreen();

  // draw title
  SetFontSize(56, 60);
  SetColor(0xff, 0xff, 0xff);
  LineHeight = TextLineHeight();
  CONST CHAR8 *Title = "EFIDroid";
  UINTN TitleWidth = TextLineWidth(Title);
  TextDrawAscii(Title, GetScreenWidth()/2 - TitleWidth/2, LineHeight);
  TitleBottom = LineHeight*2;

  if(gErrorStr) {
    SetFontSize(20, 25);
    SetColor(0xff, 0x00, 0x0);
    LineHeight = TextLineHeight();
    TextDrawAscii(gErrorStr, GetScreenWidth()/2 - TextLineWidth(gErrorStr)/2, TitleBottom);
  }

  // calculate item height
  SetFontSize(34, 40);
  LineHeight = TextLineHeight();

  // calculate vertical start position (put the center of the menu to the center of the screen)
  UINTN y = GetScreenHeight()/2 - (mActiveMenu->OptionNumber*LineHeight)/2 + LineHeight;

  // move the active item to the screen's center
  if(mActiveMenu->OptionNumber*LineHeight > GetScreenHeight() - TitleBottom) {
    y += (mActiveMenu->OptionNumber/2 - mActiveMenu->Selection)*LineHeight;
  }

  // render all entries
  Link = mActiveMenu->Head.ForwardLink;
  Index = 0;
  while (Link != NULL && Link != &mActiveMenu->Head) {
    Entry = CR (Link, MENU_ENTRY, Link, MENU_ENTRY_SIGNATURE);

    // get horizontal start position
    UINTN TextWidth = TextLineWidth(Entry->Description);
    UINTN x = GetScreenWidth()/2 - TextWidth/2;

    if(Index==mActiveMenu->Selection)
      SetColor(0xA5, 0xC5, 0x39);
    else
      SetColor(0xff, 0xff, 0xff);

    // draw text
    if(y>=TitleBottom && y<=GetScreenHeight())
      TextDrawAscii(Entry->Description, x, y);

    y += LineHeight;
    
    Link = Link->ForwardLink;
    Index++;
  }

  LCDFlush();

  gBS->RestoreTPL (OldTpl);
}

STATIC VOID
RenderBootScreen(
  MENU_ENTRY *Entry
)
{
  UINTN LineHeight;
  EFI_TPL      OldTpl;

  OldTpl = gBS->RaiseTPL (TPL_NOTIFY);

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

  gBS->RestoreTPL (OldTpl);
}

VOID
EFIDroidEnterFrontPage (
  IN UINT16                 TimeoutDefault,
  IN BOOLEAN                ConnectAllHappened
  )
{
  EFI_STATUS Status;
  UINT32     OldMode;

  // get graphics protocol
  Status = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **) &mGop);
  if (EFI_ERROR (Status)) {
    ASSERT(FALSE);
    return;
  }

  // get LKDisplay protocol
  Status = gBS->LocateProtocol (&gEfiLKDisplayProtocolGuid, NULL, (VOID **) &gLKDisplay);
  if (EFI_ERROR (Status)) {
    ASSERT(FALSE);
    return;
  }

  // use portrait mode
  OldMode = mGop->Mode->Mode;
  mGop->SetMode(mGop, gLKDisplay->GetPortraitMode());

  SetDensity(gLKDisplay->GetDensity(gLKDisplay));

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
          if(mActiveMenu->Selection>=mActiveMenu->OptionNumber)
            break;
          
          MENU_ENTRY* Entry = MenuGetEntryById(mActiveMenu, mActiveMenu->Selection);

          if(!Entry->HideBootMessage)
            RenderBootScreen(Entry);

          if(Entry->Callback) {
            if (Entry->ResetGop)
              mGop->SetMode(mGop, OldMode);

            Entry->Callback(Entry->Private);

            if (Entry->ResetGop)
              mGop->SetMode(mGop, gLKDisplay->GetPortraitMode());
          }
          break;
      }
    }
    else {
      switch(Key.ScanCode) {
        case SCAN_UP:
          if(mActiveMenu->Selection>0)
            mActiveMenu->Selection--;
          else mActiveMenu->Selection = mActiveMenu->OptionNumber-1;
          break;
        case SCAN_DOWN:
          if(mActiveMenu->Selection+1<mActiveMenu->OptionNumber)
            mActiveMenu->Selection++;
          else mActiveMenu->Selection = 0;
          break;
      }
    }
  }

  mGop->SetMode(mGop, OldMode);
}
