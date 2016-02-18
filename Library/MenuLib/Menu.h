#ifndef _EFIDROID_MENU_PRIVATE_H_
#define _EFIDROID_MENU_PRIVATE_H_

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DxeServicesLib.h>
#include <Library/Util.h>
#include <Library/Menu.h>

#include <Protocol/GraphicsOutput.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/LKDisplay.h>

#include <LittleKernel.h>
#include <aroma.h>

#define SCROLL_INDICATOR_WIDTH libaroma_dp(5)

typedef struct _MINLIST MINLIST;
struct _MINLIST {
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
  BOOLEAN enableshadow;
  BOOLEAN enablescrollbar;
};

#define MENU_STACK_SIGNATURE             SIGNATURE_32 ('m', 's', 't', 'k')

typedef struct {
  UINTN           Signature;
  LIST_ENTRY      Link;

  MENU_OPTION     *Menu;
} MENU_STACK;

byte libaroma_fb_init(void);
byte libaroma_fb_release(void);
byte libaroma_font_init(void);
byte libaroma_font_release(void);
byte libaroma_lang_init(void);
byte libaroma_lang_release(void);

#endif // _EFIDROID_MENU_PRIVATE_H_
