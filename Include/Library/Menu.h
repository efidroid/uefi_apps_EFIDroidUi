#ifndef MENU_H
#define MENU_H 1

#include <PiDxe.h>
#include <Guid/FileInfo.h>

#include <Library/BaseLib.h>

#define MENU_SIGNATURE             SIGNATURE_32 ('m', 'e', 'n', 'u')
#define MENU_ENTRY_SIGNATURE       SIGNATURE_32 ('e', 'n', 't', 'r')

typedef struct _MINLIST MINLIST;

typedef struct _MENU_ENTRY MENU_ENTRY;
struct _MENU_ENTRY {
  UINTN           Signature;
  LIST_ENTRY      Link;

  CHAR8* Name;
  CHAR8* Description;
  EFI_STATUS (*Callback) (struct _MENU_ENTRY* This);
  EFI_STATUS (*LongPressCallback) (struct _MENU_ENTRY* This);
  VOID *Private;
  BOOLEAN ResetGop;
  BOOLEAN HideBootMessage;
  LIBAROMA_STREAMP Icon;

  VOID (*FreeCallback)(struct _MENU_ENTRY* Entry);
  EFI_STATUS (*CloneCallback)(struct _MENU_ENTRY* BaseEntry, struct _MENU_ENTRY* Entry);
};

typedef struct _MENU_OPTION MENU_OPTION;
struct _MENU_OPTION {
  UINTN           Signature;
  LIST_ENTRY      Head;
  UINTN           OptionNumber;
  INT32           Selection;
  VOID            *Private;
  EFI_STATUS      (*BackCallback) (struct _MENU_OPTION* This);

  // private
  MINLIST* AromaList;
};

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
MenuCloneEntry (
  MENU_ENTRY* BaseEntry
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
SetActiveMenu(
  MENU_OPTION* Menu
);

MENU_OPTION*
GetActiveMenu(
  VOID
);

VOID
InvalidateMenu(
  MENU_OPTION  *Menu
);

VOID
RenderBootScreen(
  MENU_ENTRY *Entry
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

#endif /* ! MENU_H */
