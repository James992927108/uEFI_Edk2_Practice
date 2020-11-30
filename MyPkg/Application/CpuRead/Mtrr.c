#include "Mtrr.h"

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

// call Lib
VOID PrintAllMtrrsWorker()
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
  MtrrGetAllMtrrs(&LocalMtrrs);
  Mtrrs = &LocalMtrrs;

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