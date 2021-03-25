/*
 *  AhciReadWrite.c
 *
 *  Created on: 2021年03月23日
 *      Author: Anthony Teng
 */

#include "AhciReadWrite.h"

UINT32
EFIAPI
AhciReadReg(
    IN EFI_PCI_IO_PROTOCOL *PciIo,
    IN UINT32 Offset)
{
  UINT32 Data;
  EFI_STATUS Status;

  ASSERT(PciIo != NULL);

  Data = 0;

  Status = PciIo->Mem.Read(
      PciIo,
      EfiPciIoWidthUint32,
      EFI_AHCI_BAR_INDEX,
      (UINT64)Offset,
      1,
      &Data);
  if (EFI_ERROR(Status))
  {
    Print(L"Mem.Read Error Status -> [%x]\n", Status);
  }
  Print(L"Data %x\n", Data);
  return Data;
}

EFI_STATUS
EFIAPI
AhciCreateTransferDescriptor(
    IN EFI_PCI_IO_PROTOCOL *PciIo,
    IN OUT EFI_AHCI_REGISTERS *AhciRegisters)
{
  EFI_STATUS Status;
  // UINTN Bytes;
  VOID *Buffer;

  UINT32 Capability;
  UINT32 PortImplementBitMap;
  UINT8 MaxPortNumber;
  UINT8 MaxCommandSlotNumber;
  BOOLEAN Support64Bit;
  UINT64 MaxReceiveFisSize;
  UINT64 MaxCommandListSize;
  UINT64 MaxCommandTableSize;
  EFI_PHYSICAL_ADDRESS AhciRFisPciAddr;
  EFI_PHYSICAL_ADDRESS AhciCmdListPciAddr;
  EFI_PHYSICAL_ADDRESS AhciCommandTablePciAddr;

  Buffer = NULL;
  //
  // Collect AHCI controller information
  //
  Capability = AhciReadReg(PciIo, EFI_AHCI_CAPABILITY_OFFSET);
  //
  // Get the number of command slots per port supported by this HBA.
  //
  MaxCommandSlotNumber = (UINT8)(((Capability & 0x1F00) >> 8) + 1);
  Support64Bit = (BOOLEAN)(((Capability & BIT31) != 0) ? TRUE : FALSE);

  PortImplementBitMap = AhciReadReg(PciIo, EFI_AHCI_PI_OFFSET);
  //
  // Get the highest bit of implemented ports which decides how many bytes are allocated for recived FIS.
  //
  MaxPortNumber = (UINT8)(UINTN)(HighBitSet32(PortImplementBitMap) + 1);
  if (MaxPortNumber == 0)
  {
    return EFI_DEVICE_ERROR;
  }

  MaxReceiveFisSize = MaxPortNumber * sizeof(EFI_AHCI_RECEIVED_FIS);
  Status = PciIo->AllocateBuffer(
      PciIo,
      AllocateAnyPages,
      EfiBootServicesData,
      EFI_SIZE_TO_PAGES((UINTN)MaxReceiveFisSize),
      &Buffer,
      0);

  if (EFI_ERROR(Status))
  {
    return EFI_OUT_OF_RESOURCES;
  }

  ZeroMem(Buffer, (UINTN)MaxReceiveFisSize);

  AhciRegisters->AhciRFis = Buffer;
  AhciRegisters->MaxReceiveFisSize = MaxReceiveFisSize;

  AhciRFisPciAddr = AhciReadReg(PciIo, EFI_AHCI_PORT_START + EFI_AHCI_PORT_FB);
  AhciRegisters->AhciRFisPciAddr = (EFI_AHCI_RECEIVED_FIS *)(UINTN)AhciRFisPciAddr;

  //
  // Allocate memory for command list
  // Note that the implemenation is a single task model which only use a command list for all ports.
  //
  Buffer = NULL;
  MaxCommandListSize = MaxCommandSlotNumber * sizeof(EFI_AHCI_COMMAND_LIST);
  Status = PciIo->AllocateBuffer(
      PciIo,
      AllocateAnyPages,
      EfiBootServicesData,
      EFI_SIZE_TO_PAGES((UINTN)MaxCommandListSize),
      &Buffer,
      0);

  if (EFI_ERROR(Status))
  {
    //
    // Free mapped resource.
    //
    Status = EFI_OUT_OF_RESOURCES;
    goto Error5;
  }

  ZeroMem(Buffer, (UINTN)MaxCommandListSize);

  AhciRegisters->AhciCmdList = Buffer;
  AhciRegisters->MaxCommandListSize = MaxCommandListSize;

  AhciCmdListPciAddr = AhciReadReg(PciIo, EFI_AHCI_PORT_START + EFI_AHCI_PORT_CLB);
  AhciRegisters->AhciCmdListPciAddr = (EFI_AHCI_COMMAND_LIST *)(UINTN)AhciCmdListPciAddr;

  //
  // Allocate memory for command table
  // According to AHCI 1.3 spec, a PRD table can contain maximum 65535 entries.
  //

  Buffer = NULL;
  MaxCommandTableSize = sizeof(EFI_AHCI_COMMAND_TABLE);

  Status = PciIo->AllocateBuffer(
      PciIo,
      AllocateAnyPages,
      EfiBootServicesData,
      EFI_SIZE_TO_PAGES((UINTN)MaxCommandTableSize),
      &Buffer,
      0);
  if (EFI_ERROR(Status))
  {
    //
    // Free mapped resource.
    //
    Status = EFI_OUT_OF_RESOURCES;
    goto Error3;
  }
  ZeroMem(Buffer, (UINTN)MaxCommandTableSize);

  AhciRegisters->AhciCommandTable = Buffer;
  AhciRegisters->MaxCommandTableSize = MaxCommandTableSize;
  AhciCommandTablePciAddr = AhciCmdListPciAddr + (UINT64)0x80;
  AhciRegisters->AhciCommandTablePciAddr = (EFI_AHCI_COMMAND_TABLE *)(UINTN)AhciCommandTablePciAddr;
  return EFI_SUCCESS;
  //
  // Map error or unable to map the whole CmdList buffer into a contiguous region.
  //
// Error1:
//   PciIo->Unmap(
//       PciIo,
//       AhciRegisters->MapCommandTable);
// Error2:
//   PciIo->FreeBuffer(
//       PciIo,
//       EFI_SIZE_TO_PAGES((UINTN)MaxCommandTableSize),
//       AhciRegisters->AhciCommandTable);
Error3:
  PciIo->Unmap(
      PciIo,
      AhciRegisters->MapCmdList);
// Error4:
//   PciIo->FreeBuffer(
//       PciIo,
//       EFI_SIZE_TO_PAGES((UINTN)MaxCommandListSize),
//       AhciRegisters->AhciCmdList);
Error5:
  PciIo->Unmap(
      PciIo,
      AhciRegisters->MapRFis);
  // Error6:
  //   PciIo->FreeBuffer(
  //       PciIo,
  //       EFI_SIZE_TO_PAGES((UINTN)MaxReceiveFisSize),
  //       AhciRegisters->AhciRFis);

  return Status;
}

VOID
EFIAPI
AhciBuildCommand (
  IN     EFI_PCI_IO_PROTOCOL        *PciIo,
  IN     EFI_AHCI_REGISTERS         *AhciRegisters,
  IN     EFI_AHCI_COMMAND_FIS       *CommandFis,
  IN     EFI_AHCI_COMMAND_LIST      *CommandList,
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

VOID
EFIAPI
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

EFI_STATUS
EFIAPI
AhciPioTransfer (
  IN     EFI_PCI_IO_PROTOCOL        *PciIo,
  IN     EFI_AHCI_REGISTERS         *AhciRegisters,
  IN     BOOLEAN                    Read,
  IN     EFI_ATA_COMMAND_BLOCK      *AtaCommandBlock,
  IN OUT EFI_ATA_STATUS_BLOCK       *AtaStatusBlock,
  IN OUT VOID                       *MemoryAddr,
  IN     UINT32                     DataCount,
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
  BOOLEAN                       PioFisReceived;
  BOOLEAN                       D2hFisReceived;

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
    PciIo,
    AhciRegisters,
    &CFis,
    &CmdList,
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

  if (Read && (AtapiCommand == 0)) {
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
EFIAPI
ReadLba5FirstByte(
    IN EFI_PCI_IO_PROTOCOL *PciIo,
    IN EFI_AHCI_REGISTERS *AhciRegisters,
    IN OUT UINT8 *Buffer)
{
  EFI_STATUS Status;
  EFI_ATA_COMMAND_BLOCK AtaCommandBlock;
  EFI_ATA_STATUS_BLOCK AtaStatusBlock;

  ZeroMem(&AtaCommandBlock, sizeof(EFI_ATA_COMMAND_BLOCK));
  ZeroMem(&AtaStatusBlock, sizeof(EFI_ATA_STATUS_BLOCK));

  AtaCommandBlock.AtaCommand = ATA_CMD_READ_DMA_EXT;
  AtaCommandBlock.AtaSectorCount = 1;

  Status = AhciPioTransfer(
      PciIo,
      AhciRegisters,
      TRUE,
      &AtaCommandBlock,
      &AtaStatusBlock,
      Buffer,
      sizeof(UINT8)
      );
  return Status;
}

EFI_STATUS UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;
  UINTN TotalDevice;
  EFI_HANDLE *PciIoHandleBuffer;

  EFI_PCI_IO_PROTOCOL *PciIo;
  EFI_AHCI_REGISTERS *AhciRegisters;

  // UINTN SegmentNumber;
  // UINTN BusNumber;
  // UINTN DeviceNumber;
  // UINTN FunctionNumber;

  PCI_TYPE00 PciData;
  UINT8 ClassCode;

  UINT8 Buffer;
  Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiPciIoProtocolGuid, NULL, &TotalDevice, &PciIoHandleBuffer);
  // Print(L"Number of PCI : %d\n", TotalDevice);

  for (UINTN index = 0; index < TotalDevice; index++)
  {
    PciIo = NULL;
    Status = gBS->HandleProtocol(PciIoHandleBuffer[index], &gEfiPciIoProtocolGuid, &PciIo);
    if (EFI_ERROR(Status))
    {
      Print(L"gBS->HandleProtocol Error Status -> [%x]\n", Status);
    }
    // Status = PciIo->GetLocation(PciIo, &SegmentNumber, &BusNumber, &DeviceNumber, &FunctionNumber);
    // if (EFI_ERROR(Status))
    // {
    //   Print(L"PciIo->GetLocation Error Status -> [%x]\n", Status);
    // }
    // Print(L"SegmentNumber %x, BusNumber %x, DeviceNumber %x, FunctionNumber %x\n",
    //       SegmentNumber, BusNumber, DeviceNumber, FunctionNumber);

    Status = PciIo->Pci.Read(PciIo,
                             EfiPciIoWidthUint8,
                             PCI_CLASSCODE_OFFSET,
                             sizeof(PciData.Hdr.ClassCode),
                             PciData.Hdr.ClassCode);

    ClassCode = PciData.Hdr.ClassCode[1];
    if (ClassCode == PCI_CLASS_MASS_STORAGE_SATADPA)
    {
      break;
    }
  }

  //
  // Initialize FIS Base Address Register and Command List Base Address Register for use.
  //
  Status = AhciCreateTransferDescriptor(PciIo, AhciRegisters);
  if (EFI_ERROR(Status))
  {
    Print(L"AhciCreateTransferDescriptor Error Status -> [%x]\n", Status);
  }

  Print(L"AhciCmdListPciAd,dr: %x, AhciRFisPciAddr: %x, AhciCommandTablePciAddr: %x\n",
        AhciRegisters->AhciCmdListPciAddr,
        AhciRegisters->AhciRFisPciAddr,
        AhciRegisters->AhciCommandTablePciAddr);

  Status = ReadLba5FirstByte(PciIo, AhciRegisters, &Buffer);

  if (PciIoHandleBuffer != NULL)
  {
    Status = gBS->FreePool(PciIoHandleBuffer);
  }
  return 0;
}