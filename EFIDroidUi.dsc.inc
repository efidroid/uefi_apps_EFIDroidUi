[LibraryClasses]
  Freetype2Lib|EFIDroidUEFIApps/EFIDroidUi/Library/Freetype2Lib.inf
  ZLib|EFIDroidUEFIApps/EFIDroidUi/Library/ZLib.inf
  PngLib|EFIDroidUEFIApps/EFIDroidUi/Library/PngLib.inf
  JpegLib|EFIDroidUEFIApps/EFIDroidUi/Library/JpegLib.inf
  AromaLib|EFIDroidUEFIApps/EFIDroidUi/Library/AromaLib/AromaLib.inf

  CpioLib|EFIDroidUEFIApps/EFIDroidUi/Library/CpioLib/CpioLib.inf
  DecompressLib|EFIDroidUEFIApps/EFIDroidUi/Library/DecompressLib/DecompressLib.inf
  FstabLib|EFIDroidUEFIApps/EFIDroidUi/Library/FstabLib/FstabLib.inf
  IniLib|EFIDroidUEFIApps/EFIDroidUi/Library/IniLib/IniLib.inf
  EFIDroidMenuLib|EFIDroidUEFIApps/EFIDroidUi/Library/MenuLib/MenuLib.inf
  UEFIRamdiskLib|EFIDroidUEFIApps/EFIDroidUi/Library/UEFIRamdiskLib/UEFIRamdiskLib.inf
  UtilLib|EFIDroidUEFIApps/EFIDroidUi/Library/UtilLib/UtilLib.inf
  FileBlockIoLib|EFIDroidUEFIApps/EFIDroidUi/Library/FileBlockIoLib/FileBlockIoLib.inf
  MemoryBlockIoLib|EFIDroidUEFIApps/EFIDroidUi/Library/MemoryBlockIoLib/MemoryBlockIoLib.inf
  MakeDosFsLib|EFIDroidUEFIApps/EFIDroidUi/Library/MakeDosFsLib/MakeDosFsLib.inf

[PcdsFixedAtBuild]
  gEfiMdeModulePkgTokenSpaceGuid.PcdResetOnMemoryTypeInformationChange|FALSE

[LibraryClasses.common.UEFI_APPLICATION]
  GenericBdsLib|IntelFrameworkModulePkg/Library/GenericBdsLib/GenericBdsLib.inf
  ReportStatusCodeLib|IntelFrameworkModulePkg/Library/DxeReportStatusCodeLibFramework/DxeReportStatusCodeLib.inf

[LibraryClasses.ARM]
  TimerLib|LittleKernelPkg/Library/LKTimerLib/LKTimerLib.inf
