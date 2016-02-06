#ifndef UTIL_H
#define UTIL_H 1

#include <PiDxe.h>
#include <Guid/FileInfo.h>

#include <Library/BaseLib.h>
#include <Library/FileHandleLib.h>
#include <aroma.h>

#define ROUNDUP(a, b)   (((a) + ((b)-1)) & ~((b)-1))
#define ROUNDDOWN(a, b) ((a) & ~((b)-1))

#define STACKBUF_DMA_ALIGN(var, size) \
	UINT8 __##var[(size) + ArmDataCacheLineLength()]; UINT8 *var = (UINT8 *)(ROUNDUP((UINTN)__##var, ArmDataCacheLineLength()))

typedef
EFI_STATUS
(EFIAPI *PROTOCOL_INSTANCE_CALLBACK)(
  IN EFI_HANDLE           Handle,
  IN VOID                 *Instance,
  IN VOID                 *Context
  );

CHAR8*
Unicode2Ascii (
  CONST CHAR16* UnicodeStr
);

CHAR16*
Ascii2Unicode (
  CONST CHAR8* AsciiStr
);

CHAR8*
AsciiStrDup (
  IN CONST CHAR8* Str
  );

CHAR16*
UnicodeStrDup (
  IN CONST CHAR16* Str
  );

VOID
PathToUnix(
  CHAR16* fname
);

VOID
PathToUefi(
  CHAR16* fname
);

BOOLEAN
NodeIsDir (
  IN EFI_FILE_INFO      *NodeInfo
  );

LIBAROMA_STREAMP
libaroma_stream_ramdisk(
  CONST CHAR8* Path
);

UINT32
RangeOverlaps (
  UINT32 x1,
  UINT32 x2,
  UINT32 y1,
  UINT32 y2
);

UINT32
RangeLenOverlaps (
  UINT32 x,
  UINT32 xl,
  UINT32 y,
  UINT32 yl
);

UINT32
AlignMemoryRange (
  IN UINT32 Addr,
  IN OUT UINTN *Size,
  OUT UINTN  *AddrOffset,
  IN UINTN Alignment
);

EFI_STATUS
FreeAlignedMemoryRange (
  IN UINT32 Address,
  IN OUT UINTN Size,
  IN UINTN Alignment
);

EFI_STATUS
VisitAllInstancesOfProtocol (
  IN EFI_GUID                    *Id,
  IN PROTOCOL_INSTANCE_CALLBACK  CallBackFunction,
  IN VOID                        *Context
  );

#endif /* ! UTIL_H */
