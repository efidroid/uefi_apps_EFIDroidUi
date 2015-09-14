#include "Menu.h"
#include <ft2build.h>
#include FT_FREETYPE_H

#define calc_alpha(dest, col, alpha) \
  (((dest * (255-alpha)) + (col * alpha)) >> 8)

INCFILE(font_normal, font_normal_sz, "fonts/Roboto-Regular.ttf");

STATIC FT_Library mLibrary;
STATIC FT_Face    mFace;
STATIC UINTN mFontSize = 14;
STATIC UINTN mLineHeight = 16;

EFI_STATUS
TextInit (
  VOID
) {
  INT32 error;

  // init freetype
  error = FT_Init_FreeType(&mLibrary);
  if(error) {
    DEBUG((EFI_D_ERROR, "FT_Init error\n"));
    return EFI_UNSUPPORTED;
  }

  // create memory faces for fonts
  error = FT_New_Memory_Face(mLibrary, font_normal, font_normal_sz, 0, &mFace);
  if(error) {
    DEBUG((EFI_D_ERROR, "FT_New_Memory_Face error\n"));
    return EFI_UNSUPPORTED;
  }

  // set initial fontsize
  SetFontSize(mFontSize, mLineHeight);

  return EFI_SUCCESS;
}

VOID
SetFontSize (
  UINTN Size,
  UINTN LineHeight
)
{
  mFontSize = Size;
  mLineHeight = LineHeight;
  INT32 error = FT_Set_Pixel_Sizes(mFace, 0, dp2px(Size));
  if(error) {
    DEBUG((EFI_D_ERROR, "FT_Set_Pixel_Sizes error\n"));
  }
}

STATIC
VOID
DrawGlyph (
  FT_Bitmap   *bitmap,
  FT_Int      DestinationX,
  FT_Int      DestinationY
)
{
  FT_Int x, y, p, q;
  UINT8 *Pixel;
  UINT8 Alpha;

  UINT8 *FrameBuffer         = (UINT8*)(UINTN)mGop->Mode->FrameBufferBase;
  UINTN HorizontalResolution = mGop->Mode->Info->HorizontalResolution;
  UINTN VerticalResolution   = mGop->Mode->Info->VerticalResolution;

  for(x=DestinationX, p=0; x<DestinationX + bitmap->width; x++, p++) {
    for(y=DestinationY, q=0; y<DestinationY + bitmap->rows; y++, q++) {
      // prevent drawing outside the framebuffer
      if (x<0 || y<0 || x >= HorizontalResolution || y >= VerticalResolution) {
        continue;
      }

      // get destination pixel
      Pixel = FrameBuffer + y*HorizontalResolution*3 + x*3;
      // get alpha value from glyph
      Alpha = bitmap->buffer[q * bitmap->width + p];

      // performance improvement: set alpha as color if alpha is 255
      if (Alpha==0xff){
        Pixel[0] = mColorBlue;
        Pixel[1] = mColorGreen;
        Pixel[2] = mColorRed;
      }
      // apply alpha to current color otherwise
      else if (Alpha>0){
        Pixel[0] = calc_alpha(Pixel[0], mColorBlue, Alpha);
        Pixel[1] = calc_alpha(Pixel[1], mColorGreen, Alpha);
        Pixel[2] = calc_alpha(Pixel[2], mColorRed, Alpha);
      }
    }
  }
}

VOID
TextDrawAscii (
  CONST CHAR8 *Str,
  INT32       DestinationX,
  INT32       DestinationY
)
{
  FT_GlyphSlot  slot = mFace->glyph;
  INT32 pen_x, pen_y, n;
  INT32 error;

  pen_x = dp2px(DestinationX);
  pen_y = dp2px(DestinationY);

  for(n=0; n<ft_strlen(Str); n++){
    FT_UInt  glyph_index;

    // retrieve glyph index from character code
    glyph_index = FT_Get_Char_Index(mFace, Str[n]);

    // load glyph image into the slot (erase previous one)
    error = FT_Load_Glyph(mFace, glyph_index, FT_LOAD_DEFAULT|FT_LOAD_TARGET_LCD);
    if(error) {
      continue;
    }

    // convert to an anti-aliased bitmap
    error = FT_Render_Glyph(mFace->glyph, FT_RENDER_MODE_NORMAL);
    if(error) {
      continue;
    }

    // now, draw to our target surface
    DrawGlyph(&slot->bitmap, pen_x+slot->bitmap_left, pen_y-slot->bitmap_top);

    // increment pen position
    pen_x += slot->advance.x >> 6;
    pen_y += slot->advance.y >> 6;
  }


  if(mAutoFlush) {
    LCDFlush();
  }
}

UINTN
TextLineHeight (
  VOID
)
{
  return mLineHeight;
}

UINTN
TextLineWidth (
  CONST CHAR8* Str
)
{
  UINTN Width = 0;
  INT32 n;
  INT32 error;
  FT_GlyphSlot  slot = mFace->glyph;

  for(n=0; n<ft_strlen(Str); n++){
    FT_UInt  glyph_index;

    // retrieve glyph index from character code
    glyph_index = FT_Get_Char_Index(mFace, Str[n]);

    // load glyph image into the slot (erase previous one)
    error = FT_Load_Glyph(mFace, glyph_index, FT_LOAD_DEFAULT);
    if(error) {
      continue;
    }

    // increment pen position
    Width += slot->advance.x >> 6;
  }

  return px2dp(Width);
}
