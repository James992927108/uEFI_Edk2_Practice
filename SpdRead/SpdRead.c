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
    for (BusNum = 0; BusNum < PCI_MAX_BUS; BusNum++)
    {
        for (DevNum = 0; DevNum <= PCI_MAX_DEVICE; DevNum++)
        {
            for (FunNum = 0; FunNum <= PCI_MAX_FUNC; FunNum++)
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
                // Print(L"0x%08x, 0x%08x\n", Val, Val & 0xFFFFF00);
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

BOOLEAN GetSMBusIoPort(UINT32 *IoSpaceBase)
{
    UINT32 PciConfigAddr;
    if (FindSMBusDev(&PciConfigAddr))
    {
        Print(L"2.PciAddress %08x\n", PciConfigAddr);
        IoWrite32(PCI_CONFIG_ADDRESS, PciConfigAddr + PCI_BAR_IDX4);
        *IoSpaceBase = (IoRead32(PCI_CONFIG_DATA) & 0xFFFFFFFC);
        return TRUE;
    }
    else
    {
        Print(L"1.No find any SMBus Io port\n");
        return FALSE;
    }
}

INTN ShellAppMain(IN UINTN Argc, IN CHAR16 *Argv)
{
    UINT32 IoSpaceBase;

    if (!GetSMBusIoPort(&IoSpaceBase))
    {
        Print(L"2.Can not find SMBus Io Space base\n");

        return 0;
    }
    // SMBus controller Register
    Print(L"3.SMBus Io Space base %08x\n", IoSpaceBase);

    UINT8 SlaveAddr;
    UINT32 Index;
    UINT8 DumpData[256];
    UINT8 Data;
    for (SlaveAddr = 0xA0; SlaveAddr < 0xA0 + 32; SlaveAddr += 2)
    {
        Print(L"Slave: 0x%02x\n", SlaveAddr);
        IoWrite8(IoSpaceBase + SMB_HST_STS, 0x1E);
        IoWrite8(IoSpaceBase + SMB_HST_ADD, SlaveAddr + SLAVE_ENABLE);
        IoWrite8(IoSpaceBase + SMB_HST_CMD, 0);    //read first byte
        IoWrite8(IoSpaceBase + SMB_HST_CNT, 0x48); //read a byte
        while (IoRead8(IoSpaceBase + SMB_HST_STS) & 0x01)
            ;                            //waiting for smbus
        if (IoRead8(IoSpaceBase + SMB_HST_STS) & 0x04) //detect error bit
            continue;
        
        for (Index = 0; Index < SMB_SIZE; Index++)
        {
            IoWrite8(IoSpaceBase + SMB_HST_STS, 0x1E);
            IoWrite8(IoSpaceBase + SMB_HST_ADD, SlaveAddr + SLAVE_ENABLE);
            IoWrite8(IoSpaceBase + SMB_HST_CMD, (UINT8)Index); // read index byte
            IoWrite8(IoSpaceBase + SMB_HST_CNT, 0x48);         //read a byte
            while (IoRead8(IoSpaceBase + SMB_HST_STS) & 0x01)
                ;
            Data = IoRead8(IoSpaceBase + SMB_HST_DAT_0);
            if ((Index + 1) % 0x10 == 0)
                Print(L"%02x\r\n", Data);
            else
                Print(L"%02x ", Data);
            DumpData[Index] = Data;
        }
        UINTN SpaceSize = 2;
        DumpHex(SpaceSize, SMB_HST_DAT_0, SMB_SIZE, DumpData);
    }

    Print(L"\n");
    return 0;
}
