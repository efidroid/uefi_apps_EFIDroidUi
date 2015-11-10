#ifndef _EFIDROID_MENU_PRIVATE_H_
#define _EFIDROID_MENU_PRIVATE_H_

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/EFIDroid.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/LKDisplay.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <LittleKernel.h>

#define INCBIN(symname, sizename, filename, section)                    \
    __asm__ (".section " section "; .align 4; .globl "#symname);        \
    __asm__ (""#symname ":\n.incbin \"" filename "\"");                 \
    __asm__ (".section " section "; .align 1;");                        \
    __asm__ (""#symname "_end:");                                       \
    __asm__ (".section " section "; .align 4; .globl "#sizename);       \
    __asm__ (""#sizename ": .long "#symname "_end - "#symname " - 1");  \
    extern unsigned char symname[];                                     \
    extern unsigned int sizename

#define INCFILE(symname, sizename, filename) INCBIN(symname, sizename, filename, ".rodata")

extern EFI_GRAPHICS_OUTPUT_PROTOCOL *mGop;
extern EFI_LK_DISPLAY_PROTOCOL *gLKDisplay;

extern UINT8 mColorRed;
extern UINT8 mColorGreen;
extern UINT8 mColorBlue;
extern UINTN mDPI;
extern BOOLEAN mAutoFlush;

STATIC inline
INT32
dp2px (
  INT32 dp
)
{
  return ((dp * (INT32)mDPI) / 160);
}

STATIC inline
INT32
px2dp (
  INT32 px
)
{
  return ((px * 160) / (INT32)mDPI);
}

EFI_STATUS
TextInit (
  VOID
);

VOID
SetColor (
  UINT8 Red,
  UINT8 Green,
  UINT8 Blue
);

VOID
SetDensity (
  UINTN Density
);

VOID
SetFontSize (
  UINTN Size,
  UINTN LineHeight
);

VOID
TextDrawAscii (
  CONST CHAR8 *Str,
  INT32       DestinationX,
  INT32       DestinationY
);

UINTN
TextLineHeight (
  VOID
);

UINTN
TextLineWidth (
  CONST CHAR8* Str
);

VOID SetAutoFlushEnabled (
  BOOLEAN Enabled
);

VOID
LCDFlush (
  VOID
);

VOID
DrawRect(
  UINTN x1,
  UINTN y1,
  UINTN x2,
  UINTN y2
);

UINTN
GetScreenWidth(
  VOID
);

UINTN
GetScreenHeight(
  VOID
);

VOID
ClearScreen(
  VOID
);

#endif // _EFIDROID_MENU_PRIVATE_H_
