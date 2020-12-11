#ifndef __MTRR_H__
#define __MTRR_H__

#include <Uefi.h>
// MdePkg/Include
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h> // for use ASSERT
#include <Library/BaseMemoryLib.h> //for use MemCopy, MemZero
#include <Library/SynchronizationLib.h> //for use SPIN_LOCK
#include <Library/PcdLib.h>
#include <Library/MemoryAllocationLib.h> // AllocatePages
#include <Pi/PiMultiPhase.h> // EFI_AP_PROCEDURE

// UefiCpuPkg/Include
#include <Register/Cpuid.h> // for use CPUID_VERSION_INFO
#include <Register/ArchitecturalMsr.h>

#include <Library/LocalApicLib.h> // GetInitialApicId() & GetApicId()
#include <Library/MtrrLib.h> //for use MTRR_MEMORY_CACHE_TYPE

//
// AP loop state when APs are in idle state
// It's value is the same with PcdCpuApLoopMode
//
typedef enum {
  ApInHltLoop   = 1,
  ApInMwaitLoop = 2,
  ApInRunLoop   = 3
} AP_LOOP_MODE;

//
// AP initialization state during APs wakeup
//
typedef enum {
  ApInitConfig   = 1,
  ApInitReconfig = 2,
  ApInitDone     = 3
} AP_INIT_STATE;

//
// AP state
//
typedef enum {
  CpuStateIdle,
  CpuStateReady,
  CpuStateBusy,
  CpuStateFinished,
  CpuStateDisabled
} CPU_STATE;

//
// CPU exchange information for switch BSP
//
typedef struct {
  UINT8             State;        // offset 0
  UINTN             StackPointer; // offset 4 / 8
  IA32_DESCRIPTOR   Gdtr;         // offset 8 / 16
  IA32_DESCRIPTOR   Idtr;         // offset 14 / 26
} CPU_EXCHANGE_ROLE_INFO;

//
// CPU volatile registers around INIT-SIPI-SIPI
//
typedef struct {
  UINTN                          Cr0;
  UINTN                          Cr3;
  UINTN                          Cr4;
  UINTN                          Dr0;
  UINTN                          Dr1;
  UINTN                          Dr2;
  UINTN                          Dr3;
  UINTN                          Dr6;
  UINTN                          Dr7;
  IA32_DESCRIPTOR                Gdtr;
  IA32_DESCRIPTOR                Idtr;
  UINT16                         Tr;
} CPU_VOLATILE_REGISTERS;

//
// AP related data
//
typedef struct {
  SPIN_LOCK                      ApLock;
  volatile UINT32                *StartupApSignal;
  volatile UINTN                 ApFunction;
  volatile UINTN                 ApFunctionArgument;
  BOOLEAN                        CpuHealthy;
  volatile CPU_STATE             State;
  CPU_VOLATILE_REGISTERS         VolatileRegisters;
  BOOLEAN                        Waiting;
  BOOLEAN                        *Finished;
  UINT64                         ExpectedTime;
  UINT64                         CurrentTime;
  UINT64                         TotalTime;
  EFI_EVENT                      WaitEvent;
} CPU_AP_DATA;

//
// Basic CPU information saved in Guided HOB.
// Because the contents will be shard between PEI and DXE,
// we need to make sure the each fields offset same in different
// architecture.
//
#pragma pack (1)
typedef struct {
  UINT32                         InitialApicId;
  UINT32                         ApicId;
  UINT32                         Health;
  UINT64                         ApTopOfStack;
} CPU_INFO_IN_HOB;
#pragma pack ()


//
// AP reset code information including code address and size,
// this structure will be shared be C code and assembly code.
// It is natural aligned by design.
//
typedef struct {
  UINT8             *RendezvousFunnelAddress;
  UINTN             ModeEntryOffset;
  UINTN             RendezvousFunnelSize;
  UINT8             *RelocateApLoopFuncAddress;
  UINTN             RelocateApLoopFuncSize;
  UINTN             ModeTransitionOffset;
} MP_ASSEMBLY_ADDRESS_MAP;


typedef struct _CPU_MP_DATA  CPU_MP_DATA;

#pragma pack(1)

//
// MP CPU exchange information for AP reset code
// This structure is required to be packed because fixed field offsets
// into this structure are used in assembly code in this module
//
typedef struct {
  UINTN                 Lock;
  UINTN                 StackStart;
  UINTN                 StackSize;
  UINTN                 CFunction;
  IA32_DESCRIPTOR       GdtrProfile;
  IA32_DESCRIPTOR       IdtrProfile;
  UINTN                 BufferStart;
  UINTN                 ModeOffset;
  UINTN                 ApIndex;
  UINTN                 CodeSegment;
  UINTN                 DataSegment;
  UINTN                 EnableExecuteDisable;
  UINTN                 Cr3;
  UINTN                 InitFlag;
  CPU_INFO_IN_HOB       *CpuInfo;
  UINTN                 NumApsExecuting;
  CPU_MP_DATA           *CpuMpData;
  UINTN                 InitializeFloatingPointUnitsAddress;
  UINT32                ModeTransitionMemory;
  UINT16                ModeTransitionSegment;
  UINT32                ModeHighMemory;
  UINT16                ModeHighSegment;
} MP_CPU_EXCHANGE_INFO;

#pragma pack()

//
// CPU MP Data save in memory
//
struct _CPU_MP_DATA {
  UINT64                         CpuInfoInHob;
  UINT32                         CpuCount;
  UINT32                         BspNumber;
  //
  // The above fields data will be passed from PEI to DXE
  // Please make sure the fields offset same in the different
  // architecture.
  //
  SPIN_LOCK                      MpLock;
  UINTN                          Buffer;
  UINTN                          CpuApStackSize;
  MP_ASSEMBLY_ADDRESS_MAP        AddressMap;
  UINTN                          WakeupBuffer;
  UINTN                          WakeupBufferHigh;
  UINTN                          BackupBuffer;
  UINTN                          BackupBufferSize;

  volatile UINT32                StartCount;
  volatile UINT32                FinishedCount;
  volatile UINT32                RunningCount;
  BOOLEAN                        SingleThread;
  EFI_AP_PROCEDURE               Procedure;
  VOID                           *ProcArguments;
  BOOLEAN                        *Finished;
  UINT64                         ExpectedTime;
  UINT64                         CurrentTime;
  UINT64                         TotalTime;
  EFI_EVENT                      WaitEvent;
  UINTN                          **FailedCpuList;

  AP_INIT_STATE                  InitFlag;
  BOOLEAN                        X2ApicEnable;
  BOOLEAN                        SwitchBspFlag;
  UINTN                          NewBspNumber;
  CPU_EXCHANGE_ROLE_INFO         BSPInfo;
  CPU_EXCHANGE_ROLE_INFO         APInfo;
  MTRR_SETTINGS                  MtrrTable;
  UINT8                          ApLoopMode;
  UINT8                          ApTargetCState;
  UINT16                         PmCodeSegment;
  CPU_AP_DATA                    *CpuData;
  volatile MP_CPU_EXCHANGE_INFO  *MpCpuExchangeInfo;

  UINT32                         CurrentTimerCount;
  UINTN                          DivideValue;
  UINT8                          Vector;
  BOOLEAN                        PeriodicMode;
  BOOLEAN                        TimerInterruptState;
  UINT64                         MicrocodePatchAddress;
  UINT64                         MicrocodePatchRegionSize;
};

VOID PrintAllMtrrsWorker();

#endif