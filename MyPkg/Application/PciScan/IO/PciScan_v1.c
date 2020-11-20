/*
 *  SpdRead.c
 *
 *  Created on: 2020年11月16日
 *      Author: Anthony Teng
 */

#include "PciScan_v1.h"

EFI_STATUS EFIAPI UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
    UINT8 BusNum, DevNum, FunNum;
    INT32 Total = 0;
    UINT32 PciConfigAddress;
    PCI_DEVICE_INDEPENDENT_REGION Data;
    for (BusNum = 0; BusNum < PCI_MAX_BUS; BusNum++)
    {
        for (DevNum = 0; DevNum <= PCI_MAX_DEVICE; DevNum++)
        {
            for (FunNum = 0; FunNum <= PCI_MAX_FUNC; FunNum++)
            {
                PciConfigAddress &= CLEAN_ADDRESS; //clear PciConfigAddress
                PciConfigAddress = PCI_CFG_ADDRESS(BusNum, DevNum, FunNum, PCI_VENDOR_ID_OFFSET);
                IoWrite32(PCI_CONFIG_ADDRESS, PciConfigAddress);
                Data.VendorId = IoRead16(PCI_CONFIG_DATA);
                if (Data.VendorId == 0xFFFF)
                    continue;
                Print(L"Bus\\Dev\\Fun, %02x\\%02x\\%02x\n", BusNum, DevNum, FunNum);
                Print(L"Vendor Id 0x%04x\n", Data.VendorId);

                PciConfigAddress |= PCI_DEVICE_ID_OFFSET;
                IoWrite32(PCI_CONFIG_ADDRESS, PciConfigAddress);
                Data.DeviceId = IoRead16(PCI_CONFIG_DATA);
                Print(L"Device Id 0x%04x\n", Data.DeviceId);

                Total += 1;
            }
        }
    }
    Print(L"Total %d\n", Total);
    return 0;
}
