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
 * Filename    : fb_driver.c
 * Description : UEFI framebuffer driver
 *
 * + This is part of libaroma, an embedded ui toolkit.
 * + 26/01/15 - Author(s): Ahmad Amarullah
 *
 */

/* include fb_driver.h */
#include "fb_driver.h"

/*
 * Function    : UEFIFBDR_start_post
 * Return Value: byte
 * Descriptions: start post
 */
byte UEFIFBDR_start_post(LIBAROMA_FBP me){
  if (me == NULL) {
    return 0;
  }
  return 1;
}

/*
 * Function    : UEFIFBDR_end_post
 * Return Value: byte
 * Descriptions: end post
 */
byte UEFIFBDR_end_post(LIBAROMA_FBP me){
  if (me == NULL) {
    return 0;
  }
  UEFIFBDR_flush(me);
  return 1;
}

/*
 * Function    : UEFIFBDR_setrgbpos
 * Return Value: void
 * Descriptions: set rgbx position
 */
void UEFIFBDR_setrgbpos(LIBAROMA_FBP me, byte r, byte g, byte b) {
  if (me == NULL) {
    return;
  }
  UEFIFBDR_INTERNALP mi = (UEFIFBDR_INTERNALP) me->internal;
  /* save color position */
  mi->rgb_pos[0] = r;
  mi->rgb_pos[1] = g;
  mi->rgb_pos[2] = b;
  mi->rgb_pos[3] = r >> 3;
  mi->rgb_pos[4] = g >> 3;
  mi->rgb_pos[5] = b >> 3;
}

/*
 * Function    : UEFIFBDR_post_bgra8888
 * Return Value: byte
 * Descriptions: post data
 */
byte UEFIFBDR_post_bgra8888(
  LIBAROMA_FBP me, wordp __restrict src,
  int dx, int dy, int dw, int dh,
  int sx, int sy, int sw, int sh
  ){
  if (me == NULL) {
    return 0;
  }
  UEFIFBDR_INTERNALP mi = (UEFIFBDR_INTERNALP) me->internal;
  int sstride = (sw - dw) * 2;
  int dstride = (mi->line - (dw * mi->pixsz));
  dwordp copy_dst =
    (dwordp) (((bytep) mi->buffer)+(mi->line * dy)+(dx * mi->pixsz));
  wordp copy_src = 
    (wordp) (src + (sw * sy) + sx);
  libaroma_blt_align_to32_pos(
    copy_dst,
    copy_src,
    dw, dh,
    dstride, 
    sstride,
    mi->rgb_pos
  );
  return 1;
}

/*
 * Function    : UEFIFBDR_post_bltonly
 * Return Value: byte
 * Descriptions: post data
 */
byte UEFIFBDR_post_bltonly(
  LIBAROMA_FBP me, wordp __restrict src,
  int dx, int dy, int dw, int dh,
  int sx, int sy, int sw, int sh
  ){
  if (me == NULL) {
    return 0;
  }

  UEFIFBDR_INTERNALP mi = (UEFIFBDR_INTERNALP) me->internal;

  if(UEFIFBDR_post_bgra8888(me, src, dx, dy, dw, dh, sx, sy, sw, sh)==0)
    return 0;

  mi->gop->Blt(mi->gop, mi->buffer, EfiBltBufferToVideo, 0, 0, 0, 0, me->w, me->h, 0);
  return 1;
}

/*
 * Function    : UEFIFBDR_post_bgr888
 * Return Value: byte
 * Descriptions: post data
 */
byte UEFIFBDR_post_bgr888(
  LIBAROMA_FBP me, wordp __restrict src,
  int dx, int dy, int dw, int dh,
  int sx, int sy, int sw, int sh
  ){
  if (me == NULL) {
    return 0;
  }
  UEFIFBDR_INTERNALP mi = (UEFIFBDR_INTERNALP) me->internal;
  int sstride = (sw - dw) * 2;
  int dstride = (mi->line - (dw * mi->pixsz));
  bytep copy_dst =
    (bytep) (((bytep) mi->buffer)+(mi->line * dy)+(dx * mi->pixsz));
  wordp copy_src =
    (wordp) (src + (sw * sy) + sx);
  libaroma_blt_align24(
    copy_dst,
    copy_src,
    dw, dh,
    dstride,
    sstride
  );
  return 1;
}

/*
 * Function    : UEFIFBDR_snapshoot
 * Return Value: byte
 * Descriptions: get snapshoot
 */
byte UEFIFBDR_snapshot(LIBAROMA_FBP me, wordp dst) {
  if (me == NULL) {
    return 0;
  }
  UEFIFBDR_INTERNALP mi = (UEFIFBDR_INTERNALP) me->internal;
  libaroma_blt_align_to16_pos(
    dst, (dwordp) mi->buffer, me->w, me->h,
    0, mi->stride, mi->rgb_pos);
  return 1;
}

/*
 * Function    : UEFIFBDR_init
 * Return Value: byte
 * Descriptions: init framebuffer
 */
byte UEFIFBDR_init(LIBAROMA_FBP me) {
  EFI_STATUS Status;

  ALOGV("UEFIFBDR initialized internal data");
  
  /* allocating internal data */
  UEFIFBDR_INTERNALP mi = (UEFIFBDR_INTERNALP)
                      calloc(sizeof(UEFIFBDR_INTERNAL),1);
  if (!mi) {
    ALOGE("UEFIFBDR calloc internal data - memory error");
    return 0;
  }

  // get graphics protocol
  Status = gBS->LocateProtocol (&gEfiGraphicsOutputProtocolGuid, NULL, (VOID **) &mi->gop);
  if (EFI_ERROR (Status)) {
    ASSERT(FALSE);
    goto error;
  }

  // get LKDisplay protocol
  Status = gBS->LocateProtocol (&gEfiLKDisplayProtocolGuid, NULL, (VOID **) &mi->lk_display);
  if (EFI_ERROR (Status)) {
    mi->lk_display = NULL;
  }
  
  /* set internal address */
  me->internal = (voidp) mi;
  
  /* set release callback */
  me->release = &UEFIFBDR_release;

  /* set libaroma framebuffer instance values */
  me->w = mi->gop->Mode->Info->HorizontalResolution;    /* width */
  me->h = mi->gop->Mode->Info->VerticalResolution;    /* height */
  me->sz = me->w * me->h;   /* width x height */
  
  /* set internal useful data */
  mi->buffer    = (void*)(UINTN)mi->gop->Mode->FrameBufferBase;

  EFI_GRAPHICS_OUTPUT_MODE_INFORMATION* ModeInfo = mi->gop->Mode->Info;
  switch(ModeInfo->PixelFormat) {
    case PixelBlueGreenRedReserved8BitPerColor:
      me->post        = &UEFIFBDR_post_bgra8888;
      mi->depth     = 32;
      UEFIFBDR_setrgbpos(me,16,8,0);
      break;

    case PixelRedGreenBlueReserved8BitPerColor:
      me->post        = &UEFIFBDR_post_bgra8888;
      mi->depth     = 32;
      UEFIFBDR_setrgbpos(me,0,8,16);
      break;

    // just assume bgr888 for now
    case PixelBitMask:
      me->post        = &UEFIFBDR_post_bgr888;
      mi->depth     = 24;
      break;

    case PixelBltOnly:
      me->post        = &UEFIFBDR_post_bltonly;
      mi->depth     = 32;
      UEFIFBDR_setrgbpos(me,16,8,0);
      mi->buffer = malloc((me->w * me->h * (mi->depth >> 3)));
      break;

    default:
      ASSERT(FALSE);
      goto error;
  }
  mi->pixsz     = mi->depth >> 3;
  mi->line      = me->w * mi->pixsz;
  mi->fb_sz     = (me->w * me->h * mi->pixsz);
  
  /* swap buffer now */
  UEFIFBDR_flush(me);
 
  mi->stride = mi->line - (me->w * mi->pixsz);
  me->start_post  = &UEFIFBDR_start_post;
  me->end_post    = &UEFIFBDR_end_post;
  me->snapshoot   = &UEFIFBDR_snapshot;

  if(mi->lk_display)
    me->dpi = mi->lk_display->GetDensity(mi->lk_display);
  
  /* ok */
  goto ok;
  /* return */
error:
  free(mi);
  return 0;
ok:
  return 1;
} /* End of UEFIFBDR_init */

/*
 * Function    : UEFIFBDR_release
 * Return Value: void
 * Descriptions: release framebuffer driver
 */
void UEFIFBDR_release(LIBAROMA_FBP me) {
  if (me==NULL) {
    return;
  }
  UEFIFBDR_INTERNALP mi = (UEFIFBDR_INTERNALP) me->internal;
  if (mi==NULL){
    return;
  }

  if(me->post == UEFIFBDR_post_bltonly)
    free(mi->buffer);
  
  /* free internal data */
  ALOGV("UEFIFBDR free internal data");
  free(me->internal);
} /* End of UEFIFBDR_release */

/*
 * Function    : UEFIFBDR_flush
 * Return Value: byte
 * Descriptions: flush content into display & wait for vsync
 */
byte UEFIFBDR_flush(LIBAROMA_FBP me) {
  if (me == NULL) {
    return 0;
  }
  UEFIFBDR_INTERNALP mi = (UEFIFBDR_INTERNALP) me->internal;

  if(mi->lk_display)
    mi->lk_display->FlushScreen(mi->lk_display);

  return 1;
} /* End of UEFIFBDR_flush */



/*
 * Function    : libaroma_fb_driver_init
 * Return Value: byte
 * Descriptions: init function for libaroma fb
 */
byte libaroma_fb_driver_init(LIBAROMA_FBP me) {
  return UEFIFBDR_init(me);
} /* End of libaroma_fb_driver_init */
