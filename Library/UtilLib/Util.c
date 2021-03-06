#include <Library/Util.h>
#include <Library/UEFIRamdisk.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootManagerLib.h>
#include <Library/HobLib.h>
#include <Library/UefiLib.h>
#include <Library/DevicePathLib.h>

#include <Protocol/LoadedImage.h>

extern EFI_GUID gEFIDroidVariableGuid;
extern EFI_GUID gEFIDroidVariableDataGuid;

CHAR8*
Unicode2Ascii (
  CONST CHAR16* UnicodeStr
)
{
  CHAR8* AsciiStr = AllocatePool((StrLen (UnicodeStr) + 1) * sizeof (CHAR8));
  if (AsciiStr == NULL) {
    return NULL;
  }

  UnicodeStrToAsciiStr(UnicodeStr, AsciiStr);

  return AsciiStr;
}

CHAR16*
Ascii2Unicode (
  CONST CHAR8* AsciiStr
)
{
  CHAR16* UnicodeStr = AllocatePool((AsciiStrLen (AsciiStr) + 1) * sizeof (CHAR16));
  if (UnicodeStr == NULL) {
    return NULL;
  }

  AsciiStrToUnicodeStr(AsciiStr, UnicodeStr);

  return UnicodeStr;
}

CHAR8*
AsciiStrDup (
  IN CONST CHAR8* Str
  )
{
  return AllocateCopyPool (AsciiStrSize (Str), Str);
}

CHAR16*
UnicodeStrDup (
  IN CONST CHAR16* Str
  )
{
  return AllocateCopyPool (StrSize (Str), Str);
}

VOID
PathToUnix(
  CHAR16* fname
)
{
  CHAR16 *Tmp = fname;
  for(Tmp = fname; *Tmp != 0; Tmp++) {
    if(*Tmp=='\\')
      *Tmp = '/';
  }
}

VOID
PathToUefi(
  CHAR16* fname
)
{
  CHAR16 *Tmp = fname;
  for(Tmp = fname; *Tmp != 0; Tmp++) {
    if(*Tmp=='/')
      *Tmp = '\\';
  }
}

BOOLEAN
NodeIsDir (
  IN EFI_FILE_INFO      *NodeInfo
  )
{
  return ((NodeInfo->Attribute & EFI_FILE_DIRECTORY) == EFI_FILE_DIRECTORY);
}

LIBAROMA_STREAMP
libaroma_stream_ramdisk(
  CONST CHAR8* Path
)
{
  bytep Data;
  UINTN Size;
  EFI_STATUS Status;

  // get font data
  Status = UEFIRamdiskGetFile (Path, (VOID **) &Data, &Size);
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  return libaroma_stream_mem((UINTN)Data, Size);
}

UINT32
RangeOverlaps (
  UINT32 x1,
  UINT32 x2,
  UINT32 y1,
  UINT32 y2
)
{
  return MAX(x1,y1) <= MIN(x2,y2);
}

UINT32
RangeLenOverlaps (
  UINT32 x,
  UINT32 xl,
  UINT32 y,
  UINT32 yl
)
{
  return RangeOverlaps(x, x+xl, y, y+yl);
}

UINT32
AlignMemoryRange (
  IN UINT32 Addr,
  IN OUT UINTN *Size,
  OUT UINTN  *AddrOffset,
  IN UINTN Alignment
)
{
  // align range
  UINT32 AddrAligned = ROUNDDOWN(Addr, Alignment);

  // calculate offset
  UINTN Offset = Addr - AddrAligned;
  if (AddrOffset!=NULL)
    *AddrOffset = Offset;

  // round and return size
  *Size = ROUNDUP(Offset + (*Size), Alignment);

  return AddrAligned;
}

EFI_STATUS
FreeAlignedMemoryRange (
  IN UINT32 Address,
  IN OUT UINTN Size,
  IN UINTN Alignment
)
{
  UINTN      AlignedSize = Size;
  UINTN      AddrOffset = 0;

  EFI_PHYSICAL_ADDRESS AllocationAddress = AlignMemoryRange(Address, &AlignedSize, &AddrOffset, Alignment);

  return gBS->FreePages(AllocationAddress, EFI_SIZE_TO_PAGES(AlignedSize));
}

EFI_STATUS
VisitAllInstancesOfProtocol (
  IN EFI_GUID                    *Id,
  IN PROTOCOL_INSTANCE_CALLBACK  CallBackFunction,
  IN VOID                        *Context
  )
{
  EFI_STATUS                Status;
  UINTN                     HandleCount;
  EFI_HANDLE                *HandleBuffer;
  UINTN                     Index;
  VOID                      *Instance;

  //
  // Start to check all the PciIo to find all possible device
  //
  HandleCount = 0;
  HandleBuffer = NULL;
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  Id,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (HandleBuffer[Index], Id, &Instance);
    if (EFI_ERROR (Status)) {
      continue;
    }

    Status = (*CallBackFunction) (
               HandleBuffer[Index],
               Instance,
               Context
               );
  }

  gBS->FreePool (HandleBuffer);

  return EFI_SUCCESS;
}

/**

  Function opens and returns a file handle to the root directory of a volume.

  @param DeviceHandle    A handle for a device

  @return A valid file handle or NULL is returned

**/
EFI_FILE_HANDLE
UtilOpenRoot (
  IN EFI_HANDLE                   DeviceHandle
  )
{
  EFI_STATUS                      Status;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Volume;
  EFI_FILE_HANDLE                 File;

  File = NULL;

  //
  // File the file system interface to the device
  //
  Status = gBS->HandleProtocol (
                  DeviceHandle,
                  &gEfiSimpleFileSystemProtocolGuid,
                  (VOID *) &Volume
                  );

  //
  // Open the root directory of the volume
  //
  if (!EFI_ERROR (Status)) {
    Status = Volume->OpenVolume (
                      Volume,
                      &File
                      );
  }
  //
  // Done
  //
  return EFI_ERROR (Status) ? NULL : File;
}

/**

  Function gets the file information from an open file descriptor, and stores it
  in a buffer allocated from pool.

  @param FHand           File Handle.
  @param InfoType        Info type need to get.

  @retval                A pointer to a buffer with file information or NULL is returned

**/
VOID *
UtilFileInfo (
  IN EFI_FILE_HANDLE      FHand,
  IN EFI_GUID             *InfoType
  )
{
  EFI_STATUS    Status;
  EFI_FILE_INFO *Buffer;
  UINTN         BufferSize;

  Buffer      = NULL;
  BufferSize  = 0;

  Status = FHand->GetInfo (
                    FHand,
                    InfoType,
                    &BufferSize,
                    Buffer
                    );
  if (Status == EFI_BUFFER_TOO_SMALL) {
    Buffer = AllocatePool (BufferSize);
    ASSERT (Buffer != NULL);
  }

  Status = FHand->GetInfo (
                    FHand,
                    InfoType,
                    &BufferSize,
                    Buffer
                    );

  return Buffer;
}

/**

  Get file type base on the file name.
  Just cut the file name, from the ".". eg ".efi"

  @param FileName  File need to be checked.

  @retval the file type string.

**/
CHAR16*
UtilGetTypeFromName (
  IN CHAR16   *FileName
  )
{
  UINTN    Index;

  Index = StrLen (FileName) - 1;
  while ((FileName[Index] != L'.') && (Index != 0)) {
    Index--;
  }

  Index++;

  return Index == 0 ? NULL : &FileName[Index];
}

/**
  Converts the unicode character of the string from uppercase to lowercase.
  This is a internal function.

  @param ConfigString  String to be converted

**/
VOID
UtilToLowerString (
  IN CHAR16  *String
  )
{
  CHAR16      *TmpStr;

  for (TmpStr = String; *TmpStr != L'\0'; TmpStr++) {
    if (*TmpStr >= L'A' && *TmpStr <= L'Z') {
      *TmpStr = (CHAR16) (*TmpStr - L'A' + L'a');
    }
  }
}

/**
  Converts the unicode character of the string from uppercase to lowercase.
  This is a internal function.

  @param ConfigString  String to be converted

**/
VOID
UtilAsciiToLowerString (
  IN CHAR8  *String
  )
{
  CHAR8      *TmpStr;

  for (TmpStr = String; *TmpStr != '\0'; TmpStr++) {
    if (*TmpStr >= 'A' && *TmpStr <= 'Z') {
      *TmpStr = (CHAR8) (*TmpStr - 'A' + 'a');
    }
  }
}

/**

  Check whether current FileName point to a valid
  Efi Image File.

  @param FileName  File need to be checked.

  @retval TRUE  Is Efi Image
  @retval FALSE Not a valid Efi Image

**/
CHAR16*
UtilGetExtensionLower (
  IN UINT16  *FileName
  )
{
  CHAR16     *InputFileType;
  CHAR16     *TmpStr;

  InputFileType = UtilGetTypeFromName (FileName);
  //
  // If the file not has *.* style, always return TRUE.
  //
  if (InputFileType == NULL) {
    return NULL;
  }

  TmpStr = AllocateCopyPool (StrSize (InputFileType), InputFileType);
  UtilToLowerString(TmpStr);

  return TmpStr;
}

EFI_STATUS
UtilIterateVariables (
  IN VARIABLE_ITERATION_CALLBACK CallbackFunction,
  IN VOID                        *Context
)
{
  RETURN_STATUS               Status;
  UINTN                       VariableNameBufferSize;
  UINTN                       VariableNameSize;
  CHAR16                      *VariableName;
  EFI_GUID                    VendorGuid;
  UINTN                       VariableDataBufferSize;
  UINTN                       VariableDataSize;
  VOID                        *VariableData;
  UINT32                      VariableAttributes;
  VOID                        *NewBuffer;

  //
  // Initialize the variable name and data buffer variables.
  //
  VariableNameBufferSize = sizeof (CHAR16);
  VariableName = AllocateZeroPool (VariableNameBufferSize);

  VariableDataBufferSize = 0;
  VariableData = NULL;

  for (;;) {
    //
    // Get the next variable name and guid
    //
    VariableNameSize = VariableNameBufferSize;
    Status = gRT->GetNextVariableName (
                    &VariableNameSize,
                    VariableName,
                    &VendorGuid
                    );
    if (Status == EFI_BUFFER_TOO_SMALL) {
      //
      // The currently allocated VariableName buffer is too small,
      // so we allocate a larger buffer, and copy the old buffer
      // to it.
      //
      NewBuffer = AllocatePool (VariableNameSize);
      if (NewBuffer == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        break;
      }
      CopyMem (NewBuffer, VariableName, VariableNameBufferSize);
      if (VariableName != NULL) {
        FreePool (VariableName);
      }
      VariableName = NewBuffer;
      VariableNameBufferSize = VariableNameSize;

      //
      // Try to get the next variable name again with the larger buffer.
      //
      Status = gRT->GetNextVariableName (
                      &VariableNameSize,
                      VariableName,
                      &VendorGuid
                      );
    }

    if (EFI_ERROR (Status)) {
      if (Status == EFI_NOT_FOUND) {
        Status = EFI_SUCCESS;
      }
      break;
    }

    //
    // Get the variable data and attributes
    //
    VariableDataSize = VariableDataBufferSize;
    Status = gRT->GetVariable (
                    VariableName,
                    &VendorGuid,
                    &VariableAttributes,
                    &VariableDataSize,
                    VariableData
                    );
    if (Status == EFI_BUFFER_TOO_SMALL) {
      //
      // The currently allocated VariableData buffer is too small,
      // so we allocate a larger buffer.
      //
      if (VariableDataBufferSize != 0) {
        FreePool (VariableData);
        VariableData = NULL;
        VariableDataBufferSize = 0;
      }
      VariableData = AllocatePool (VariableDataSize);
      if (VariableData == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        break;
      }
      VariableDataBufferSize = VariableDataSize;

      //
      // Try to read the variable again with the larger buffer.
      //
      Status = gRT->GetVariable (
                      VariableName,
                      &VendorGuid,
                      &VariableAttributes,
                      &VariableDataSize,
                      VariableData
                      );
    }
    if (EFI_ERROR (Status)) {
      break;
    }

    //
    // Run the callback function
    //
    Status = (*CallbackFunction) (
               Context,
               VariableName,
               &VendorGuid,
               VariableAttributes,
               VariableDataSize,
               VariableData
               );
    if (EFI_ERROR (Status)) {
      break;
    }
  }

  if (VariableName != NULL) {
    FreePool (VariableName);
  }

  if (VariableData != NULL) {
    FreePool (VariableData);
  }

  return Status;
}

EFI_STATUS
UtilSetEFIDroidVariable (
  IN CONST CHAR8* Name,
  IN CONST CHAR8* Value
)
{
  EFI_STATUS Status;
  CHAR16     *Name16;

  // convert name to unicode
  Name16 = Ascii2Unicode(Name);
  if (Name16 == NULL)
    return EFI_OUT_OF_RESOURCES;

  // set variable
  Status = gRT->SetVariable (
              Name16,
              &gEFIDroidVariableGuid,
              (EFI_VARIABLE_NON_VOLATILE|EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS),
              Value?AsciiStrSize(Value):0, (VOID*)Value
            );

  // free name
  FreePool(Name16);

  return Status;
}

CHAR8*
UtilGetEFIDroidVariable (
  IN CONST CHAR8* Name
)
{
  EFI_STATUS Status;
  UINTN      Size;
  CHAR16     *Name16;
  CHAR8*     ReturnValue;

  ReturnValue = NULL;

  // convert name to unicode
  Name16 = Ascii2Unicode(Name);
  if (Name16 == NULL)
    return NULL;

  // get size of 'EFIDroidErrorStr'
  Size = 0;
  Status = gRT->GetVariable (Name16, &gEFIDroidVariableGuid, NULL, &Size, NULL);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    // allocate memory
    CHAR8* Data = AllocateZeroPool(Size);
    if (Data) {
      // get actual variable value
      Status = gRT->GetVariable (Name16, &gEFIDroidVariableGuid, NULL, &Size, Data);
      if (Status == EFI_SUCCESS) {
        ReturnValue = Data;
      }
      else {
        FreePool(Data);
      }
    }
  }

  // free name
  FreePool(Name16);

  return ReturnValue;
}

EFI_STATUS
UtilSetEFIDroidDataVariable (
  IN CONST CHAR16 *Name,
  IN CONST VOID   *Value,
  IN UINTN        Size
)
{
  EFI_STATUS Status;

  // set variable
  Status = gRT->SetVariable (
              (CHAR16*)Name,
              &gEFIDroidVariableDataGuid,
              (EFI_VARIABLE_NON_VOLATILE|EFI_VARIABLE_BOOTSERVICE_ACCESS|EFI_VARIABLE_RUNTIME_ACCESS),
              Size, (VOID*)Value
            );

  return Status;
}

VOID*
UtilGetEFIDroidDataVariable (
  IN CONST CHAR16* Name
)
{
  EFI_STATUS Status;
  UINTN      Size;
  CHAR8*     ReturnValue;

  ReturnValue = NULL;

  // get size of 'EFIDroidErrorStr'
  Size = 0;
  Status = gRT->GetVariable ((CHAR16*)Name, &gEFIDroidVariableDataGuid, NULL, &Size, NULL);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    // allocate memory
    CHAR8* Data = AllocateZeroPool(Size);
    if (Data) {
      // get actual variable value
      Status = gRT->GetVariable ((CHAR16*)Name, &gEFIDroidVariableDataGuid, NULL, &Size, Data);
      if (Status == EFI_SUCCESS) {
        ReturnValue = Data;
      }
      else {
        FreePool(Data);
      }
    }
  }

  return ReturnValue;
}

BOOLEAN
UtilVariableExists (
  IN CONST CHAR16    *Name,
  IN CONST EFI_GUID  *Guid
)
{
  UINTN                               Size;
  EFI_STATUS                          Status;

  Size = 0;
  Status = gRT->GetVariable ((CHAR16*)Name, (EFI_GUID*)Guid, NULL, &Size, NULL);
  if (Status == EFI_NOT_FOUND) {
    return FALSE;
  }

  return TRUE;
}

BOOLEAN
SettingBoolGet (
  CONST CHAR8* Name
)
{
  CHAR8* Value = UtilGetEFIDroidVariable(Name);
  if (!Value)
    return FALSE;

  BOOLEAN Ret = (!AsciiStrCmp(Value, "1"));
  FreePool(Value);
  return Ret;
}

VOID
SettingBoolSet (
  CONST CHAR8* Name,
  BOOLEAN Value
)
{
  UtilSetEFIDroidVariable(Name, Value?"1":"0");
}

STATIC
CHAR8*
IniReaderEfiFile (
  CHAR8 *String,
  INT32 Size,
  VOID *Stream
)
{
    EFI_FILE_PROTOCOL  *File = (EFI_FILE_PROTOCOL*) Stream;
    EFI_STATUS         Status;
    UINTN              BufferSize;
    UINT64             Position;
    UINTN              Index;

    // stop here if we reached EOF already
    if (FileHandleEof(File)) {
        return NULL;
    }

    // get current position
    Status = FileHandleGetPosition(File, &Position);
    if (EFI_ERROR(Status))
      return NULL;

    // read data
    BufferSize = Size-1;
    Status = FileHandleRead(File, &BufferSize, String);
    if (EFI_ERROR(Status)) {
      return NULL;
    }

    // EOF or error
    if (BufferSize==0) {
      return NULL;
    }

    // terminate buffer
    String[BufferSize] = '\0';

    // search for newline
    for(Index=0; Index<BufferSize; Index++) {
      CHAR8 c = String[Index];
      if(c=='\n' || c=='\r') {
        String[Index+1] = '\0';

        // seek back file position
        Status = FileHandleSetPosition(File, Position+Index+1);
        if (EFI_ERROR(Status))
          return NULL;

        break;
      }
    }

    return String;
}

INT32
IniParseEfiFile (
  EFI_FILE_PROTOCOL *File,
  ini_handler       Handler,
  VOID              *User
)
{
  return ini_parse_stream(IniReaderEfiFile, File, Handler, User);
}


EFI_STATUS
UtilReadFileToMemory (
  EFI_FILE_PROTOCOL *File,
  VOID              **OutBuffer,
  UINTN             *OutSize
)
{
  EFI_STATUS Status;
  UINT64     FileSize = 0;
  VOID       *Buffer = NULL;
  UINTN      ReadSize;

  if(!File)
    return EFI_INVALID_PARAMETER;

  // get size
  Status = FileHandleGetSize(File, &FileSize);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // allocate buffer
  Buffer = AllocatePool(FileSize);
  if (Buffer==NULL)
    return EFI_OUT_OF_RESOURCES;

  // read data
  ReadSize = (UINTN) FileSize;
  Status = FileHandleRead(File, &ReadSize, Buffer);
  if (EFI_ERROR (Status)) {
    FreePool (Buffer);
    return Status;
  }

  *OutBuffer = Buffer;
  *OutSize   = ReadSize;

  return EFI_SUCCESS;
}

LIBAROMA_STREAMP
libaroma_stream_efifile(
  EFI_FILE_PROTOCOL *File
)
{
  bytep Data;
  UINTN Size;
  EFI_STATUS Status;

  // get font data
  Status = UtilReadFileToMemory (File, (VOID **) &Data, &Size);
  if (EFI_ERROR (Status)) {
    return NULL;
  }

  return libaroma_stream_mem((UINTN)Data, Size);
}

EFI_STATUS
UtilGetFvApplicationDevicePath (
  EFI_GUID                          *FileGuid,
  OUT EFI_DEVICE_PATH_PROTOCOL      **OutDevicePath
)
{
  EFI_STATUS                        Status;
  MEDIA_FW_VOL_FILEPATH_DEVICE_PATH FileNode;
  EFI_LOADED_IMAGE_PROTOCOL         *LoadedImage;
  EFI_DEVICE_PATH_PROTOCOL          *DevicePath;

  Status = gBS->HandleProtocol (
                  gImageHandle,
                  &gEfiLoadedImageProtocolGuid,
                  (VOID **) &LoadedImage
                  );
  if (EFI_ERROR(Status))
    return Status;

  EfiInitializeFwVolDevicepathNode (&FileNode, FileGuid);
  DevicePath = DevicePathFromHandle (LoadedImage->DeviceHandle);
  if (DevicePath == NULL)
    return EFI_NOT_FOUND;

  DevicePath = AppendDevicePathNode (
                 DevicePath,
                 (EFI_DEVICE_PATH_PROTOCOL *) &FileNode
                 );
  if (DevicePath == NULL)
    return EFI_NOT_FOUND;

  *OutDevicePath = DevicePath;

  return EFI_SUCCESS;
}

EFI_STATUS
UtilStartEfiApplication (
  IN EFI_DEVICE_PATH_PROTOCOL    *DevicePath,
  IN UINTN                       LoadOptionsSize,
  IN VOID*                       LoadOptions
  )
{
  EFI_STATUS                        Status;
  EFI_BOOT_MANAGER_LOAD_OPTION      NewOption;

  Status = EfiBootManagerInitializeLoadOption (
             &NewOption,
             LoadOptionNumberUnassigned,
             LoadOptionTypeSysPrep,
             LOAD_OPTION_ACTIVE,
             L"",
             DevicePath,
             LoadOptions,
             LoadOptionsSize
             );
  if (EFI_ERROR(Status))
    return Status;

  EfiBootManagerProcessLoadOption(&NewOption);
  Status = NewOption.Status;

  EfiBootManagerFreeLoadOption(&NewOption);

  return Status;
}

EFI_STATUS
UtilShutdownUefiBootServices (
  VOID
  )
{
  EFI_STATUS              Status;
  UINTN                   MemoryMapSize;
  EFI_MEMORY_DESCRIPTOR   *MemoryMap;
  UINTN                   MapKey;
  UINTN                   DescriptorSize;
  UINT32                  DescriptorVersion;
  UINTN                   Pages;

  MemoryMap = NULL;
  MemoryMapSize = 0;
  Pages = 0;

  do {
    Status = gBS->GetMemoryMap (
                    &MemoryMapSize,
                    MemoryMap,
                    &MapKey,
                    &DescriptorSize,
                    &DescriptorVersion
                    );
    if (Status == EFI_BUFFER_TOO_SMALL) {

      Pages = EFI_SIZE_TO_PAGES (MemoryMapSize) + 1;
      MemoryMap = AllocatePages (Pages);

      //
      // Get System MemoryMap
      //
      Status = gBS->GetMemoryMap (
                      &MemoryMapSize,
                      MemoryMap,
                      &MapKey,
                      &DescriptorSize,
                      &DescriptorVersion
                      );
    }

    // Don't do anything between the GetMemoryMap() and ExitBootServices()
    if (!EFI_ERROR(Status)) {
      Status = gBS->ExitBootServices (gImageHandle, MapKey);
      if (EFI_ERROR(Status)) {
        FreePages (MemoryMap, Pages);
        MemoryMap = NULL;
        MemoryMapSize = 0;
      }
    }
  } while (EFI_ERROR(Status));

  return Status;
}

STATIC
EFI_STATUS
InsertSystemMemoryResources (
  LIST_ENTRY *ResourceList,
  EFI_HOB_RESOURCE_DESCRIPTOR *ResHob
  )
{
  SYSTEM_MEMORY_RESOURCE  *NewResource;
  LIST_ENTRY              *Link;
  LIST_ENTRY              *NextLink;
  LIST_ENTRY              AttachedResources;
  SYSTEM_MEMORY_RESOURCE  *Resource;
  EFI_PHYSICAL_ADDRESS    NewResourceEnd;

  if (IsListEmpty (ResourceList)) {
    NewResource = AllocateZeroPool (sizeof (SYSTEM_MEMORY_RESOURCE));
    NewResource->PhysicalStart = ResHob->PhysicalStart;
    NewResource->ResourceLength = ResHob->ResourceLength;
    InsertTailList (ResourceList, &NewResource->Link);
    return EFI_SUCCESS;
  }

  InitializeListHead (&AttachedResources);

  Link = ResourceList->ForwardLink;
  ASSERT (Link != NULL);
  while (Link != ResourceList) {
    Resource = (SYSTEM_MEMORY_RESOURCE*)Link;

    // Sanity Check. The resources should not overlapped.
    ASSERT (!((ResHob->PhysicalStart >= Resource->PhysicalStart) && (ResHob->PhysicalStart < (Resource->PhysicalStart + Resource->ResourceLength))));
    ASSERT (!((ResHob->PhysicalStart + ResHob->ResourceLength - 1 >= Resource->PhysicalStart) &&
        ((ResHob->PhysicalStart + ResHob->ResourceLength - 1) < (Resource->PhysicalStart + Resource->ResourceLength))));

    // The new resource is attached after this resource descriptor
    if (ResHob->PhysicalStart == Resource->PhysicalStart + Resource->ResourceLength) {
      Resource->ResourceLength =  Resource->ResourceLength + ResHob->ResourceLength;

      NextLink = RemoveEntryList (&Resource->Link);
      InsertTailList (&AttachedResources, &Resource->Link);
      Link = NextLink;
    }
    // The new resource is attached before this resource descriptor
    else if (ResHob->PhysicalStart + ResHob->ResourceLength == Resource->PhysicalStart) {
      Resource->PhysicalStart = ResHob->PhysicalStart;
      Resource->ResourceLength =  Resource->ResourceLength + ResHob->ResourceLength;

      NextLink = RemoveEntryList (&Resource->Link);
      InsertTailList (&AttachedResources, &Resource->Link);
      Link = NextLink;
    } else {
      Link = Link->ForwardLink;
    }
  }

  if (!IsListEmpty (&AttachedResources)) {
    // See if we can merge the attached resource with other resources

    NewResource = (SYSTEM_MEMORY_RESOURCE*)GetFirstNode (&AttachedResources);
    Link = RemoveEntryList (&NewResource->Link);
    while (!IsListEmpty (&AttachedResources)) {
      // Merge resources
      Resource = (SYSTEM_MEMORY_RESOURCE*)Link;

      // Ensure they overlap each other
      ASSERT (
          ((NewResource->PhysicalStart >= Resource->PhysicalStart) && (NewResource->PhysicalStart < (Resource->PhysicalStart + Resource->ResourceLength))) ||
          (((NewResource->PhysicalStart + NewResource->ResourceLength) >= Resource->PhysicalStart) && ((NewResource->PhysicalStart + NewResource->ResourceLength) < (Resource->PhysicalStart + Resource->ResourceLength)))
      );

      NewResourceEnd = MAX (NewResource->PhysicalStart + NewResource->ResourceLength, Resource->PhysicalStart + Resource->ResourceLength);
      NewResource->PhysicalStart = MIN (NewResource->PhysicalStart, Resource->PhysicalStart);
      NewResource->ResourceLength = NewResourceEnd - NewResource->PhysicalStart;

      Link = RemoveEntryList (Link);
    }
  } else {
    // None of the Resource of the list is attached to this ResHob. Create a new entry for it
    NewResource = AllocateZeroPool (sizeof (SYSTEM_MEMORY_RESOURCE));
    NewResource->PhysicalStart = ResHob->PhysicalStart;
    NewResource->ResourceLength = ResHob->ResourceLength;
  }
  InsertTailList (ResourceList, &NewResource->Link);
  return EFI_SUCCESS;
}

EFI_STATUS
UtilGetSystemMemoryResources (
  IN  LIST_ENTRY *ResourceList
  )
{
  EFI_HOB_RESOURCE_DESCRIPTOR *ResHob;

  InitializeListHead (ResourceList);

  // Find the first System Memory Resource Descriptor
  ResHob = (EFI_HOB_RESOURCE_DESCRIPTOR *)GetFirstHob (EFI_HOB_TYPE_RESOURCE_DESCRIPTOR);
  while ((ResHob != NULL) && (ResHob->ResourceType != EFI_RESOURCE_SYSTEM_MEMORY)) {
    ResHob = (EFI_HOB_RESOURCE_DESCRIPTOR *)GetNextHob (EFI_HOB_TYPE_RESOURCE_DESCRIPTOR, (VOID *)((UINTN)ResHob + ResHob->Header.HobLength));
  }

  // Did not find any
  if (ResHob == NULL) {
    return EFI_NOT_FOUND;
  } else {
    InsertSystemMemoryResources (ResourceList, ResHob);
  }

  ResHob = (EFI_HOB_RESOURCE_DESCRIPTOR *)GetNextHob (EFI_HOB_TYPE_RESOURCE_DESCRIPTOR, (VOID *)((UINTN)ResHob + ResHob->Header.HobLength));
  while (ResHob != NULL) {
    if (ResHob->ResourceType == EFI_RESOURCE_SYSTEM_MEMORY) {
      InsertSystemMemoryResources (ResourceList, ResHob);
    }
    ResHob = (EFI_HOB_RESOURCE_DESCRIPTOR *)GetNextHob (EFI_HOB_TYPE_RESOURCE_DESCRIPTOR, (VOID *)((UINTN)ResHob + ResHob->Header.HobLength));
  }

  return EFI_SUCCESS;
}
