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
                Print(L"Vender ID 0x%04x, Device ID 0x%04x\n", 0x0000FFFF & Val, Val >> 16);
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
    UINT8  *InputStr = NULL;
    BOOLEAN Continue;

    UINT8 SlaveAddr;
    UINT32 Index;
    UINT8 DumpData[256];
    BOOLEAN Failed;

    if (GetSMBusIoSpaceBase(&IoSpaceBase))
    {
        Print(L"Find SMBus IO Space base %08x\n", IoSpaceBase);
        Continue = TRUE;
        while (Continue)
        {

            while (1)
            {
                ShellPromptForResponse(ShellPromptResponseTypeFreeform, L"Please select Sdram channel(1 ~ 4):", (VOID **)&InputStr);
                if ((*InputStr) < 49 || (*InputStr) > 52)
                {
                    Print(L"Out of the Range\n");
                }
                else
                {
                    Print(L"Select Sdram channel is (Dex)%02u (Glyph)%02u\n", *InputStr, *InputStr - 48);
                    SlaveAddr = 0xA0 + (*InputStr);
                    Print(L"SlaveAddr -> (Dex)%02u (Glyph)0x%02u\n", SlaveAddr, SlaveAddr - 48);
                    SlaveAddr -= 0x30;
                    Print(L"Slave: 0x%02x\n", SlaveAddr);
                    // 先判斷 此 slva 是否讀取成功 -> 0x04
                    //18.2.1 HST_STS-HOST Status Register(SMBus) SMB_BASE + 00h, 8 bits
                    // Set 1001 1110
                    // Note [1:5] [7]clear bit by write a 1 to it. 1011 1110
                    // [1] INTR = 1, [2] DEV_ERR = 1, [3] BUS_ERR = 1, [4]FAILED = 1, [5] SMBALERT_STS = 1[7]  Done Status = 1
                    IoWrite8(IoSpaceBase + SMB_HST_STS, 0xBE); // -> 40
                    //18.2.4 XMIT_SLVA Transmit Slave Transmit Slave Address Register(SMBus) SMB_BASE + 04h, 8 bits
                    //set Address[7:1] + RW[0] -> SlaveAddr + 0x01(0W/1R)，So for "read"
                    IoWrite8(IoSpaceBase + SMB_XMIT_SLVA, SlaveAddr | 0x01);
                    //18.2.3 HST_CMD-HOST Command Register(SMBus) SMB_BASE + 03h, 8 bits
                    // set 0000 0000 read first(index = 0) byte
                    IoWrite8(IoSpaceBase + SMB_HST_CMD, 0);
                    //18.2.2 HST_CNT-HOST Contorl Register(SMBus) SMB_BASE + 02h, 8 bits
                    //Set 0100 1000 [0] INTREN = 1 -> Enable [4:2] SMB_CMD = 010 -> "Byte" Data, [6] START = 1
                    IoWrite8(IoSpaceBase + SMB_HST_CNT, 0x48);
                    // -> 42 (0100 0010) bit 6 INUSE_STS = 1, mean read, the next time read will set 0
                    // bit 1 INTR, interrupt was the successful.

                    UINT8 Temp = IoRead8(IoSpaceBase + SMB_HST_STS);
                    // -> 01 (0000 0001)
                    // bit 0 -> set 1, "No" SMB registers should be accessed while is set.
                    // bit 1 -> the PCH de-assert the interrut, so set 0.
                    // bit 6, 1 -> 0 (next time read will reset 0)
                    Print(L"Before: 0x%02x\n", Temp);
                    while (IoRead8(IoSpaceBase + SMB_HST_STS) & 0x01)
                    {
                        Temp = IoRead8(IoSpaceBase + SMB_HST_STS);
                    }
                    Print(L"After: 0x%02x\n", Temp);
                    // -> X0 (0X00 0000) -> X maybe 0 or 1
                    if ((IoRead8(IoSpaceBase + SMB_HST_STS) & 0x01) == 0)
                    {
                        Print(L"Transaction is completed\n");
                        if ((IoRead8(IoSpaceBase + SMB_HST_STS) & 0x04) == 1)
                        {
                            Failed = TRUE;
                            Print(L"SMB_HST_STS Failed -> 1\n");
                        }
                        else
                        {
                            Failed = FALSE;
                            Print(L"SMB_HST_STS Failed -> 0\n");
                        }
                    }
                    else
                    {
                        Print(L"Transaction is not completed\n");
                    }
                    break;
                }
            }

            if (!Failed)
            {
                for (Index = 0; Index < SMB_SIZE; Index++)
                {
                    IoWrite8(IoSpaceBase + SMB_HST_STS, 0x1E);
                    IoWrite8(IoSpaceBase + SMB_XMIT_SLVA, SlaveAddr + SLAVE_ENABLE);
                    IoWrite8(IoSpaceBase + SMB_HST_CMD, (UINT8)Index);
                    IoWrite8(IoSpaceBase + SMB_HST_CNT, 0x49);
                    while (IoRead8(IoSpaceBase + SMB_HST_STS) & 0x01)
                        ;
                    DumpData[Index] = IoRead8(IoSpaceBase + SMB_HST_DAT_0);
                    if ((Index + 1) % 0x10 == 0)
                        Print(L"%02x\r\n", DumpData[Index]);
                    else
                        Print(L"%02x ", DumpData[Index]);
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