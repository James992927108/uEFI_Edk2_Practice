
/*
 *  PciScan.c
 *
 *  Created on: 2020年11月09日
 *      Author: Anthony Teng
 */

#include "PciScan_v2.h"

BOOLEAN GetBit(UINT32 num, UINT32 index)
{
    return ((num & (1 << index)) != 0);
}

UINT32 ClearBit(UINT32 num, UINT32 index)
{
    UINT32 mask = ~(1 << index);
    return num & mask;
}

UINT32 SetBit(UINT32 num, UINT32 index)
{
    return num | (1 << index);
}

EFI_STATUS BarExisted(IN EFI_PCI_IO_PROTOCOL *PciIo, IN UINTN Offset, OUT UINT32 *BarLengthValue, OUT UINT32 *OriginalBarValue)
{
    UINT32 OriginalValue;
    UINT32 Value;
    EFI_STATUS Status;

    Status = PciIo->Pci.Read(PciIo, EfiPciIoWidthUint32, (UINT8)Offset, 1, &OriginalValue);
    if (EFI_ERROR (Status))
        return Status;
    // Print(L"OriginalValue %08x\n", OriginalValue);

    Status = PciIo->Pci.Write(PciIo, EfiPciIoWidthUint32, (UINT8)Offset, 1, &gAllOne);
    if (EFI_ERROR (Status))
        return Status;
    // Print(L"Pci.Write gAllOne success\n");

    Status = PciIo->Pci.Read(PciIo, EfiPciIoWidthUint32, (UINT8)Offset, 1, &Value);
    if (EFI_ERROR (Status))
        return Status;

    // Print(L"Value %08x\n", Value);
    Status = PciIo->Pci.Write(PciIo, EfiPciIoWidthUint32, (UINT8)Offset, 1, &OriginalValue);
    if (EFI_ERROR (Status))
        return Status;

    if (BarLengthValue != NULL)
        *BarLengthValue = Value;

    if (OriginalBarValue != NULL)
        *OriginalBarValue = OriginalValue;

    if (Value == 0)
        return EFI_NOT_FOUND;
    else
        return EFI_SUCCESS;
}

UINTN PciParseBar(IN EFI_PCI_IO_PROTOCOL *PciIo, IN UINTN Offset)
{
    UINT32 Value = 0;
    UINT32 OriginalValue = 0;
    UINT32 Mask;
    EFI_STATUS Status;

    PCI_BAR BarData;
    Status = BarExisted(PciIo, Offset, &Value, &OriginalValue);
    if (EFI_ERROR (Status))
    {
        BarData.BaseAddress = OriginalValue;
        BarData.Length = Value;
        Print(L"MEM   32 0x%08x 0x%08x\n", BarData.BaseAddress, BarData.Length);
        return Offset + 4;
    }
    if (OriginalValue & 0x01)
    {
        // Device I/Os
        Mask = 0xfffffffc;
        if (Value & 0xFFFF0000)
        {
            // It is a IO32 bar
            BarData.Length = ((~(Value & Mask)) + 1);
        }
        else
        {
            // It is a IO16 bar
            BarData.Length = 0x0000FFFF & ((~(Value & Mask)) + 1);
        }
        BarData.Prefetchable = FALSE;
        BarData.BaseAddress = OriginalValue & Mask;
        Print(L"IO    32 0x%08x 0x%08x\n", BarData.BaseAddress, BarData.Length);
    }
    else
    {
        Mask = 0xfffffff0;
        BarData.BaseAddress = OriginalValue & Mask;
        switch ((Value & 0x07))
        {
        case 0x00:
            if(Value & 0x08){
                //1 XX X
                Print(L"PfMEM ");
            }else{
                //0 XX X
                Print(L"MEM   ");
            }
            // X 00 0
            //memory space; anywhere in 32 bit address space
            BarData.Length = (~(Value & Mask)) + 1;
            Print(L"32 0x%08x 0x%08x\n", BarData.BaseAddress, BarData.Length);
            break;
        case 0x04:
            // X 10 0
            // memory space; anywhere in 64 bit address space
            if (Value & 0x08)
            {
                //1 XX X
                Print(L"PfMEM ");
            }
            else
            {
                //0 XX X
                Print(L"MEM   ");
            }
            BarData.Length = Value & Mask;
            Offset += 4;
            Status = BarExisted(PciIo, Offset, &Value, &OriginalValue);
            if (EFI_ERROR (Status))
            {
                return Offset + 4;
            }

            Value |= ((UINT32)(-1) << HighBitSet32(Value));
            BarData.BaseAddress |= LShiftU64((UINT64)OriginalValue, 32);

            BarData.Length = BarData.Length | LShiftU64((UINT64)Value, 32);
            BarData.Length = (~(BarData.Length)) + 1;
            Print(L"64 0x%08x %08x 0x%08x %08x\n", BarData.BaseAddress >> 32, BarData.BaseAddress, BarData.Length >> 32, BarData.Length);
            break;
        default:
            // X 01/11 0
            // reserved
            BarData.Length = (~(Value & Mask)) + 1;
            Print(L"32 0x%08x 0x%08x\n", BarData.BaseAddress, BarData.Length);
            break;
        }
    }
    return Offset + 4;
}

BOOLEAN PrintIO(PCI_TYPE_GENERIC *PciData, UINT16 *IoMin, UINT16 *IoMax)
{
    if (GetBit(PciData->Bridge.Bridge.IoBase, 0) == 0)
        Print(L"IoBase 16 bits 0x%02x\n", PciData->Bridge.Bridge.IoBase);
    else
        Print(L"IoBase 32 bits 0x%04x\n", PciData->Bridge.Bridge.IoBase);

    if (GetBit(PciData->Bridge.Bridge.IoBase, 0) == 0)
        Print(L"IoLimit 16 bits 0x%02x\n", PciData->Bridge.Bridge.IoLimit);
    else
        Print(L"IoLimit 32 bits 0x%04x\n", PciData->Bridge.Bridge.IoLimit);

    Print(L"IoBaseUpper16 0x%04x\n", PciData->Bridge.Bridge.IoBaseUpper16);
    Print(L"IoLimitUpper16 0x%04x\n", PciData->Bridge.Bridge.IoLimitUpper16);
    *IoMin = (UINT16)(PciData->Bridge.Bridge.IoBase << 8);
    *IoMax = (UINT16)(PciData->Bridge.Bridge.IoLimit << 8 | 0XFFF);
    if (*IoMin < *IoMax)
    {
        Print(L"-> IO Range 0x%08x ~ 0x%08x\n", *IoMin, *IoMax);
        return TRUE;
    }
    else
    {
        Print(L"-> IO Range Not use\n");
        return FALSE;
    }
}

BOOLEAN PrintMem(PCI_TYPE_GENERIC *PciData, UINT32 *MemoryMin, UINT32 *MemoryMax)
{
    Print(L"MemoryBase 0x%04x\n", PciData->Bridge.Bridge.MemoryBase);
    Print(L"MemoryLimit 0x%04x\n", PciData->Bridge.Bridge.MemoryLimit);

    *MemoryMin = (UINT32)(PciData->Bridge.Bridge.MemoryBase << 16);
    *MemoryMax = (UINT32)((PciData->Bridge.Bridge.MemoryLimit << 16) | 0XFFFFF);
    if (*MemoryMin < *MemoryMax)
    {
        Print(L"-> Memory Range 0x%08x ~ 0x%08x\n", *MemoryMin, *MemoryMax);
        return TRUE;
    }
    else
    {
        Print(L"-> Memory Range Not use\n");
        return FALSE;
    }
}

BOOLEAN PrintPrefetchableMem(PCI_TYPE_GENERIC *PciData, UINT64 *PrefetchableMin, UINT64 *PrefetchableMax)
{
    if (GetBit(PciData->Bridge.Bridge.PrefetchableMemoryBase, 0) == 0)
    {
        Print(L"PrefetchableMemoryBase 0x%04x\n", PciData->Bridge.Bridge.PrefetchableMemoryBase);
    }
    else
    {
        Print(L"PrefetchableBaseUpper32 0x%08x\n", PciData->Bridge.Bridge.PrefetchableBaseUpper32);
        Print(L"PrefetchableMemoryBase 0x%08x\n", PciData->Bridge.Bridge.PrefetchableBaseUpper32 << 16 | PciData->Bridge.Bridge.PrefetchableMemoryBase);
    }

    if (GetBit(PciData->Bridge.Bridge.PrefetchableMemoryLimit, 0) == 0)
    {
        Print(L"PrefetchableMemoryLimit 0x%04x\n", PciData->Bridge.Bridge.PrefetchableMemoryLimit);
    }
    else
    {
        Print(L"PrefetchableLimitUpper32 0x%08x\n", PciData->Bridge.Bridge.PrefetchableLimitUpper32);
        Print(L"PrefetchableMemoryLimit 0x%08x\n", PciData->Bridge.Bridge.PrefetchableLimitUpper32 << 16 | PciData->Bridge.Bridge.PrefetchableMemoryLimit);
    }
    *PrefetchableMin = (UINT64)(PciData->Bridge.Bridge.PrefetchableBaseUpper32 << 32 | ClearBit(PciData->Bridge.Bridge.PrefetchableMemoryBase, 0) << 16);
    *PrefetchableMax = (UINT64)(PciData->Bridge.Bridge.PrefetchableLimitUpper32 << 32 | ClearBit(PciData->Bridge.Bridge.PrefetchableMemoryLimit, 0) << 16 | 0XFFFFF);
    if (*PrefetchableMin < *PrefetchableMax)
    {
        Print(L"-> PrefetchableMemory Range 0x%08x %08x ~ 0x%08x %08x\n",
              PciData->Bridge.Bridge.PrefetchableBaseUpper32, ClearBit(PciData->Bridge.Bridge.PrefetchableMemoryBase, 0) << 16,
              PciData->Bridge.Bridge.PrefetchableLimitUpper32, ClearBit(PciData->Bridge.Bridge.PrefetchableMemoryLimit, 0) << 16 | 0XFFFFF);
        return TRUE;
    }
    else
    {
        Print(L"-> PrefetchableMemory Range Not use\n");
        return FALSE;
    }
}

EFI_STATUS EFIAPI ReadPci(EFI_HANDLE *PciIoHandleBuffer, UINTN TotalDevice)
{
    EFI_STATUS Status;
    // PCI_IO_DEVICE *Pci;
    EFI_PCI_IO_PROTOCOL *PciIo;
    // Step 3. open the handle by gEfiPciIoProtocolGuid
    Status = gBS->HandleProtocol(PciIoHandleBuffer, &gEfiPciIoProtocolGuid, &PciIo);
    if (EFI_ERROR (Status))
        return Status;

    //--------------------------------------------------
    UINTN SegNum = 0, BusNum = 0, DevNum = 0, FunNum = 0;
    Status = PciIo->GetLocation(PciIo, &SegNum, &BusNum, &DevNum, &FunNum);
    if (EFI_ERROR (Status))
        return Status;
    Print(L"BusNum: %02x DevNum: %02x FunNum: %02x\n", BusNum, DevNum, FunNum);

    //--------------------------------------------------
    PCI_TYPE_GENERIC PciData;
    Status = PciIo->Pci.Read(PciIo, EfiPciWidthUint32, PCI_VENDOR_ID_OFFSET, sizeof(PCI_TYPE_GENERIC) / sizeof(UINT32), &PciData);
    if (EFI_ERROR (Status))
        return Status;
    Print(L"Vendor ID: 0x%04x Device ID: 0x%04x\n", PciData.Device.Hdr.VendorId, PciData.Device.Hdr.DeviceId);

    //--------------------------------------------------
    UINT8 HeaderType = 0;
    Status = PciIo->Pci.Read(PciIo, EfiPciIoWidthUint8, PCI_HEADER_TYPE_OFFSET, 1, &HeaderType);
    if (EFI_ERROR (Status))
        return Status;
    Print(L"HeaderType is 0x%02x\n", HeaderType);
    if (GetBit(HeaderType, 7) == 1)
        Print(L"It's Multi function\n");

    //--------------------------------------------------
    UINT32 Exp_ROM_BAR = 0;
    switch (GetBit(HeaderType, 0))
    {
    case HEADER_TYPE_DEVICE:
        // HEADER_TYPE_DEVICE 0x00
        Print(L"=> DEVICE\n");
        for (UINTN Offset = 0x10; Offset <= 0x24;)
        {
            Offset = PciParseBar(PciIo, Offset);
        }
        Exp_ROM_BAR = PciData.Device.Device.ExpansionRomBar;
        break;
    case HEADER_TYPE_PCI_TO_PCI_BRIDGE:
        // HEADER_TYPE_PCI_TO_PCI_BRIDGE 0x01
        Print(L"=> PCI_TO_PCI_BRIDGE\n");
        for (UINTN Offset = 0x10; Offset <= 0x14;)
        {
            Offset = PciParseBar(PciIo, Offset);
        }
        Print(L"PrimaryBus 0x%08x\n", PciData.Bridge.Bridge.PrimaryBus);
        Print(L"SecondaryBus 0x%08x\n", PciData.Bridge.Bridge.SecondaryBus);
        Print(L"SubordinateBus 0x%08x\n", PciData.Bridge.Bridge.SubordinateBus);
        //--------------------------------------------------
        UINT16 IoMin, IoMax;
        UINT32 MemoryMin, MemoryMax;
        UINT64 PrefetchableMin, PrefetchableMax;
        BOOLEAN IOUsed = PrintIO(&PciData, &IoMin, &IoMax);
        BOOLEAN MemUsed = PrintMem(&PciData, &MemoryMin, &MemoryMax);
        BOOLEAN PrefetchableMemUsed = PrintPrefetchableMem(&PciData, &PrefetchableMin, &PrefetchableMax);
        //--------------------------------------------------
        Exp_ROM_BAR = PciData.Bridge.Bridge.ExpansionRomBAR;
        //--------------------------------------------------
        // check conflict
        //1. 取的相同 bus 底下的 device
        //2. 判斷底下 device Io/Mem/PrintPrefetchableMem 有無超過
        UINT8 ParentSecondaryBus = PciData.Bridge.Bridge.SecondaryBus;
        UINT8 ParentSubordinateBus = PciData.Bridge.Bridge.SubordinateBus;

        for (UINT8 BusNumIndex = ParentSecondaryBus; BusNumIndex < ParentSubordinateBus; BusNumIndex++)
        {
            for (UINTN DeviceNumIndex = 0; DeviceNumIndex < TotalDevice; DeviceNumIndex++)
            {
                Status = gBS->HandleProtocol(PciIoHandleBuffer[DeviceNumIndex], &gEfiPciIoProtocolGuid, &PciIo);
                if (EFI_ERROR (Status))
                    return Status;
                Status = PciIo->GetLocation(PciIo, &SegNum, &BusNum, &DevNum, &FunNum);
                if (EFI_ERROR (Status))
                    return Status;
                if (BusNum == (UINTN)BusNumIndex)
                {
                    Print(L"*Next* BusNum: %02x DevNum: %02x FunNum: %02x\n", BusNum, DevNum, FunNum);
                    if (IOUsed)
                    {
                        UINT16 Temp_IoMin = PciData.Bridge.Bridge.IoBase << 8;
                        UINT16 Temp_IoMax = PciData.Bridge.Bridge.IoLimit << 8 | 0XFFF;
                        if (Temp_IoMax > IoMax)
                        {
                            Print(L"IO Conflict (Temp_IoMax 0x%08x IoMax 0x%08x)\n", Temp_IoMax, IoMax);
                        }
                        if (Temp_IoMin < IoMin)
                        {
                            Print(L"IO Conflict (Temp_IoMin 0x%08x IoMin 0x%08x)\n", Temp_IoMin, IoMin);
                        }
                    }
                    if (MemUsed)
                    {
                        UINT32 Temp_MemoryMin = PciData.Bridge.Bridge.MemoryBase << 16;
                        UINT32 Temp_MemoryMax = (PciData.Bridge.Bridge.MemoryLimit << 16) | 0XFFFFF;
                        if (Temp_MemoryMax > MemoryMax)
                        {
                            Print(L"Memoory Conflict (Temp_MemoryMax 0x%08x MemoryMax 0x%08x)\n", Temp_MemoryMax, MemoryMax);
                        }
                        if (Temp_MemoryMin < MemoryMin)
                        {
                            Print(L"Memoory Conflict (Temp_ITemp_MemoryMinoMin 0x%08x MemoryMin 0x%08x)\n", Temp_MemoryMin, MemoryMin);
                        }
                    }
                    if (PrefetchableMemUsed)
                    {
                        UINT64 Temp_PrefetchableMin = PciData.Bridge.Bridge.PrefetchableBaseUpper32 << 32 | ClearBit(PciData.Bridge.Bridge.PrefetchableMemoryBase, 0) << 16;
                        UINT64 Temp_PrefetchableMax = PciData.Bridge.Bridge.PrefetchableLimitUpper32 << 32 | ClearBit(PciData.Bridge.Bridge.PrefetchableMemoryLimit, 0) << 16 | 0XFFFFF;
                        if (Temp_PrefetchableMax > PrefetchableMax)
                        {
                            Print(L"PrefetchableMemory Conflict (Temp_PrefetchableMax 0x%08x PrefetchableMax 0x%08x)\n", Temp_PrefetchableMax, PrefetchableMax);
                        }
                        if (Temp_PrefetchableMin < PrefetchableMin)
                        {
                            Print(L"PrefetchableMemory Conflict (Temp_PrefetchableMin 0x%08x PrefetchableMin 0x%08x)\n", Temp_PrefetchableMin, PrefetchableMin);
                        }
                    }

                } // End If BusNum == (UINTN)BusNumIndex
            } // End DeviceNumIndex
        }// End BusNumIndex

        break;
    default:
        // HEADER_TYPE_CARDBUS_BRIDGE 0x02
        Print(L"=> CARDBUS_BRIDGE\n");
        break;
    }
    //--------------------------------------------------
    Print(L"ExpansionRomBar 0x%08x\n", Exp_ROM_BAR);
    if ((Exp_ROM_BAR & BIT0) == 0)
    {
        Print(L"Disable ExpansionRomBar\n");
    }
    else
    {
        Print(L"Enable ExpansionRomBar\n");
        PciParseBar(PciIo, PCI_EXPANSION_ROM_BASE);
    }
}

EFI_STATUS EFIAPI UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
    //Method 1: Using EFI_PCI_ROOT_BRIDGE_IO_PROTOCOL
    //Method 2: Using EFI_PCI_IO_PROTOCOL
    EFI_STATUS Status;
    UINTN TotalDevice;
    EFI_HANDLE *PciIoHandleBuffer;
    //Step 1, Find out all Handle with gEfiPciIoProtocolGuid
    Status = gBS->LocateHandleBuffer(ByProtocol, &gEfiPciIoProtocolGuid, NULL, &TotalDevice, &PciIoHandleBuffer);
    if (EFI_ERROR (Status))
        return Status;
    Print(L"Number of PCI : %d\n", TotalDevice);
    //Step 2, Traverse all the handle which have fined.
    Print(L"--------------------------------\n");
    for (UINTN index = 0; index < TotalDevice; index++)
    {
        Print(L"Device Index :%d\n", index);
        // pass TotalDevice is use to detect conflict if device is pci-to-pci bridge.
        ReadPci(PciIoHandleBuffer[index], TotalDevice);
        Print(L"\n");
    }
    // step 4, Release EFI_HANDLE
    if(PciIoHandleBuffer != NULL)
    {
        Status = gBS->FreePool(PciIoHandleBuffer);
    }
    return EFI_SUCCESS;
}