#include "EFIDroidUi.h"

STATIC VOID
CommandReboot (
  CHAR8 *Arg,
  VOID *Data,
  UINT32 Size
)
{
  FastbootOkay("");
  gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
}

VOID
FastbootCommandsAdd (
  VOID
)
{
  FastbootRegister("reboot", CommandReboot);
}
