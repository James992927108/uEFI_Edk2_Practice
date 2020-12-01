#ifndef __MTRR_H__
#define __MTRR_H__

#include <Uefi.h>
// MdePkg/Include
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Library/DebugLib.h> // for use ASSERT
#include <Library/BaseMemoryLib.h> //for use MemCopy, MemZero

// UefiCpuPkg/Include
#include <Register/Cpuid.h> // for use CPUID_VERSION_INFO
#include <Register/ArchitecturalMsr.h>

#include <Library/MtrrLib.h> //for use MTRR_MEMORY_CACHE_TYPE 

VOID PrintAllMtrrsWorker();

#endif