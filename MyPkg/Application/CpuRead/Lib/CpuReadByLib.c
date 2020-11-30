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

// BOOLEAN IsMtrrSupported()
// {
//   CPUID_VERSION_INFO_EDX Edx;
//   MSR_IA32_MTRRCAP_REGISTER MtrrCap;

//   // Check CPUID(1).EDX[12] for MTRR capability
//   AsmCpuid(CPUID_VERSION_INFO, NULL, NULL, NULL, &Edx.Uint32);
//   if (Edx.Bits.MTRR == 0)
//   {
//     return FALSE;
//   }

//   // Check number of variable MTRRs and fixed MTRRs existence.
//   // If number of variable MTRRs is zero, or fixed MTRRs do not
//   // exist, return false.
//   MtrrCap.Uint64 = AsmReadMsr64(MSR_IA32_MTRRCAP);
//   if ((MtrrCap.Bits.VCNT == 0) || (MtrrCap.Bits.FIX == 0))
//   {
//     return FALSE;
//   }
//   return TRUE;
// }

GLOBAL_REMOVE_IF_UNREFERENCED CONST CHAR8 *mMtrrMemoryCacheTypeShortName[] = {
    "UC-", // CacheUncacheable - 0
    "WC",  // CacheWriteCombining - 1
    "R*",  // Invalid
    "R*",  // Invalid
    "WT",  // CacheWriteThrough - 4
    "WP",  // CacheWriteProtected - 5
    "WB",  // CacheWriteBack - 6
    "R*"   // Invalid
};
VOID GetMtrrMemCacheType()
{
  MSR_IA32_MTRR_DEF_TYPE_REGISTER MtrrDefTypeMsr;
  // reference 3A - 11.11 MEMORY TYPE RANGE REGISTERS
  if (!IsMtrrSupported())
  {
    MtrrDefTypeMsr.Bits.Type = CacheUncacheable;
  }
  else
  {
    MtrrDefTypeMsr.Uint64 = AsmReadMsr64(MSR_IA32_MTRR_DEF_TYPE);
  }
  Print(L"Mtrr Mem Cache Type %a\n", mMtrrMemoryCacheTypeShortName[MIN(MtrrDefTypeMsr.Bits.Type, CacheInvalid)]);
}
CONST FIXED_MTRR mMtrrLibFixedMtrrTable[] = {
    {MSR_IA32_MTRR_FIX64K_00000,
     0,
     SIZE_64KB},
    {MSR_IA32_MTRR_FIX16K_80000,
     0x80000,
     SIZE_16KB},
    {MSR_IA32_MTRR_FIX16K_A0000,
     0xA0000,
     SIZE_16KB},
    {MSR_IA32_MTRR_FIX4K_C0000,
     0xC0000,
     SIZE_4KB},
    {MSR_IA32_MTRR_FIX4K_C8000,
     0xC8000,
     SIZE_4KB},
    {MSR_IA32_MTRR_FIX4K_D0000,
     0xD0000,
     SIZE_4KB},
    {MSR_IA32_MTRR_FIX4K_D8000,
     0xD8000,
     SIZE_4KB},
    {MSR_IA32_MTRR_FIX4K_E0000,
     0xE0000,
     SIZE_4KB},
    {MSR_IA32_MTRR_FIX4K_E8000,
     0xE8000,
     SIZE_4KB},
    {MSR_IA32_MTRR_FIX4K_F0000,
     0xF0000,
     SIZE_4KB},
    {MSR_IA32_MTRR_FIX4K_F8000,
     0xF8000,
     SIZE_4KB}};

VOID MtrrPrintAllMtrrsWorker(IN MTRR_SETTINGS *MtrrSetting)
{

  MTRR_SETTINGS LocalMtrrs;
  MTRR_SETTINGS *Mtrrs;
  UINTN Index;
  UINTN RangeCount;
  UINT64 MtrrValidBitsMask;
  UINT64 MtrrValidAddressMask;
  UINT32 VariableMtrrCount;
  BOOLEAN ContainVariableMtrr;
  MTRR_MEMORY_RANGE Ranges[ARRAY_SIZE(mMtrrLibFixedMtrrTable) * sizeof(UINT64) + 2 * ARRAY_SIZE(Mtrrs->Variables.Mtrr) + 1];
  MTRR_MEMORY_RANGE RawVariableRanges[ARRAY_SIZE(Mtrrs->Variables.Mtrr)];

  if (!IsMtrrSupported())
  {
    return;
  }

  VariableMtrrCount = GetVariableMtrrCountWorker();

  if (MtrrSetting != NULL)
  {
    Mtrrs = MtrrSetting;
  }
  else
  {
    MtrrGetAllMtrrs(&LocalMtrrs);
    Mtrrs = &LocalMtrrs;
  }

  //
  // Dump RAW MTRR contents
  //
  Print(L"MTRR Settings:\n");
  Print(L"=============\n");
  Print(L"MTRR Default Type: %016lx\n", Mtrrs->MtrrDefType);
  for (Index = 0; Index < ARRAY_SIZE(mMtrrLibFixedMtrrTable); Index++)
  {
    Print(L"Fixed MTRR[%02d]   : %016lx\n", Index, Mtrrs->Fixed.Mtrr[Index]);
  }
  ContainVariableMtrr = FALSE;
  for (Index = 0; Index < VariableMtrrCount; Index++)
  {
    if (((MSR_IA32_MTRR_PHYSMASK_REGISTER *)&Mtrrs->Variables.Mtrr[Index].Mask)->Bits.V == 0)
    {
      //
      // If mask is not valid, then do not display range
      //
      continue;
    }
    ContainVariableMtrr = TRUE;
    Print(L"Variable MTRR[%02d]: Base=%016lx Mask=%016lx\n",
          Index,
          Mtrrs->Variables.Mtrr[Index].Base,
          Mtrrs->Variables.Mtrr[Index].Mask);
  }
  if (!ContainVariableMtrr)
  {
    Print(L"Variable MTRR    : None.\n");
  }
  Print(L"\n");

  //
  // Dump MTRR setting in ranges
  //
  Print(L"Memory Ranges:\n");
  Print(L"====================================\n");
  MtrrLibInitializeMtrrMask(&MtrrValidBitsMask, &MtrrValidAddressMask);
  Ranges[0].BaseAddress = 0;
  Ranges[0].Length = MtrrValidBitsMask + 1;
  Ranges[0].Type = MtrrGetDefaultMemoryTypeWorker(Mtrrs);
  RangeCount = 1;

  MtrrLibGetRawVariableRanges(
      &Mtrrs->Variables, VariableMtrrCount,
      MtrrValidBitsMask, MtrrValidAddressMask, RawVariableRanges);
  MtrrLibApplyVariableMtrrs(
      RawVariableRanges, VariableMtrrCount,
      Ranges, ARRAY_SIZE(Ranges), &RangeCount);

  MtrrLibApplyFixedMtrrs(&Mtrrs->Fixed, Ranges, ARRAY_SIZE(Ranges), &RangeCount);

  for (Index = 0; Index < RangeCount; Index++)
  {
    Print(L"%a:%016lx-%016lx\n",
          mMtrrMemoryCacheTypeShortName[Ranges[Index].Type],
          Ranges[Index].BaseAddress, Ranges[Index].BaseAddress + Ranges[Index].Length - 1);
  }
}

EFI_STATUS EFIAPI UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  UINT32 gMaxBasicFunc = BASIC_FUNC_ADDR;

  UINT32 gMaxExtendedFunc = EXTENTED_FUNC_ADDR;

  VendorIdAndMaxBasicFunction(&gMaxBasicFunc);
  GetCpuFeatureInfo(gMaxBasicFunc);
  CpuidExtendedFunction(&gMaxExtendedFunc);
  GetCpuBrandString(gMaxExtendedFunc);

  GetMicroCodeVersion();
  GetMtrrMemCacheType();

  MtrrPrintAllMtrrsWorker(NULL);

  return EFI_SUCCESS;
}
