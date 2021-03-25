/*
 *  AhciReadWrite.c
 *
 *  Created on: 2021年03月23日
 *      Author: Anthony Teng
 */

#include "AhciReadWrite.h"

EFI_STATUS UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  UINT32 AhciBar;
  UINTN PciSataRegBase;
  UINT32 PxCLB;
  UINT32 PxFB;

  EFI_AHCI_COMMAND_FIS          CFis;
  EFI_AHCI_COMMAND_LIST         CmdList;

  PciSataRegBase = PCI_SEGMENT_LIB_ADDRESS(
      DEFAULT_PCI_SEGMENT_NUMBER_PCH,
      DEFAULT_PCI_BUS_NUMBER_PCH,
      PCI_DEVICE_NUMBER_PCH_SATA,
      PCI_FUNCTION_NUMBER_PCH_SATA,
      0);
  Print(L"PciSataRegBase %x\n", PciSataRegBase);
  
  AhciBar = PciSegmentRead32(PciSataRegBase + R_SATA_CFG_AHCI_BAR) & 0xFFFFF800;
  Print(L"AhciBar %x\n", AhciBar);

  PxCLB = MmioRead32(AhciBar + R_SATA_MEM_AHCI_P0CLB);
  Print(L"PxCLB %x\n", PxCLB);

  PxFB = MmioRead32(AhciBar + R_SATA_MEM_AHCI_P0FB);
  Print(L"PxFB %x\n", PxFB);


  // Read First
  ZeroMem (&CmdList, sizeof (EFI_AHCI_COMMAND_LIST));

  CmdList.AhciCmdCfl = 0x5;
  CmdList.AhciCmdW   = 0x0;
  CmdList.AhciCmdPrdtl = 0x1;

  CmdList.AhciCmdPrdbc = 0x200;

  DATA_64    Data64;

  Data64.Uint64 = (UINTN) (AhciRegisters->AhciRFis) + sizeof (EFI_AHCI_RECEIVED_FIS) * Port;


  

  return 0;
}