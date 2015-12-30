[LibraryClasses]
  Freetype2Lib|EFIDroidUEFIApps/EFIDroidUi/Library/Freetype2Lib.inf
  ZLib|EFIDroidUEFIApps/EFIDroidUi/Library/ZLib.inf
  PngLib|EFIDroidUEFIApps/EFIDroidUi/Library/PngLib.inf
  AromaLib|EFIDroidUEFIApps/EFIDroidUi/Library/AromaLib/AromaLib.inf

  CpioLib|EFIDroidUEFIApps/EFIDroidUi/Library/CpioLib/CpioLib.inf
  DecompressLib|EFIDroidUEFIApps/EFIDroidUi/Library/DecompressLib/DecompressLib.inf
  FstabLib|EFIDroidUEFIApps/EFIDroidUi/Library/FstabLib/FstabLib.inf
  IniLib|EFIDroidUEFIApps/EFIDroidUi/Library/IniLib/IniLib.inf
  EFIDroidMenuLib|EFIDroidUEFIApps/EFIDroidUi/Library/MenuLib/MenuLib.inf

[PcdsFixedAtBuild]
  gEfiMdeModulePkgTokenSpaceGuid.PcdResetOnMemoryTypeInformationChange|FALSE

[LibraryClasses.common.UEFI_APPLICATION]
  GenericBdsLib|IntelFrameworkModulePkg/Library/GenericBdsLib/GenericBdsLib.inf
  ReportStatusCodeLib|IntelFrameworkModulePkg/Library/DxeReportStatusCodeLibFramework/DxeReportStatusCodeLib.inf

[LibraryClasses.ARM]
  TimerLib|LittleKernelPkg/Library/LKTimerLib/LKTimerLib.inf
