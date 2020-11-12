#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Library/DebugLib.h> // for use ASSERT_EFI_ERROR

#include <Protocol/PciIo.h>

#include <Protocol/PciRootBridgeIo.h> // for use gEfiPciRootBridgeIoProtocolGuid in MdePkg.dec

#include <IndustryStandard/Pci22.h>  // for use #define PCI_XXXXX_OFFSET
#include <IndustryStandard/Acpi10.h> // for usse EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR
#include <AArch64/ProcessorBind.h>

#include <Uefi/UefiBaseType.h> // for use EFI_ERROR

#include <Library/ShellLib.h>

#include <Library/UefiShellDebug1CommandsLib/Pci.h>

#include <Library/PciLib.h> // for use PCI_LIB_ADDRESS, PciReadBuffer

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
