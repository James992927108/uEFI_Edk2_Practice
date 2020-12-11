// #include <Uefi.h>
// #include <Pi/PiDxeCis.h>

#ifndef _AP_MPPROTOCOL_H_
#define _AP_MPPROTOCOL_H_

#include <PiDxe.h> // same as <Pi/PiDxeCis.h> +  <Uefi.h>
#include <Library/UefiBootServicesTableLib.h> // for use EFI_BOOT_SERVICES *gBS;

// MdePkg/Include
#include <Protocol/MpService.h>
#include <Library/DebugLib.h>
#include <Library/SynchronizationLib.h>
#include <Library/UefiLib.h>
// ShellPkg/Include
#include <Library/ShellCEntryLib.h>
// UefiCpuPkg/Includes
// UefiCpuPkg/Include
#include <Register/Cpuid.h> // for use CPUID_VERSION_INFO
#include <Register/ArchitecturalMsr.h> // for use MSR_IA32_BIOS_SIGN_ID

#define PRINT_VALUE(Variable, Description) \
  Print (L"%5a%42a: %x\n", #Variable, #Description, Variable);

#define CPUID_STANDARD_FUNCTION_ADDR 0x00000000

#endif
