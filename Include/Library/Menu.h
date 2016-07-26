#ifndef MENU_H
#define MENU_H 1

#include <PiDxe.h>
#include <Guid/FileInfo.h>

#include <Library/BaseLib.h>

#define MENU_SIGNATURE             SIGNATURE_32 ('m', 'e', 'n', 'u')
#define MENU_ENTRY_SIGNATURE       SIGNATURE_32 ('e', 'n', 't', 'r')

#define MENU_ITEM_FLAG_MASK_ICON_COLOR        0x1 /* mask icon with text color */
#define MENU_ITEM_FLAG_SEPARATOR              0x2 /* add separator below item */
#define MENU_ITEM_FLAG_SEPARATOR_ALIGN_TEXT   0x4 /* align the separator line with text position */

typedef struct _LIBAROMA_CANVAS * LIBAROMA_CANVASP;

typedef struct _MENU_ENTRY MENU_ENTRY;
struct _MENU_ENTRY {
  UINTN           Signature;
  LIST_ENTRY      Link;

  // UI
  CHAR8* Name;
  CHAR8* Description;
  LIBAROMA_STREAMP Icon;
  BOOLEAN ShowToggle;
  BOOLEAN ToggleEnabled;
  UINTN ItemHeight;
  BOOLEAN IsGroupItem;

  // selection
  BOOLEAN Hidden;
  BOOLEAN Selectable;
  VOID (*Update) (struct _MENU_ENTRY* This);

  // callback
  EFI_STATUS (*Callback) (struct _MENU_ENTRY* This);
  EFI_STATUS (*LongPressCallback) (struct _MENU_ENTRY* This);
  BOOLEAN ResetGop;
  BOOLEAN HideBootMessage;

  // Private
  VOID *Private;
  VOID (*FreeCallback)(struct _MENU_ENTRY* Entry);
  EFI_STATUS (*CloneCallback)(struct _MENU_ENTRY* BaseEntry, struct _MENU_ENTRY* Entry);
};

typedef struct _MENU_OPTION MENU_OPTION;
struct _MENU_OPTION {
  UINTN           Signature;

  // entries
  LIST_ENTRY      Head;

  // UI
  CHAR8   *Title;
  UINTN   ListWidth;
  UINT32  BackgroundColor;
  UINT32  SelectionColor;
  UINT8   SelectionAlpha;
  UINT32  TextColor;
  UINT32  TextSelectionColor;
  BOOLEAN EnableShadow;
  BOOLEAN EnableScrollbar;
  UINT32  ItemFlags;
  LIBAROMA_STREAMP ActionIcon;

  // callback
  EFI_STATUS      (*BackCallback) (struct _MENU_OPTION* This);
  EFI_STATUS      (*ActionCallback) (struct _MENU_OPTION* This);

  // selection
  UINTN           OptionNumber;
  INT32           Selection;
  BOOLEAN         HideBackIcon;

  // private
  VOID            *Private;

  // internal
  LIBAROMA_CANVASP cv;
  LIBAROMA_CANVASP cva;
};

typedef struct _SCREENSHOT SCREENSHOT;
struct _SCREENSHOT {
  struct _SCREENSHOT *Next;
  VOID *Data;
  UINTN Len;
};

extern SCREENSHOT *gScreenShotList;

VOID
MenuInit (
  VOID
  );

VOID
MenuEnter (
  IN UINT16                 TimeoutDefault,
  IN BOOLEAN                ConnectAllHappened
  );

VOID
MenuDeInit (
  VOID
  );

MENU_OPTION*
MenuCreate (
  VOID
);

VOID
MenuFree (
  MENU_OPTION *Menu
);

MENU_ENTRY*
MenuCreateEntry (
  VOID
);

MENU_ENTRY*
MenuCreateGroupEntry (
  VOID
);

MENU_ENTRY*
MenuCloneEntry (
  MENU_ENTRY* BaseEntry
);

MENU_OPTION*
MenuClone (
  MENU_OPTION  *Menu
);

VOID
MenuFreeEntry (
  MENU_ENTRY* Entry
);

VOID
MenuAddEntry (
  MENU_OPTION  *Menu,
  MENU_ENTRY   *Entry
);

VOID
MenuRemoveEntry (
  MENU_OPTION  *Menu,
  MENU_ENTRY   *Entry
);


MENU_ENTRY *
MenuGetEntryById (
  MENU_OPTION         *MenuOption,
  UINTN               MenuNumber
  );

VOID
InvalidateMenu(
  MENU_OPTION  *Menu
);

VOID
InvalidateActiveMenu(
  VOID
);

VOID
RenderBootScreen(
  MENU_ENTRY *Entry
);

VOID
MenuDrawDarkBackground (
  VOID
);

VOID MenuShowMessage(
  CONST CHAR8* Title,
  CONST CHAR8* Message
);

INT32 MenuShowDialog(
  CONST CHAR8* Title,
  CONST CHAR8* Message,
  CONST CHAR8* Button1,
  CONST CHAR8* Button2
);

VOID MenuShowProgressDialog(
  CONST CHAR8* Text,
  BOOLEAN ShowBackground
);

EFI_STATUS
MenuShowSelectionDialog (
  MENU_OPTION* Menu
);

VOID
MenuPreBoot (
  VOID
);

VOID
MenuPostBoot (
  VOID
);

VOID
RenderActiveMenu(
  VOID
);

VOID
MenuStackPush (
  MENU_OPTION *Menu
);

MENU_OPTION*
MenuStackPop (
  VOID
);

#endif /* ! MENU_H */
