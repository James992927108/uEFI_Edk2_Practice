/*
 *  SpdRead.c
 *
 *  Created on: 2020年11月16日
 *      Author: Anthony Teng
 */

#include "SpdRead.h"

BOOLEAN FindSMBusDev(UINT32 *PciConfigAddr)
{
    UINT8 BusNum, DevNum, FunNum;
    UINT32 PciAddress;
    UINT32 Val;
    for (BusNum = 0x00; BusNum < 0xFF; BusNum++)
    {
        for (DevNum = 0x00; DevNum <= 0x1F; DevNum++)
        {
            for (FunNum = 0x00; FunNum <= 0x07; FunNum++)
            {
                PciAddress &= 0xFF000000;
                PciAddress |= BASE_ADDRESS | ((BusNum << 16) | (DevNum << 11) | (FunNum << 8)) | PCI_VENDOR_ID_OFFSET; //set the pci bus dev and fun
                IoWrite32(PCI_CONFIG_ADDRESS, PciAddress);
                Val = IoRead32(PCI_CONFIG_DATA);
                if (Val == 0xFFFFFFFF)
                    continue;

                Print(L"Vender ID 0x%04x, Device ID 0x%04x\n", 0x0000FFFF & Val, Val >> 16);
                PciAddress = PciAddress | PCI_CLASSCODE_OFFSET; //read class code
                IoWrite32(PCI_CONFIG_ADDRESS, PciAddress);
                Val = IoRead32(PCI_CONFIG_DATA);
                Print(L"0x%08x, 0x%08x\n", Val, Val & 0xFFFFF00);
                if ((Val & 0xFFFFFF00) == 0x0C050000) // 0x0C0500 is SMBus
                {
                    Print(L"1.PciAddress %08x\n", PciAddress - PCI_CLASSCODE_OFFSET);
                    *PciConfigAddr = (PciAddress - PCI_CLASSCODE_OFFSET);
                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

BOOLEAN GetSMBusIoPort(UINT32 *SMBusIoPort)
{
    UINT32 PciConfigAddr;
    if (FindSMBusDev(&PciConfigAddr))
    {
        Print(L"2.PciAddress %08x\n", PciConfigAddr);
        IoWrite32(PCI_CONFIG_ADDRESS, PciConfigAddr + PCI_BAR_IDX4);
        *SMBusIoPort = (IoRead32(PCI_CONFIG_DATA) & 0xFFFFFFFC);
        return TRUE;
    }
    else
    {
        Print(L"1.No Find SMBus Io Port\n");
        return FALSE;
    }
}

INTN ShellAppMain(IN UINTN Argc, IN CHAR16 *Argv)
{
    UINT32 SMBusIoPort;

    if (!GetSMBusIoPort(&SMBusIoPort))
    {
        Print(L"2.No Find SMBus Io Port\n");

        return 0;
    }
    Print(L"3.SMBusIoPort %08x\n", SMBusIoPort);

    UINT32 IoSpaceBaseAddress;
    UINT32 IoSpace[64];
    for (UINT16 index = 0; index < 64; index++)
    {
        IoSpaceBaseAddress = IO_LIB_ADDRESS(0, SMBusIoPort + index * 4);
        IoSpace[index] = IoRead32(IoSpaceBaseAddress);
    }
    INT32 i;
    INT32 j;
    for (j = 3; j < 16; j += 4)
    {
        for (i = j; i > j - 4; i--)
        {
            Print(L"%02x", i);
            if (i % 4 == 0)
            {
                Print(L" ");
            }
        }
    }
    Print(L"\n");
    i = 0, j = 0;
    for (j = 0; j < 64; j++)
    {
        Print(L"%08X ", IoSpace[j]);
        if ((j % 4) == 3)
        {
            Print(L"%02X \n", i);
            i = i + 1;
        }
    }
    Print(L"\n");
    return 0;
}
