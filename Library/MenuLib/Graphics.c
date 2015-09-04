#include "Menu.h"

UINT8 mColorRed   = 0xff;
UINT8 mColorGreen = 0xff;
UINT8 mColorBlue  = 0xff;
UINTN mDPI = 320;
BOOLEAN mAutoFlush = FALSE;

VOID
SetColor (
  UINT8 Red,
  UINT8 Green,
  UINT8 Blue
)
{
  mColorRed = Red;
  mColorGreen = Green;
  mColorBlue = Blue;
}

VOID SetAutoFlushEnabled (
  BOOLEAN Enabled
)
{
  mAutoFlush = Enabled;
}

VOID
LCDFlush (
  VOID
)
{
  // flush framebuffer contents
  mLKApi->lcd_flush();
}

VOID
DrawRect(
  UINTN x1,
  UINTN y1,
  UINTN x2,
  UINTN y2
)
{
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL px = {mColorBlue, mColorGreen, mColorRed, 0xff};
  mGop->Blt(mGop, &px, EfiBltVideoFill, 0, 0, dp2px(x1), dp2px(y1), dp2px(x2)-dp2px(x1), dp2px(y2)-dp2px(y1), 0);
}

UINTN
GetScreenWidth(
  VOID
)
{
  return px2dp(mGop->Mode->Info->HorizontalResolution);
}

UINTN
GetScreenHeight(
  VOID
)
{
  return px2dp(mGop->Mode->Info->VerticalResolution);
}

VOID
ClearScreen(
  VOID
)
{
  UINT8 *FrameBuffer         = (UINT8*)(UINTN)mGop->Mode->FrameBufferBase;
  UINTN HorizontalResolution = mGop->Mode->Info->HorizontalResolution;
  UINTN VerticalResolution   = mGop->Mode->Info->VerticalResolution;

  SetMem(FrameBuffer, HorizontalResolution*VerticalResolution*3, 0);
}
