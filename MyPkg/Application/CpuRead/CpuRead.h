#ifndef __CPUREAD_H__
#define __CPUREAD_H__

// MdePkg/Include
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
// UefiCpuPkg/Include
#include <Register/Cpuid.h> // for use CPUID_VERSION_INFO
#include <Register/ArchitecturalMsr.h> // for use MSR_IA32_BIOS_SIGN_ID

#define PRINT_BIT_FIELD(Variable, FieldName) \
  Print (L"%5a%42a: %x\n", #Variable, #FieldName, Variable.Bits.FieldName);

#define PRINT_VALUE(Variable, Description) \
  Print (L"%5a%42a: %x\n", #Variable, #Description, Variable);
#define BASIC_FUNC_ADDR 0x00000000
#define EXTENTED_FUNC_ADDR 0x80000000

VOID VendorIdAndMaxBasicFunction(IN OUT UINT32 *gMaxBasicFunc);
VOID GetCpuFeatureInfo(IN UINT32 gMaxBasicFunc);
VOID CpuidExtendedFunction(IN OUT UINT32 *gMaxExtendedFunc);
VOID GetCpuBrandString(IN UINT32 gMaxExtendedFunc);
VOID GetMicroCodeVersion();

#endif
