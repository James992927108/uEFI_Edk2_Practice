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
                IoWrite32(PCI_CONFIG_ADDRESS, PciAddress);
                Val = IoRead32(PCI_CONFIG_DATA);
                // 0B 0A 09 08 ->  0000 0000 0000 0000 0000 | 0000 0000
                //                 class code [9:B]         |  Revision id [8]
                // read [9:B]
                if ((Val & 0xFFFFFF00) == 0x0C050000) // 0x0C0500 is SMBus
                {
                    PciAddress -= PCI_CLASSCODE_OFFSET;
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

BOOLEAN CheckContinue(BOOLEAN Continue)
{
    SHELL_PROMPT_RESPONSE *Resp = NULL;
    ShellPromptForResponse(ShellPromptResponseTypeYesNo, L"\nType 'y' or 'n' for continue: ", (VOID **)&Resp);
    switch (*(SHELL_PROMPT_RESPONSE *)Resp)
    {
    case ShellPromptResponseNo:
        Continue = FALSE;
        Print(L"Choose No\n");
        break;
    case ShellPromptResponseYes:
        Continue = TRUE;
        Print(L"Choose Yes\n");
        break;
    default:
        break;
    }
    return Continue;
}

EFI_STATUS UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
    UINT32 IoSpaceBase;
    UINT8 *InputStr = NULL;
    BOOLEAN Continue;

    UINT8 SlaveAddr;
    UINT32 Index;
    UINT8 DumpData[256];
    BOOLEAN DEV_ERR;

    UINT8 Exp;

    if (GetSMBusIoSpaceBase(&IoSpaceBase))
    {
        Print(L"Find SMBus IO Space base %08x\n", IoSpaceBase);
        Continue = TRUE;
        while (Continue)
        {
            while (1)
            {
                ShellPromptForResponse(ShellPromptResponseTypeFreeform, L"Please select Sdram channel(0 ~ 3):", (VOID **)&InputStr);
                if ((*InputStr) < 0x30 || (*InputStr) > 0x33)
                {
                    Print(L"Out of the Range\n");
                }
                else
                {
                    Print(L"Select Sdram channel is (Dex)%02x (Glyph)%02x\n", *InputStr, *InputStr - 0x30);
                    break;
                }
            } // End while

            Exp = *InputStr - 0x30;
            SlaveAddr = 0xA0 + ( 1 << Exp);
            Print(L"Base 0x%02x, Slave: 0x%02x\n", 0xA0, SlaveAddr);

            //18.2.1 HST_STS-HOST Status Register(SMBus) SMB_BASE + 00h, 8 bits
            // Set 1001 1110
            // Note [1:5] [7]clear bit by write a 1 to it. 1011 1110
            // [1] INTR = 1, [2] DEV_ERR = 1, [3] DEV_ERR = 1, [4]FAILED = 1, [5] SMBALERT_STS = 1[7]  Done Status = 1
            IoWrite8(IoSpaceBase + SMB_HST_STS, 0xFF); // -> 40
            //18.2.4 XMIT_SLVA Transmit Slave Transmit Slave Address Register(SMBus) SMB_BASE + 04h, 8 bits
            //set Address[7:1] + RW[0] -> SlaveAddr + 0x01(0W/1R)，So for "read"
            Print(L"1.-->  0x%02x\n",SlaveAddr | SLAVE_ENABLE);

            IoWrite8(IoSpaceBase + SMB_XMIT_SLVA, SlaveAddr | SLAVE_ENABLE);
            //18.2.3 HST_CMD-HOST Command Register(SMBus) SMB_BASE + 03h, 8 bits
            // set 0000 0000 read first(index = 0) byte
            IoWrite8(IoSpaceBase + SMB_HST_CMD, 0);
            //18.2.2 HST_CNT-HOST Contorl Register(SMBus) SMB_BASE + 02h, 8 bits
            //Set 0100 1000 [0] INTREN = 1 -> Enable [4:2] SMB_CMD = 010 -> "Byte" Data, [6] START = 1
            IoWrite8(IoSpaceBase + SMB_HST_CNT, 0x48);
            // -> 42 (0100 0010) bit 6 INUSE_STS = 1, mean read, the next time read will set 0
            // bit 1 INTR, interrupt was the successful.

            // -> 01 (0000 0001)
            // bit 0 -> set 1, "No" SMB registers should be accessed while is set.
            // bit 1 -> the PCH de-assert the interrut, so set 0.
            // bit 6, 1 -> 0 (next time read will reset 0)
            UINT8 Temp;
            Temp = IoRead8(IoSpaceBase + SMB_HST_STS);
            Print(L"Line: %d, Temp: 0x%02x\n", __LINE__, Temp);
            while (IoRead8(IoSpaceBase + SMB_HST_STS) & 0x01)
                ;

            Print(L"Transaction is completed\n");
            if (IoRead8(IoSpaceBase + SMB_HST_STS) & 0x04)
            {
                DEV_ERR = TRUE;
                Print(L"DEV_ERR -> 1\n");
                Temp = IoRead8(IoSpaceBase + SMB_HST_STS);
                Print(L"Line: %d, Temp: 0x%02x\n", __LINE__, Temp);
            }
            else
            {
                DEV_ERR = FALSE;
                Print(L"DEV_ERR -> 0\n");
                Temp = IoRead8(IoSpaceBase + SMB_HST_STS);
                Print(L"Line: %d, Temp: 0x%02x\n", __LINE__, Temp);
            }

            if (DEV_ERR == FALSE)
            {
                Print(L"2.-->  0x%02x\n",SlaveAddr | SLAVE_ENABLE);
                for (Index = 0; Index < SMB_SIZE; Index++)
                {
                    IoWrite8(IoSpaceBase + SMB_HST_STS, 0x1E);
                    IoWrite8(IoSpaceBase + SMB_XMIT_SLVA, SlaveAddr | SLAVE_ENABLE);
                    IoWrite8(IoSpaceBase + SMB_HST_CMD, (UINT8)Index);
                    IoWrite8(IoSpaceBase + SMB_HST_CNT, 0x48);
                    while (IoRead8(IoSpaceBase + SMB_HST_STS) & 0x01)
                        ;
                    DumpData[Index] = IoRead8(IoSpaceBase + SMB_HST_DAT_0);
                    if ((Index + 1) % 0x10 == 0)
                        Print(L"%02x\r\n", DumpData[Index]);
                    else
                        Print(L"%02x ", DumpData[Index]);
                }
                for (Index = 0; Index < SMB_SIZE; Index++)
                {
                    DumpData[Index] = 0;
                }
            }
            // UINTN SpaceSize = 2;
            // Print(L"\nCall UefiShellCommandLib.c DumpHex()\n");
            // DumpHex(SpaceSize, SMB_HST_DAT_0, SMB_SIZE, DumpData);

            Continue = CheckContinue(Continue);
        }
    }
    else
    {
        Print(L"3.Can not find SMBus Io Space base\n");
    }
    return 0;
}