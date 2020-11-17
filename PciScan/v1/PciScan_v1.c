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
    UINT32 PciAddress;
    PCI_DEVICE_INDEPENDENT_REGION Data;
    for (BusNum = 0x00; BusNum < 0xFF; BusNum++)
    {
        for (DevNum = 0x00; DevNum <= 0x1F; DevNum++)
        {
            for (FunNum = 0x00; FunNum <= 0x07; FunNum++)
            {
                PciAddress &= 0xFF000000; //clear PciAddress
                PciAddress |= BASE_ADDRESS | ((BusNum << 16) | (DevNum << 11) | (FunNum << 8));

                PciAddress |= PCI_VENDOR_ID_OFFSET; 
                IoWrite32(PCI_CONFIG_ADDRESS, PciAddress);
                Data.VendorId = IoRead16(PCI_CONFIG_DATA);
                if (Data.VendorId == 0xFFFF)
                    continue;
                Print(L"Bus\\Dev\\Fun, %02x\\%02x\\ss%02x\n", BusNum, DevNum, FunNum);
                Print(L"Vendor Id 0x%04x\n", Data.VendorId);

                PciAddress |= PCI_DEVICE_ID_OFFSET;
                IoWrite32(PCI_CONFIG_ADDRESS, PciAddress);
                Data.DeviceId = IoRead16(PCI_CONFIG_DATA);
                Print(L"Device Id 0x%04x\n", Data.DeviceId);

                PciAddress |= PCI_COMMAND_OFFSET;
                IoWrite32(PCI_CONFIG_ADDRESS, PciAddress);
                Data.Command = IoRead16(PCI_CONFIG_DATA);
                Print(L"Command 0x%04x\n", Data.Command);

            }
        }
    }

    return 0;
}
