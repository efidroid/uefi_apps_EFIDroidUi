#include <aroma.h>

#include "Menu.h"

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL *mGop;
STATIC EFI_LK_DISPLAY_PROTOCOL *gLKDisplay;
STATIC MENU_OPTION* mActiveMenu = NULL;
STATIC LIBAROMA_CANVASP dc;
STATIC MINLIST *list = NULL;

EFI_STATUS
AromaInit (
  VOID
)
{
  bytep FontData;
  UINTN FontSize;
  EFI_STATUS Status;

  // get font data
  Status = GetSectionFromAnyFv (PcdGetPtr(PcdTruetypeFont), EFI_SECTION_RAW, 0, (VOID **) &FontData, &FontSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  /* init graph and font */
  if (!libaroma_fb_init()) {
    printf("cannot init framebuffer\n");
    return EFI_DEVICE_ERROR;
  }
  if (!libaroma_font_init()) {
    printf("Cannot init font\n");
    libaroma_fb_release();
    return EFI_DEVICE_ERROR;
  }
  if (!libaroma_lang_init()) {
    printf("cannot init lang");
    return EFI_DEVICE_ERROR;
  }

  /* load font */
  libaroma_font(0,
    libaroma_stream_mem(
      FontData, FontSize
    )
  );

  dc=libaroma_fb()->canvas;
 
  /* clean display */
  libaroma_canvas_blank(dc);

  Status = EFI_SUCCESS;

  return Status;
}

VOID
AromaRelease (
  VOID
)
{
  libaroma_lang_release();
  libaroma_font_release();
  libaroma_fb_release();
}
 
MINLIST * list_create(int width, int itemheight, word bg, word sl, word tbg, word tsl){
  MINLIST * list = (MINLIST *) calloc(sizeof(MINLIST),1);
  list->w = width;
  list->ih = itemheight;
  list->n = 0;
  list->bgcolor=bg;
  list->selcolor=sl;
  list->textcolor=tbg;
  list->textselcolor=tsl;
  return list;
}
 
void list_free(MINLIST * list){
  if (list->n>0){
    libaroma_canvas_free(list->cv);
    libaroma_canvas_free(list->cva);
  }
  free(list);
}
 
void list_add(MINLIST * list, CONST CHAR8 * text){
  int new_n  = list->n + 1;
  int item_y = list->n * list->ih;
  LIBAROMA_CANVASP cv = libaroma_canvas(list->w, list->ih*new_n);
  LIBAROMA_CANVASP cva= libaroma_canvas(list->w, list->ih*new_n);
 
  /* draw previous */
  if (list->n>0){
    libaroma_draw(cv,list->cv,0,0,0);
    libaroma_draw(cva,list->cva,0,0,0);
    libaroma_canvas_free(list->cv);
    libaroma_canvas_free(list->cva);
  }
 
  /* draw bg */
  libaroma_draw_rect(cv, 0, item_y, list->w, list->ih, list->bgcolor, 0xff);
 
  /* selected bg */
  libaroma_draw_rect(cva, 0, item_y, list->w, list->ih, list->selcolor, 0xff);
 
  /* draw text */
  LIBAROMA_TEXT txt = libaroma_text(
    text,
    list->textcolor, list->w,
    LIBAROMA_FONT(0,4)|LIBAROMA_TEXT_CENTER,
    100
  );
  if (txt){
    int txty=item_y + ((list->ih>>1)-(libaroma_text_height(txt)>>1));
    libaroma_text_draw(
      cv, txt, 0, txty
    );
    libaroma_text_draw_color(
      cva, txt, 0, txty, list->textselcolor
    );
    libaroma_text_free(txt);
  }
 
  list->cv  = cv;
  list->cva = cva;
  list->n   = new_n;
}
 
void list_show(MINLIST * list, int selectedid, int x, int y, int h){
 
  /* cleanup */
  libaroma_draw_rect(dc, x, y, list->w, h, list->bgcolor, 0xff);
 
  if (list->n<1){
    /* forget it */
    goto syncit;
  }
  if ((selectedid<0)||(selectedid>=list->n)){
    /* just blit non active canvas */
    libaroma_draw_ex(
      dc, list->cv, x, y, 0, 0,list->w, h, 0, 0xff
    );
    goto syncit;
  }
 
  int sel_y  = selectedid * list->ih;
 
  if (list->cv->h<h){
    /* dont need scroll */
    libaroma_draw_ex(
      dc, list->cv, x, y, 0, 0,list->w, h, 0, 0xff
    );
    libaroma_draw_ex(
      dc, list->cva, x, y+sel_y,0,sel_y,list->w, list->ih, 0, 0xff
    );
    goto syncit;
  }
 
 
  int sel_cy = sel_y + (list->ih>>1);
  int draw_y = (h>>1) - sel_cy;
  draw_y = (draw_y<0)?(0-draw_y):0;
  if (list->cv->h-draw_y<h){
    draw_y = list->cv->h-h;
  }
  libaroma_draw_ex(
    dc, list->cv, x, y, 0, draw_y,list->w, h, 0, 0xff
  );
  libaroma_draw_ex(
    dc, list->cva, x, y+(sel_y-draw_y),0,sel_y,list->w,list->ih, 0, 0xff
  );
 
  /* draw scroll indicator */
  int si_h = (h * h) / list->cv->h;
  int si_y = (selectedid+1) * (h-si_h);
  if (si_y>0){
    si_y /= list->n;
    int si_w = SCROLL_INDICATOR_WIDTH;
    int pad  = libaroma_dp(1);
   
    /* draw indicator */
    libaroma_draw_rect(dc, x+list->w-si_w,
      y+si_y, si_w, si_h, list->bgcolor, 0xff
    );
   
    libaroma_draw_rect(dc, x+list->w-si_w+pad,
      y+si_y+pad, si_w-pad*2, si_h-pad*2, list->textcolor, 0xff
    );
  }
 
syncit:
  libaroma_sync();
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
  InvalidateActiveMenu();
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
  if(Entry->FreeCallback)
    Entry->FreeCallback(Entry);
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
  InvalidateActiveMenu();
}

VOID
MenuRemoveEntry (
  MENU_OPTION  *Menu,
  MENU_ENTRY   *Entry
)
{
  RemoveEntryList (&Entry->Link);
  Menu->OptionNumber--;
  InvalidateActiveMenu();
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

  if(list) {
    list_free(list);
    list = NULL;
  }
}

MENU_OPTION*
GetActiveMenu(
  VOID
)
{
  return mActiveMenu;
}

VOID
InvalidateActiveMenu(
  VOID
)
{
  SetActiveMenu(mActiveMenu);
}

VOID
BuildAromaMenu (
  VOID
)
{
  LIST_ENTRY   *Link;
  MENU_ENTRY   *Entry;
  UINTN        Index;

  list = list_create(
    dc->w-libaroma_dp(32),
    libaroma_dp(48),
    RGB(000000),
    RGB(446688),
    RGB(ffffff),
    RGB(ffdd88)
  );

  Link = mActiveMenu->Head.ForwardLink;
  Index = 0;
  while (Link != NULL && Link != &mActiveMenu->Head) {
    Entry = CR (Link, MENU_ENTRY, Link, MENU_ENTRY_SIGNATURE);

    list_add(list, Entry->Description);

    Link = Link->ForwardLink;
    Index++;
  }
}

STATIC VOID
RenderActiveMenu(
  VOID
)
{
  if(mActiveMenu==NULL)
    return;

  if(list==NULL)
    BuildAromaMenu();

  int list_height = dc->h - libaroma_dp(32);

  /* draw frame */
  libaroma_draw_rect(dc, libaroma_dp(8), libaroma_dp(8), dc->w-libaroma_dp(16), list_height+libaroma_dp(16), RGB(666666), 0xff);

  if(list) {
    list_show(list, mActiveMenu->Selection, libaroma_dp(16), libaroma_dp(16), list_height);
  }
}

STATIC VOID
RenderBootScreen(
  MENU_ENTRY *Entry
)
{
  libaroma_canvas_blank(dc);

  LIBAROMA_TEXT txt = libaroma_text(
    "Booting",
    RGB(FFFFFF), dc->w,
    LIBAROMA_FONT(0,10)|LIBAROMA_TEXT_CENTER,
    150
  );
  int txty=(dc->h>>1) - libaroma_text_height(txt);
  if (txt){
    libaroma_text_draw(
      dc, txt, 0, txty
    );
    txty += libaroma_text_height(txt);
    libaroma_text_free(txt);
  }

  txt = libaroma_text(
    Entry->Description,
    RGB(A5C539), dc->w,
    LIBAROMA_FONT(0,10)|LIBAROMA_TEXT_CENTER,
    100
  );
  if (txt){
    libaroma_text_draw(
      dc, txt, 0, txty
    );
    libaroma_text_free(txt);
  }

  libaroma_sync();
}

VOID
EFIDroidEnterFrontPage (
  IN UINT16                 TimeoutDefault,
  IN BOOLEAN                ConnectAllHappened
  )
{
  EFI_STATUS Status;
  UINT32     OldMode;
  LK_DISPLAY_FLUSH_MODE     OldFlushMode;

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

  // manual flush mode
  OldFlushMode = gLKDisplay->GetFlushMode(gLKDisplay);
  gLKDisplay->SetFlushMode(gLKDisplay, LK_DISPLAY_FLUSH_MODE_MANUAL);

  Status = AromaInit();
  if (EFI_ERROR (Status)) {
    ASSERT(FALSE);
    return;
  }

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
            if (Entry->ResetGop) {
              mGop->SetMode(mGop, OldMode);
              gLKDisplay->SetFlushMode(gLKDisplay, OldFlushMode);
            }

            Entry->Callback(Entry->Private);

            if (Entry->ResetGop) {
              mGop->SetMode(mGop, gLKDisplay->GetPortraitMode());
              gLKDisplay->SetFlushMode(gLKDisplay, LK_DISPLAY_FLUSH_MODE_MANUAL);
            }
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

  AromaRelease();
  mGop->SetMode(mGop, OldMode);
  gLKDisplay->SetFlushMode(gLKDisplay, OldFlushMode);
}
