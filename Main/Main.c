// http://iorlvskyo.blogspot.com/2011/10/pci-configuration-space.html

#include <stdio.h>
#include <stdlib.h>

#include <AArch64/ProcessorBind.h>  // for use INTXX
#include <IndustryStandard/Pci22.h> // for use #define PCI_XXXXX_OFFSET
#include <Library/PciLib.h>         // for use PCI_LIB_ADDRESS, PciReadBuffer

#define PCI_CONFIG_ADDRESS 0xcf8
#define PCI_CONFIG_DATA 0xcfc

INT32 Read_Data(INT32 PciAddress)
{
    INT32 data;
    //put address into 0xcf8
    outpd(PCI_CONFIG_ADDRESS, PciAddress);
    //get the value from 0xcfc
    data = inpd(PCI_CONFIG_DATA);
    return data;
}

int main(int argc, char **argv)
{
    INT32 Bus_No, Dev_No, Fun_No, Val, PciAddress, Address2;
    INT32 total = 0;
    // INT32 i = 0;
    INT32 j = 0;
    UINT32 PciSpace[64];
    //the range of the Bus_No is 0xFF
    for (Bus_No = 0; Bus_No <= PCI_MAX_BUS; Bus_No++)
    {
        //the range of the Dev_No is 0x1F
        for (Dev_No = 0; Dev_No < PCI_MAX_DEVICE; Dev_No++)
        {
            //the range of the Fun_No is 0x07
            for (Fun_No = 0; Fun_No < PCI_MAX_FUNC; Fun_No++)
            {
                // PciAddress value is (0x80'Bus_No''Dev_No,Fun_No''Offset')
                // Bus_No has 8 bits , Dev_No has 5 bits , Fun_No has 3 bits , Offset has 8 hits
                // use I/O Mapped I/O mathod
                // PciAddress = 0x80000000+(Bus_No<<16)+(Dev_No<<11)+(Fun_No<<8)+Offset;
                PciAddress = 0x80000000 | ((Bus_No & 0xFF) << 16) | ((Dev_No & 0x1F) << 11) | ((Fun_No & 0x7) << 8) | PCI_VENDOR_ID_OFFSET;
                
                Val = Read_Data(PciAddress);

                if (Bus_No == 0 && Dev_No < 2)
                {
                    printf("%02x %02x\n", PciAddress, Val);
                    printf("----------------\n");
                }   
                if (Val != 0xffffffff)
                {
                    // printf("%02X \n", PciAddress);
                    for (j = 0; j < 64; j++)
                    {
                        // PciRead32 = Read 4 btye each time -> 4 * 64 = 256 (for pci), only loop 64 times is enough.
                        // j * 4 is j << 2
                        Address2 = PCI_LIB_ADDRESS(Bus_No, Dev_No, Fun_No, j * 4);
                        PciSpace[j] = PciRead32(Address2);
                    }
                    // printf("Bus_No: %02x Dev_No: %02x Fun_No: %02x Val: %04x\n", Bus_No, Dev_No, Fun_No, Val);
                    // PciAddress = 0x80000000 | ((Bus_No & 0xFF) << 16) | ((Dev_No & 0x1F) << 11) | ((Fun_No & 0x7) << 8) | PCI_CACHELINE_SIZE_OFFSET;
                    // Val = Read_Data(PciAddress);
                    // printf("Header type: %02x\n", Val);
                    // for (i = 0, j = 0; j < 64; j++)
                    // {
                    //     printf("%08X ", PciSpace[j]);
                    //     if ((j % 4) == 3)
                    //     {
                    //         printf("%02X \n", i);
                    //         i = i + 1;
                    //     }
                    // }
                    printf("\n");
                    total += 1;
                }
                
            }
        }
    }
    printf("Total: %d\n", total);
    return 0;
}