/********************************************************************[libaroma]*
 * Copyright (C) 2011-2015 Ahmad Amarullah (http://amarullz.com/)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *______________________________________________________________________________
 *
 * Filename    : fb_driver.h
 * Description : UEFI framebuffer driver header
 *
 * + This is part of libaroma, an embedded ui toolkit.
 * + 03/03/15 - Author(s): Ahmad Amarullah
 *
 */
#ifndef __libaroma_uefi_fb_driver_h__
#define __libaroma_uefi_fb_driver_h__

/*
 * headers
 */

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Protocol/GraphicsOutput.h>
#include <Protocol/LKDisplay.h>
#include <Library/UefiBootServicesTableLib.h>
#include <aroma_internal.h>

typedef struct _UEFIFBDR_INTERNAL UEFIFBDR_INTERNAL;
typedef struct _UEFIFBDR_INTERNAL * UEFIFBDR_INTERNALP;
						
/*
 * structure : internal framebuffer data
 */
struct _UEFIFBDR_INTERNAL{
  EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
  EFI_LK_DISPLAY_PROTOCOL *lk_display;
  int       fb_sz;                      /* framebuffer memory size */
  voidp     buffer;                     /* direct buffer */
  int       stride;                     /* stride size */
  int       line;                       /* line size */
  byte      depth;                      /* color depth */
  byte      pixsz;                      /* memory size per pixel */
  byte      rgb_pos[6];                 /* framebuffer 32bit rgb position */
};

/* release function */
void UEFIFBDR_release(LIBAROMA_FBP me);

/* flush function */
byte UEFIFBDR_flush(LIBAROMA_FBP me);

/* start post */
byte UEFIFBDR_start_post(LIBAROMA_FBP me);

/* end post */
byte UEFIFBDR_end_post(LIBAROMA_FBP me);


#endif /* __libaroma_uefi_fb_driver_h__ */
