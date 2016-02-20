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

double round(double x);

#endif // _EFIDROID_MENU_PRIVATE_H_
