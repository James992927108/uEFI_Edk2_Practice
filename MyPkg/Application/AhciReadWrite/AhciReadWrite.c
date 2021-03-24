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

  if (PciIoHandleBuffer != NULL)
  {
    Status = gBS->FreePool(PciIoHandleBuffer);
  }
  return 0;
}