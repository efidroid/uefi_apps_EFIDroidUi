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

STATIC VOID
CommandBoot (
  CHAR8 *Arg,
  VOID *Data,
  UINT32 Size
)
{
  FastbootOkay("");
  AndroidBootFromBuffer(Data, Size, NULL, TRUE);
}

VOID
FastbootCommandsAdd (
  VOID
)
{
  FastbootRegister("reboot", CommandReboot);
  FastbootRegister("boot", CommandBoot);
}
