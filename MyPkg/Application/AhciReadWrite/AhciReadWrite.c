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
    IN UINT8 cmd,
    IN UINT8 lba)
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
  Cfis->lba_low = lba;
  Cfis->sector_count = 1;
}

VOID BuildPrdt(
    IN OUT PRDT_REG *Prdt,
    IN UINT32 Address,
    IN BOOLEAN Read)
{
  // 31: Interrupt on Completion
  // 30:22 Reserved
  // 21:00 Data Byte Count
  // Command Table 0x80 -> PRDT -> DW3, DBC: Byte Count
  if (Read)
  {
    // set DBA
    Prdt->dba = Address;
    // 0x80000001, Read 1 byte (Homework ask)
    Prdt->dbc = 1;
    Prdt->i = 1;
  }
  else
  {
    // 0x000001FF
    Prdt->dbc = 0x1FF;
    Prdt->i = 0;
  }
}

VOID BuildCmdList(
    IN OUT SM_CMD_LIST *CmdList,
    IN UINT32 Address,
    IN BOOLEAN Read)
{
  // Command List Structure
  // DW0 PRDTL, W, CFL 0x10045
  CmdList->cfl = 5;
  CmdList->prdtl = 1;
  // DW1 PRDBC: PRD Byre Count (512 byte)
  CmdList->prdbc = 0x200;
  // DW2 Command Table Base Address
  CmdList->ctba = (UINT32)Address;
  if (Read)
  {
    CmdList->w = 0;
  }
  else
  {
    CmdList->w = 1;
  }
}
EFI_STATUS
StartCmd(
    IN SM_PORT *port,
    IN UINT32 Slot)
{
  UINTN i;
  EFI_STATUS Status = EFI_SUCCESS;
  // Print(L"[DEBUG] PortAddr 0x%x\n", port);
  // clear 0x18 cmd bit 0 start
  if (port->cmd & BIT0)
  {
    port->cmd &= ~BIT0;
  }
  //make sure 0x18 cmd, FIS Receive Enable (FRE)
  port->cmd |= BIT4;

  //clear status
  port->serr = 0;
  port->is = 0;

  //if err status or busy, reset PxCMD.
  //Port x Interrupt Status(0x10) or Port x Task File Data(0x20)
  if ((port->is & (BIT30 | BIT29 | BIT28 | BIT27 | BIT26 | BIT24 | BIT23)) ||
      (port->tfd & (BIT0 | BIT7)))
  {
    port->cmd &= ~BIT0;
    //Start|Command List Override|FIS Receive Enable
    port->cmd |= BIT0 | BIT3 | BIT4;
    port->is = 0;
  }
  // Print(L"[DEBUG] Before ci 0x%08x\n", MmioRead8((UINT32)(UINTN)port + EFI_AHCI_PORT_CI));
  MmioWrite8((UINT32)(UINTN)port + EFI_AHCI_PORT_CI, 1 << Slot);
  // Print(L"[DEBUG] after ci 0x%08x\n", MmioRead8((UINT32)(UINTN)port + EFI_AHCI_PORT_CI));
  // Print(L"[DEBUG] Befor cmd 0x%08x\n", port->cmd);
  port->cmd |= BIT0;
  // Print(L"[DEBUG] after cmd 0x%08x\n", port->cmd);
  // Waiting cmd
  for (i = 0; i < 100; i++)
  {
    gBS->Stall(500);
    //  PxTFD.BSY -> BIT7
    if (!(port->tfd & BIT7))
      break;
  }
  //  PxTFD.ERR -> BIT0
  if (port->tfd & BIT0)
    Status = EFI_DEVICE_ERROR;

  return Status;
}

EFI_STATUS UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;

  SM_MEM *ABar;
  SM_PORT *PortAddr = NULL;
  PortAddr = AllocatePool(sizeof(SM_PORT));

  DATA_64 Data64;
  UINTN AhciPciAddr;

  SM_CMD_LIST *CmdList;

  SM_CMD_TBL *CmdTbl;
  CFIS_REG *Cfis;
  PRDT_REG *Prdt;
  UINT8 *Dba;

  UINT8 *PortList;
  UINT32 Length;

  UINT8 Result;

  AhciPciAddr = PCI_SEGMENT_LIB_ADDRESS(
      DEFAULT_PCI_SEGMENT_NUMBER_PCH,
      DEFAULT_PCI_BUS_NUMBER_PCH,
      PCI_DEVICE_NUMBER_PCH_SATA,
      PCI_FUNCTION_NUMBER_PCH_SATA,
      0);
  Print(L"[INFO] AHCI controller address: %x\n", AhciPciAddr);

  ABar = (SM_MEM *)(PciSegmentRead32(AhciPciAddr + R_SATA_CFG_AHCI_BAR));
  Print(L"[INFO] AHCI bar: %x\n", ABar);

  //allocate align memory address for command list, PxCLB bit9:0 is reserved, Max support 32 command slot
  CmdList = AllocatePages(sizeof(SM_CMD_LIST) * 32);
  ZeroMem(CmdList, sizeof(SM_CMD_LIST) * 32);
  Print(L"[DEBUG] CmdList -> %x \n", CmdList);

  //select slot
  UINT32 Slot = 7;
  CmdList = (SM_CMD_LIST *)((UINT32)CmdList + 0x20 * Slot);
  Print(L"[DEBUG] CmdList += 0x20 * 0x%x -> %x \n", Slot, CmdList);
  //allocate alignment memory for command table
  CmdTbl = AllocatePages(sizeof(SM_CMD_TBL));
  ZeroMem(CmdTbl, sizeof(SM_CMD_TBL));
  //allocate Command FIS
  Cfis = AllocateZeroPool(sizeof(CFIS_REG));
  //allocate buffer for read return data, max 512 bytes
  Dba = AllocateZeroPool(sizeof(UINT8) * 0x200);

  PortList = AllocatePool(sizeof(UINT8) * 0x200);
  Status = AhciCheckWhichPortUsed(ABar, PortList, &Length);
  UINT8 Lba = 0x04;
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
      PortAddr = (SM_PORT *)((UINT32)ABar + EFI_AHCI_PORT_START + EFI_AHCI_PORT_REG_WIDTH * PortNum);
      Print(L"[INFO] PortAddr %x\n", (UINT32)PortAddr);

      // Read
      // --------------------------------

      // Command Tables
      // Set Cfis, Command Table 0x00 -> CFIS, Read -> 0x00258027
      BuildCfis(Cfis, ATA_CMD_READ_DMA_EXT, Lba);
      CopyMem(CmdTbl->cfis, Cfis, sizeof(CFIS_REG));

      // Physical region descriptor table
      Prdt = CmdTbl->prdt;
      // Set DBA
      // Command Table 0x80 -> PRDT (Physical Region Descriptor Table) -> DW0, Data Base Address
      BuildPrdt(Prdt, (UINT32)Dba, TRUE);
      CopyMem(CmdTbl->prdt, Prdt, sizeof(PRDT_REG));
      Print(L"[DEBUG] Read -> Data Base Addr: 0x%08x, value = 0x%08x\n", CmdTbl->prdt[0].dba, MmioRead32((UINTN)(CmdTbl->prdt[0].dba)));

      // Command List Structure，point to Command Table
      BuildCmdList(CmdList, (UINT32)CmdTbl, TRUE);
      Print(L"[INFO] Command Table Base Addr: 0x%08x, 0x%08x\n", CmdList->ctba, MmioRead32((UINTN)(CmdList->ctba)));
      Print(L"[INFO] Command List Base Addr + slot: 0x%08x, 0x%08x\n", (UINT32)CmdList, MmioRead32((UINTN)CmdList));

      // pointer to port address，can put in Abar.ports_ctl_reg
      Data64.Uint64 = (UINTN)CmdList;
      // Print(L"0x%08x, 0x%08x\n", Data64.Uint32.Lower32, Data64.Uint32.Upper32);
      PortAddr->clb = Data64.Uint32.Lower32;
      PortAddr->clbu = Data64.Uint32.Upper32;
      // Print(L"0x%08x, 0x%08x\n", PortAddr->clb, PortAddr->clbu);
      // Print(L"[INFO] Command List Base Addr: 0x%08x, 0x%08x\n", PortAddr->clb, MmioRead32((UINTN)(PortAddr->clb)));

      Status = StartCmd(PortAddr, Slot);
      if (EFI_ERROR(Status))
      {
        Print(L"Issue Command Fail!\n");
        continue;
      }
      Result = MmioRead8((UINTN)Dba);
      Print(L"[Read] -> data is 0x%08x \n", Result);
      // Command Table 0x80 -> PRDT (Physical Region Descriptor Table)
      // -> DW0, Data Base Address, read data in DBA and write back value with plus 1.
      Result += 1;
      Print(L"[DEBUG] Result + 1 = 0x%08x \n", Result);
      MmioWrite8((UINTN)Prdt->dba, (UINT8)Result);
      // write
      // --------------------------------
      // Command Table 0x00 -> CFIS, Write -> 0x00358027
      BuildCfis(Cfis, ATA_CMD_WRITE_DMA_EXT, Lba);
      CopyMem(CmdTbl->cfis, Cfis, sizeof(CFIS_REG));

      BuildPrdt(Prdt, (UINT32)Dba, FALSE);
      CopyMem(CmdTbl->prdt, Prdt, sizeof(PRDT_REG));
      Print(L"[INFO] Write -> Data Base Addr: 0x%08x, Value = 0x%08x\n", CmdTbl->prdt[0].dba, MmioRead32((UINTN)(CmdTbl->prdt[0].dba)));

      BuildCmdList(CmdList, (UINT32)CmdTbl, FALSE);
      Print(L"[INFO] Command Table Base Addr: 0x%08x, 0x%08x\n", CmdList->ctba, MmioRead32((UINTN)(CmdList->ctba)));
      Print(L"[INFO] Command List Base Addr + slot: 0x%08x, 0x%08x\n", (UINT32)CmdList, MmioRead32((UINTN)CmdList));

      Data64.Uint64 = (UINTN)CmdList;
      // Print(L"0x%08x, 0x%08x\n", Data64.Uint32.Lower32, Data64.Uint32.Upper32);
      PortAddr->clb = Data64.Uint32.Lower32;
      PortAddr->clbu = Data64.Uint32.Upper32;
      // Print(L"0x%08x, 0x%08x\n", PortAddr->clb, PortAddr->clbu);
      // Print(L"[INFO] Command List Base Addr: 0x%08x, 0x%08x\n", PortAddr->clb, MmioRead32((UINTN)(PortAddr->clb)));

      Status = StartCmd(PortAddr, Slot);
      if (EFI_ERROR(Status))
      {
        Print(L"Issue Command Fail!\n");
        continue;
      }
    }
  }

  return 0;
}