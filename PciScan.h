
/*
 * PciScan.c
 *
 *  Created on: 2020年11月09日
 *      Author: Anthony Teng
 */

//MdePkg
#include <Include/Library/UefiLib.h>
#include <Include/Library/UefiBootServicesTableLib.h>
#include <Include/Library/DebugLib.h> // for use ASSERT_EFI_ERROR
#include <Include/Protocol/PciIo.h>
#include <Include/Protocol/PciRootBridgeIo.h> // for use gEfiPciRootBridgeIoProtocolGuid in MdePkg.dec
#include <Include/IndustryStandard/Pci22.h>  // for use #define PCI_XXXXX_OFFSET
#include <Include/IndustryStandard/Acpi10.h> // for usse EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR
#include <Include/AArch64/ProcessorBind.h>
#include <Include/Uefi/UefiBaseType.h> // for use EFI_ERROR
#include <Include/Library/PciLib.h> // for use PCI_LIB_ADDRESS, PciReadBuffer

//shellPkg
#include <Include/Library/ShellLib.h>
#include <Library/UefiShellDebug1CommandsLib/Pci.h>

#define PCI_Type_0_MAX_BAR 0x0006
#define PCI_Type_1_MAX_BAR 0x0002

UINT32 gAllOne = 0xFFFFFFFF;

typedef struct
{
    UINT64 BaseAddress;
    UINT64 Length;
    BOOLEAN Prefetchable;
    UINT8 Offset;
} PCI_BAR;
