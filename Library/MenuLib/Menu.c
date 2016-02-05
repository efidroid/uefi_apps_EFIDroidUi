#include <aroma.h>

#include "Menu.h"

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL *mGop;
STATIC EFI_LK_DISPLAY_PROTOCOL *gLKDisplay;
STATIC MENU_OPTION* mActiveMenu = NULL;
STATIC LIBAROMA_CANVASP dc;
STATIC MINLIST *list = NULL;
STATIC BOOLEAN Initialized = FALSE;

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
 
MINLIST * list_create(int width, int itemheight, word bg, word sl, word tbg, word tsl){
  MINLIST * list = (MINLIST *) calloc(sizeof(MINLIST),1);
  list->w = width;
  list->ih = itemheight;
  list->n = 0;
  list->bgcolor=bg;
  list->selcolor=sl;
  list->selalpha = alphaSelection;
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
 
#define LIST_ADD_MASK_ICON_COLOR        0x1 /* mask icon with text color */
#define LIST_ADD_WITH_SEPARATOR         0x2 /* add separator below item */
#define LIST_ADD_SEPARATOR_ALIGN_TEXT   0x4 /* align the separator line with text position */
void list_add(MINLIST * list, LIBAROMA_STREAMP icon, const char * title, const char * subtitle, byte flags){
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
  libaroma_draw_rect(cva, 0, item_y, list->w, list->ih, list->bgcolor, 0xff);
  libaroma_draw_rect(cva, 0, item_y+libaroma_dp(1), list->w, list->ih-libaroma_dp(1), list->selcolor, list->selalpha);
 
  char text[256];
  if (subtitle!=NULL){
    word scolor = colorTextSecondary;
    snprintf(text,256,"<b>%s</b>\n<#%02X%02X%02X><size=3>%s</size></#>",title?title:"",
      libaroma_color_r(scolor),
      libaroma_color_g(scolor),
      libaroma_color_b(scolor),
      subtitle);
  }
  else{
    snprintf(text,256,"<b>%s</b>",title?title:"");
  }
 
  /* draw text */
  int left_pad=libaroma_dp(72);
  int right_pad=libaroma_dp(16);
  LIBAROMA_TEXT txt = libaroma_text(
    text,
    list->textcolor, list->w-(left_pad+right_pad),
    LIBAROMA_FONT(0,4),
    100
  );
  if (txt){
    int txty=item_y + ((list->ih>>1)-((libaroma_text_height(txt)>>1))-libaroma_dp(2));
    libaroma_text_draw(
      cv, txt, left_pad, txty
    );
    libaroma_text_draw_color(
      cva, txt, left_pad, txty, list->textselcolor
    );
    libaroma_text_free(txt);
  }
 
  if (icon!=NULL){
    LIBAROMA_CANVASP ico =libaroma_image_ex(icon, 0, 0);
    if (ico){
      int dpsz=libaroma_dp(40);
      int icoy=item_y + ((list->ih>>1) - (dpsz>>1));
      int icox=libaroma_dp(16);
      byte ismask=(LIST_ADD_MASK_ICON_COLOR&flags)?1:0;
       
      if (ismask){
        libaroma_canvas_fillcolor(ico,libaroma_alpha(list->bgcolor,list->textcolor,0xcc));
      }
      libaroma_draw_scale_smooth(
        cv, ico,
        icox,icoy,
        dpsz, dpsz,
        0, 0, ico->w, ico->h
      );
      if (ismask){
        libaroma_canvas_fillcolor(ico,libaroma_alpha(list->selcolor,list->textselcolor,0xcc));
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
 
  if (LIST_ADD_WITH_SEPARATOR&flags){
    int sepxp=0;
    if (flags&LIST_ADD_SEPARATOR_ALIGN_TEXT){
      sepxp=libaroma_dp(72);
    }
    libaroma_draw_rect(
      cv,
      sepxp,
      item_y-libaroma_dp(1),
      cv->w-sepxp,
      libaroma_dp(1),
      colorSeparator,
      alphaSeparator
    );
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
  int si_y = draw_y * h;
  if (si_y>0){
    si_y /= list->cv->h;
  }
  int si_w = SCROLL_INDICATOR_WIDTH;
  //int pad  = libaroma_dp(1);
  byte is_dark = libaroma_color_isdark(list->bgcolor);
  word indicator_color = is_dark?RGB(cccccc):RGB(666666);
  /* draw indicator */
  libaroma_draw_rect(dc, x+list->w-si_w,  y+si_y, si_w-libaroma_dp(2),
    si_h, indicator_color, 120);
 
syncit:
  /* draw shadow ;) */
  libaroma_gradient_ex1(dc, x, y, list->w,libaroma_dp(5),0,0,0,0,80,0,2);
  //libaroma_sync();
}

#define APPBAR_FLAG_ICON_BACK   1 /* back arrow */
#define APPBAR_FLAG_ICON_DRAWER 2 /* drawer */
#define APPBAR_FLAG_SELECTED    4 /* selected */
#define APPBAR_FLAG_WIDEGAP     8 /* align text with text in listbox */
void appbar_draw(
  char * text,
  word bgcolor,
  word textcolor,
  int y,
  int h,
  byte flags
){
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
  //libaroma_sync();
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

  if(BaseEntry->Name)
    Entry->Name = AsciiStrDup(BaseEntry->Name);
  if(BaseEntry->Description)
    Entry->Description = AsciiStrDup(BaseEntry->Description);
  Entry->Callback = BaseEntry->Callback;
  Entry->ResetGop = BaseEntry->ResetGop;
  Entry->HideBootMessage = BaseEntry->HideBootMessage;
  Entry->Icon = BaseEntry->Icon;
  Entry->FreeCallback = BaseEntry->FreeCallback;
  Entry->CloneCallback = BaseEntry->CloneCallback;

  return Entry;
}

VOID
MenuFreeEntry (
  MENU_ENTRY* Entry
)
{
  if(Entry->FreeCallback)
    Entry->FreeCallback(Entry);
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
    dc->w,
    libaroma_dp(72),
    colorBackground,
    colorSelection,
    colorTextPrimary,
    colorTextPrimary
  );

  Link = mActiveMenu->Head.ForwardLink;
  Index = 0;
  while (Link != NULL && Link != &mActiveMenu->Head) {
    Entry = CR (Link, MENU_ENTRY, Link, MENU_ENTRY_SIGNATURE);

    list_add(list, Entry->Icon, Entry->Name, Entry->Description, LIST_ADD_WITH_SEPARATOR);

    Link = Link->ForwardLink;
    Index++;
  }
}

void button_draw(const char* text, int x, int y, int w, int h) {

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


int button_width(const char* text) {

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

  int w = libaroma_dp(8) + libaroma_text_width(textp) + libaroma_dp(8);

  libaroma_text_free(textp);

  return w;
}

STATIC VOID
RenderActiveMenu(
  VOID
);

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
  if(mGop->Mode->Mode!=gLKDisplay->GetPortraitMode()) {
    OldMode = mGop->Mode->Mode;
    mGop->SetMode(mGop, gLKDisplay->GetPortraitMode());
  }

  /* Mask Dark */
  libaroma_draw_rect(
    dc, 0, 0, dc->w, dc->h, RGB(000000), 0x7a
  );
  
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
      int button_w = button_width(Button1);
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
      button_draw(Button1, button_x, button_y-libaroma_dp(2), button_w, libaroma_dp(36));
  
      /* button2 */
      if(Button2) {
        button_w = button_width(Button2);
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
        button_draw(Button2, button_x, button_y-libaroma_dp(2), button_w, libaroma_dp(36));
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

VOID MenuShowMessage(
  CONST CHAR8* Title,
  CONST CHAR8* Message
)
{
  MenuShowDialog(Title, Message, "OK", NULL);
}

VOID MenuShowProgressDialog(
  CONST CHAR8* Text,
  BOOLEAN ShowBackground
)
{
  if(ShowBackground) {
    libaroma_draw_rect(
      dc, 0, 0, dc->w, dc->h, RGB(000000), 0x7a
    );
  }

  int dialog_w = dc->w-libaroma_dp(48);
  int dialog_h = libaroma_dp(88);
  int dialog_x = libaroma_dp(24);
  int dialog_y = (dc->h>>1)-(dialog_h>>1);

  libaroma_draw_rect(
    dc, dialog_x, dialog_y, dialog_w, dialog_h, colorBackground, 0xff
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

STATIC VOID
RenderActiveMenu(
  VOID
)
{
  if(mActiveMenu==NULL)
    return;

  if(list==NULL)
    BuildAromaMenu();

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
  if(mActiveMenu->BackCallback)
    appbar_flags |= APPBAR_FLAG_ICON_BACK;
  if(mActiveMenu->Selection==-1)
    appbar_flags |= APPBAR_FLAG_SELECTED;

  /* set appbar */
  appbar_draw(
    "Please Select OS",
    colorPrimary,
    colorText,
    statusbar_height,
    appbar_height,
    appbar_flags
  );

  if(list) {
    list_show(list, mActiveMenu->Selection, 0, list_y, list_height);
  }

  libaroma_sync();
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

STATIC EFI_STATUS Status;
STATIC UINT32     OldMode;
STATIC LK_DISPLAY_FLUSH_MODE     OldFlushMode;

VOID
MenuInit (
  VOID
  )
{
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
  MENU_ENTRY* Entry;
  while(TRUE) {
    RenderActiveMenu();

    Status = gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &WaitIndex);
    ASSERT_EFI_ERROR (Status);

    Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);

    if(Key.ScanCode==SCAN_NULL) {
      switch(Key.UnicodeChar) {
        case CHAR_CARRIAGE_RETURN:
          if(mActiveMenu->Selection==-1) {
            mActiveMenu->BackCallback();
            break;
          }

          if(mActiveMenu->Selection>=mActiveMenu->OptionNumber || mActiveMenu->Selection<0)
            break;
          
          Entry = MenuGetEntryById(mActiveMenu, mActiveMenu->Selection);

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

        // spacebar
        case 32:
          if(mActiveMenu->Selection>=mActiveMenu->OptionNumber || mActiveMenu->Selection<0)
            break;

          Entry = MenuGetEntryById(mActiveMenu, mActiveMenu->Selection);

          if(Entry->LongPressCallback) {
            Entry->LongPressCallback(Entry->Private);
          }

          break;
      }
    }
    else {
      INT32 MinSelection = mActiveMenu->BackCallback?-1:0;
      switch(Key.ScanCode) {
        case SCAN_UP:
          if(mActiveMenu->Selection>MinSelection)
            mActiveMenu->Selection--;
          else mActiveMenu->Selection = mActiveMenu->OptionNumber-1;
          break;
        case SCAN_DOWN:
          if(mActiveMenu->Selection+1<mActiveMenu->OptionNumber)
            mActiveMenu->Selection++;
          else mActiveMenu->Selection = MinSelection;
          break;
      }
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
  mGop->SetMode(mGop, OldMode);
  gLKDisplay->SetFlushMode(gLKDisplay, OldFlushMode);
}

VOID
MenuPreBoot (
  VOID
)
{
  mGop->SetMode(mGop, OldMode);
  gLKDisplay->SetFlushMode(gLKDisplay, OldFlushMode);
}

VOID
MenuPostBoot (
  VOID
)
{
  mGop->SetMode(mGop, gLKDisplay->GetPortraitMode());
  gLKDisplay->SetFlushMode(gLKDisplay, LK_DISPLAY_FLUSH_MODE_MANUAL);
}