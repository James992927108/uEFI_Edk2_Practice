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
    Print(L"Error Mem.Read Status -> [%x]\n", Status);
  }
  Print(L"Data %x\n", Data);
  return Data;
}

EFI_STATUS UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  EFI_STATUS Status;
  UINTN TotalDevice;
  EFI_HANDLE *PciIoHandleBuffer;
  EFI_PCI_IO_PROTOCOL *PciIo;

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
      Print(L"Error gBS->HandleProtocol Status -> [%x]\n", Status);
    }
    // Status = PciIo->GetLocation(PciIo, &SegmentNumber, &BusNumber, &DeviceNumber, &FunctionNumber);
    // if (EFI_ERROR(Status))
    // {
    //   Print(L"Error PciIo->GetLocation Status -> [%x]\n", Status);
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

  if (PciIoHandleBuffer != NULL)
  {
    Status = gBS->FreePool(PciIoHandleBuffer);
  }

  AhciBar = AhciReadReg(PciIo, 0);
  Print(L"AhciBar %x\n", AhciBar);

  return 0;
}