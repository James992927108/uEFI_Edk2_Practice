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
                    // Print(L"1.PciAddress %08x\n", PciAddress - PCI_CLASSCODE_OFFSET);
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
        Print(L"2.No find any SMBus Io port\n");
        return FALSE;
    }
}

BOOLEAN CheckContinue(BOOLEAN Continue)
{
    SHELL_PROMPT_RESPONSE *Resp = NULL;
    ShellPromptForResponse(ShellPromptResponseTypeYesNo, L"Type 'y' or 'n' for continue: ", (VOID **)&Resp);
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

    if (GetSMBusIoPort(&IoSpaceBase))
    {
        UINT8 *InputStr = NULL;
        BOOLEAN Continue = TRUE;
        Print(L"3. Find SMBus IO Space base %08x\n", IoSpaceBase);
        while (Continue)
        {
            ShellPromptForResponse(ShellPromptResponseTypeFreeform, L"Please select Sdram channel:", (VOID **)&InputStr);
            Print(L"Select Sdram channel is ASCII (Glyph)%02x (Dec)%02x (Hex)0x%02x\n", *InputStr, *InputStr - 48, *InputStr - 0x30);
            
            UINT8 SlaveAddr = 0xA0;
            SlaveAddr += (*InputStr);
            Print(L"SlaveAddr ASCII (Glyph)%02x (Dec)%02x (Hex)0x%02x\n", SlaveAddr, SlaveAddr - 48, SlaveAddr - 0x30);
            SlaveAddr -= 0x30; 
            Print(L"Slave: 0x%02x\n", SlaveAddr);
            
            // 0001 1110
            IoWrite8(IoSpaceBase + SMB_HST_STS, 0x1E);
            IoWrite8(IoSpaceBase + SMB_XMIT_SLVA, SlaveAddr + SLAVE_ENABLE);
            IoWrite8(IoSpaceBase + SMB_HST_CMD, 0);    //read first byte
            IoWrite8(IoSpaceBase + SMB_HST_CNT, 0x48); //read a byte
            while (IoRead8(IoSpaceBase + SMB_HST_STS) & 0x01)
                ;                                          //waiting for smbus
            if (IoRead8(IoSpaceBase + SMB_HST_STS) & 0x04) //detect error bit
                continue;

            UINT32 Index;
            UINT8 DumpData[256];
            for (Index = 0; Index < SMB_SIZE; Index++)
            {
                IoWrite8(IoSpaceBase + SMB_HST_STS, 0x1E);
                IoWrite8(IoSpaceBase + SMB_XMIT_SLVA, SlaveAddr + SLAVE_ENABLE);
                IoWrite8(IoSpaceBase + SMB_HST_CMD, (UINT8)Index); // read index byte
                IoWrite8(IoSpaceBase + SMB_HST_CNT, 0x48);         //read a byte
                while (IoRead8(IoSpaceBase + SMB_HST_STS) & 0x01)
                    ;
                DumpData[Index] = IoRead8(IoSpaceBase + SMB_HST_DAT_0);
                if ((Index + 1) % 0x10 == 0)
                    Print(L"%02x\r\n", DumpData[Index]);
                else
                    Print(L"%02x ", DumpData[Index]);
            }
            UINTN SpaceSize = 2;
            DumpHex(SpaceSize, SMB_HST_DAT_0, SMB_SIZE, DumpData);

            Print(L"\n");
            Continue = CheckContinue(Continue);
        }
    }
    else
    {
        Print(L"3.Can not find SMBus Io Space base\n");
    }
    return 0;
}
