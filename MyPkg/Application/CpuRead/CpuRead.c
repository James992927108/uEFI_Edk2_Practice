#include "CpuRead.h"

UINT32 GetLargestStanardFunction()
{
  UINT32 Eax;
  Print(L"FUNCTION (Leaf %08x)\n", CPUID_STANDARD_FUNCTION_ADDR);
  AsmCpuid(CPUID_STANDARD_FUNCTION_ADDR, &Eax, 0, 0, 0);
  Print(L"  EAX:%08x  EBX:%08x  ECX:%08x  EDX:%08x\n", Eax, 0, 0, 0);
  PRINT_VALUE(Eax, MaximumBasicFunction);
  return Eax;
}

VOID PrintVendorId()
{
  UINT32 Ebx, Ecx, Edx;
  CHAR8 VendorId[4 * 3 + 1];

  AsmCpuid(CPUID_STANDARD_FUNCTION_ADDR, 0, &Ebx, &Ecx, &Edx);
  Print(L"  EAX:%08x  EBX:%08x  ECX:%08x  EDX:%08x\n", 0, Ebx, Ecx, Edx);

  // The order of the register cannot be swapped
  *(UINT32 *)(VendorId + 0) = Ebx;
  *(UINT32 *)(VendorId + 4) = Edx;
  *(UINT32 *)(VendorId + 8) = Ecx;
  VendorId[12] = 0;
  Print(L"  VendorId = %a\n", VendorId);
}

VOID PrintCpuFeatureInfo()
{
  // Processor Signature
  CPUID_VERSION_INFO_EAX Eax;
  // Misc info
  CPUID_VERSION_INFO_EBX Ebx;
  // Feature Flags
  CPUID_VERSION_INFO_ECX Ecx;
  // Feature Flags
  CPUID_VERSION_INFO_EDX Edx;

  UINT32 DisplayFamily;
  UINT32 DisplayModel;

  Print(L"FUNCTION (Leaf %08x)\n", CPUID_VERSION_INFO);
  if (CPUID_VERSION_INFO > GetLargestStanardFunction())
    return;

  AsmCpuid(CPUID_VERSION_INFO, &Eax.Uint32, &Ebx.Uint32, &Ecx.Uint32, &Edx.Uint32);

  Print(L"  EAX:%08x  EBX:%08x  ECX:%08x  EDX:%08x\n", Eax.Uint32, Ebx.Uint32, Ecx.Uint32, Edx.Uint32);

  //CPUID201205-5.1.2.2
  DisplayFamily = Eax.Bits.FamilyId;
  DisplayFamily |= (Eax.Bits.ExtendedFamilyId << 4);

  DisplayModel = Eax.Bits.Model;
  DisplayModel |= (Eax.Bits.ExtendedModelId << 4);

  Print(L"EAX:  Type = 0x%x  Family = 0x%x  Model = 0x%x  Stepping = 0x%x\n", Eax.Bits.ProcessorType, DisplayFamily, DisplayModel, Eax.Bits.SteppingId);

  Print(L"EBX:  APIC ID = 0x%x\n", Ebx.Bits.InitialLocalApicId);
  if (Edx.Bits.HTT == 1)
  {
    Print(L"EBX:  MaxLogicalProcessors(per package)= 0x%x\n", Ebx.Bits.MaximumAddressableIdsForLogicalProcessors);
  }

  // AsmCpuid(CPUID_CACHE_PARAMS, &Eax.Uint32, &Ebx.Uint32, &Ecx.Uint32, &Edx.Uint32);
}

UINT32 GetLargestExtendedFunction()
{
  UINT32 Eax;
  Print(L"FUNCTION (Leaf %08x)\n", CPUID_EXTENDED_FUNCTION_ADDR);
  AsmCpuid(CPUID_EXTENDED_FUNCTION_ADDR, &Eax, NULL, NULL, NULL);
  Print(L"  EAX:%08x  EBX:%08x  ECX:%08x  EDX:%08x\n", Eax, 0, 0, 0);
  PRINT_VALUE(Eax, MaximumExtendedFunction);
  return Eax;
}

VOID PrintCpuBrandString()
{
  CPUID_BRAND_STRING_DATA Eax;
  CPUID_BRAND_STRING_DATA Ebx;
  CPUID_BRAND_STRING_DATA Ecx;
  CPUID_BRAND_STRING_DATA Edx;
  //
  // Array to store brand string from 3 brand string leafs with
  // 4 32-bit brand string values per leaf and an extra value to
  // null terminate the string.
  //
  UINT32 BrandString[3 * 4 + 1];
  UINT32 CpuidBrandStr1 = CPUID_EXTENDED_FUNCTION_ADDR | 0x02;
  UINT32 CpuidBrandStr2 = CPUID_EXTENDED_FUNCTION_ADDR | 0x03;
  UINT32 CpuidBrandStr3 = CPUID_EXTENDED_FUNCTION_ADDR | 0x04;
  UINT32 gMaxExtendedFunc = GetLargestExtendedFunction();
  if (CpuidBrandStr1 <= gMaxExtendedFunc)
  {
    AsmCpuid(CpuidBrandStr1, &Eax.Uint32, &Ebx.Uint32, &Ecx.Uint32, &Edx.Uint32);
    Print(L"CpuidBrandStr1 (Leaf %08x)\n", CpuidBrandStr1);
    Print(L"  EAX:%08x  EBX:%08x  ECX:%08x  EDX:%08x\n", Eax.Uint32, Ebx.Uint32, Ecx.Uint32, Edx.Uint32);
    BrandString[0] = Eax.Uint32;
    Print(L"1. Brand String = %a\n", (CHAR8 *)BrandString);
    BrandString[1] = Ebx.Uint32;
    Print(L"1. Brand String = %a\n", (CHAR8 *)BrandString);
    BrandString[2] = Ecx.Uint32;
    Print(L"1. Brand String = %a\n", (CHAR8 *)BrandString);
    BrandString[3] = Edx.Uint32;
    Print(L"1. Brand String = %a\n", (CHAR8 *)BrandString);
  }
  UINT32 test;

  if (CpuidBrandStr2 <= gMaxExtendedFunc)
  {
    AsmCpuid(CpuidBrandStr2, &Eax.Uint32, &Ebx.Uint32, &Ecx.Uint32, &Edx.Uint32);
    Print(L"CpuidBrandStr2 (Leaf %08x)\n", CpuidBrandStr2);
    Print(L"  EAX:%08x  EBX:%08x  ECX:%08x  EDX:%08x\n", Eax.Uint32, Ebx.Uint32, Ecx.Uint32, Edx.Uint32);
    BrandString[4] = Eax.Uint32;
    Print(L"2. Brand String = %a\n", (CHAR8 *)BrandString);
    BrandString[5] = Ebx.Uint32;
    Print(L"2. Brand String = %a\n", (CHAR8 *)BrandString);
    BrandString[6] = Ecx.Uint32;
    Print(L"2. Brand String = %a\n", (CHAR8 *)BrandString);
    BrandString[7] = Edx.Uint32;
    Print(L"2. Brand String = %a\n", (CHAR8 *)BrandString);
  }

  if (CpuidBrandStr3 <= gMaxExtendedFunc)
  {
    AsmCpuid(CpuidBrandStr3, &Eax.Uint32, &Ebx.Uint32, &Ecx.Uint32, &Edx.Uint32);
    Print(L"CpuidBrandStr3 (Leaf %08x)\n", CpuidBrandStr3);
    Print(L"  EAX:%08x  EBX:%08x  ECX:%08x  EDX:%08x\n", Eax.Uint32, Ebx.Uint32, Ecx.Uint32, Edx.Uint32);
    BrandString[8] = Eax.Uint32;
    Print(L"3. Brand String = %a\n", (CHAR8 *)BrandString);

    BrandString[9] = Ebx.Uint32;
    Print(L"3. Brand String = %a\n", (CHAR8 *)BrandString);
    BrandString[10] = Ecx.Uint32;
    Print(L"3. Brand String = %a\n", (CHAR8 *)BrandString);
    BrandString[11] = Edx.Uint32;
    Print(L"3. Brand String = %a\n", (CHAR8 *)BrandString);
  }

  BrandString[12] = 0;
  
  Print(L"Brand String = %a\n", (CHAR8 *)BrandString);
}

void PrintMicroCodeVersion()
{
  MSR_IA32_BIOS_SIGN_ID_REGISTER BiosSignIdMsr;
  AsmWriteMsr64(MSR_IA32_BIOS_SIGN_ID, 0);
  AsmCpuid(CPUID_VERSION_INFO, NULL, NULL, NULL, NULL);
  BiosSignIdMsr.Uint64 = AsmReadMsr64(MSR_IA32_BIOS_SIGN_ID);
  // BiosSignIdMsr.Bits.Reserved = (UINT32)BiosSignIdMsr.Uint64;
  // BiosSignIdMsr.Bits.MicrocodeUpdateSignature = (UINT32)RShiftU64(BiosSignIdMsr.Uint64, 32);
  Print(L"MicroCodeVersion 0x%08x\n", BiosSignIdMsr.Bits.MicrocodeUpdateSignature);
}