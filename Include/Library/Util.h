#ifndef UTIL_H
#define UTIL_H 1

#include <PiDxe.h>
#include <Guid/FileInfo.h>

#include <Library/BaseLib.h>
#include <Library/FileHandleLib.h>

#ifndef LIBUTIL_NOAROMA
#include <aroma.h>
#endif

#define ROUNDUP(a, b)   (((a) + ((b)-1)) & ~((b)-1))
#define ROUNDDOWN(a, b) ((a) & ~((b)-1))

#if defined (MDE_CPU_IA32)
#define CACHE_LINE 32
#elif defined (MDE_CPU_X64)
#define CACHE_LINE 32
#elif defined (MDE_CPU_ARM)
#define CACHE_LINE ArmDataCacheLineLength()
#else
#error "Unsupported platform"
#endif

#define STACKBUF_DMA_ALIGN(var, size) \
	UINT8 __##var[(size) + CACHE_LINE]; UINT8 *var = (UINT8 *)(ROUNDUP((UINTN)__##var, CACHE_LINE))

#define BASE64_ENCODED_SIZE(n) (ROUNDUP(4*((n)/3)+1, 4)+1)

#define UtilBase64Encode __b64_ntop
#define UtilBase64Decode __b64_pton

INT32 __b64_ntop (UINT8 CONST *, UINTN, CHAR8 *, UINTN);
INT32 __b64_pton (CHAR8 CONST *, UINT8 *, UINTN);

typedef
EFI_STATUS
(EFIAPI *PROTOCOL_INSTANCE_CALLBACK)(
  IN EFI_HANDLE           Handle,
  IN VOID                 *Instance,
  IN VOID                 *Context
  );

typedef
RETURN_STATUS
(EFIAPI *VARIABLE_ITERATION_CALLBACK)(
  IN  VOID                         *Context,
  IN  CHAR16                       *VariableName,
  IN  EFI_GUID                     *VendorGuid,
  IN  UINT32                       Attributes,
  IN  UINTN                        DataSize,
  IN  VOID                         *Data
  );

// so we don't need std headers
typedef
CHAR8*
(*ini_reader)(
  CHAR8 *String,
  INT32 Size,
  VOID *Stream
);

typedef
INT32
(*ini_handler)(
  VOID        *User,
  CONST CHAR8 *Section,
  CONST CHAR8 *Name,
  CONST CHAR8 *Value
);

INT32
ini_parse_stream (
  ini_reader  Reader,
  VOID        *Stream,
  ini_handler Handler,
  VOID        *User
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

#ifndef LIBUTIL_NOAROMA
LIBAROMA_STREAMP
libaroma_stream_ramdisk(
  CONST CHAR8* Path
);
#endif

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

EFI_FILE_HANDLE
UtilOpenRoot (
  IN EFI_HANDLE                   DeviceHandle
  );

VOID *
UtilFileInfo (
  IN EFI_FILE_HANDLE      FHand,
  IN EFI_GUID             *InfoType
  );

CHAR16*
UtilGetTypeFromName (
  IN CHAR16   *FileName
  );

VOID
UtilToLowerString (
  IN CHAR16  *String
  );

CHAR16*
UtilGetExtensionLower (
  IN UINT16  *FileName
  );

EFI_STATUS
UtilIterateVariables (
  IN VARIABLE_ITERATION_CALLBACK CallbackFunction,
  IN VOID                        *Context
);

EFI_STATUS
UtilSetEFIDroidVariable (
  IN CONST CHAR8* Name,
  IN CONST CHAR8* Value
);

CHAR8*
UtilGetEFIDroidVariable (
  IN CONST CHAR8* Name
);

EFI_STATUS
UtilSetEFIDroidDataVariable (
  IN CONST CHAR16 *Name,
  IN CONST VOID   *Value,
  IN UINTN        Size
);

VOID*
UtilGetEFIDroidDataVariable (
  IN CONST CHAR16* Name
);

BOOLEAN
UtilVariableExists (
  IN CONST CHAR16    *Name,
  IN CONST EFI_GUID  *Guid
);

BOOLEAN
SettingBoolGet (
  CONST CHAR8* Name
);

VOID
SettingBoolSet (
  CONST CHAR8* Name,
  BOOLEAN Value
);

INT32
IniParseEfiFile (
  EFI_FILE_PROTOCOL *Filename,
  ini_handler       Handler,
  VOID              *User
);

#endif /* ! UTIL_H */
