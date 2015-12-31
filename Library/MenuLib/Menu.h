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
#include <PiDxe.h>
#include <Library/DxeServicesLib.h>
#include <LittleKernel.h>
#include <aroma.h>

#define SCROLL_INDICATOR_WIDTH libaroma_dp(5)

typedef struct {
  LIBAROMA_CANVASP cv;
  LIBAROMA_CANVASP cva;
  int n;  /* item count */
  int w;  /* list width */
  int ih; /* item height */
  word bgcolor;
  word selcolor;
  byte selalpha;
  word textcolor;
  word textselcolor;
} MINLIST;

byte libaroma_fb_init(void);
byte libaroma_fb_release(void);
byte libaroma_font_init(void);
byte libaroma_font_release(void);
byte libaroma_lang_init(void);
byte libaroma_lang_release(void);

#endif // _EFIDROID_MENU_PRIVATE_H_
