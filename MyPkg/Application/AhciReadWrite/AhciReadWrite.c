/*
 *  AhciReadWrite.c
 *
 *  Created on: 2021年03月23日
 *      Author: Anthony Teng
 */

#include "AhciReadWrite.h"

EFI_STATUS UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  HBA_MEM *ABar;
  UINTN AhciPciAddr;

  UINT8 *TmpBuffer;

  HBA_CMD_HEADER *CmdList;
  HBA_CMD_TBL *CmdTbl;
  FIS_REG_H2D *Fis;

  AhciPciAddr = PCI_SEGMENT_LIB_ADDRESS(
      DEFAULT_PCI_SEGMENT_NUMBER_PCH,
      DEFAULT_PCI_BUS_NUMBER_PCH,
      PCI_DEVICE_NUMBER_PCH_SATA,
      PCI_FUNCTION_NUMBER_PCH_SATA,
      0);
  Print(L"AHCI controller address: %x\n", AhciPciAddr);

  ABar = (HBA_MEM *)(PciSegmentRead32(AhciPciAddr + R_SATA_CFG_AHCI_BAR) & 0xFFFFF800);
  Print(L"AHCI bar: %x\n", ABar);

  //allocate buffer for read return data
  TmpBuffer = AllocateZeroPool(sizeof(UINT8) * 512);

  //allocate align memory address for command list, PxCLB bit9:0 is reserved
  CmdList = AllocatePages(sizeof(HBA_CMD_HEADER));
  ZeroMem(CmdList, sizeof(HBA_CMD_HEADER));

  //allocate alignment memory for command table
  CmdTbl = AllocatePages(sizeof(HBA_CMD_TBL));
  ZeroMem(CmdTbl, sizeof(HBA_CMD_TBL));

  //allocate h2d FIS
  Fis = AllocateZeroPool(sizeof(FIS_REG_H2D));

  Fis->fis_type = FIS_TYPE_REG_H2D;
  Fis->command = ATA_CMD_IDENTIFY; // 0xEC
  Fis->device = 0;                 // Master device
  Fis->c = 1;                      // Write command register

  UINT32 Pi;
  UINTN Index, PortAddr;
  for (Pi = ABar->pi, Index = 0; Pi > 0; Pi >>= 1, Index++)
  {
    if (Pi & 1)
    {
      PortAddr = (UINTN)ABar + 0x100 + 0x80 * Index;
      Print(L"\nport: %d, address: %x\n", Index, PortAddr);
      if(ABar->ports_ctl_reg[0].ssts.det == 0x03)
      {
        Print(L" Device Detection (DET) = 0x03 n");
      }
    }
  }
  return 0;
}