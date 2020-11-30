#ifndef __MTRR_H__
#define __MTRR_H__
// MdePkg/Include
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
// UefiCpuPkg/Include
#include <Register/ArchitecturalMsr.h>

#include <Library/MtrrLib.h> //for use MTRR_MEMORY_CACHE_TYPE

CONST FIXED_MTRR  mMtrrLibFixedMtrrTable[] = {
  {
    MSR_IA32_MTRR_FIX64K_00000,
    0,
    SIZE_64KB
  },
  {
    MSR_IA32_MTRR_FIX16K_80000,
    0x80000,
    SIZE_16KB
  },
  {
    MSR_IA32_MTRR_FIX16K_A0000,
    0xA0000,
    SIZE_16KB
  },
  {
    MSR_IA32_MTRR_FIX4K_C0000,
    0xC0000,
    SIZE_4KB
  },
  {
    MSR_IA32_MTRR_FIX4K_C8000,
    0xC8000,
    SIZE_4KB
  },
  {
    MSR_IA32_MTRR_FIX4K_D0000,
    0xD0000,
    SIZE_4KB
  },
  {
    MSR_IA32_MTRR_FIX4K_D8000,
    0xD8000,
    SIZE_4KB
  },
  {
    MSR_IA32_MTRR_FIX4K_E0000,
    0xE0000,
    SIZE_4KB
  },
  {
    MSR_IA32_MTRR_FIX4K_E8000,
    0xE8000,
    SIZE_4KB
  },
  {
    MSR_IA32_MTRR_FIX4K_F0000,
    0xF0000,
    SIZE_4KB
  },
  {
    MSR_IA32_MTRR_FIX4K_F8000,
    0xF8000,
    SIZE_4KB
  }
};

GLOBAL_REMOVE_IF_UNREFERENCED CONST CHAR8 *mMtrrMemoryCacheTypeShortName[] = {
  "UC",  // CacheUncacheable
  "WC",  // CacheWriteCombining
  "R*",  // Invalid
  "R*",  // Invalid
  "WT",  // CacheWriteThrough
  "WP",  // CacheWriteProtected
  "WB",  // CacheWriteBack
  "R*"   // Invalid
};

VOID GetMtrrMemCacheType();
VOID PrintAllMtrrsWorker();

#endif