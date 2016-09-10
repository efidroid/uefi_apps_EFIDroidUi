#ifndef __INTERNAL_FASTBOOT_H__
#define __INTERNAL_FASTBOOT_H__

#define FASTBOOT_COMMAND_MAX_LENGTH 64

VOID
FastbootInit (
  VOID
);

VOID
FastbootInfo (
  CONST CHAR8 *Reason
);

VOID
FastbootFail (
  CONST CHAR8 *Reason
);

VOID
FastbootOkay (
  CONST CHAR8 *Info
);

VOID
FastbootRegister (
  CHAR8 *Prefix,
  VOID (*Handle)(CHAR8 *Arg, VOID *Data, UINT32 Size)
);

VOID
FastbootPublish (
  CONST CHAR8 *Name,
  CONST CHAR8 *Value
);

VOID
FastbootCommandsAdd (
  VOID
);

VOID
FastbootRequestStop (
  VOID
);

VOID
FastbootStopNow (
  VOID
);

VOID
FastbootSendString (
  IN CONST CHAR8 *Data,
  IN UINTN Size
);

VOID
FastbootSendBuf (
  IN CONST VOID *Data,
  IN UINTN Size
);

#endif /* __INTERNAL_FASTBOOT_H__ */
