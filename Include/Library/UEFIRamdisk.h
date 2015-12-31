#ifndef UEFIRAMDISK_H
#define UEFIRAMDISK_H 1

EFI_STATUS
EFIAPI
UEFIRamdiskGetFile (
  CONST CHAR8 *Path,
  VOID  **Ptr,
  UINTN *Size
  );

#endif /* ! UEFIRAMDISK_H */
