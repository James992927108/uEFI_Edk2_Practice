#include "CpuRead.h"

VOID VendorIdAndMaxBasicFunction(IN OUT UINT32 *gMaxBasicFunc)
{
  UINT32 Eax, Ebx, Ecx, Edx;
  CHAR8 VendorId[13];

  UINT32 FunctionNum = *gMaxBasicFunc;
  Print(L"FUNCTION (Leaf %08x)\n", FunctionNum);

  AsmCpuid(FunctionNum, &Eax, &Ebx, &Ecx, &Edx);
  Print(L"  EAX:%08x  EBX:%08x  ECX:%08x  EDX:%08x\n", Eax, Ebx, Ecx, Edx);
  PRINT_VALUE(Eax, MaximumBasicFunction);
  *gMaxBasicFunc = Eax;

  // The order of the register cannot be swapped
  *(UINT32 *)(VendorId + 0) = Ebx;
  *(UINT32 *)(VendorId + 4) = Edx;
  *(UINT32 *)(VendorId + 8) = Ecx;
  VendorId[12] = 0;
  Print(L"  VendorId = %a\n", VendorId);
}

VOID GetCpuFeatureInfo(IN UINT32 gMaxBasicFunc)
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

  UINT32 FunctionNum = CPUID_VERSION_INFO;
  Print(L"FUNCTION (Leaf %08x)\n", FunctionNum);
  if (FunctionNum > gMaxBasicFunc)
    return;

  AsmCpuid(FunctionNum, &Eax.Uint32, &Ebx.Uint32, &Ecx.Uint32, &Edx.Uint32);

  Print(L"  EAX:%08x  EBX:%08x  ECX:%08x  EDX:%08x\n", Eax.Uint32, Ebx.Uint32, Ecx.Uint32, Edx.Uint32);

  DisplayFamily = Eax.Bits.FamilyId;
  // 0000 1111
  if (Eax.Bits.FamilyId == 0x0F)
  {
    DisplayFamily |= (Eax.Bits.ExtendedFamilyId << 4);
  }

  DisplayModel = Eax.Bits.Model;
  // 0000 0110 || 0000 1111
  if (Eax.Bits.FamilyId == 0x06 || Eax.Bits.FamilyId == 0x0f)
  {
    DisplayModel |= (Eax.Bits.ExtendedModelId << 4);
  }

  Print(L"  Type = %x  Family = %x  Model = %x  Stepping = %x\n", Eax.Bits.ProcessorType, DisplayFamily, DisplayModel, Eax.Bits.SteppingId);
}

VOID CpuidExtendedFunction(IN OUT UINT32 *gMaxExtendedFunc)
{
  UINT32 Eax;

  UINT32 FunctionNum = *gMaxExtendedFunc;
  Print(L"FUNCTION (Leaf %08x)\n", FunctionNum);

  AsmCpuid(CPUID_EXTENDED_FUNCTION, &Eax, NULL, NULL, NULL);
  Print(L"  EAX:%08x  EBX:%08x  ECX:%08x  EDX:%08x\n", Eax, 0, 0, 0);

  PRINT_VALUE(Eax, MaximumExtendedFunction);
  *gMaxExtendedFunc = Eax;
}

VOID GetCpuBrandString(IN UINT32 gMaxExtendedFunc)
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
  UINT32 CpuidBrandStr1 = EXTENTED_FUNC_ADDR | 0x02;
  UINT32 CpuidBrandStr2 = EXTENTED_FUNC_ADDR | 0x03;
  UINT32 CpuidBrandStr3 = EXTENTED_FUNC_ADDR | 0x04;

  if (CpuidBrandStr1 <= gMaxExtendedFunc)
  {
    AsmCpuid(CpuidBrandStr1, &Eax.Uint32, &Ebx.Uint32, &Ecx.Uint32, &Edx.Uint32);
    Print(L"CpuidBrandStr1 (Leaf %08x)\n", CpuidBrandStr1);
    Print(L"  EAX:%08x  EBX:%08x  ECX:%08x  EDX:%08x\n", Eax.Uint32, Ebx.Uint32, Ecx.Uint32, Edx.Uint32);
    BrandString[0] = Eax.Uint32;
    BrandString[1] = Ebx.Uint32;
    BrandString[2] = Ecx.Uint32;
    BrandString[3] = Edx.Uint32;
  }

  if (CpuidBrandStr2 <= gMaxExtendedFunc)
  {
    AsmCpuid(CpuidBrandStr2, &Eax.Uint32, &Ebx.Uint32, &Ecx.Uint32, &Edx.Uint32);
    Print(L"CpuidBrandStr2 (Leaf %08x)\n", CpuidBrandStr2);
    Print(L"  EAX:%08x  EBX:%08x  ECX:%08x  EDX:%08x\n", Eax.Uint32, Ebx.Uint32, Ecx.Uint32, Edx.Uint32);
    BrandString[4] = Eax.Uint32;
    BrandString[5] = Ebx.Uint32;
    BrandString[6] = Ecx.Uint32;
    BrandString[7] = Edx.Uint32;
  }

  if (CpuidBrandStr3 <= gMaxExtendedFunc)
  {
    AsmCpuid(CpuidBrandStr3, &Eax.Uint32, &Ebx.Uint32, &Ecx.Uint32, &Edx.Uint32);
    Print(L"CpuidBrandStr3 (Leaf %08x)\n", CpuidBrandStr3);
    Print(L"  EAX:%08x  EBX:%08x  ECX:%08x  EDX:%08x\n", Eax.Uint32, Ebx.Uint32, Ecx.Uint32, Edx.Uint32);
    BrandString[8] = Eax.Uint32;
    BrandString[9] = Ebx.Uint32;
    BrandString[10] = Ecx.Uint32;
    BrandString[11] = Edx.Uint32;
  }

  BrandString[12] = 0;

  Print(L"Brand String = %a\n", (CHAR8 *)BrandString);
}

void GetMicroCodeVersion()
{
  MSR_IA32_BIOS_SIGN_ID_REGISTER BiosSignIdMsr;
  AsmWriteMsr64(MSR_IA32_BIOS_SIGN_ID, 0);
  AsmCpuid(CPUID_VERSION_INFO, NULL, NULL, NULL, NULL);
  BiosSignIdMsr.Uint64 = AsmReadMsr64(MSR_IA32_BIOS_SIGN_ID);
  BiosSignIdMsr.Bits.Reserved = (UINT32)BiosSignIdMsr.Uint64;
  BiosSignIdMsr.Bits.MicrocodeUpdateSignature = (UINT32)RShiftU64(BiosSignIdMsr.Uint64, 32);
  Print(L"MicroCodeVersion 0x%08x\n", BiosSignIdMsr.Bits.MicrocodeUpdateSignature);
}