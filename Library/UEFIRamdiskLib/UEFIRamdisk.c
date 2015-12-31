#include <PiDxe.h>
#include <Library/DevicePathLib.h>
#include <Library/DebugLib.h>
#include <Library/Cpio.h>
#include <Library/DxeServicesLib.h>

EFI_STATUS
EFIAPI
UEFIRamdiskGetFile (
  CONST CHAR8 *Path,
  VOID  **Ptr,
  UINTN *Size
  )
{
  CPIO_NEWC_HEADER                    *Ramdisk;
  UINTN                               RamdiskSize;
  EFI_STATUS                          Status;

  // get ramdisk
  Status = GetSectionFromAnyFv (PcdGetPtr(PcdUEFIRamdisk), EFI_SECTION_RAW, 0, (VOID **) &Ramdisk, &RamdiskSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  if(!CpioIsValid (Ramdisk))
    return EFI_UNSUPPORTED;

  CPIO_NEWC_HEADER* CpioFile = CpioGetByName(Ramdisk, Path);
  if (!CpioFile)
    return EFI_NOT_FOUND;
  
  return CpioGetData(CpioFile, Ptr, Size);
}
