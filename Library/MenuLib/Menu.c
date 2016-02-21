#include <aroma.h>

#include "Menu.h"
#include <Library/UefiLib.h>

SCREENSHOT *gScreenShotList;

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL *mGop;
STATIC EFI_LK_DISPLAY_PROTOCOL *gLKDisplay = NULL;
STATIC MENU_OPTION* mActiveMenu = NULL;
STATIC LIBAROMA_CANVASP dc;
STATIC BOOLEAN Initialized = FALSE;
STATIC UINT32 mOldMode;
STATIC UINT32 mOurMode;
STATIC LK_DISPLAY_FLUSH_MODE OldFlushMode;
STATIC LIST_ENTRY mMenuStack;

word colorPrimary;
word colorPrimaryLight;
word colorPrimaryDark;
word colorText;

word colorAccent;
word colorTextPrimary;
word colorTextSecondary;

word colorSelection;
byte alphaSelection;
word colorSeparator;
byte alphaSeparator;
word colorBackground;

STATIC
EFI_STATUS
MenuTakeScreenShot (
  VOID
);

UINTN
MenuGetItemPosY (
  MENU_OPTION         *Menu,
  UINTN               Id
  );

MENU_ENTRY *
MenuGetEntryByUiId (
  MENU_OPTION         *Menu,
  UINTN               MenuNumber
  );

EFI_STATUS
AromaInit (
  VOID
)
{
  EFI_STATUS Status;

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
    libaroma_stream_ramdisk("fonts/Roboto-Regular.ttf")
  );
  libaroma_font(1,
    libaroma_stream_ramdisk("fonts/Roboto-Medium.ttf")
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

VOID
MenuDraw (
  MENU_OPTION *Menu,
  UINTN x,
  UINTN y,
  UINTN h
)
{
  UINTN        MenuWidth  = Menu->ListWidth?libaroma_dp(Menu->ListWidth):dc->w;

  /* cleanup */
  libaroma_draw_rect(dc, x, y, MenuWidth, h, libaroma_rgb_to16(Menu->BackgroundColor), 0xff);
  if (!Menu->cv || !Menu->cva){
    /* forget it */
    goto syncit;
  }

  MENU_ENTRY   *Entry;
  if (Menu->Selection<0 || (Entry = MenuGetEntryByUiId(Menu, Menu->Selection))==NULL){
    /* just blit non active canvas */
    libaroma_draw_ex(
      dc, Menu->cv, x, y, 0, 0,MenuWidth, h, 0, 0xff
    );
    goto syncit;
  }
 
  int sel_y  = libaroma_dp(MenuGetItemPosY(Menu, Menu->Selection));
  UINTN ItemHeight = libaroma_dp(Entry->ItemHeight);
  if (Menu->cv->h<h){
    /* dont need scroll */
    libaroma_draw_ex(
      dc, Menu->cv, x, y, 0, 0,MenuWidth, h, 0, 0xff
    );
    libaroma_draw_ex(
      dc, Menu->cva, x, y+sel_y,0,sel_y,MenuWidth, ItemHeight, 0, 0xff
    );
    goto syncit;
  }
 
  int sel_cy = sel_y + (ItemHeight>>1);
  int draw_y = (h>>1) - sel_cy;
  draw_y = (draw_y<0)?(0-draw_y):0;
  if (Menu->cv->h-draw_y<h){
    draw_y = Menu->cv->h-h;
  }
  libaroma_draw_ex(
    dc, Menu->cv, x, y, 0, draw_y,MenuWidth, h, 0, 0xff
  );
  libaroma_draw_ex(
    dc, Menu->cva, x, y+(sel_y-draw_y),0,sel_y,MenuWidth,ItemHeight, 0, 0xff
  );
  /* draw scroll indicator */
  if(Menu->EnableScrollbar) {
    int si_h = (h * h) / Menu->cv->h;
    int si_y = draw_y * h;
    if (si_y>0){
      si_y /= Menu->cv->h;
    }
    int si_w = SCROLL_INDICATOR_WIDTH;
    //int pad  = libaroma_dp(1);
    byte is_dark = libaroma_color_isdark(libaroma_rgb_to16(Menu->BackgroundColor));
    word indicator_color = is_dark?RGB(cccccc):RGB(666666);
    /* draw indicator */
    libaroma_draw_rect(dc, x+MenuWidth-si_w,  y+si_y, si_w-libaroma_dp(2),
      si_h, indicator_color, 120);
  }
 
syncit:
  /* draw shadow ;) */
  if(Menu->EnableShadow)
    libaroma_gradient_ex1(dc, x, y, MenuWidth,libaroma_dp(5),0,0,0,0,80,0,2);
}

#define APPBAR_FLAG_ICON_BACK   1 /* back arrow */
#define APPBAR_FLAG_ICON_DRAWER 2 /* drawer */
#define APPBAR_FLAG_SELECTED    4 /* selected */
#define APPBAR_FLAG_WIDEGAP     8 /* align text with text in listbox */
#define APPBAR_FLAG_ICON_SELECTED     16

VOID 
AppBarDraw (
  CHAR8            *text,
  word             bgcolor,
  word             textcolor,
  INT32            y,
  INT32            h,
  byte             flags,
  LIBAROMA_STREAMP icon
)
{
  /* draw appbar */
  libaroma_draw_rect(
    dc, 0, y, dc->w, h, bgcolor, 0xff
  );
  int txt_x = libaroma_dp(16);
 
  if ((flags&APPBAR_FLAG_ICON_BACK)||(flags&APPBAR_FLAG_ICON_DRAWER)){
    if (flags&APPBAR_FLAG_ICON_BACK){
      libaroma_art_arrowdrawer(
        dc,1,0,
        txt_x,
        y+libaroma_dp(16),
        libaroma_dp(24),
        textcolor,
        0xff, 0, 0.5
      );
    }
    else{
      libaroma_art_arrowdrawer(
        dc,1,1,
        txt_x,
        y+libaroma_dp(16),
        libaroma_dp(24),
        textcolor,
        0xff, 0, 0.5
      );
    }
    txt_x = libaroma_dp((flags&APPBAR_FLAG_WIDEGAP)?72:60);
     
    if (flags&APPBAR_FLAG_SELECTED){
      int sel_w=txt_x+libaroma_dp(16);
      LIBAROMA_CANVASP carea=libaroma_canvas_area(dc,0,y,sel_w,h);
      if (carea){
        int center_xy=(h>>1);
        libaroma_draw_circle(
          carea, textcolor, center_xy-libaroma_dp(16), center_xy, sel_w+libaroma_dp(20), 0x40
        );
        libaroma_canvas_free(carea);
      }
    }
  }
 
  LIBAROMA_TEXT txt = libaroma_text(
    text,
    textcolor,
    dc->w-txt_x,
    LIBAROMA_FONT(0,6)|
    LIBAROMA_TEXT_SINGLELINE|
    LIBAROMA_TEXT_LEFT|
    LIBAROMA_TEXT_BOLD|
    LIBAROMA_TEXT_FIXED_INDENT|
    LIBAROMA_TEXT_FIXED_COLOR|
    LIBAROMA_TEXT_NOHR,
    100
  );
  if (txt){
    int txty=y + ((h>>1)-((libaroma_text_height(txt)>>1))-libaroma_dp(2));
    libaroma_text_draw(
      dc, txt, txt_x, txty
    );
    libaroma_text_free(txt);
  }

  int dpsz=libaroma_dp(24);
  int icon_x = dc->w - dpsz - libaroma_dp(16);
  if(icon) {
    LIBAROMA_CANVASP ico  = libaroma_image_ex(icon, 0, 0);

    if(ico) {
      libaroma_draw_scale_smooth(
        dc, ico,
        icon_x,
        y + libaroma_dp(16),
        dpsz, dpsz,
        0, 0, ico->w, ico->h
      );
    }
  }

  if (flags&APPBAR_FLAG_ICON_SELECTED){
    int sel_w=libaroma_dp(16 + 24 + 16);
    LIBAROMA_CANVASP carea=libaroma_canvas_area(dc,icon_x - libaroma_dp(16),y,sel_w,h);
    if (carea){
      int center_x=(sel_w>>1);
      int center_y=(h>>1);
      libaroma_draw_circle(
        carea, textcolor, center_x + libaroma_dp(8), center_y, sel_w + libaroma_dp(16), 0x40
      );
      libaroma_canvas_free(carea);
    }
  }
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
  
  // signature
  Menu->Signature          = MENU_SIGNATURE;

  // entries
  InitializeListHead (&Menu->Head);

  // UI
  Menu->Title              = NULL;
  Menu->ListWidth          = 0;
  Menu->BackgroundColor    = 0x212121;
  Menu->SelectionColor     = 0xFFFFFF;
  Menu->SelectionAlpha     = 0x30;
  Menu->TextColor          = 0xFFFFFF;
  Menu->TextSelectionColor = 0xFFFFFF;
  Menu->EnableShadow       = TRUE;
  Menu->EnableScrollbar    = TRUE;
  Menu->ItemFlags          = MENU_ITEM_FLAG_SEPARATOR|MENU_ITEM_FLAG_SEPARATOR_ALIGN_TEXT;
  Menu->ActionIcon         = NULL;

  // callback
  Menu->BackCallback       = NULL;
  Menu->ActionCallback     = NULL;

  // selection
  Menu->OptionNumber       = 0;
  Menu->Selection          = 0;
  Menu->HideBackIcon       = FALSE;

  // private
  Menu->Private            = NULL;

  // internal
  Menu->cv                 = NULL;
  Menu->cva                = NULL;

  return Menu;
}

VOID
MenuFree (
  MENU_OPTION  *Menu
)
{
  // entries
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

  // UI
  if(Menu->Title)
    FreePool(Menu->Title);

  // internal
  InvalidateMenu(Menu);

  FreePool(Menu);
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

  // signature
  Entry->Signature  = MENU_ENTRY_SIGNATURE;

  // UI
  Entry->Name              = NULL;
  Entry->Description       = NULL;
  Entry->Icon              = NULL;
  Entry->ShowToggle        = FALSE;
  Entry->ToggleEnabled     = FALSE;
  Entry->ItemHeight        = 72;
  Entry->IsGroupItem       = FALSE;

  // selection
  Entry->Hidden            = FALSE;
  Entry->Selectable        = TRUE;

  // callback
  Entry->Callback          = NULL;
  Entry->LongPressCallback = NULL;
  Entry->ResetGop          = FALSE;
  Entry->HideBootMessage   = FALSE;

  // private
  Entry->Private           = NULL;
  Entry->FreeCallback      = NULL;
  Entry->CloneCallback     = NULL;

  return Entry;
}

MENU_ENTRY*
MenuCreateGroupEntry (
  VOID
)
{
  MENU_ENTRY *Entry;

  // allocate menu
  Entry = MenuCreateEntry();
  if(Entry==NULL)
    return NULL;

  Entry->ItemHeight        = 48;
  Entry->IsGroupItem       = TRUE;
  Entry->Selectable        = FALSE;

  return Entry;
}

MENU_ENTRY*
MenuCloneEntry (
  MENU_ENTRY* BaseEntry
)
{
  MENU_ENTRY* Entry;
  EFI_STATUS Status;

  Entry = MenuCreateEntry();
  if(Entry==NULL)
    return NULL;

  // UI
  if(BaseEntry->Name)
    Entry->Name            = AsciiStrDup(BaseEntry->Name);
  if(BaseEntry->Description)
    Entry->Description     = AsciiStrDup(BaseEntry->Description);
  Entry->Icon              = BaseEntry->Icon;
  Entry->ShowToggle        = BaseEntry->ShowToggle;
  Entry->ToggleEnabled     = BaseEntry->ToggleEnabled;
  Entry->ItemHeight        = BaseEntry->ItemHeight;
  Entry->IsGroupItem       = BaseEntry->IsGroupItem;

  // selection
  Entry->Hidden            = BaseEntry->Hidden;
  Entry->Selectable        = BaseEntry->Selectable;

  // callback
  Entry->Callback          = BaseEntry->Callback;
  Entry->LongPressCallback = BaseEntry->LongPressCallback;
  Entry->ResetGop          = BaseEntry->ResetGop;
  Entry->HideBootMessage   = BaseEntry->HideBootMessage;

  // private
  if(BaseEntry->CloneCallback) {
    Status = BaseEntry->CloneCallback(BaseEntry, Entry);
    if(EFI_ERROR(Status)) {
      MenuFreeEntry(Entry);
      return NULL;
    }
  }
  else {
    Entry->Private = BaseEntry->Private;
  }
  Entry->FreeCallback  = BaseEntry->FreeCallback;
  Entry->CloneCallback = BaseEntry->CloneCallback;

  return Entry;
}

MENU_OPTION*
MenuClone (
  MENU_OPTION  *Menu
)
{
  LIST_ENTRY   *Link;
  MENU_ENTRY   *Entry;
  UINTN        Index;
  MENU_OPTION* NewMenu;

  NewMenu = MenuCreate();
  if (NewMenu == NULL)
    return NULL;

  // entries
  Link = Menu->Head.ForwardLink;
  Index = 0;
  while (Link != NULL && Link != &Menu->Head) {
    Entry = CR (Link, MENU_ENTRY, Link, MENU_ENTRY_SIGNATURE);

    MENU_ENTRY* CloneEntry = MenuCloneEntry(Entry);
    if (CloneEntry) {
      MenuAddEntry(NewMenu, CloneEntry);
    }

    Link = Link->ForwardLink;
    Index++;
  }

  // UI
  if(Menu->Title)
    NewMenu->Title            = AsciiStrDup(Menu->Title);
  NewMenu->ListWidth          = Menu->ListWidth;
  NewMenu->BackgroundColor    = Menu->BackgroundColor;
  NewMenu->SelectionColor     = Menu->SelectionColor;
  NewMenu->SelectionAlpha     = Menu->SelectionAlpha;
  NewMenu->TextColor          = Menu->TextColor;
  NewMenu->TextSelectionColor = Menu->TextSelectionColor;
  NewMenu->EnableShadow       = Menu->EnableShadow;
  NewMenu->EnableScrollbar    = Menu->EnableScrollbar;
  NewMenu->ItemFlags          = Menu->ItemFlags;
  NewMenu->ActionIcon         = Menu->ActionIcon;

  // callback
  NewMenu->BackCallback       = Menu->BackCallback;
  NewMenu->ActionCallback     = Menu->ActionCallback;

  // selection
  NewMenu->HideBackIcon       = Menu->HideBackIcon;

  // private
  NewMenu->Private = Menu->Private;

  return NewMenu;
}

VOID
MenuFreeEntry (
  MENU_ENTRY* Entry
)
{
  // private
  if(Entry->FreeCallback)
    Entry->FreeCallback(Entry);

  // UI
  if(Entry->Name)
    FreePool(Entry->Name);
  if(Entry->Description)
    FreePool(Entry->Description);

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

UINTN
MenuGetSize (
  MENU_OPTION  *Menu
)
{
  UINTN Count = 0;
  LIST_ENTRY   *Link;
  MENU_ENTRY   *Entry;

  Link = Menu->Head.ForwardLink;
  while (Link != NULL && Link != &Menu->Head) {
    Entry = CR (Link, MENU_ENTRY, Link, MENU_ENTRY_SIGNATURE);

    if (!Entry->Hidden && Entry->Selectable)
      Count++;

    Link = Link->ForwardLink;
  }

  return Count;
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

  if (MenuNumber>=MenuOption->OptionNumber)
    return NULL;

  List = MenuOption->Head.ForwardLink;
  for (Index = 0; Index < MenuNumber; Index++) {
    List = List->ForwardLink;
  }

  NewMenuEntry = CR (List, MENU_ENTRY, Link, MENU_ENTRY_SIGNATURE);

  return NewMenuEntry;
}

MENU_ENTRY *
MenuGetEntryByUiId (
  MENU_OPTION         *Menu,
  UINTN               MenuNumber
  )
{
  LIST_ENTRY   *Link;
  MENU_ENTRY   *Entry;
  UINTN        Count = 0;
  UINTN MenuSize = MenuGetSize(Menu);

  if (MenuNumber>=MenuSize)
    return NULL;

  Link = Menu->Head.ForwardLink;
  while (Link != NULL && Link != &Menu->Head) {
    Entry = CR (Link, MENU_ENTRY, Link, MENU_ENTRY_SIGNATURE);

    if (!Entry->Hidden && Entry->Selectable) {
      if (Count==MenuNumber)
        return Entry;

      Count++;
    }

    Link = Link->ForwardLink;
  }

  return NULL;
}

VOID
InvalidateMenu (
  MENU_OPTION  *Menu
)
{
  if (Menu->cv) {
    libaroma_canvas_free(Menu->cv);
    Menu->cv = NULL;
  }
  if (Menu->cva) {
    libaroma_canvas_free(Menu->cva);
    Menu->cva = NULL;
  }
}

VOID
InvalidateActiveMenu (
  VOID
)
{
  InvalidateMenu(mActiveMenu);
}

UINTN
MenuGetHeight (
  MENU_OPTION         *Menu
  )
{
  LIST_ENTRY   *Link;
  MENU_ENTRY   *Entry;
  UINTN        Height = 0;

  Link = Menu->Head.ForwardLink;
  while (Link != NULL && Link != &Menu->Head) {
    Entry = CR (Link, MENU_ENTRY, Link, MENU_ENTRY_SIGNATURE);

    if (!Entry->Hidden) {
      Height += Entry->ItemHeight;
    }

    Link = Link->ForwardLink;
  }

  return Height;
}

UINTN
MenuGetItemPosY (
  MENU_OPTION         *Menu,
  UINTN               Id
  )
{
  LIST_ENTRY   *Link;
  MENU_ENTRY   *Entry;
  UINTN        Y = 0;
  UINTN        Index = 0;

  Link = Menu->Head.ForwardLink;
  while (Link != NULL && Link != &Menu->Head) {
    Entry = CR (Link, MENU_ENTRY, Link, MENU_ENTRY_SIGNATURE);

    if (!Entry->Hidden) {
      if (Entry->Selectable) {
        if (Index==Id)
          return Y;

        Index++;
      }

      Y += Entry->ItemHeight;
    }

    Link = Link->ForwardLink;
  }

  return Y;
}

#define DRAWBOTH(x) {\
  LIBAROMA_CANVASP c = cv; \
  x \
  c = cva; \
  x \
}

VOID
DrawListItem (
  IN MENU_OPTION   *Menu,
  IN MENU_ENTRY    *Entry,
  IN UINTN         item_y,
  IN UINTN         MenuHeight
)
{
  LIBAROMA_CANVASP cv  = Menu->cv;
  LIBAROMA_CANVASP cva = Menu->cva;
  UINTN MenuWidth  = Menu->ListWidth?libaroma_dp(Menu->ListWidth):dc->w;
  UINTN ItemHeight = libaroma_dp(Entry->ItemHeight);
 
  char text[256];
  if (Entry->Description!=NULL){
    word scolor = colorTextSecondary;
    snprintf(text,256,"<b>%s</b>\n<#%02X%02X%02X><size=3>%s</size></#>",Entry->Name?:"",
      libaroma_color_r(scolor),
      libaroma_color_g(scolor),
      libaroma_color_b(scolor),
      Entry->Description);
  }
  else{
    snprintf(text,256,"<b>%s</b>",Entry->Name?:"");
  }
 
  /* draw text */
  int left_pad=libaroma_dp(16);
  int right_pad=libaroma_dp(16);

  if(Entry->Icon)
    left_pad = libaroma_dp(72);
  if(Entry->ShowToggle)
    right_pad = libaroma_dp(72);

  LIBAROMA_TEXT txt = libaroma_text(
    text,
    libaroma_rgb_to16(Menu->TextColor), MenuWidth-(left_pad+right_pad),
    LIBAROMA_FONT(0,4),
    100
  );

  if (txt){
    int txty=item_y + ((ItemHeight>>1)-((libaroma_text_height(txt)>>1))-libaroma_dp(2));

    libaroma_text_draw(
      cv, txt, left_pad, txty
    );
    libaroma_text_draw_color(
      cva, txt, left_pad, txty, libaroma_rgb_to16(Menu->TextSelectionColor)
    );
    libaroma_text_free(txt);
  }
 
  if (Entry->Icon!=NULL){
    LIBAROMA_CANVASP ico =libaroma_image_ex(Entry->Icon, 0, 0);
    if (ico){
      int dpsz=libaroma_dp(40);
      int icoy=item_y + ((ItemHeight>>1) - (dpsz>>1));
      int icox=libaroma_dp(16);
      byte ismask=(Menu->ItemFlags&MENU_ITEM_FLAG_MASK_ICON_COLOR)?1:0;
       
      if (ismask){
        libaroma_canvas_fillcolor(ico,libaroma_alpha(libaroma_rgb_to16(Menu->BackgroundColor),libaroma_rgb_to16(Menu->TextColor),0xcc));
      }
      libaroma_draw_scale_smooth(
        cv, ico,
        icox,icoy,
        dpsz, dpsz,
        0, 0, ico->w, ico->h
      );
      if (ismask){
        libaroma_canvas_fillcolor(ico,libaroma_alpha(libaroma_rgb_to16(Menu->SelectionColor),libaroma_rgb_to16(Menu->TextSelectionColor),0xcc));
      }
      libaroma_draw_scale_smooth(
        cva, ico,
        icox,icoy,
        dpsz, dpsz,
        0, 0, ico->w, ico->h
      );
      libaroma_canvas_free(ico);
    }
  }

  if(Entry->ShowToggle) {
    int selected = (Entry->ToggleEnabled);
    float relstate=1;
    int xpos = MenuWidth - libaroma_dp(16 + 20);
    int ypos = item_y + (ItemHeight>>1);

    word h_color_rest   = RGB(ECECEC);
    word h_color_active = colorPrimary;
    word b_color_rest   = RGB(B2B2B2);
    word b_color_active = colorPrimaryLight;
    
    word bc0=selected?b_color_rest:b_color_active;
    word bc1=selected?b_color_active:b_color_rest;
    word hc0=selected?h_color_rest:h_color_active;
    word hc1=selected?h_color_active:h_color_rest;
      
    word bc = libaroma_alpha(bc0,bc1,relstate*0xff);
    word hc = libaroma_alpha(hc0,hc1,relstate*0xff);
    
    /* draw background */
    int b_width   = libaroma_dp(34);
    int b_height  = libaroma_dp(14);
    
    float selrelstate = selected?relstate:1-relstate;
    int base_x = xpos-(b_width>>1);
    int h_sz = libaroma_dp(20);
    int base_w = b_width - h_sz;
    int h_draw_x = base_x + round(base_w*selrelstate);
    int h_draw_y = ypos-(h_sz>>1);
    
    DRAWBOTH(
      libaroma_gradient_ex1(c,
        xpos-(b_width>>1),
        ypos-(b_height>>1),
        b_width,
        b_height,
        bc,bc,
        (b_height>>1),0x1111,
        0xff,0xff,
        0
      );
    );

    int rsz = libaroma_dp(1);
    
    LIBAROMA_CANVASP bmask = libaroma_canvas_ex(h_sz,h_sz,1);
    libaroma_canvas_setcolor(bmask,0,0);
    libaroma_gradient(bmask,0,0,h_sz,h_sz,0,0,h_sz>>1,0x1111);
    LIBAROMA_CANVASP scv = libaroma_blur_ex(bmask,rsz,1,0);
    libaroma_canvas_free(bmask);
    
    DRAWBOTH(
      libaroma_draw_opacity(c,scv,h_draw_x-rsz,h_draw_y,3,0x30);
    );
    libaroma_canvas_free(scv);

    /* handle */
    DRAWBOTH(
      libaroma_gradient_ex1(c,
        h_draw_x,
        h_draw_y,
        h_sz,
        h_sz,
        hc,hc,
        (h_sz>>1),0x1111,
        0xff,0xff,
        0
      );
    );
  }
 
  if ((Menu->ItemFlags & MENU_ITEM_FLAG_SEPARATOR) && item_y!=MenuHeight-ItemHeight){
    int sepxp=0;
    if ((Menu->ItemFlags & MENU_ITEM_FLAG_SEPARATOR_ALIGN_TEXT) && Entry->Icon){
      sepxp=libaroma_dp(72);
    }
    DRAWBOTH(
      libaroma_draw_rect(
        c,
        sepxp,
        item_y + ItemHeight - libaroma_dp(1),
        cv->w-sepxp,
        libaroma_dp(1),
        colorSeparator,
        alphaSeparator
      );
    );
  }

  if(Entry->LongPressCallback) {
    LIBAROMA_STREAMP icon = libaroma_stream_ramdisk("icons/ic_more_vert_white_24dp.png");
    LIBAROMA_CANVASP ico  = libaroma_image_ex(icon, 0, 0);
    if(ico) {
      int dpsz=libaroma_dp(30);
      int icoy=item_y + ((ItemHeight>>1) - (dpsz>>1));
      int icox=MenuWidth - ico->w - libaroma_dp(16);

      DRAWBOTH(
        libaroma_draw_scale_smooth(
          c, ico,
          icox,icoy,
          dpsz, dpsz,
          0, 0, ico->w, ico->h
        );
      );
    }
  }
}

VOID
DrawGroupItem (
  IN MENU_OPTION   *Menu,
  IN MENU_ENTRY    *Entry,
  IN UINTN         item_y,
  IN UINTN         MenuHeight
)
{
  LIBAROMA_CANVASP cv  = Menu->cv;
  UINTN MenuWidth  = Menu->ListWidth?libaroma_dp(Menu->ListWidth):dc->w;
  UINTN ItemHeight = libaroma_dp(Entry->ItemHeight);

  char text[256];
  snprintf(text,256,"<b>%s</b>",Entry->Name?:"");
 
  /* draw text */
  int left_pad=libaroma_dp(16);
  int right_pad=libaroma_dp(16);

  if (Menu->ItemFlags & MENU_ITEM_FLAG_SEPARATOR_ALIGN_TEXT)
    left_pad=libaroma_dp(72);

  LIBAROMA_TEXT txt = libaroma_text(
    text,
    colorTextSecondary, MenuWidth-(left_pad+right_pad),
    LIBAROMA_FONT(0,2),
    100
  );

  if (txt){
    int txty=item_y + ((ItemHeight>>1)-((libaroma_text_height(txt)>>1))-libaroma_dp(2));

    libaroma_text_draw(
      cv, txt, left_pad, txty
    );
    libaroma_text_free(txt);
  }

  if (!(Menu->ItemFlags & MENU_ITEM_FLAG_SEPARATOR)){
    int sepxp=0;
    if (Menu->ItemFlags & MENU_ITEM_FLAG_SEPARATOR_ALIGN_TEXT){
      sepxp=libaroma_dp(72);
    }
    libaroma_draw_rect(
      cv,
      sepxp,
      item_y - libaroma_dp(1),
      cv->w-sepxp,
      libaroma_dp(1),
      colorSeparator,
      alphaSeparator
    );
  }
}

VOID
BuildAromaMenu (
  IN MENU_OPTION* Menu
)
{
  LIST_ENTRY   *Link;
  MENU_ENTRY   *Entry;
  UINTN        Index;
  UINTN        item_y = 0;
  UINTN        MenuHeight = libaroma_dp(MenuGetHeight(Menu));
  UINTN        MenuWidth  = Menu->ListWidth?libaroma_dp(Menu->ListWidth):dc->w;

  InvalidateMenu(Menu);

  LIBAROMA_CANVASP cv  = libaroma_canvas(MenuWidth, MenuHeight);
  LIBAROMA_CANVASP cva = libaroma_canvas(MenuWidth, MenuHeight);
  Menu->cv  = cv;
  Menu->cva = cva;

  Link = Menu->Head.ForwardLink;
  Index = 0;
  while (Link != NULL && Link != &Menu->Head) {
    Entry = CR (Link, MENU_ENTRY, Link, MENU_ENTRY_SIGNATURE);
    UINTN ItemHeight = libaroma_dp(Entry->ItemHeight);

    if (Entry->Hidden)
      goto NEXT;

    DRAWBOTH(
      /* draw bg */
      libaroma_draw_rect(c, 0, item_y, MenuWidth, ItemHeight, libaroma_rgb_to16(Menu->BackgroundColor), 0xff);
    );
   
    /* selected bg */
    libaroma_draw_rect(cva, 0, item_y, MenuWidth, ItemHeight-libaroma_dp(1), libaroma_rgb_to16(Menu->SelectionColor), Menu->SelectionAlpha);

    if (Entry->IsGroupItem)
      DrawGroupItem(Menu, Entry, item_y, MenuHeight);
    else
      DrawListItem(Menu, Entry, item_y, MenuHeight);

    item_y += ItemHeight;

NEXT:
    Link = Link->ForwardLink;
    Index++;
  }
}

#undef DRAWBOTH

VOID
ButtonDraw (
  CONST CHAR8 *text,
  INT32       x,
  INT32       y,
  INT32       w,
  INT32       h
)
{
  /* draw text */
  LIBAROMA_TEXT textp = libaroma_text(
    text,
    colorAccent,
    w - libaroma_dp(16),
    LIBAROMA_FONT(1,4)|
    LIBAROMA_TEXT_SINGLELINE|
    LIBAROMA_TEXT_CENTER|
    LIBAROMA_TEXT_FIXED_INDENT|
    LIBAROMA_TEXT_FIXED_COLOR|
    LIBAROMA_TEXT_NOHR,
    100
  );

  int ty = y + (h>>1) - ((libaroma_text_height(textp)>>1));
  libaroma_text_draw(dc,textp,x + libaroma_dp(8),ty);

  libaroma_text_free(textp);
}

INT32
ButtonWidth (
  CONST CHAR8 *text
)
{
  /* draw text */
  LIBAROMA_TEXT textp = libaroma_text(
    text,
    colorAccent,
    dc->w,
    LIBAROMA_FONT(1,4)|
    LIBAROMA_TEXT_SINGLELINE|
    LIBAROMA_TEXT_CENTER|
    LIBAROMA_TEXT_FIXED_INDENT|
    LIBAROMA_TEXT_FIXED_COLOR|
    LIBAROMA_TEXT_NOHR,
    100
  );

  INT32 w = libaroma_dp(8) + libaroma_text_width(textp) + libaroma_dp(8);

  libaroma_text_free(textp);

  return w;
}

VOID
MenuDrawDarkBackground (
  VOID
)
{
  /* Mask Dark */
  libaroma_draw_rect(
    dc, 0, 0, dc->w, dc->h, RGB(000000), 0x7a
  );
}

INT32 MenuShowDialog(
  CONST CHAR8* Title,
  CONST CHAR8* Message,
  CONST CHAR8* Button1,
  CONST CHAR8* Button2
)
{
  UINT32 Selection = 0;
  UINT32 MaxSelection = (Button1?1:0) + (Button2?1:0);

  if(Initialized==FALSE)
    return -1;

  UINT32 OldMode = UINT32_MAX;
  if(mGop->Mode->Mode!=mOurMode) {
    OldMode = mGop->Mode->Mode;
    mGop->SetMode(mGop, mOurMode);
  }

REDRAW:
  MenuDrawDarkBackground();
  
  /* Init Message & Title Text */
  int dialog_w = dc->w-libaroma_dp(48);
  LIBAROMA_TEXT messagetextp = libaroma_text(
    Message,
    colorTextSecondary,
    dialog_w-libaroma_dp(48),
    LIBAROMA_FONT(0,4)|
    LIBAROMA_TEXT_LEFT|
    LIBAROMA_TEXT_FIXED_INDENT|
    LIBAROMA_TEXT_FIXED_COLOR|
    LIBAROMA_TEXT_NOHR,
    100
  );
  LIBAROMA_TEXT textp = libaroma_text(
    Title,
    colorTextPrimary,
    dialog_w-libaroma_dp(48),
    LIBAROMA_FONT(1,6)|
    LIBAROMA_TEXT_LEFT|
    LIBAROMA_TEXT_FIXED_INDENT|
    LIBAROMA_TEXT_FIXED_COLOR|
    LIBAROMA_TEXT_NOHR,
    100
  );

  int dialog_h =
    libaroma_dp(120)+
    libaroma_text_height(textp)+
    libaroma_text_height(messagetextp);
  int dialog_x = libaroma_dp(24);
  int dialog_y = (dc->h>>1)-(dialog_h>>1);
  
  /* create & draw dialog canvas */
  LIBAROMA_CANVASP dialog_canvas = libaroma_canvas_ex(dialog_w,dialog_h,1);
  libaroma_canvas_setcolor(dialog_canvas,0,0);
  libaroma_gradient(dialog_canvas,
    0,0,
    dialog_w, dialog_h,
    colorBackground,colorBackground,
    libaroma_dp(2), /* rounded 2dp */
    0x1111 /* all corners */
  );
  
  /* draw texts */
  libaroma_text_draw(
    dialog_canvas,
    textp,
    libaroma_dp(24),
    libaroma_dp(24)
  );

  /* draw text */
  libaroma_text_draw(
    dialog_canvas,
    messagetextp,
    libaroma_dp(24),
    libaroma_dp(24)+libaroma_text_height(textp) + libaroma_dp(20)
  );
  libaroma_text_free(textp);
  libaroma_text_free(messagetextp);
  
  /* draw fake shadow */
  int z;
  int shadow_sz=libaroma_dp(2);
  byte shadow_opa = (byte) (0x60 / shadow_sz);
  for (z=1;z<shadow_sz;z++){
    int wp=z*2;
    libaroma_gradient_ex(dc,
      dialog_x-z, dialog_y+(z>>1),
      dialog_w+wp, dialog_h+wp,
      0,0,
      libaroma_dp(4),
      0x1111,
      shadow_opa, shadow_opa
    );
  }

  /* draw dialog into display canvas */
  libaroma_draw(dc,dialog_canvas,dialog_x, dialog_y, 1);
  
  UINTN           WaitIndex;
  EFI_INPUT_KEY   Key;
  while(TRUE) {
    int button_y = dialog_y+dialog_h-libaroma_dp(52);
    
    /* clean button area */
    libaroma_draw_rect(dc, dialog_x, button_y, dialog_w, libaroma_dp(36), colorBackground, 0xff);
      
    if(Button1) {
      /* button1 */
      int button_w = ButtonWidth(Button1);
      int button_x = dialog_x+dialog_w-button_w-libaroma_dp(16);
      
      if(Selection==0) {
        libaroma_gradient_ex(dc,
          button_x,button_y,
          button_w, libaroma_dp(36),
          colorSelection,colorSelection,
          libaroma_dp(2), /* rounded 2dp */
          0x1111, /* all corners */
          0x40, 0x40 /* start & end alpha */
        );
      }
      ButtonDraw(Button1, button_x, button_y-libaroma_dp(2), button_w, libaroma_dp(36));
  
      /* button2 */
      if(Button2) {
        button_w = ButtonWidth(Button2);
        button_x -= (libaroma_dp(8)+button_w);
        if(Selection==1) {
          libaroma_gradient_ex(dc,
            button_x,button_y,
            button_w, libaroma_dp(36),
            colorSelection,colorSelection,
            libaroma_dp(2), /* rounded 2dp */
            0x1111, /* all corners */
            0x40, 0x40 /* start & end alpha */
          );
        }
        ButtonDraw(Button2, button_x, button_y-libaroma_dp(2), button_w, libaroma_dp(36));
      }
    }
    libaroma_sync();

    EFI_STATUS Status = gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &WaitIndex);
    ASSERT_EFI_ERROR (Status);

    Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);

    if(Key.ScanCode==SCAN_NULL) {
      switch(Key.UnicodeChar) {
        case CHAR_CARRIAGE_RETURN:
          RenderActiveMenu();
          if(OldMode!=UINT32_MAX)
            mGop->SetMode(mGop, OldMode);
          return Selection;

      // 's'
      case 0x73:
        MenuTakeScreenShot();
        goto REDRAW;
        break;
      }
    }
    else {
      switch(Key.ScanCode) {
        case SCAN_UP:
          if(Selection>0)
            Selection--;
          else Selection = MaxSelection-1;
          break;
        case SCAN_DOWN:
          if(Selection+1<MaxSelection)
            Selection++;
          else Selection = 0;
          break;
      }
    }
  }

  return -1;
}

VOID MenuShowMessage (
  CONST CHAR8* Title,
  CONST CHAR8* Message
)
{
  MenuShowDialog(Title, Message, "OK", NULL);
}

VOID MenuShowProgressDialog (
  CONST CHAR8* Text,
  BOOLEAN ShowBackground
)
{
  if(ShowBackground) {
    MenuDrawDarkBackground();
  }

  int dialog_w = dc->w-libaroma_dp(48);
  int dialog_h = libaroma_dp(88);
  int dialog_x = libaroma_dp(24);
  int dialog_y = (dc->h>>1)-(dialog_h>>1);

  /* draw fake shadow */
  int z;
  int shadow_sz=libaroma_dp(2);
  byte shadow_opa = (byte) (0x60 / shadow_sz);
  for (z=1;z<shadow_sz;z++){
    int wp=z*2;
    libaroma_gradient_ex(dc,
      dialog_x-z, dialog_y+(z>>1),
      dialog_w+wp, dialog_h+wp,
      0,0,
      libaroma_dp(4),
      0x1111,
      shadow_opa, shadow_opa
    );
  }

  libaroma_gradient(dc,
    dialog_x,dialog_y,
    dialog_w, dialog_h,
    colorBackground,colorBackground,
    libaroma_dp(2), /* rounded 2dp */
    0x1111 /* all corners */
  );

  LIBAROMA_TEXT txt = libaroma_text(
    Text,
    colorTextPrimary, dialog_w-libaroma_dp(16),
    LIBAROMA_FONT(0,5)|LIBAROMA_TEXT_CENTER,
    100
  );

  libaroma_text_draw(
    dc, txt, dialog_x + libaroma_dp(8), dialog_y + (dialog_h>>1) - (libaroma_text_height(txt)>>1)
  );

  libaroma_text_free(txt);

  libaroma_sync(); 
}

STATIC
VOID
MenuAddScreenShot (
  VOID* Data,
  UINTN Len
)
{
  SCREENSHOT *ScreenShot;

  ScreenShot = AllocatePool(sizeof(*ScreenShot));
  if (ScreenShot) {
    ScreenShot->Data = Data;
    ScreenShot->Len = Len;
    ScreenShot->Next = gScreenShotList;
    gScreenShotList = ScreenShot;
  }
}

STATIC
EFI_STATUS
MenuTakeScreenShot (
  VOID
)
{
  UINTN Len = dc->w*dc->h*4;
  VOID* Buffer = AllocateZeroPool(Len);
  if (Buffer ==NULL)
    return EFI_OUT_OF_RESOURCES;

  int rc = libaroma_png_save_buffer(dc, Buffer, Len);
  if(rc<=0) {
    FreePool(Buffer);
    return EFI_DEVICE_ERROR;
  }

  MenuAddScreenShot(Buffer, rc);
  MenuShowMessage("Info", "Screenshot taken");

  return EFI_SUCCESS;
}


STATIC
EFI_STATUS
MenuHandleKey (
  IN MENU_OPTION* Menu,
  IN EFI_INPUT_KEY Key,
  IN BOOLEAN SelectableBackItem
)
{
  MENU_ENTRY* Entry;
  EFI_STATUS Status = EFI_SUCCESS;
  UINTN MenuSize = MenuGetSize(Menu);

  if(Key.ScanCode==SCAN_NULL) {
    switch(Key.UnicodeChar) {
      case CHAR_CARRIAGE_RETURN:
        if(Menu->ActionCallback) {
          if(Menu->Selection==-1) {
            Status = Menu->ActionCallback(Menu);
            break;
          }
          else if(Menu->BackCallback && !Menu->HideBackIcon && Menu->Selection==-2) {
            if (Menu->BackCallback)
              Status = Menu->BackCallback(Menu);
            break;
          }
        }
        else {
          if(Menu->BackCallback && !Menu->HideBackIcon && Menu->Selection==-1) {
            if (Menu->BackCallback)
              Status = Menu->BackCallback(Menu);
            break;
          }
        }

        if(Menu->Selection>=MenuSize || Menu->Selection<0)
          break;

        Entry = MenuGetEntryByUiId(Menu, Menu->Selection);

        if(!Entry->HideBootMessage)
          RenderBootScreen(Entry);

        if(Entry->Callback) {
          if (Entry->ResetGop) {
            mGop->SetMode(mGop, mOldMode);
            if(gLKDisplay)
              gLKDisplay->SetFlushMode(gLKDisplay, OldFlushMode);
          }

          Status = Entry->Callback(Entry);

          if (Entry->ResetGop) {
            mGop->SetMode(mGop, mOurMode);
            if(gLKDisplay)
              gLKDisplay->SetFlushMode(gLKDisplay, LK_DISPLAY_FLUSH_MODE_MANUAL);
          }
        }
        break;

      // spacebar
      case 32:
        if(Menu->Selection>=MenuSize || Menu->Selection<0)
          break;

        Entry = MenuGetEntryByUiId(Menu, Menu->Selection);

        if(Entry->LongPressCallback) {
          Status = Entry->LongPressCallback(Entry);
        }

        break;

      // 's'
      case 0x73:
        MenuTakeScreenShot();
        break;

      // 'e'
      case 0x65:
        if (Menu->BackCallback)
          Status = Menu->BackCallback(Menu);
        break;
    }
  }
  else {
    INT32 MinSelection = 0;
    if(Menu->BackCallback && !Menu->HideBackIcon)
      MinSelection--;
    if(Menu->ActionCallback)
      MinSelection--;

    switch(Key.ScanCode) {
      case SCAN_UP:
        if(Menu->Selection>MinSelection)
          Menu->Selection--;
        else Menu->Selection = MenuSize-1;
        break;
      case SCAN_DOWN:
        if(Menu->Selection+1<MenuSize)
          Menu->Selection++;
        else Menu->Selection = MinSelection;
        break;
    }
  }

  return Status;
}

EFI_STATUS
MenuShowSelectionDialog (
  MENU_OPTION* Menu
)
{
  EFI_STATUS Status;

  if(Menu==NULL)
    return EFI_INVALID_PARAMETER;

  UINTN MenuSize = MenuGetSize(Menu);

  int dialog_w = dc->w-libaroma_dp(48);
  int dialog_h = MIN(libaroma_dp(72)*MenuSize, dc->h-libaroma_dp(48));
  int dialog_x = libaroma_dp(24);
  int dialog_y = (dc->h>>1)-(dialog_h>>1);

  // force some ui settings here
  Menu->ListWidth       = libaroma_px(dialog_w);
  Menu->EnableShadow    = FALSE;
  Menu->EnableScrollbar = FALSE;
  Menu->ItemFlags       = 0;
  BuildAromaMenu(Menu);

  MenuDrawDarkBackground();

  /* draw fake shadow */
  int z;
  int shadow_sz=libaroma_dp(2);
  byte shadow_opa = (byte) (0x60 / shadow_sz);
  for (z=1;z<shadow_sz;z++){
    int wp=z*2;
    libaroma_gradient_ex(dc,
      dialog_x-z, dialog_y+(z>>1),
      dialog_w+wp, dialog_h+wp,
      0,0,
      libaroma_dp(4),
      0x1111,
      shadow_opa, shadow_opa
    );
  }

  UINTN           WaitIndex;
  EFI_INPUT_KEY   Key;
  while(TRUE) {
    MenuDraw(Menu, dialog_x, dialog_y, dialog_h);
    libaroma_sync();

    Status = gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &WaitIndex);
    ASSERT_EFI_ERROR (Status);

    Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
    if(!EFI_ERROR(Status)) {
      Status = MenuHandleKey(Menu, Key, FALSE);
      if (Status == EFI_ABORTED)
        break;
    }
  }

  return 0;
}

VOID
RenderActiveMenu(
  VOID
)
{
  if(mActiveMenu->cv==NULL || mActiveMenu->cva==NULL) {
    BuildAromaMenu(mActiveMenu);
  }

  libaroma_canvas_blank(dc);

  int statusbar_height = libaroma_dp(24);
  int appbar_height    = libaroma_dp(56);
  int list_y           = statusbar_height + appbar_height;
  int list_height      = dc->h-list_y;

  /*
   * DRAW STATUSBAR
   */
  libaroma_draw_rect(
    dc, 0, 0, dc->w, statusbar_height, colorPrimaryDark, 0xff
  );
  libaroma_draw_text(
      dc,
      "EFIDroid",
      0, libaroma_dp(2) ,colorText, dc->w,
      LIBAROMA_FONT(0,3)|LIBAROMA_TEXT_CENTER,
      100
  );

  int appbar_flags = 0;
  if(mActiveMenu->BackCallback && !mActiveMenu->HideBackIcon)
    appbar_flags |= APPBAR_FLAG_ICON_BACK;

  if(mActiveMenu->ActionCallback) {
    if(mActiveMenu->Selection==-1)
      appbar_flags |= APPBAR_FLAG_ICON_SELECTED;
    else if(mActiveMenu->BackCallback && !mActiveMenu->HideBackIcon && mActiveMenu->Selection==-2)
      appbar_flags |= APPBAR_FLAG_SELECTED;
  }
  else {
    if(mActiveMenu->BackCallback && !mActiveMenu->HideBackIcon && mActiveMenu->Selection==-1)
      appbar_flags |= APPBAR_FLAG_SELECTED;
  }

  /* set appbar */
  AppBarDraw(
    mActiveMenu->Title?:"",
    colorPrimary,
    colorText,
    statusbar_height,
    appbar_height,
    appbar_flags,
    mActiveMenu->ActionIcon
  );

  MenuDraw(mActiveMenu, 0, list_y, list_height);
}

VOID
RenderBootScreen(
  MENU_ENTRY *Entry
)
{
  char text[256];
  snprintf(text, sizeof(text), "Booting %s ...", Entry->Name);

  MenuShowProgressDialog(text, TRUE);
}

VOID
MenuInit (
  VOID
  )
{
  EFI_STATUS Status;

  InitializeListHead(&mMenuStack);

  // get graphics protocol
  Status = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **) &mGop);
  if (EFI_ERROR (Status)) {
    ASSERT(FALSE);
    return;
  }

  // get LKDisplay protocol
  Status = gBS->LocateProtocol (&gEfiLKDisplayProtocolGuid, NULL, (VOID **) &gLKDisplay);
  if (EFI_ERROR (Status)) {
    gLKDisplay = NULL;
  }

  // backup current mode
  mOldMode = mGop->Mode->Mode;

  if(gLKDisplay) {
    // set our mode id
    mOurMode = gLKDisplay->GetPortraitMode();

    // manual flush mode
    OldFlushMode = gLKDisplay->GetFlushMode(gLKDisplay);
    gLKDisplay->SetFlushMode(gLKDisplay, LK_DISPLAY_FLUSH_MODE_MANUAL);
  }
  else {
    UINTN                                MaxMode;
    UINT32                               ModeIndex;
    UINTN                                SizeOfInfo;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *Info;
    UINT32                               BestModePixels = 0;
    UINT32                               BestModeIndex  = 0;

    // get max mode
    MaxMode = mGop->Mode->MaxMode;

    // get best GOP Mode
    for (ModeIndex = 0; ModeIndex < MaxMode; ModeIndex++) {
      Status = mGop->QueryMode (mGop, ModeIndex, &SizeOfInfo, &Info);
      if (!EFI_ERROR (Status)) {
        UINT32 Pixels = Info->HorizontalResolution * Info->VerticalResolution;
        if(ModeIndex==0 || BestModePixels<Pixels) {
          BestModePixels = ModeIndex;
          BestModeIndex = ModeIndex;
        }

        FreePool (Info);
      }
    }

    // set our mode
    mOurMode = BestModeIndex;
  }

  mGop->SetMode(mGop, mOurMode);

  Status = AromaInit();
  if (EFI_ERROR (Status)) {
    ASSERT(FALSE);
    return;
  }

  colorPrimary = RGB(4CAF50);
  colorPrimaryLight = RGB(C8E6C9);
  colorPrimaryDark = RGB(388E3C);
  colorText = RGB(FFFFFF);

  colorAccent = RGB(FF4081);
  colorTextPrimary = RGB(212121);
  colorTextSecondary = RGB(727272);

  colorSelection = RGB(000000);
  alphaSelection = 0x30;
  colorSeparator = RGB(dddddd);
  alphaSeparator = 0xff;
  colorBackground = RGB(FFFFFF);

#if 1
  colorText = RGB(000000);
  colorTextPrimary = RGB(FFFFFF);
  colorTextSecondary = RGB(E4E4E4);
  colorSelection = RGB(FFFFFF);
  colorSeparator = RGB(555555);
  colorBackground = RGB(212121);
#endif

  Initialized = TRUE;
}

VOID
MenuEnter (
  IN UINT16                 TimeoutDefault,
  IN BOOLEAN                ConnectAllHappened
  )
{
  UINTN           WaitIndex;
  EFI_INPUT_KEY   Key;
  EFI_STATUS      Status;

  while(TRUE) {
    if(mActiveMenu==NULL)
      break;

    RenderActiveMenu();
    libaroma_sync();

    Status = gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &WaitIndex);
    ASSERT_EFI_ERROR (Status);

    Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
    if(!EFI_ERROR(Status)) {
      Status = MenuHandleKey(mActiveMenu, Key, TRUE);
      if (Status == EFI_ABORTED)
        break;
    }
  }
}

VOID
MenuDeInit (
  VOID
  )
{
  Initialized = FALSE;
  AromaRelease();
  mGop->SetMode(mGop, mOldMode);
  if(gLKDisplay)
    gLKDisplay->SetFlushMode(gLKDisplay, OldFlushMode);
}

VOID
MenuPreBoot (
  VOID
)
{
  mGop->SetMode(mGop, mOldMode);
  if(gLKDisplay)
    gLKDisplay->SetFlushMode(gLKDisplay, OldFlushMode);
}

VOID
MenuPostBoot (
  VOID
)
{
  mGop->SetMode(mGop, mOurMode);
  if(gLKDisplay)
    gLKDisplay->SetFlushMode(gLKDisplay, LK_DISPLAY_FLUSH_MODE_MANUAL);
}

EFI_STATUS
MenuStackPushInternal (
  MENU_OPTION *Menu
)
{
  MENU_STACK *StackItem;

  // allocate stack item
  StackItem = AllocateZeroPool (sizeof(*StackItem));
  if(StackItem==NULL)
    return EFI_OUT_OF_RESOURCES;

  StackItem->Signature = MENU_STACK_SIGNATURE;
  StackItem->Menu = Menu;

  InsertHeadList (&mMenuStack, &StackItem->Link);

  return EFI_SUCCESS;
}

MENU_OPTION*
MenuStackPopInternal (
  VOID
)
{
  LIST_ENTRY *Entry;
  MENU_STACK *StackItem;
  MENU_OPTION* Menu = NULL;

  Entry = GetFirstNode (&mMenuStack);
  if (Entry != &mMenuStack) {
    StackItem = CR (Entry, MENU_STACK, Link, MENU_STACK_SIGNATURE);
    RemoveEntryList (Entry);
    Menu = StackItem->Menu;
    FreePool (StackItem);
  }

  return Menu;
}

VOID
MenuStackPush (
  MENU_OPTION *Menu
)
{
  if(mActiveMenu) {
    MenuStackPushInternal(mActiveMenu);
  }

  mActiveMenu = Menu;
}

MENU_OPTION*
MenuStackPop (
  VOID
)
{
  MENU_OPTION* OldMenu;

  OldMenu = mActiveMenu;

  mActiveMenu = MenuStackPopInternal();

  return OldMenu;
}
