[LibraryClasses]
  Freetype2Lib|EFIDroidUEFIApps/EFIDroidUi/Library/Freetype2Lib.inf
  ZLib|EFIDroidUEFIApps/EFIDroidUi/Library/ZLib.inf
  PngLib|EFIDroidUEFIApps/EFIDroidUi/Library/PngLib/PngLib.inf
  JpegLib|EFIDroidUEFIApps/EFIDroidUi/Library/JpegLib.inf
  AromaLib|EFIDroidUEFIApps/EFIDroidUi/Library/AromaLib/AromaLib.inf
  SquirrelLib|EFIDroidUEFIApps/EFIDroidUi/Library/SquirrelLib.inf
  BootLib|EFIDroidUEFIApps/EFIDroidUi/Library/BootLib/BootLib.inf
  FdtLib|EmbeddedPkg/Library/FdtLib/FdtLib.inf

  CpioLib|EFIDroidUEFIApps/EFIDroidUi/Library/CpioLib/CpioLib.inf
  DecompressLib|EFIDroidUEFIApps/EFIDroidUi/Library/DecompressLib/DecompressLib.inf
  FstabLib|EFIDroidUEFIApps/EFIDroidUi/Library/FstabLib/FstabLib.inf
  IniLib|EFIDroidUEFIApps/EFIDroidUi/Library/IniLib.inf
  EFIDroidMenuLib|EFIDroidUEFIApps/EFIDroidUi/Library/MenuLib/MenuLib.inf
  UEFIRamdiskLib|EFIDroidUEFIApps/EFIDroidUi/Library/UEFIRamdiskLib/UEFIRamdiskLib.inf
  UtilLib|EFIDroidUEFIApps/EFIDroidUi/Library/UtilLib/UtilLib.inf
  MakeDosFsLib|EFIDroidUEFIApps/EFIDroidUi/Library/MakeDosFsLib/MakeDosFsLib.inf

[PcdsFixedAtBuild]
  gEfiMdeModulePkgTokenSpaceGuid.PcdResetOnMemoryTypeInformationChange|FALSE

[LibraryClasses.common.UEFI_APPLICATION]
  UefiBootManagerLib|MdeModulePkg/Library/UefiBootManagerLib/UefiBootManagerLib.inf
  ReportStatusCodeLib|MdeModulePkg/Library/DxeReportStatusCodeLib/DxeReportStatusCodeLib.inf

[LibraryClasses.ARM]
  TimerLib|LittleKernelPkg/Library/LKTimerLib/LKTimerLib.inf

!include StdLib/StdLibNoShell.inc
