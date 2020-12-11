#include <Uefi.h>

#include "CpuRead.h"
#include "Mtrr.h"
EFI_STATUS EFIAPI UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  PrintCpuFeatureInfo();
  PrintCpuBrandString();
  PrintMicroCodeVersion();

  // PrintAllMtrrsWorker();
  
  return EFI_SUCCESS;
}