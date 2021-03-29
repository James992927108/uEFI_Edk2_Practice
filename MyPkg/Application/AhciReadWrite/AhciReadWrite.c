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
    IN SM_MEM *ABar,
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

VOID BuildCfis(
    IN OUT CFIS_REG *Cfis,
    IN UINT8 cmd)
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
  Cfis->fis_type = FIS_TYPE_REG_H2D; // 0x27
  Cfis->pmport = 0;
  Cfis->c = 1;
  Cfis->command = cmd;
  Cfis->featurel = 0;

  Cfis->device = 0x40;
  // Read or Write on lba5 (Homework ask)
  Cfis->lba_low = 0x04;
  Cfis->sector_count = 1;
}

VOID BuildPrdt(
    IN OUT PRDT_REG *Prdt,
    IN UINT32 Address,
    IN BOOLEAN Read)
{
  UINT8 Result;

  // 31: Interrupt on Completion
  // 30:22 Reserved
  // 21:00 Data Byte Count
  // Command Table 0x80 -> PRDT -> DW3, DBC: Byte Count
  Prdt->dbc = 0x1FF;
  if (Read)
  {
    // set DBA
    Prdt->dba = Address;
    // 0x800001FF,
    Prdt->i = 1;
  }
  else
  {
    // Command Table 0x80 -> PRDT (Physical Region Descriptor Table)
    // -> DW0, Data Base Address, read data in DBA and write back value with plus 1.
    Result = MmioRead8((UINTN)Address) + 1;
    Print(L"[DEBUG] Result value = 0x%08x \n", Result);
    MmioWrite8((UINTN)Prdt->dba, (UINT8)Result);
  }
}

EFI_STATUS
StartCmd(
    SM_PORT *port)
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
  SM_MEM *ABar;
  UINTN AhciPciAddr;

  UINT8 *TmpBuffer;
  UINT8 Result;

  SM_CMD_LIST *CmdList;

  SM_CMD_TBL *CmdTbl;
  CFIS_REG *Cfis;
  PRDT_REG *Prdt;

  UINT8 *PortList;
  UINT32 Length;

  SM_PORT *TempPortAddr = NULL;
  DATA_64 Data64;

  AhciPciAddr = PCI_SEGMENT_LIB_ADDRESS(
      DEFAULT_PCI_SEGMENT_NUMBER_PCH,
      DEFAULT_PCI_BUS_NUMBER_PCH,
      PCI_DEVICE_NUMBER_PCH_SATA,
      PCI_FUNCTION_NUMBER_PCH_SATA,
      0);
  Print(L"[INFO] AHCI controller address: %x\n", AhciPciAddr);

  ABar = (SM_MEM *)(PciSegmentRead32(AhciPciAddr + R_SATA_CFG_AHCI_BAR) & 0xFFFFF800);
  Print(L"[INFO] AHCI bar: %x\n", ABar);

  //allocate align memory address for command list, PxCLB bit9:0 is reserved
  CmdList = AllocatePages(sizeof(SM_CMD_LIST));
  ZeroMem(CmdList, sizeof(SM_CMD_LIST));

  //allocate alignment memory for command table
  CmdTbl = AllocatePages(sizeof(SM_CMD_TBL));
  ZeroMem(CmdTbl, sizeof(SM_CMD_TBL));

  //allocate Command FIS & Physical region descriptor table
  Cfis = AllocateZeroPool(sizeof(CFIS_REG));
  Prdt = CmdTbl->prdt;

  PortList = AllocatePool(sizeof(UINT8) * 0x200);
  Status = AhciCheckWhichPortUsed(ABar, PortList, &Length);
  if (EFI_ERROR(Status))
  {
    Print(L"No activity port used\n");
  }
  else
  {
    for (UINT32 Index = 0; Index < Length; Index++)
    {
      UINT32 PortNum = PortList[Index];
      Print(L"[INFO] Port %x in use\n", PortNum);
      // Read
      // --------------------------------
      // Command Tables
      // 0x00258027
      // Command Table 0x00 -> CFIS
      BuildCfis(Cfis, ATA_CMD_READ_DMA_EXT);
      CopyMem(CmdTbl->cfis, Cfis, sizeof(CFIS_REG));

      // Command Table 0x80 -> PRDT (Physical Region Descriptor Table)
      // -> DW0, Data Base Address, allocate buffer for read return data
      // max 512 bytes
      TmpBuffer = AllocateZeroPool(sizeof(UINT8) * 0x200);

      // set DBA
      BuildPrdt(Prdt, (UINT32)TmpBuffer, TRUE);
      CopyMem(CmdTbl->prdt, Prdt, sizeof(PRDT_REG));

      Print(L"[DEBUG] Read -> Data Base Addr: 0x%08x, value = 0x%08x\n", CmdTbl->prdt[0].dba, MmioRead32((UINTN)(CmdTbl->prdt[0].dba)));
      // Command List Structure
      // DW0 PRDTL, W, CFL 0x10045
      CmdList->cfl = 5;
      CmdList->w = 0;
      CmdList->prdtl = 1;
      // DW1 PRDBC: PRD Byre Count
      CmdList->prdbc = 0x200;
      // DW2 Command Table Base Address
      CmdList->ctba = (UINT32)(UINTN)CmdTbl;
      Print(L"[INFO] Command Table Base Addr: 0x%08x \n", CmdList->ctba);
      Print(L"[DEBUG] Command List Value = 0x%08x \n", MmioRead32((UINTN)CmdList));

      // pointer to port address
      Print(L"[DEBUG]----> ABar 0x%x, Port %x \n", (UINT32)ABar, PortNum);
      TempPortAddr = (SM_PORT *)((UINT32)ABar + EFI_AHCI_PORT_START + EFI_AHCI_PORT_REG_WIDTH * PortNum);
      Print(L"[DEBUG] TempPortAddr 0x%x\n", TempPortAddr);
      Data64.Uint64 = (UINTN)CmdList;
      TempPortAddr->clb = Data64.Uint32.Lower32;
      TempPortAddr->clbu = Data64.Uint32.Upper32;
      Print(L"[INFO] Command List Base Addr: 0x%08x \n", TempPortAddr->clb);

      Status = StartCmd(TempPortAddr);
      if (EFI_ERROR(Status))
      {
        Print(L"Issue Command Fail!\n");
        continue;
      }
      Print(L"[Read] -> data is 0x%08x \n", MmioRead32((UINTN)TmpBuffer));

      // write
      // --------------------------------
      // 0x00358027
      // Command Table 0x00 -> CFIS
      BuildCfis(Cfis, ATA_CMD_WRITE_DMA_EXT);
      CopyMem(CmdTbl->cfis, Cfis, sizeof(CFIS_REG));

      // Command Table 0x80 -> PRDT (Physical Region Descriptor Table)
      // -> DW0, Data Base Address, read data in DBA and write back value with plus 1.
      Result = MmioRead8((UINTN)TmpBuffer) + 1;
      Print(L"[DEBUG] Result value = 0x%08x \n", Result);
      MmioWrite8((UINTN)CmdTbl->prdt[0].dba, (UINT8)Result);

      BuildPrdt(Prdt, (UINT32)TmpBuffer, FALSE);
      CopyMem(CmdTbl->prdt, Prdt, sizeof(PRDT_REG));

      // Command Table 0x80 -> PRDT -> DW3, DBC: Byte Count
      // 0x000001FF,
      CmdTbl->prdt[0].dbc = 0x1FF; // Read 1 Byte， Max 512 (0x1ff)bytes

      Print(L"[DEBUG] Write -> Data Base Addr: 0x%08x, Value = 0x%08x\n", CmdTbl->prdt[0].dba, MmioRead32((UINTN)(CmdTbl->prdt[0].dba)));

      // Command List Structure
      // DW0 PRDTL, W, CFL 0x10045
      CmdList->cfl = 5;
      CmdList->w = 1;
      CmdList->prdtl = 1;
      // DW1 PRDBC: PRD Byre Count
      CmdList->prdbc = 0x200;
      // DW2 Command Table Base Address
      CmdList->ctba = (UINT32)(UINTN)CmdTbl;
      Print(L"[INFO] Command Table Base Addr: 0x%08x \n", CmdList->ctba);
      Print(L"[DEBUG] Command List Value = 0x%08x \n", MmioRead32((UINTN)CmdList));

      // pointer to port address
      TempPortAddr = (SM_PORT *)((UINT32)ABar + EFI_AHCI_PORT_START + EFI_AHCI_PORT_REG_WIDTH * PortList[Index]);
      // Print(L"[DEBUG] TempPortAddr 0x%x\n", TempPortAddr);
      Data64.Uint64 = (UINTN)CmdList;
      TempPortAddr->clb = Data64.Uint32.Lower32;
      TempPortAddr->clbu = Data64.Uint32.Upper32;
      Print(L"[INFO] Command List Base Add: 0x%08x \n", TempPortAddr->clb);

      Status = StartCmd(TempPortAddr);
      if (EFI_ERROR(Status))
      {
        Print(L"Issue Command Fail!\n");
        continue;
      }
    }
  }

  return 0;
}