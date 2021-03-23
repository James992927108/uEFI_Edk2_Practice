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
#define R_SATA_CFG_AHCI_BAR                 0x24
EFI_STATUS UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
  UINT32 AhciBar;
  UINTN PciSataRegBase;

  PciSataRegBase = PCI_SEGMENT_LIB_ADDRESS(
      DEFAULT_PCI_SEGMENT_NUMBER_PCH,
      DEFAULT_PCI_BUS_NUMBER_PCH,
      PCI_DEVICE_NUMBER_PCH_SATA,
      PCI_FUNCTION_NUMBER_PCH_SATA,
      0
      );
  Print(L"PciSataRegBase %x\n", PciSataRegBase);

  AhciBar = PciSegmentRead32(PciSataRegBase + R_SATA_CFG_AHCI_BAR) & 0xFFFFF800;

  Print(L"AhciBar %x\n", AhciBar);

  return 0;
}