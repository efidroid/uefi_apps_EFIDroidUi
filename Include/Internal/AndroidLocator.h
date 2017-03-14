#ifndef __INTERNAL_ANDROIDLOCATOR_H__
#define __INTERNAL_ANDROIDLOCATOR_H__

#define MENU_ANDROID_BOOT_ENTRY_SIGNATURE   SIGNATURE_32 ('m', 'a', 'b', 'e')

typedef struct {
  UINTN                 Signature;
  bootimg_context_t     *context;
  multiboot_handle_t    *mbhandle;
  BOOLEAN               DisablePatching;
  BOOLEAN               IsRecovery;
  LAST_BOOT_ENTRY       LastBootEntry;
} MENU_ENTRY_PDATA;

#define RECOVERY_MENU_SIGNATURE             SIGNATURE_32 ('r', 'e', 'c', 'm')

typedef struct {
  UINTN           Signature;
  LIST_ENTRY      Link;

  MENU_OPTION     *SubMenu;
  MENU_ENTRY      *RootEntry;
  MENU_ENTRY      *BaseEntry;
  MENU_ENTRY      *NoPatchEntry;
} RECOVERY_MENU;

typedef struct {
  CHAR8 Name[30];
  CHAR8 IconPath[30];
  BOOLEAN IsRecovery;
  BOOLEAN IsDual;
} IMGINFO_CACHE;

EFI_STATUS
AndroidLocatorInit (
  VOID
);

EFI_STATUS
AndroidLocatorAddItems (
  VOID
);

EFI_STATUS
AndroidLocatorHandleRecoveryMode (
  LAST_BOOT_ENTRY *LastBootEntry
);

FSTAB*
AndroidLocatorGetMultibootFsTab (
  VOID
);

EFI_FILE_PROTOCOL*
AndroidLocatorGetEspDir (
  VOID
);

CONST CHAR8*
AndroidLocatorGetInternalROMName (
  VOID
);

CONST CHAR8*
AndroidLocatorGetInternalROMIconPath (
  VOID
);

UINTN
GetMenuIdFromLastBootEntry (
  MENU_OPTION     *Menu,
  LAST_BOOT_ENTRY *LastBootEntry
);

#endif /* __INTERNAL_ANDROIDLOCATOR_H__ */
