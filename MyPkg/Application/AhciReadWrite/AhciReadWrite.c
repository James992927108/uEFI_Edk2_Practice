/*
 *  AhciReadWrite.c
 *
 *  Created on: 2021年03月23日
 *      Author: Anthony Teng
 */

#include "AhciReadWrite.h"

EFI_STATUS
EFIAPI
AhciCheckDeviceStatus(
    IN UINT32 PortAddr)
{
  UINT32 Data;
  UINT32 Address;

  Address = PortAddr + EFI_AHCI_PORT_SSTS;
  Data = MmioRead32(Address) & EFI_AHCI_PORT_SSTS_DET_MASK;

  // Print(L"Address: 0x%08x, Data: 0x%08x\n", Address, Data);
  if (Data == EFI_AHCI_PORT_SSTS_DET_PCE)
  {
    return EFI_SUCCESS;
  }
  return EFI_NOT_READY;
}

EFI_STATUS
EFIAPI
AhciCheckWhichPortUsed(
    IN HBA_MEM *ABar,
    OUT UINT8 *PortList,
    OUT UINT32 *length)
{
  EFI_STATUS Status;
  UINT32 Pi;
  UINT8 Port;
  UINT32 PortAddr;
  UINT32 Index = 0;

  for (Pi = ABar->pi, Port = 0; Pi > 0; Pi >>= 1, Port++)
  {
    PortList[Port] = 0;
  }
  for (Pi = ABar->pi, Port = 0; Pi > 0; Pi >>= 1, Port++)
  {
    if (Pi & 1)
    {
      PortAddr = (UINT32)ABar + EFI_AHCI_PORT_START + EFI_AHCI_PORT_REG_WIDTH * Port;
      // Print(L"\nport: %d, address: %x\n", Port, PortAddr);
      Status = AhciCheckDeviceStatus(PortAddr);
      if (Status == EFI_SUCCESS)
      {
        Print(L" Port %x Device Detection (DET) = 0x03 \n", Port);
        PortList[Index++] = Port;
      }
    }
  }
  if (Index > 0)
  {
    *length = Index;
    return EFI_SUCCESS;
  }
  return EFI_NOT_READY;
}

VOID BuildH2DFis(
    FIS_REG_H2D *FisH2D,
    UINT8 cmd)
{
  //  initial H2D FIS (Serial ATA Revision 2.6 specification)
  //  +-+-------------+--------------+----------------+---------------+
  //  |0| Features    |   Command    |0|C|R|R|R|PmPort| FIS Type (27h)|
  //  +-+-------------+--------------+----------------+---------------+
  //  |1|   Device    |   LBA High   |    LBA Mid     |  LBA Low      |
  //  +-+-------------+--------------+----------------+---------------+
  //  |2|Features(exp)|LBA High (exp)| LBA Mid (exp)  | LBA Low (exp) |
  //  +-+-------------+--------------+----------------+---------------+
  //  |3|   Control   | Reserved (0) |SectorCount(exp)|  Sector Count |
  //  +-+-------------+--------------+----------------+---------------+
  //  |4| Reserved (0)| Reserved (0) |  Reserved (0)  |  Reserved (0) |
  //  +-+-------------+--------------+----------------+---------------+
  FisH2D->fis_type = FIS_TYPE_REG_H2D; // 0x27
  FisH2D->pmport = 0;
  FisH2D->c = 1;
  FisH2D->command = cmd;
  FisH2D->featurel = 0;

  FisH2D->device = 0x40;
  // Read lba5
  FisH2D->lba_low = 0x04;
  FisH2D->sector_count = 1;
}

EFI_STATUS
StartCmd(
    HBA_PORT *port)
{
  UINTN i;
  EFI_STATUS Status = EFI_SUCCESS;
  // Print(L"[DEBUG] TempPortAddr 0x%x\n", port);
  // clear 0x18 cmd bit 0 start
  if (port->cmd & BIT0)
  {
    port->cmd &= ~BIT0;
  }
  //make sure 0x18 cmd, FIS Receive Enable (FRE)
  port->cmd |= BIT4;

  //clear status
  port->serr = port->serr;
  port->is = port->is;

  //if err status or busy, reset PxCMD.
  if ((port->is & (BIT30 | BIT29 | BIT28 | BIT27 | BIT26 | BIT24 | BIT23)) ||
      (port->tfd & (BIT0 | BIT7)))
  {
    port->cmd &= ~BIT0;
    //Start|Command List Override|FIS Receive Enable
    port->cmd |= BIT0 | BIT3 | BIT4;
    port->is = port->is;
  }
  Print(L"[DEBUG] Before ci 0x%08x\n", MmioRead8((UINT32)(UINTN)port + EFI_AHCI_PORT_CI));
  MmioWrite8((UINT32)(UINTN)port + EFI_AHCI_PORT_CI, 1);
  // port->ci = 1;
  Print(L"[DEBUG] after ci 0x%08x\n", MmioRead8((UINT32)(UINTN)port + EFI_AHCI_PORT_CI));
  // start
  Print(L"[DEBUG] Befor cmd 0x%08x\n", port->cmd);
  port->cmd |= BIT0;
  Print(L"[DEBUG] after cmd 0x%08x\n", port->cmd);
  //wait cmd
  for (i = 0; i < 100; i++)
  {
    gBS->Stall(500);
    if (!(port->tfd & BIT7))
      break;
  }

  if (port->tfd & BIT0)
    Status = EFI_DEVICE_ERROR;

  return Status;
}

EFI_STATUS UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;
  HBA_MEM *ABar;
  UINTN AhciPciAddr;

  UINT8 *TmpBuffer;
  UINT8 Result;

  HBA_CMD_HEADER *CmdList;
  HBA_CMD_TBL *CmdTbl;
  FIS_REG_H2D *FisH2D;

  UINT8 *PortList;
  UINT32 Length;

  HBA_PORT *TempPortAddr = NULL;
  DATA_64 Data64;

  AhciPciAddr = PCI_SEGMENT_LIB_ADDRESS(
      DEFAULT_PCI_SEGMENT_NUMBER_PCH,
      DEFAULT_PCI_BUS_NUMBER_PCH,
      PCI_DEVICE_NUMBER_PCH_SATA,
      PCI_FUNCTION_NUMBER_PCH_SATA,
      0);
  Print(L"[INFO] AHCI controller address: %x\n", AhciPciAddr);

  ABar = (HBA_MEM *)(PciSegmentRead32(AhciPciAddr + R_SATA_CFG_AHCI_BAR) & 0xFFFFF800);
  Print(L"[INFO] AHCI bar: %x\n", ABar);

  //allocate buffer for read return data
  TmpBuffer = AllocateZeroPool(sizeof(UINT8) * 512);

  //allocate align memory address for command list, PxCLB bit9:0 is reserved
  CmdList = AllocatePages(sizeof(HBA_CMD_HEADER));
  ZeroMem(CmdList, sizeof(HBA_CMD_HEADER));

  //allocate alignment memory for command table
  CmdTbl = AllocatePages(sizeof(HBA_CMD_TBL));
  ZeroMem(CmdTbl, sizeof(HBA_CMD_TBL));

  //allocate h2d FIS
  FisH2D = AllocateZeroPool(sizeof(FIS_REG_H2D));

  Status = AhciCheckWhichPortUsed(ABar, PortList, &Length);
  if (EFI_ERROR(Status))
  {
    Print(L"No activity port used\n");
  }
  else
  {
    for (UINT32 Index = 0; Index < Length; Index++)
    {
      Print(L"[INFO] Port %x in use\n", PortList[Index]);
      // 0x00258027
      BuildH2DFis(FisH2D, ATA_CMD_READ_DMA_EXT);
      CopyMem(CmdTbl->cfis, FisH2D, sizeof(FIS_REG_H2D));
      // DW0 Data Base Address
      CmdTbl->prdt_entry[0].dba = (UINT32)(UINTN)TmpBuffer;
      Print(L"[INFO] Data Base Addr: 0x%08x \n", CmdTbl->prdt_entry[0].dba);

      // DW3 0x80000001
      CmdTbl->prdt_entry[0].dbc = 0x01; // Read 1 Byte， Max 512 (0x1ff)bytes
      CmdTbl->prdt_entry[0].i = 1;

      // DW0 0x00010005
      CmdList->cfl = 5;
      CmdList->prdtl = 1;
      // DW1 0x200
      CmdList->prdbc = 0x200;
      // DW2 Command Table Base Address
      CmdList->ctba = (UINT32)(UINTN)CmdTbl;
      Print(L"[INFO] Cmd Tbl Base Addr: 0x%08x \n", CmdList->ctba);

      // pointer to port address
      TempPortAddr = (HBA_PORT *)((UINT32)ABar + EFI_AHCI_PORT_START + EFI_AHCI_PORT_REG_WIDTH * PortList[Index]);
      // Print(L"[DEBUG] TempPortAddr 0x%x\n", TempPortAddr);
      Data64.Uint64 = (UINTN)CmdList;
      TempPortAddr->clb = Data64.Uint32.Lower32;
      TempPortAddr->clbu = Data64.Uint32.Upper32;
      Print(L"[INFO] Cmd List Base Add: 0x%08x \n", TempPortAddr->clb);

      Status = StartCmd(TempPortAddr);
      if (EFI_ERROR(Status))
      {
        Print(L"Issue cmd fail!\n");
        continue;
      }

      Print(L"[Read] -> data is 0x%08x \n", MmioRead32((UINTN)TmpBuffer));

      //write value to date base address
      Result = MmioRead8((UINTN)TmpBuffer) + 1;
      Print(L"[DEBUG] Result value = 0x%08x \n", Result);
      MmioWrite8((UINTN)CmdTbl->prdt_entry[0].dba, (UINT8)Result);
      Print(L"[DEBUG] dba value = 0x%08x \n", MmioRead32((UINTN)CmdTbl->prdt_entry[0].dba));

      BuildH2DFis(FisH2D, ATA_CMD_WRITE_DMA_EXT);
      CopyMem(CmdTbl->cfis, FisH2D, sizeof(FIS_REG_H2D));
      Print(L"[DEBUG] cfis value = 0x%08x \n", MmioRead32((UINTN)CmdList->ctba));

       // DW3 0x80000001
      CmdTbl->prdt_entry[0].dbc = 0x01; // Read 1 Byte， Max 512 (0x1ff)bytes
      CmdTbl->prdt_entry[0].i = 1;

      // DW0 0x00010005
      CmdList->cfl = 5;
      CmdList->prdtl = 1;
      CmdList->w = 1;
      // DW1 0x200
      CmdList->prdbc = 0x200;
      // DW2 Command Table Base Address
      CmdList->ctba = (UINT32)(UINTN)CmdTbl;
      Print(L"[DEBUG] cmd list header value = 0x%08x \n", MmioRead32((UINTN)TempPortAddr->clb));

      // Status = StartCmd(TempPortAddr);
      MmioWrite8((UINTN)TempPortAddr + EFI_AHCI_PORT_CMD, 0x16);
      MmioWrite8((UINTN)TempPortAddr + EFI_AHCI_PORT_CI, 0x01);
      MmioWrite8((UINTN)TempPortAddr + EFI_AHCI_PORT_CMD, 0x17);

      // if (EFI_ERROR(Status))
      // {
      //   Print(L"Issue cmd fail!\n");
      //   continue;
      // }
    }
  }

  return 0;
}