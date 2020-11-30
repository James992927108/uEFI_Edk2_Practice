#include <Uefi.h>

#include "CpuRead.h"
#include "Mtrr.h"
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
  PrintAllMtrrsWorker();

  return EFI_SUCCESS;
}