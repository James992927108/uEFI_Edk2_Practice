/*
 *  SpdRead.c
 *
 *  Created on: 2020年11月16日
 *      Author: Anthony Teng
 */

#include "SpdRead.h"

BOOLEAN GetSMBusIoSpaceBase(UINT32 *IoSpaceBase)
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
                // Print(L"Vender ID 0x%04x, Device ID 0x%04x\n", 0x0000FFFF & Val, Val >> 16);
                PciAddress = PciAddress | PCI_CLASSCODE_OFFSET; //read class code
                // PciAddress = 0x8000FC09;
                IoWrite32(PCI_CONFIG_ADDRESS, PciAddress);
                Val = IoRead32(PCI_CONFIG_DATA);
                // 0B 0A 09 08 ->  0000 0000 0000 0000 0000 0000 | 0000 0000
                //                 class code [9:B]         |  Revision id [8]
                // read [9:B]
                if ((Val & 0xFFFFFF00) == 0x0C050000) // 0x0C0500 is SMBus
                {
                    // Print(L"->%x %x %x %x\n",BusNum << 16, DevNum << 11, FunNum << 8, PciAddress);
                    Print(L"PciAddress with PCI_CLASSCODE_OFFSET %08x\n", PciAddress);
                    PciAddress -= PCI_CLASSCODE_OFFSET;
                    Print(L"PciAddress %08x\n", PciAddress);
                    //18.1 PCI configuration Register (PCI_BAR_IDX4 0x20) -> SMBus Base Address offset is 20h ~ 23h
                    Print(L"PciAddress with PCI_BAR_IDX4 %08x\n", PciAddress + PCI_BAR_IDX4);
                    IoWrite32(PCI_CONFIG_ADDRESS, PciAddress + PCI_BAR_IDX4);
                    // SMB_Base default is 0000 0001h ->BAR bit [0] is 1 -> always is IO, [1] return 0 for read.
                    // so Mark = 1 1 1 1  1 1 1111 1100
                    //           F F F F  F F F    C
                    // if bit [0] is 0 -> always is mem,[3] Prefetchable [2:1] type
                    *IoSpaceBase = (IoRead32(PCI_CONFIG_DATA) & 0xFFFFFFFC);
                    return TRUE;
                }
            }
        }
    }
    Print(L"No find any SMBus Io port\n");
    return FALSE;
}

BOOLEAN CheckContinue()
{
    SHELL_PROMPT_RESPONSE *Resp = NULL;
    ShellPromptForResponse(ShellPromptResponseTypeYesNo, L"\nType 'y' or 'n' for continue: ", (VOID **)&Resp);
    switch (*(SHELL_PROMPT_RESPONSE *)Resp)
    {
    case ShellPromptResponseNo:
        Print(L"Choose No\n");
        return FALSE;
        break;
    case ShellPromptResponseYes:
        return TRUE;
        Print(L"Choose Yes\n");
        break;
    default:
        return FALSE;
        break;
    }
}

BOOLEAN GetSlaveAddrList(UINT32 IoSpaceBase, UINT8 *SlaveAddrList, UINT8 *Count)
{
    INT32 Index = 0;
    BOOLEAN Result = FALSE;
    for (INT32 i = 0; i < 16; i++)
    {
        SlaveAddrList[i] = 0;
    }
    for (UINT8 SlaveAddr = 0xA0; SlaveAddr < 0xA0 + 32; SlaveAddr += 2)
    {
        //18.2.1 HST_STS-HOST Status Register(SMBus) SMB_BASE + 00h, 8 bits
        // Set 1001 1110
        // Note [1:5] [7]clear bit by write a 1 to it. 1011 1110
        // [1] INTR = 1, [2] DEV_ERR = 1, [3] DEV_ERR = 1, [4]FAILED = 1, [5] SMBALERT_STS = 1[7]  Done Status = 1
        IoWrite8(IoSpaceBase + SMB_HST_STS, 0xFF); // -> 40
        //18.2.4 XMIT_SLVA Transmit Slave Transmit Slave Address Register(SMBus) SMB_BASE + 04h, 8 bits
        //set Address[7:1] + RW[0] -> SlaveAddr + 0x01(0W/1R)，So for "read"
        IoWrite8(IoSpaceBase + SMB_XMIT_SLVA, SlaveAddr | SLAVE_READ_ENABLE);
        //18.2.3 HST_CMD-HOST Command Register(SMBus) SMB_BASE + 03h, 8 bits
        // set 0000 0000 read first(index = 0) byte
        IoWrite8(IoSpaceBase + SMB_HST_CMD, 0);
        //18.2.2 HST_CNT-HOST Contorl Register(SMBus) SMB_BASE + 02h, 8 bits
        //Set 0100 1000 [0] INTREN = 1 -> Enable [4:2] SMB_CMD = 010 -> "Byte" Data, [6] START = 1
        IoWrite8(IoSpaceBase + SMB_HST_CNT, 0x48);
        // -> 42 (0100 0010) bit 6 INUSE_STS = 1, mean read, the next time read will set 0
        // bit 1 INTR, interrupt was the successful.
        while (IoRead8(IoSpaceBase + SMB_HST_STS) & 0x01)
            ;
        // Print(L"Transaction is completed\n");
        if (IoRead8(IoSpaceBase + SMB_HST_STS) & 0x04)
        {
            // Print(L"DEV_ERR (bit 3) -> 1\n");
            continue;
        }
        else
        {
            // Print(L"DEV_ERR (bit 3)-> 0\n");
            SlaveAddrList[Index++] = SlaveAddr;
            Result = TRUE;
        }
    }
    // for (INT32 i = 0; i < 16; i++)
    // {
    //     Print(L"i = %d, 0x%02x\n", i, SlaveAddrList[i]);
    // }
    *Count = (UINT8)Index;
    return Result;
}

void ReadSpdInfo(UINT32 IoSpaceBase, UINT8 SlaveAddr)
{
    Print(L"--> Slave 0x%02x\n", SlaveAddr);
    Print(L"--> Slave & 0x01: 0x%02x\n", SlaveAddr | SLAVE_READ_ENABLE);
    BOOLEAN Page_0 = TRUE;
    UINT8 DumpData = 0;

    for (INT32 Index = 0; Index < SPD_DDR4_SIZE; Index++)
    {
        if (Index > 0x100 && Page_0 == TRUE)
        {
            IoWrite8(IoSpaceBase + SMB_HST_STS, 0xFF);
            // Switch to page 1
            IoWrite8(IoSpaceBase + SMB_XMIT_SLVA, 0x6E | SLAVE_READ_ENABLE);
            IoWrite8(IoSpaceBase + SMB_HST_CNT, 0x48);
            while (IoRead8(IoSpaceBase + SMB_HST_STS) & 0x01)
                ;
            Page_0 = FALSE;
        }
        
        IoWrite8(IoSpaceBase + SMB_HST_STS, 0xFF);
        IoWrite8(IoSpaceBase + SMB_XMIT_SLVA, SlaveAddr | SLAVE_READ_ENABLE);
        IoWrite8(IoSpaceBase + SMB_HST_CMD, (UINT8)Index);
        IoWrite8(IoSpaceBase + SMB_HST_CNT, 0x48);
        while (IoRead8(IoSpaceBase + SMB_HST_STS) & 0x01)
            ;
        DumpData = IoRead8(IoSpaceBase + SMB_HST_DAT_0);
        if ((Index + 1) % 0x10 == 0)
            Print(L"%02x\r\n", DumpData);
        else
            Print(L"%02x ", DumpData);
    }
    // Switch back to page 0
    IoWrite8(IoSpaceBase + SMB_HST_STS, 0xFF);
    // Switch page 0
    IoWrite8(IoSpaceBase + SMB_XMIT_SLVA, 0x6C | SLAVE_READ_ENABLE);
    IoWrite8(IoSpaceBase + SMB_HST_CNT, 0x48);
    while (IoRead8(IoSpaceBase + SMB_HST_STS) & 0x01)
        ;
}

EFI_STATUS UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
    UINT32 IoSpaceBase;

    UINT8 SlaveAddrList[16];
    UINT8 Length;

    UINT8 SlaveAddr;
    UINT8 *InputStr = NULL;

    if (!GetSMBusIoSpaceBase(&IoSpaceBase))
    {
        Print(L"Can not find SMBus Io Space base\n");
        return 0;
    }

    Print(L"Find SMBus IO Space base %08x\n", IoSpaceBase);

    do
    {
        if (!GetSlaveAddrList(IoSpaceBase, SlaveAddrList, &Length))
        {
            Print(L"Can not find any available slave on board\n");
            return 0;
        }

        for (INT32 Index = 0; Index < Length; Index++)
        {
            Print(L"Index: %d, Addr: 0x%02x\n", Index, SlaveAddrList[Index]);
        }
        Print(L"----------------------------------\n");

        while (1)
        {
            ShellPromptForResponse(ShellPromptResponseTypeFreeform, L"Please tpye sdram channel from SlaveAddrList by Index:", (VOID **)&InputStr);
            UINT8 Min = 0x30;
            UINT8 Max = 0x30 + Length;
            UINT8 Input = (*InputStr);
            UINT8 Select = *InputStr - 0x30;
            UINT8 SelectValue = SlaveAddrList[Select];
            // Print(L"Debug 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n", Min, Max, Input, Select, SelectValue);
            if (Input < Min || Input >= Max)
            {
                Print(L"Out of the Range\n");
            }
            else
            {
                Print(L"Select sdram channel is %d, 0x%02x\n", Select, SelectValue);
                SlaveAddr = SelectValue;
                break;
            }
        } // End while

        ReadSpdInfo(IoSpaceBase, SlaveAddr);

        // UINTN SpaceSize = 2;
        // Print(L"\nCall UefiShellCommandLib.c DumpHex()\n");
        // DumpHex(SpaceSize, SMB_HST_DAT_0, SPD_DDR4_SIZE, DumpData);
    } while (CheckContinue());

    return 0;
}