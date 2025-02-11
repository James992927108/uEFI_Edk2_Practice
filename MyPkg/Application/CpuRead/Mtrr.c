#include "Mtrr.h"

/**
  This function will fill the exchange info structure.

  @param[in] CpuMpData          Pointer to CPU MP Data

**/
VOID FillExchangeInfoData(
    IN CPU_MP_DATA *CpuMpData)
{
  volatile MP_CPU_EXCHANGE_INFO *ExchangeInfo;
  UINTN Size;
  IA32_SEGMENT_DESCRIPTOR *Selector;

  ExchangeInfo->Lock = 0;
  ExchangeInfo->StackStart = CpuMpData->Buffer;
  ExchangeInfo->StackSize = CpuMpData->CpuApStackSize;
  ExchangeInfo->BufferStart = CpuMpData->WakeupBuffer;
  ExchangeInfo->ModeOffset = CpuMpData->AddressMap.ModeEntryOffset;

  ExchangeInfo->CodeSegment = AsmReadCs();
  ExchangeInfo->DataSegment = AsmReadDs();

  ExchangeInfo->Cr3 = AsmReadCr3();

  // ExchangeInfo->CFunction       = (UINTN) ApWakeupFunction;
  ExchangeInfo->ApIndex = 0;
  ExchangeInfo->NumApsExecuting = 0;
  ExchangeInfo->InitFlag = (UINTN)CpuMpData->InitFlag;
  ExchangeInfo->CpuInfo = (CPU_INFO_IN_HOB *)(UINTN)CpuMpData->CpuInfoInHob;
  ExchangeInfo->CpuMpData = CpuMpData;

  // ExchangeInfo->EnableExecuteDisable = IsBspExecuteDisableEnabled ();

  // ExchangeInfo->InitializeFloatingPointUnitsAddress = (UINTN)InitializeFloatingPointUnits;

  //
  // Get the BSP's data of GDT and IDT
  //
  AsmReadGdtr((IA32_DESCRIPTOR *)&ExchangeInfo->GdtrProfile);
  AsmReadIdtr((IA32_DESCRIPTOR *)&ExchangeInfo->IdtrProfile);

  //
  // Find a 32-bit code segment
  //
  Selector = (IA32_SEGMENT_DESCRIPTOR *)ExchangeInfo->GdtrProfile.Base;
  Size = ExchangeInfo->GdtrProfile.Limit + 1;
  while (Size > 0)
  {
    if (Selector->Bits.L == 0 && Selector->Bits.Type >= 8)
    {
      ExchangeInfo->ModeTransitionSegment =
          (UINT16)((UINTN)Selector - ExchangeInfo->GdtrProfile.Base);
      break;
    }
    Selector += 1;
    Size -= sizeof(IA32_SEGMENT_DESCRIPTOR);
  }

  //
  // Copy all 32-bit code and 64-bit code into memory with type of
  // EfiBootServicesCode to avoid page fault if NX memory protection is enabled.
  //
  if (CpuMpData->WakeupBufferHigh != 0)
  {
    Size = CpuMpData->AddressMap.RendezvousFunnelSize -
           CpuMpData->AddressMap.ModeTransitionOffset;
    CopyMem(
        (VOID *)CpuMpData->WakeupBufferHigh,
        CpuMpData->AddressMap.RendezvousFunnelAddress +
            CpuMpData->AddressMap.ModeTransitionOffset,
        Size);

    ExchangeInfo->ModeTransitionMemory = (UINT32)CpuMpData->WakeupBufferHigh;
  }
  else
  {
    ExchangeInfo->ModeTransitionMemory = (UINT32)(ExchangeInfo->BufferStart + CpuMpData->AddressMap.ModeTransitionOffset);
  }

  ExchangeInfo->ModeHighMemory = ExchangeInfo->ModeTransitionMemory +
                                 (UINT32)ExchangeInfo->ModeOffset -
                                 (UINT32)CpuMpData->AddressMap.ModeTransitionOffset;
  ExchangeInfo->ModeHighSegment = (UINT16)ExchangeInfo->CodeSegment;
}

/**
  Detect whether Mwait-monitor feature is supported.

  @retval TRUE    Mwait-monitor feature is supported.
  @retval FALSE   Mwait-monitor feature is not supported.
**/
BOOLEAN
IsMwaitSupport (
  VOID
  )
{
  CPUID_VERSION_INFO_ECX        VersionInfoEcx;

  AsmCpuid (CPUID_VERSION_INFO, NULL, NULL, &VersionInfoEcx.Uint32, NULL);
  return (VersionInfoEcx.Bits.MONITOR == 1) ? TRUE : FALSE;
}

/**
  Get AP loop mode.

  @param[out] MonitorFilterSize  Returns the largest monitor-line size in bytes.

  @return The AP loop mode.
**/
UINT8
GetApLoopMode (
  OUT UINT32     *MonitorFilterSize
  )
{
  UINT8                         ApLoopMode;
  CPUID_MONITOR_MWAIT_EBX       MonitorMwaitEbx;

  ASSERT (MonitorFilterSize != NULL);

  ApLoopMode = PcdGet8 (PcdCpuApLoopMode);
  ASSERT (ApLoopMode >= ApInHltLoop && ApLoopMode <= ApInRunLoop);
  if (ApLoopMode == ApInMwaitLoop) {
    if (!IsMwaitSupport ()) {
      //
      // If processor does not support MONITOR/MWAIT feature,
      // force AP in Hlt-loop mode
      //
      ApLoopMode = ApInHltLoop;
    }
  }

  if (ApLoopMode != ApInMwaitLoop) {
    *MonitorFilterSize = sizeof (UINT32);
  } else {
    //
    // CPUID.[EAX=05H]:EBX.BIT0-15: Largest monitor-line size in bytes
    // CPUID.[EAX=05H].EDX: C-states supported using MWAIT
    //
    AsmCpuid (CPUID_MONITOR_MWAIT, NULL, &MonitorMwaitEbx.Uint32, NULL, NULL);
    *MonitorFilterSize = MonitorMwaitEbx.Bits.LargestMonitorLineSize;
  }

  return ApLoopMode;
}

/**
  Set the Application Processors state.

  @param[in]   CpuData    The pointer to CPU_AP_DATA of specified AP
  @param[in]   State      The AP status
**/
VOID
SetApState (
  IN  CPU_AP_DATA     *CpuData,
  IN  CPU_STATE       State
  )
{
  AcquireSpinLock (&CpuData->ApLock);
  CpuData->State = State;
  ReleaseSpinLock (&CpuData->ApLock);
}

/**
  Initialize CPU AP Data when AP is wakeup at the first time.

  @param[in, out] CpuMpData        Pointer to PEI CPU MP Data
  @param[in]      ProcessorNumber  The handle number of processor
  @param[in]      BistData         Processor BIST data
  @param[in]      ApTopOfStack     Top of AP stack

**/
VOID
InitializeApData (
  IN OUT CPU_MP_DATA      *CpuMpData,
  IN     UINTN            ProcessorNumber,
  IN     UINT32           BistData,
  IN     UINT64           ApTopOfStack
  )
{
  CPU_INFO_IN_HOB          *CpuInfoInHob;

  CpuInfoInHob = (CPU_INFO_IN_HOB *) (UINTN) CpuMpData->CpuInfoInHob;
  CpuInfoInHob[ProcessorNumber].InitialApicId = GetInitialApicId ();
  CpuInfoInHob[ProcessorNumber].ApicId        = GetApicId ();
  CpuInfoInHob[ProcessorNumber].Health        = BistData;
  CpuInfoInHob[ProcessorNumber].ApTopOfStack  = ApTopOfStack;

  CpuMpData->CpuData[ProcessorNumber].Waiting    = FALSE;
  CpuMpData->CpuData[ProcessorNumber].CpuHealthy = (BistData == 0) ? TRUE : FALSE;
  if (CpuInfoInHob[ProcessorNumber].InitialApicId >= 0xFF) {
    //
    // Set x2APIC mode if there are any logical processor reporting
    // an Initial APIC ID of 255 or greater.
    //
    AcquireSpinLock(&CpuMpData->MpLock);
    CpuMpData->X2ApicEnable = TRUE;
    ReleaseSpinLock(&CpuMpData->MpLock);
  }

  InitializeSpinLock(&CpuMpData->CpuData[ProcessorNumber].ApLock);
  SetApState (&CpuMpData->CpuData[ProcessorNumber], CpuStateIdle);
}

VOID ForWakeUpAP()
{
  // CPU_INFO_IN_HOB *CpuInfoInHob;
  UINT32 MaxLogicalProcessorNumber;
  UINT32 ApStackSize;
  MP_ASSEMBLY_ADDRESS_MAP AddressMap;
  UINTN BufferSize;
  UINT32 MonitorFilterSize;
  VOID *MpBuffer;
  UINTN Buffer;
  CPU_MP_DATA *CpuMpData;
  UINT8 ApLoopMode;
  UINT8 *MonitorBuffer;
  UINTN Index;
  UINTN ApResetVectorSize;
  UINTN BackupBufferAddr;

  MaxLogicalProcessorNumber = PcdGet32(PcdCpuMaxLogicalProcessorNumber);

  ASSERT(MaxLogicalProcessorNumber != 0);

  // AsmGetAddressMap(&AddressMap);
  ApResetVectorSize = AddressMap.RendezvousFunnelSize + sizeof(MP_CPU_EXCHANGE_INFO);
  ApStackSize = PcdGet32(PcdCpuApStackSize);
  ApLoopMode = GetApLoopMode(&MonitorFilterSize);

  BufferSize = ApStackSize * MaxLogicalProcessorNumber;
  BufferSize += MonitorFilterSize * MaxLogicalProcessorNumber;
  BufferSize += sizeof(CPU_MP_DATA);
  BufferSize += ApResetVectorSize;
  BufferSize += (sizeof(CPU_AP_DATA) + sizeof(CPU_INFO_IN_HOB)) * MaxLogicalProcessorNumber;
  MpBuffer = AllocatePages(EFI_SIZE_TO_PAGES(BufferSize));
  ASSERT(MpBuffer != NULL);
  ZeroMem(MpBuffer, BufferSize);
  Buffer = (UINTN)MpBuffer;

  MonitorBuffer = (UINT8 *)(Buffer + ApStackSize * MaxLogicalProcessorNumber);
  BackupBufferAddr = (UINTN)MonitorBuffer + MonitorFilterSize * MaxLogicalProcessorNumber;
  CpuMpData = (CPU_MP_DATA *)(BackupBufferAddr + ApResetVectorSize);
  CpuMpData->Buffer = Buffer;
  CpuMpData->CpuApStackSize = ApStackSize;
  CpuMpData->BackupBuffer = BackupBufferAddr;
  CpuMpData->BackupBufferSize = ApResetVectorSize;
  CpuMpData->WakeupBuffer = (UINTN)-1;
  CpuMpData->CpuCount = 1;
  CpuMpData->BspNumber = 0;
  CpuMpData->WaitEvent = NULL;
  CpuMpData->SwitchBspFlag = FALSE;
  CpuMpData->CpuData = (CPU_AP_DATA *)(CpuMpData + 1);
  CpuMpData->CpuInfoInHob = (UINT64)(UINTN)(CpuMpData->CpuData + MaxLogicalProcessorNumber);
  CpuMpData->MicrocodePatchAddress = PcdGet64(PcdCpuMicrocodePatchAddress);
  CpuMpData->MicrocodePatchRegionSize = PcdGet64(PcdCpuMicrocodePatchRegionSize);
  InitializeSpinLock(&CpuMpData->MpLock);
  //
  // Save BSP's Control registers to APs
  //
  // SaveVolatileRegisters(&CpuMpData->CpuData[0].VolatileRegisters);
  //
  // Set BSP basic information
  //
  InitializeApData(CpuMpData, 0, 0, CpuMpData->Buffer + ApStackSize);
  //
  // Save assembly code information
  //
  CopyMem(&CpuMpData->AddressMap, &AddressMap, sizeof(MP_ASSEMBLY_ADDRESS_MAP));
  //
  // Finally set AP loop mode
  //
  CpuMpData->ApLoopMode = ApLoopMode;
  DEBUG((DEBUG_INFO, "AP Loop Mode is %d\n", CpuMpData->ApLoopMode));
  //
  // Set up APs wakeup signal buffer
  //
  for (Index = 0; Index < MaxLogicalProcessorNumber; Index++)
  {
    CpuMpData->CpuData[Index].StartupApSignal =
        (UINT32 *)(MonitorBuffer + MonitorFilterSize * Index);
  }
  //
  // Load Microcode on BSP
  //
  // MicrocodeDetect(CpuMpData);
  // CPU_AP_DATA *CpuData;

  FillExchangeInfoData(CpuMpData);

  // ExchangeInfo = CpuMpData->MpCpuExchangeInfo;
  
  // SendInitSipiSipiAllExcludingSelf ((UINT32) ExchangeInfo->BufferStart);
  // for (UINTN Index = 0; Index < CpuMpData->CpuCount; Index++)
  // {
  //   CpuData = &CpuMpData->CpuData[Index];
  //   if (Index != CpuMpData->BspNumber)
  //   {
  //     WaitApWakeup(CpuData->StartupApSignal);
  //   }
  // }
}

BOOLEAN IsMtrrSupported(VOID)
{
  CPUID_VERSION_INFO_EDX Edx;
  // vol 3 11.11.1 MTRR Fuature Identification
  AsmCpuid(CPUID_VERSION_INFO, NULL, NULL, NULL, &Edx.Uint32);
  if (Edx.Bits.MTRR)
  {
    Print(L"Support MTRR\n");
    return TRUE;
  }
  else
  {
    Print(L"Not Support MTRR\n");
    return FALSE;
  }
}

VOID GetMttrCap(OUT UINT32 *VCNT, OUT UINT32 *FIX, OUT UINT32 *SMRR)
{
  MSR_IA32_MTRRCAP_REGISTER MtrrCap;
  MtrrCap.Uint64 = AsmReadMsr64(MSR_IA32_MTRRCAP);
  Print(L"MtrrCap: 0x%016lx\n", MtrrCap.Uint64);
  Print(L"VCNT: 0x%08x, FIX: 0x%08x, SMRR: 0x%08x\n", MtrrCap.Bits.VCNT, MtrrCap.Bits.FIX, MtrrCap.Bits.SMRR);
  *VCNT = MtrrCap.Bits.VCNT;
  *FIX = MtrrCap.Bits.FIX;
  *SMRR = MtrrCap.Bits.SMRR;
}

MTRR_FIXED_SETTINGS *LoadFixedRangeMtrrs(
    OUT MTRR_FIXED_SETTINGS *FixedSettings)
{
  for (UINT32 Index = 0; Index < MTRR_NUMBER_OF_FIXED_MTRR; Index++)
  {
    FixedSettings->Mtrr[Index] = AsmReadMsr64(mMtrrLibFixedMtrrTable[Index].Msr);
  }
  return FixedSettings;
}

MTRR_VARIABLE_SETTINGS *LoadVariableRangeMtrrs(
    IN UINT32 VCNT,
    OUT MTRR_VARIABLE_SETTINGS *VariableSettings)
{
  for (UINT32 Index = 0; Index < VCNT; Index++)
  {
    VariableSettings->Mtrr[Index].Mask = AsmReadMsr64(MSR_IA32_MTRR_PHYSMASK0 + (Index << 1));
    // When the valid flag in the IA32_SMRR_PHYSMASK MSR is 1
    if (((MSR_IA32_MTRR_PHYSMASK_REGISTER *)&VariableSettings->Mtrr[Index].Mask)->Bits.V == 1)
    {
      // If the logical processor is in SMM, accesses uses the memory type in the IA32_SMRR_PHYSBASE MSR.
      VariableSettings->Mtrr[Index].Base = AsmReadMsr64(MSR_IA32_MTRR_PHYSBASE0 + (Index << 1));
    }
    else
    {
      Print(L"Index %d, is not SMM. Mem type is UC\n", Index);
      // If the logical processor is not in SMM, write accesses are ignored and read accesses return a fixed value for each
      // byte. The uncacheable memory type (UC) is used in this case.
    }
  }
  return VariableSettings;
}

VOID DumpInitMTRRContent(MTRR_SETTINGS *Mtrrs, UINT32 VCNT)
{
  Print(L"DumpInitMTRRContent:\n");
  Print(L"=============\n");

  MSR_IA32_MTRR_DEF_TYPE_REGISTER *MtrrDefType = (MSR_IA32_MTRR_DEF_TYPE_REGISTER *)&Mtrrs->MtrrDefType;
  Print(L"MTRR Define Type: %016lx\n", MtrrDefType->Uint64);

  if (MtrrDefType->Bits.FE)
  {
    Print(L"Define Fix MTRR Enable\n");
  }

  if (MtrrDefType->Bits.E)
  {
    Print(L"Define MTRR Enable\n");
  }

  for (UINTN Index = 0; Index < ARRAY_SIZE(mMtrrLibFixedMtrrTable); Index++)
  {
    Print(L"Fixed MTRR[%02d]   : %016lx\n", Index, Mtrrs->Fixed.Mtrr[Index]);
  }

  for (UINTN Index = 0; Index < VCNT; Index++)
  {
    if (((MSR_IA32_MTRR_PHYSMASK_REGISTER *)&Mtrrs->Variables.Mtrr[Index].Mask)->Bits.V == 1)
    {
      Print(L"Variable MTRR[%02d]: Base=%016lx Mask=%016lx\n", Index,
            Mtrrs->Variables.Mtrr[Index].Base,
            Mtrrs->Variables.Mtrr[Index].Mask);
    }
  }
  Print(L"\n");
}

VOID InitializeMtrrMask(OUT UINT64 *MtrrValidBitsMask, OUT UINT64 *MtrrValidAddressMask)
{
  UINT32 MaxExtendedFunction;
  CPUID_VIR_PHY_ADDRESS_SIZE_EAX VirPhyAddressSize;

  AsmCpuid(CPUID_EXTENDED_FUNCTION, &MaxExtendedFunction, NULL, NULL, NULL);
  *MtrrValidBitsMask = 0;
  *MtrrValidAddressMask = 0;
  // 11.11.3.1
  if (MaxExtendedFunction >= CPUID_VIR_PHY_ADDRESS_SIZE)
  {
    // CPUID 201205 - 5.2.7 Virtul and Physical Address Size
    // If this function is suported, the Physical Address Size return in EAX[7:0] should be
    // used to determine the number of bits to configure MTRRn_PhysMask values with.
    AsmCpuid(CPUID_VIR_PHY_ADDRESS_SIZE, &VirPhyAddressSize.Uint32, NULL, NULL, NULL);
  }
  else
  {
    //11.11.3 The base and mask values entered in variable-range MTRR pairs are 24-bit values that the processor extends to 36-bits
    VirPhyAddressSize.Bits.PhysicalAddressBits = 36;
  }
  Print(L"VirPhyAddressSize: %08x\n", VirPhyAddressSize.Uint32);
  *MtrrValidBitsMask = LShiftU64(1, VirPhyAddressSize.Bits.PhysicalAddressBits) - 1;
  // [0:11]->0 [12:63]-> 1
  *MtrrValidAddressMask = *MtrrValidBitsMask & 0xfffffffffffff000ULL;
}

UINT32 GetRawVariableRanges(
    IN MTRR_VARIABLE_SETTINGS *VariableSettings,
    IN UINTN VCNT,
    IN UINT64 MtrrValidBitsMask,
    IN UINT64 MtrrValidAddressMask,
    OUT MTRR_MEMORY_RANGE *VariableMtrr)
{
  UINT32 UsedMtrr;
  ZeroMem(VariableMtrr, sizeof(MTRR_MEMORY_RANGE) * ARRAY_SIZE(VariableSettings->Mtrr));
  for (UINTN Index = 0, UsedMtrr = 0; Index < VCNT; Index++)
  {
    if (((MSR_IA32_MTRR_PHYSMASK_REGISTER *)&VariableSettings->Mtrr[Index].Mask)->Bits.V == 1)
    {
      VariableMtrr[Index].BaseAddress = (VariableSettings->Mtrr[Index].Base & MtrrValidAddressMask);
      VariableMtrr[Index].Length = ((~(VariableSettings->Mtrr[Index].Mask & MtrrValidAddressMask)) & MtrrValidBitsMask) + 1;
      VariableMtrr[Index].Type = (MTRR_MEMORY_CACHE_TYPE)(VariableSettings->Mtrr[Index].Base & 0x0ff);
      UsedMtrr++;
    }
  }
  return UsedMtrr;
}

RETURN_STATUS SetMemoryType(
    IN MTRR_MEMORY_RANGE *Ranges,
    IN UINTN Capacity,
    IN OUT UINTN *Count,
    IN UINT64 BaseAddress,
    IN UINT64 Length,
    IN MTRR_MEMORY_CACHE_TYPE Type)
{
  UINTN Index;
  UINT64 Limit;
  UINT64 LengthLeft;
  UINT64 LengthRight;
  UINTN StartIndex;
  UINTN EndIndex;
  UINTN DeltaCount;
  LengthRight = 0;
  LengthLeft = 0;
  Limit = BaseAddress + Length;
  StartIndex = *Count;
  EndIndex = *Count;
  for (Index = 0; Index < *Count; Index++)
  {
    if ((StartIndex == *Count) &&
        (Ranges[Index].BaseAddress <= BaseAddress) &&
        (BaseAddress < Ranges[Index].BaseAddress + Ranges[Index].Length))
    {
      StartIndex = Index;
      LengthLeft = BaseAddress - Ranges[Index].BaseAddress;
    }

    if ((EndIndex == *Count) &&
        (Ranges[Index].BaseAddress < Limit) &&
        (Limit <= Ranges[Index].BaseAddress + Ranges[Index].Length))
    {
      EndIndex = Index;
      LengthRight = Ranges[Index].BaseAddress + Ranges[Index].Length - Limit;
      break;
    }
  }

  ASSERT(StartIndex != *Count && EndIndex != *Count);
  if (StartIndex == EndIndex && Ranges[StartIndex].Type == Type)
  {
    return RETURN_ALREADY_STARTED;
  }

  //
  // The type change may cause merging with previous range or next range.
  // Update the StartIndex, EndIndex, BaseAddress, Length so that following
  // logic doesn't need to consider merging.
  //
  if (StartIndex != 0)
  {
    if (LengthLeft == 0 && Ranges[StartIndex - 1].Type == Type)
    {
      StartIndex--;
      Length += Ranges[StartIndex].Length;
      BaseAddress -= Ranges[StartIndex].Length;
    }
  }
  if (EndIndex != (*Count) - 1)
  {
    if (LengthRight == 0 && Ranges[EndIndex + 1].Type == Type)
    {
      EndIndex++;
      Length += Ranges[EndIndex].Length;
    }
  }

  //
  // |- 0 -|- 1 -|- 2 -|- 3 -| StartIndex EndIndex DeltaCount  Count (Count = 4)
  //   |++++++++++++++++++|    0          3         1=3-0-2    3
  //   |+++++++|               0          1        -1=1-0-2    5
  //   |+|                     0          0        -2=0-0-2    6
  // |+++|                     0          0        -1=0-0-2+1  5
  //
  //
  DeltaCount = EndIndex - StartIndex - 2;
  if (LengthLeft == 0)
  {
    DeltaCount++;
  }
  if (LengthRight == 0)
  {
    DeltaCount++;
  }
  if (*Count - DeltaCount > Capacity)
  {
    return RETURN_OUT_OF_RESOURCES;
  }

  //
  // Reserve (-DeltaCount) space
  //
  CopyMem(&Ranges[EndIndex + 1 - DeltaCount], &Ranges[EndIndex + 1], (*Count - EndIndex - 1) * sizeof(Ranges[0]));
  *Count -= DeltaCount;

  if (LengthLeft != 0)
  {
    Ranges[StartIndex].Length = LengthLeft;
    StartIndex++;
  }
  if (LengthRight != 0)
  {
    Ranges[EndIndex - DeltaCount].BaseAddress = BaseAddress + Length;
    Ranges[EndIndex - DeltaCount].Length = LengthRight;
    Ranges[EndIndex - DeltaCount].Type = Ranges[EndIndex].Type;
  }
  Ranges[StartIndex].BaseAddress = BaseAddress;
  Ranges[StartIndex].Length = Length;
  Ranges[StartIndex].Type = Type;
  return RETURN_SUCCESS;
}

RETURN_STATUS ApplyVariableMtrrs(
    IN CONST MTRR_MEMORY_RANGE *VariableMtrr,
    IN UINT32 VCNT,
    IN UINTN RangeCapacity,
    IN OUT MTRR_MEMORY_RANGE *Ranges,
    IN OUT UINTN *RangeCount)
{
  RETURN_STATUS Status;
  UINTN Index;

  //
  // WT > WB
  // UC > *
  // UC > * (except WB, UC) > WB
  //

  //
  // 1. Set WB
  //
  for (Index = 0; Index < VCNT; Index++)
  {
    if ((VariableMtrr[Index].Length != 0) && (VariableMtrr[Index].Type == CacheWriteBack))
    {
      Status = SetMemoryType(
          Ranges, RangeCapacity, RangeCount,
          VariableMtrr[Index].BaseAddress, VariableMtrr[Index].Length, VariableMtrr[Index].Type);
      if (Status == RETURN_OUT_OF_RESOURCES)
      {
        return Status;
      }
    }
  }

  //
  // 2. Set other types than WB or UC
  //
  for (Index = 0; Index < VCNT; Index++)
  {
    if ((VariableMtrr[Index].Length != 0) &&
        (VariableMtrr[Index].Type != CacheWriteBack) && (VariableMtrr[Index].Type != CacheUncacheable))
    {
      Status = SetMemoryType(
          Ranges, RangeCapacity, RangeCount,
          VariableMtrr[Index].BaseAddress, VariableMtrr[Index].Length, VariableMtrr[Index].Type);
      if (Status == RETURN_OUT_OF_RESOURCES)
      {
        return Status;
      }
    }
  }

  //
  // 3. Set UC
  //
  for (Index = 0; Index < VCNT; Index++)
  {
    if (VariableMtrr[Index].Length != 0 && VariableMtrr[Index].Type == CacheUncacheable)
    {
      Status = SetMemoryType(
          Ranges, RangeCapacity, RangeCount,
          VariableMtrr[Index].BaseAddress, VariableMtrr[Index].Length, VariableMtrr[Index].Type);
      if (Status == RETURN_OUT_OF_RESOURCES)
      {
        return Status;
      }
    }
  }
  return RETURN_SUCCESS;
}

RETURN_STATUS ApplyFixedMtrrs(
    IN MTRR_FIXED_SETTINGS *Fixed,
    IN OUT MTRR_MEMORY_RANGE *Ranges,
    IN UINTN RangeCapacity,
    IN OUT UINTN *RangeCount)
{
  RETURN_STATUS Status;
  UINTN MsrIndex;
  UINTN Index;
  MTRR_MEMORY_CACHE_TYPE MemoryType;
  UINT64 Base;
  Base = 0;
  for (MsrIndex = 0; MsrIndex < ARRAY_SIZE(mMtrrLibFixedMtrrTable); MsrIndex++)
  {
    ASSERT(Base == mMtrrLibFixedMtrrTable[MsrIndex].BaseAddress);
    for (Index = 0; Index < sizeof(UINT64); Index++)
    {
      MemoryType = (MTRR_MEMORY_CACHE_TYPE)((UINT8 *)(&Fixed->Mtrr[MsrIndex]))[Index];
      Status = SetMemoryType(Ranges, RangeCapacity, RangeCount, Base, mMtrrLibFixedMtrrTable[MsrIndex].Length, MemoryType);
      if (Status == RETURN_OUT_OF_RESOURCES)
      {
        return Status;
      }
      Base += mMtrrLibFixedMtrrTable[MsrIndex].Length;
    }
  }
  ASSERT(Base == BASE_1MB);
  return RETURN_SUCCESS;
}

void DumpMTRRSetting(MTRR_SETTINGS *Mtrrs, UINT32 VCNT)
{
  Print(L"Memory Ranges:\n");
  Print(L"====================================\n");

  UINT64 MtrrValidBitsMask;
  UINT64 MtrrValidAddressMask;

  InitializeMtrrMask(&MtrrValidBitsMask, &MtrrValidAddressMask);
  Print(L"MtrrValidBitsMask: %016lx\n", MtrrValidBitsMask);
  Print(L"MtrrValidAddressMask: %016lx\n", MtrrValidAddressMask);

  UINTN RangeCount;
  MTRR_MEMORY_RANGE Ranges[ARRAY_SIZE(mMtrrLibFixedMtrrTable) * sizeof(UINT64) + 2 * ARRAY_SIZE(Mtrrs->Variables.Mtrr) + 1];

  RangeCount = 1;
  Ranges[0].BaseAddress = 0;
  Ranges[0].Length = MtrrValidBitsMask + 1;
  Ranges[0].Type = (MTRR_MEMORY_CACHE_TYPE)((MSR_IA32_MTRR_DEF_TYPE_REGISTER *)&Mtrrs->MtrrDefType)->Bits.Type;

  MTRR_MEMORY_RANGE RawVariableRanges[ARRAY_SIZE(Mtrrs->Variables.Mtrr)];

  UINT32 UsedMtrr = GetRawVariableRanges(&Mtrrs->Variables, VCNT, MtrrValidBitsMask, MtrrValidAddressMask, RawVariableRanges);
  Print(L"UsedMtrr %d\n", UsedMtrr);

  ApplyVariableMtrrs(RawVariableRanges, VCNT, ARRAY_SIZE(Ranges), Ranges, &RangeCount);

  ApplyFixedMtrrs(&Mtrrs->Fixed, Ranges, ARRAY_SIZE(Ranges), &RangeCount);

  for (UINTN Index = 0; Index < RangeCount; Index++)
  {
    Print(L"[%02d]%a:%016lx-%016lx\n", Index, mMtrrMemoryCacheTypeShortName[Ranges[Index].Type],
          Ranges[Index].BaseAddress, Ranges[Index].BaseAddress + Ranges[Index].Length - 1);
  }
}

VOID PrintAllMtrrsWorker()
{
  MTRR_SETTINGS Mtrrs;
  UINT32 VCNT, FIX, SMRR;
  if (!IsMtrrSupported())
  {
    return;
  }
  // 11.11.1
  // Software must read IA32_MTRRCAP VCNT field to determine the number of variable MTRRs and query other
  // feature bits in IA32_MTRRCAP to determine additional capabilities that are supported in a processor
  GetMttrCap(&VCNT, &FIX, &SMRR);

  if (!FIX)
  {
    Print(L"IA32_MTRRCAP not support FIX\n");
    return;
  }
  else
  {
    Print(L"IA32_MTRRCAP support FIX\n");
    // 11.11.2
    // The memory ranges and the types of memory specified in each range are set by three groups of registers: the
    // IA32_MTRR_DEF_TYPE MSR, the fixed-range MTRRs, and the variable range MTRRs.

    // Get fixed MTRRs
    LoadFixedRangeMtrrs(&Mtrrs.Fixed);
    // Get variable MTRRs
    // 11.11.2.3 The number m (VNCT) of ranges supported is given in bits 7:0 of the IA32_MTRRCAP MSR
    // 11.11.2.4 Before attempting to access these SMRR registers, software must test bit 11 in the IA32_MTRRCAP register. If
    // (SMRR) is not supported, reads from or writes to registers cause general-protection exceptions.
    if (!SMRR)
    {
      Print(L"IA32_MTRRCAP not support SMRR\n");
      return;
    }
    Print(L"IA32_MTRRCAP support SMRR\n");
    LoadVariableRangeMtrrs(VCNT, &Mtrrs.Variables);

    // Get MTRR_DEF_TYPE value
    // 11.11.2.1 IA32_MTRR_DEF_TYPE
    Mtrrs.MtrrDefType = AsmReadMsr64(MSR_IA32_MTRR_DEF_TYPE);

    DumpInitMTRRContent(&Mtrrs, VCNT);
    // Dump MTRR setting in ranges
    DumpMTRRSetting(&Mtrrs, VCNT);
  }
}