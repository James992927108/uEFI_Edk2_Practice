/*
 *  AhciReadWrite.c
 *
 *  Created on: 2021年03月23日
 *      Author: Anthony Teng
 */

#include "AhciReadWrite.h"
///
/// The default PCH PCI bus number
///
#define DEFAULT_PCI_SEGMENT_NUMBER_PCH 0
#define DEFAULT_PCI_BUS_NUMBER_PCH 0
//
//  SATA Controller Registers (D23:F0)
//
#define PCI_DEVICE_NUMBER_PCH_SATA 23
#define PCI_FUNCTION_NUMBER_PCH_SATA 0
//
//  SATA Controller common Registers
//
#define R_SATA_CFG_AHCI_BAR 0x24

//
// AHCI BAR Area related Registers
//
#define R_SATA_MEM_AHCI_P0CLB 0x100
#define R_SATA_MEM_AHCI_P0FB 0x108

VOID
AhciBuildCommandFis (
  IN OUT EFI_AHCI_COMMAND_FIS    *CmdFis,
  IN     EFI_ATA_COMMAND_BLOCK   *AtaCommandBlock
  )
{
  ZeroMem (CmdFis, sizeof (EFI_AHCI_COMMAND_FIS));

  CmdFis->AhciCFisType = EFI_AHCI_FIS_REGISTER_H2D;
  //
  // Indicator it's a command
  //
  CmdFis->AhciCFisCmdInd      = 0x1;
  CmdFis->AhciCFisCmd         = AtaCommandBlock->AtaCommand;

  CmdFis->AhciCFisFeature     = AtaCommandBlock->AtaFeatures;
  CmdFis->AhciCFisFeatureExp  = AtaCommandBlock->AtaFeaturesExp;

  CmdFis->AhciCFisSecNum      = AtaCommandBlock->AtaSectorNumber;
  CmdFis->AhciCFisSecNumExp   = AtaCommandBlock->AtaSectorNumberExp;

  CmdFis->AhciCFisClyLow      = AtaCommandBlock->AtaCylinderLow;
  CmdFis->AhciCFisClyLowExp   = AtaCommandBlock->AtaCylinderLowExp;

  CmdFis->AhciCFisClyHigh     = AtaCommandBlock->AtaCylinderHigh;
  CmdFis->AhciCFisClyHighExp  = AtaCommandBlock->AtaCylinderHighExp;

  CmdFis->AhciCFisSecCount    = AtaCommandBlock->AtaSectorCount;
  CmdFis->AhciCFisSecCountExp = AtaCommandBlock->AtaSectorCountExp;

  CmdFis->AhciCFisDevHead     = (UINT8) (AtaCommandBlock->AtaDeviceHead | 0xE0);
}

VOID
AhciBuildCommand (
  // IN     EFI_PCI_IO_PROTOCOL        *PciIo,
  // IN     EFI_AHCI_REGISTERS         *AhciRegisters,
  IN     UINT8                      Port,
  // IN     UINT8                      PortMultiplier,
  IN     EFI_AHCI_COMMAND_FIS       *CommandFis,
  IN     EFI_AHCI_COMMAND_LIST      *CommandList,
  // IN     EFI_AHCI_ATAPI_COMMAND     *AtapiCommand OPTIONAL,
  IN     UINT8                      AtapiCommandLength,
  IN     UINT8                      CommandSlotNumber,
  IN OUT VOID                       *DataPhysicalAddr,
  IN     UINT32                     DataLength
  )
{
  UINT64     BaseAddr;
  UINT32     PrdtNumber;
  UINT32     PrdtIndex;
  UINTN      RemainedData;
  UINTN      MemAddr;
  DATA_64    Data64;
  UINT32     Offset;

  //
  // Filling the PRDT
  //
  PrdtNumber = (UINT32)DivU64x32 (((UINT64)DataLength + EFI_AHCI_MAX_DATA_PER_PRDT - 1), EFI_AHCI_MAX_DATA_PER_PRDT);

  //
  // According to AHCI 1.3 spec, a PRDT entry can point to a maximum 4MB data block.
  // It also limits that the maximum amount of the PRDT entry in the command table
  // is 65535.
  //
  ASSERT (PrdtNumber <= 65535);

  Data64.Uint64 = (UINTN) (AhciRegisters->AhciRFis) + sizeof (EFI_AHCI_RECEIVED_FIS) * Port;

  BaseAddr = Data64.Uint64;

  ZeroMem ((VOID *)((UINTN) BaseAddr), sizeof (EFI_AHCI_RECEIVED_FIS));

  ZeroMem (AhciRegisters->AhciCommandTable, sizeof (EFI_AHCI_COMMAND_TABLE));

  CommandFis->AhciCFisPmNum = PortMultiplier;

  CopyMem (&AhciRegisters->AhciCommandTable->CommandFis, CommandFis, sizeof (EFI_AHCI_COMMAND_FIS));

  Offset = EFI_AHCI_PORT_START + Port * EFI_AHCI_PORT_REG_WIDTH + EFI_AHCI_PORT_CMD;
  if (AtapiCommand != NULL) {
    CopyMem (
      &AhciRegisters->AhciCommandTable->AtapiCmd,
      AtapiCommand,
      AtapiCommandLength
      );

    CommandList->AhciCmdA = 1;
    CommandList->AhciCmdP = 1;

    AhciOrReg (PciIo, Offset, (EFI_AHCI_PORT_CMD_DLAE | EFI_AHCI_PORT_CMD_ATAPI));
  } else {
    AhciAndReg (PciIo, Offset, (UINT32)~(EFI_AHCI_PORT_CMD_DLAE | EFI_AHCI_PORT_CMD_ATAPI));
  }

  RemainedData = (UINTN) DataLength;
  MemAddr      = (UINTN) DataPhysicalAddr;
  CommandList->AhciCmdPrdtl = PrdtNumber;

  for (PrdtIndex = 0; PrdtIndex < PrdtNumber; PrdtIndex++) {
    if (RemainedData < EFI_AHCI_MAX_DATA_PER_PRDT) {
      AhciRegisters->AhciCommandTable->PrdtTable[PrdtIndex].AhciPrdtDbc = (UINT32)RemainedData - 1;
    } else {
      AhciRegisters->AhciCommandTable->PrdtTable[PrdtIndex].AhciPrdtDbc = EFI_AHCI_MAX_DATA_PER_PRDT - 1;
    }

    Data64.Uint64 = (UINT64)MemAddr;
    AhciRegisters->AhciCommandTable->PrdtTable[PrdtIndex].AhciPrdtDba  = Data64.Uint32.Lower32;
    AhciRegisters->AhciCommandTable->PrdtTable[PrdtIndex].AhciPrdtDbau = Data64.Uint32.Upper32;
    RemainedData -= EFI_AHCI_MAX_DATA_PER_PRDT;
    MemAddr      += EFI_AHCI_MAX_DATA_PER_PRDT;
  }

  //
  // Set the last PRDT to Interrupt On Complete
  //
  if (PrdtNumber > 0) {
    AhciRegisters->AhciCommandTable->PrdtTable[PrdtNumber - 1].AhciPrdtIoc = 1;
  }

  CopyMem (
    (VOID *) ((UINTN) AhciRegisters->AhciCmdList + (UINTN) CommandSlotNumber * sizeof (EFI_AHCI_COMMAND_LIST)),
    CommandList,
    sizeof (EFI_AHCI_COMMAND_LIST)
    );

  Data64.Uint64 = (UINT64)(UINTN) AhciRegisters->AhciCommandTablePciAddr;
  AhciRegisters->AhciCmdList[CommandSlotNumber].AhciCmdCtba  = Data64.Uint32.Lower32;
  AhciRegisters->AhciCmdList[CommandSlotNumber].AhciCmdCtbau = Data64.Uint32.Upper32;
  AhciRegisters->AhciCmdList[CommandSlotNumber].AhciCmdPmp   = PortMultiplier;

}

EFI_STATUS
AhciPioTransfer (
  // IN     EFI_PCI_IO_PROTOCOL        *PciIo,
  // IN     EFI_AHCI_REGISTERS         *AhciRegisters,
  IN     UINT8                      Port,
  // IN     UINT8                      PortMultiplier,
  // IN     EFI_AHCI_ATAPI_COMMAND     *AtapiCommand OPTIONAL,
  IN     UINT8                      AtapiCommandLength,
  IN     BOOLEAN                    Read,
  IN     EFI_ATA_COMMAND_BLOCK      *AtaCommandBlock,
  // IN OUT EFI_ATA_STATUS_BLOCK       *AtaStatusBlock,
  IN OUT VOID                       *MemoryAddr,
  IN     UINT32                     DataCount,
  // IN     UINT64                     Timeout,
  // IN     ATA_NONBLOCK_TASK          *Task
  )
{
  EFI_STATUS                    Status;
  UINTN                         FisBaseAddr;
  UINTN                         Offset;
  EFI_PHYSICAL_ADDRESS          PhyAddr;
  VOID                          *Map;
  UINTN                         MapLength;
  EFI_PCI_IO_PROTOCOL_OPERATION Flag;
  UINT64                        Delay;
  EFI_AHCI_COMMAND_FIS          CFis;
  EFI_AHCI_COMMAND_LIST         CmdList;
  UINT32                        PortTfd;
  UINT32                        PrdCount;
  // BOOLEAN                       InfiniteWait;
  BOOLEAN                       PioFisReceived;
  BOOLEAN                       D2hFisReceived;

  // if (Timeout == 0) {
  //   InfiniteWait = TRUE;
  // } else {
  //   InfiniteWait = FALSE;
  // }

  if (Read) {
    Flag = EfiPciIoOperationBusMasterWrite;
  } else {
    Flag = EfiPciIoOperationBusMasterRead;
  }

  //
  // construct command list and command table with pci bus address
  //
  MapLength = DataCount;
  Status = PciIo->Map (
                    PciIo,
                    Flag,
                    MemoryAddr,
                    &MapLength,
                    &PhyAddr,
                    &Map
                    );

  if (EFI_ERROR (Status) || (DataCount != MapLength)) {
    return EFI_BAD_BUFFER_SIZE;
  }

  //
  // Package read needed
  //
  AhciBuildCommandFis (&CFis, AtaCommandBlock);

  ZeroMem (&CmdList, sizeof (EFI_AHCI_COMMAND_LIST));

  CmdList.AhciCmdCfl = EFI_AHCI_FIS_REGISTER_H2D_LENGTH / 4;
  CmdList.AhciCmdW   = Read ? 0 : 1;

  AhciBuildCommand (
    // PciIo,
    // AhciRegisters,
    Port,
    // PortMultiplier,
    &CFis,
    &CmdList,
    // AtapiCommand,
    AtapiCommandLength,
    0,
    (VOID *)(UINTN)PhyAddr,
    DataCount
    );

  Status = AhciStartCommand (
             PciIo,
             Port,
             0,
             Timeout
             );
  if (EFI_ERROR (Status)) {
    goto Exit;
  }

  //
  // Check the status and wait the driver sending data
  //
  FisBaseAddr = (UINTN)AhciRegisters->AhciRFis + Port * sizeof (EFI_AHCI_RECEIVED_FIS);

  // if (Read && (AtapiCommand == 0)) {
  if (Read) {
    //
    // Wait device sends the PIO setup fis before data transfer
    //
    Status = EFI_TIMEOUT;
    Delay  = DivU64x32 (Timeout, 1000) + 1;
    do {
      PioFisReceived = FALSE;
      D2hFisReceived = FALSE;
      Offset = FisBaseAddr + EFI_AHCI_PIO_FIS_OFFSET;
      Status = AhciCheckMemSet (Offset, EFI_AHCI_FIS_TYPE_MASK, EFI_AHCI_FIS_PIO_SETUP, NULL);
      if (!EFI_ERROR (Status)) {
        PioFisReceived = TRUE;
      }
      //
      // According to SATA 2.6 spec section 11.7, D2h FIS means an error encountered.
      // But Qemu and Marvel 9230 sata controller may just receive a D2h FIS from device
      // after the transaction is finished successfully.
      // To get better device compatibilities, we further check if the PxTFD's ERR bit is set.
      // By this way, we can know if there is a real error happened.
      //
      Offset = FisBaseAddr + EFI_AHCI_D2H_FIS_OFFSET;
      Status = AhciCheckMemSet (Offset, EFI_AHCI_FIS_TYPE_MASK, EFI_AHCI_FIS_REGISTER_D2H, NULL);
      if (!EFI_ERROR (Status)) {
        D2hFisReceived = TRUE;
      }

      if (PioFisReceived || D2hFisReceived) {
        Offset = EFI_AHCI_PORT_START + Port * EFI_AHCI_PORT_REG_WIDTH + EFI_AHCI_PORT_TFD;
        PortTfd = AhciReadReg (PciIo, (UINT32) Offset);
        //
        // PxTFD will be updated if there is a D2H or SetupFIS received. 
        //
        if ((PortTfd & EFI_AHCI_PORT_TFD_ERR) != 0) {
          Status = EFI_DEVICE_ERROR;
          break;
        }

        PrdCount = *(volatile UINT32 *) (&(AhciRegisters->AhciCmdList[0].AhciCmdPrdbc));
        if (PrdCount == DataCount) {
          Status = EFI_SUCCESS;
          break;
        }
      }

      //
      // Stall for 100 microseconds.
      //
      MicroSecondDelay(100);

      Delay--;
      if (Delay == 0) {
        Status = EFI_TIMEOUT;
      }
    // } while (InfiniteWait || (Delay > 0));
    } while (Delay > 0);
  } else {
    //
    // Wait for D2H Fis is received
    //
    Offset = FisBaseAddr + EFI_AHCI_D2H_FIS_OFFSET;
    Status = AhciWaitMemSet (
               Offset,
               EFI_AHCI_FIS_TYPE_MASK,
               EFI_AHCI_FIS_REGISTER_D2H,
               Timeout
               );

    if (EFI_ERROR (Status)) {
      goto Exit;
    }

    Offset = EFI_AHCI_PORT_START + Port * EFI_AHCI_PORT_REG_WIDTH + EFI_AHCI_PORT_TFD;
    PortTfd = AhciReadReg (PciIo, (UINT32) Offset);
    if ((PortTfd & EFI_AHCI_PORT_TFD_ERR) != 0) {
      Status = EFI_DEVICE_ERROR;
    }
  }

Exit:
  AhciStopCommand (
    PciIo,
    Port,
    Timeout
    );

  AhciDisableFisReceive (
    PciIo,
    Port,
    Timeout
    );

  PciIo->Unmap (
    PciIo,
    Map
    );

  AhciDumpPortStatus (PciIo, AhciRegisters, Port, AtaStatusBlock);

  return Status;
}

EFI_STATUS
AhciIdentify (
  // IN EFI_PCI_IO_PROTOCOL      *PciIo,
  // IN EFI_AHCI_REGISTERS       *AhciRegisters,
  // IN UINT8                    Port,
  // IN UINT8                    PortMultiplier,
  // IN OUT EFI_IDENTIFY_DATA    *Buffer
  IN OUT UINT32    *Buffer
  )
{
  EFI_STATUS                   Status;
  EFI_ATA_COMMAND_BLOCK        AtaCommandBlock;
  // EFI_ATA_STATUS_BLOCK         AtaStatusBlock;

  // if (PciIo == NULL || AhciRegisters == NULL || Buffer == NULL) {
  //   return EFI_INVALID_PARAMETER;
  // }

  ZeroMem (&AtaCommandBlock, sizeof (EFI_ATA_COMMAND_BLOCK));
  // ZeroMem (&AtaStatusBlock, sizeof (EFI_ATA_STATUS_BLOCK));

  AtaCommandBlock.AtaCommand     = ATA_CMD_IDENTIFY_DRIVE; // 0cEC
  AtaCommandBlock.AtaSectorCount = 1;

  Status = AhciPioTransfer (
            //  PciIo,
            //  AhciRegisters,
             Port,
            //  PortMultiplier,
            //  NULL,
             0,
             TRUE,
             &AtaCommandBlock,
            //  &AtaStatusBlock,
             Buffer,
             sizeof (UINT32), //  sizeof (EFI_IDENTIFY_DATA),
            //  ATA_ATAPI_TIMEOUT,
            //  NULL
             );
  return Status;
}


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