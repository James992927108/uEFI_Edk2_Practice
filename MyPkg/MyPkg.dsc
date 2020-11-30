[Defines]
  PLATFORM_NAME                  = MyPkg
  PLATFORM_GUID                  = 0458dade-8b6e-4e45-b773-1b27cbda3e06
  PLATFORM_VERSION               = 0.01
  DSC_SPECIFICATION              = 0x00010006
  OUTPUT_DIRECTORY               = Build/MyPkg
  SUPPORTED_ARCHITECTURES        = IA32|X64|ARM|AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER               = DEFAULT

#
#  Debug output control
#
  DEFINE DEBUG_ENABLE_OUTPUT      = FALSE       # Set to TRUE to enable debug output
  DEFINE DEBUG_PRINT_ERROR_LEVEL  = 0x80000040  # Flags to control amount of debug output
  DEFINE DEBUG_PROPERTY_MASK      = 0

[PcdsFeatureFlag]

[PcdsFixedAtBuild]
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|$(DEBUG_PROPERTY_MASK)
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|$(DEBUG_PRINT_ERROR_LEVEL)

[PcdsFixedAtBuild.IPF]

[LibraryClasses]
  #
  # Entry Point Libraries
  #
  UefiApplicationEntryPoint|MdePkg/Library/UefiApplicationEntryPoint/UefiApplicationEntryPoint.inf #  MdePkg/MdePkg.dec
  ShellCEntryLib|ShellPkg/Library/UefiShellCEntryLib/UefiShellCEntryLib.inf
  UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
  #
  # Common Libraries
  #
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  !if $(DEBUG_ENABLE_OUTPUT)
    DebugLib|MdePkg/Library/UefiDebugLibConOut/UefiDebugLibConOut.inf
    DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
  !else   ## DEBUG_ENABLE_OUTPUT
    DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  !endif  ## DEBUG_ENABLE_OUTPUT

  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  PeCoffGetEntryPointLib|MdePkg/Library/BasePeCoffGetEntryPointLib/BasePeCoffGetEntryPointLib.inf
  IoLib|MdePkg/Library/BaseIoLibIntrinsic/BaseIoLibIntrinsic.inf
  PciLib|MdePkg/Library/BasePciLibCf8/BasePciLibCf8.inf
  PciCf8Lib|MdePkg/Library/BasePciCf8Lib/BasePciCf8Lib.inf
  SynchronizationLib|MdePkg/Library/BaseSynchronizationLib/BaseSynchronizationLib.inf
  UefiRuntimeLib|MdePkg/Library/UefiRuntimeLib/UefiRuntimeLib.inf
  HobLib|MdePkg/Library/DxeHobLib/DxeHobLib.inf
  FileHandleLib|MdePkg/Library/UefiFileHandleLib/UefiFileHandleLib.inf
  CacheMaintenanceLib|MdePkg/Library/BaseCacheMaintenanceLib/BaseCacheMaintenanceLib.inf
  # add for CpuRead.inf
  CpuLib|MdePkg/Library/BaseCpuLib/BaseCpuLib.inf
  
  HiiLib|MdeModulePkg/Library/UefiHiiLib/UefiHiiLib.inf
  UefiHiiServicesLib|MdeModulePkg/Library/UefiHiiServicesLib/UefiHiiServicesLib.inf
  PerformanceLib|MdeModulePkg/Library/DxePerformanceLib/DxePerformanceLib.inf
  SortLib|MdeModulePkg/Library/UefiSortLib/UefiSortLib.inf

  ShellLib|ShellPkg/Library/UefiShellLib/UefiShellLib.inf
  # Add for SpdRead.inf
  ShellCommandLib|ShellPkg/Library/UefiShellCommandLib/UefiShellCommandLib.inf
  HandleParsingLib|ShellPkg/Library/UefiHandleParsingLib/UefiHandleParsingLib.inf
  # 
  # add for CpuRead.inf
  MtrrLib|UefiCpuPkg/Library/MtrrLib/MtrrLib.inf
[Components]

  DEFINE MYPAKG_PATH = MyPkg/Application
  # $(MYPAKG_PATH)/HelloWorld/HelloWorld.inf
  # $(MYPAKG_PATH)/Hello/Hello.inf
  # $(MYPAKG_PATH)/Main/Main.inf
  # $(MYPAKG_PATH)/PciScan/Protocol/PciScan_v2.inf
  # $(MYPAKG_PATH)/PciScan/IO/PciScan_v1.inf
  # $(MYPAKG_PATH)/PciScan/MMIO/PciScan_v3.inf
  # $(MYPAKG_PATH)/SpdRead/SpdRead.inf
  # $(MYPAKG_PATH)/CpuRead/CpuRead.inf
  $(MYPAKG_PATH)/CpuRead/Lib/CpuReadbyLib.inf
  
#### A simple fuzzer for OrderedCollectionLib, in particular for
#### BaseOrderedCollectionRedBlackTreeLib.
  AppPkg/Applications/OrderedCollectionTest/OrderedCollectionTest.inf {
    <LibraryClasses>
      OrderedCollectionLib|MdePkg/Library/BaseOrderedCollectionRedBlackTreeLib/BaseOrderedCollectionRedBlackTreeLib.inf
      DebugLib|MdePkg/Library/UefiDebugLibConOut/UefiDebugLibConOut.inf
      DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf
    <PcdsFeatureFlag>
      gEfiMdePkgTokenSpaceGuid.PcdValidateOrderedCollection|TRUE
    <PcdsFixedAtBuild>
      gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x2F
      gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x80400040
  }

#### Un-comment the following line to build Python 2.7.2.
#  AppPkg/Applications/Python/PythonCore.inf

#### Un-comment the following line to build Python 2.7.10.
# AppPkg/Applications/Python/Python-2.7.10/Python2710.inf

#### Un-comment the following line to build Lua.
#  AppPkg/Applications/Lua/Lua.inf


##############################################################################
#
# Specify whether we are running in an emulation environment, or not.
# Define EMULATE if we are, else keep the DEFINE commented out.
#
DEFINE  EMULATE = 1

##############################################################################
#
#  Include Boilerplate text required for building with the Standard Libraries.
#
##############################################################################
!include StdLib/StdLib.inc
!include AppPkg/Applications/Sockets/Sockets.inc
