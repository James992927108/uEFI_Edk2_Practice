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

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

#define PCI_BASE_ADDRESS 0x80000000
#define CLEAN_ADDRESS 0xFF000000

#define PCI_CFG_ADDRESS(bus, dev, func, reg) \
            ((UINTN)(PCI_BASE_ADDRESS + ((UINT8)(bus) << 16) + \
            ((UINT8)(dev) << 11) + ((UINT8)(func) << 8) + (reg)))
