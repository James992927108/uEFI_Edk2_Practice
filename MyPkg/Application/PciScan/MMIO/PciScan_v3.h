/*
 *  PciScan_v1.h
 *
 *  Created on: 2020年11月17日
 *      Author: Anthony Teng
 */

//MdePkg
#include <Include/Uefi.h>
#include <Include/Library/UefiBootServicesTableLib.h>
#include <Include/Library/IoLib.h>
#include <Include/Library/UefiLIb.h>
#include <IndustryStandard/Pci22.h> // for use #define PCI_XXXXX_OFFSET
//ShellPkg
#include <Include/Library/ShellCommandLib.h>

//----> cppy from AmiLib.h
#define MMIO_READ8(Address) (*(volatile UINT8*)(Address))
#define MMIO_WRITE8(Address,Value) (*(volatile UINT8*)(Address)=(Value))
#define MMIO_READ16(Address) (*(volatile UINT16*)(Address))
#define MMIO_WRITE16(Address,Value) (*(volatile UINT16*)(Address)=(Value))
#define MMIO_READ32(Address) (*(volatile UINT32*)(Address))
#define MMIO_WRITE32(Address,Value) (*(volatile UINT32*)(Address)=(Value))
#define MMIO_READ64(Address) (*(volatile UINT64*)(Address))
#define MMIO_WRITE64(Address,Value) (*(volatile UINT64*)(Address)=(Value))
//---->

#define PCIEX_BASE_ADDRESS 0xE0000000
#define CLEAN_ADDRESS 0xFF000000

#define PCIE_CFG_ADDRESS(bus, dev, func, reg) \
            ((UINTN)(PCIEX_BASE_ADDRESS + ((UINT8)(bus) << 20) + \
            ((UINT8)(dev) << 15) + ((UINT8)(func) << 12) + (reg)))
