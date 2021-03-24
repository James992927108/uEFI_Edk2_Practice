/*
 *  SpdRead.h
 *
 *  Created on: 2020年11月16日
 *      Author: Anthony Teng
 */

// MdePkg.dec has include -> folder [include]
// so want to use MdePkg/Include/Library/UefiBootServicesTableLib.h`
// example:
// #include <Library/UefiBootServicesTableLib.h>
// because in *.inf -> [Packages] MdePkg/MdePkg.dec -> and in *.dec -> [Includes] Include 
//                     MdePkg/                                         Include/
#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
// #include <Library/BaseMemoryLib.h>
// #include <Library/MemoryAllocationLib.h>
#include <Library/IoLib.h>

#include <Library/PciSegmentLib.h>

//
// Command List structure includes total 32 entries.
// The entry data structure is listed at the following.
//
typedef struct {
  UINT32   AhciCmdCfl:5;      //Command FIS Length
  UINT32   AhciCmdA:1;        //ATAPI
  UINT32   AhciCmdW:1;        //Write
  UINT32   AhciCmdP:1;        //Prefetchable
  UINT32   AhciCmdR:1;        //Reset
  UINT32   AhciCmdB:1;        //BIST
  UINT32   AhciCmdC:1;        //Clear Busy upon R_OK
  UINT32   AhciCmdRsvd:1;
  UINT32   AhciCmdPmp:4;      //Port Multiplier Port
  UINT32   AhciCmdPrdtl:16;   //Physical Region Descriptor Table Length
  UINT32   AhciCmdPrdbc;      //Physical Region Descriptor Byte Count
  UINT32   AhciCmdCtba;       //Command Table Descriptor Base Address
  UINT32   AhciCmdCtbau;      //Command Table Descriptor Base Address Upper 32-BITs
  UINT32   AhciCmdRsvd1[4]; 
} EFI_AHCI_COMMAND_LIST;

//
// This is a software constructed FIS.
// For data transfer operations, this is the H2D Register FIS format as 
// specified in the Serial ATA Revision 2.6 specification.
//
typedef struct {
  UINT8    AhciCFisType;
  UINT8    AhciCFisPmNum:4;
  UINT8    AhciCFisRsvd:1;
  UINT8    AhciCFisRsvd1:1;
  UINT8    AhciCFisRsvd2:1;
  UINT8    AhciCFisCmdInd:1;
  UINT8    AhciCFisCmd;
  UINT8    AhciCFisFeature;
  UINT8    AhciCFisSecNum;
  UINT8    AhciCFisClyLow;
  UINT8    AhciCFisClyHigh;
  UINT8    AhciCFisDevHead;
  UINT8    AhciCFisSecNumExp;
  UINT8    AhciCFisClyLowExp;
  UINT8    AhciCFisClyHighExp;
  UINT8    AhciCFisFeatureExp;
  UINT8    AhciCFisSecCount;
  UINT8    AhciCFisSecCountExp;
  UINT8    AhciCFisRsvd3;
  UINT8    AhciCFisControl;
  UINT8    AhciCFisRsvd4[4];
  UINT8    AhciCFisRsvd5[44];
} EFI_AHCI_COMMAND_FIS;

//
// ACMD: ATAPI command (12 or 16 bytes)
//
typedef struct {
  UINT8    AtapiCmd[0x10];
} EFI_AHCI_ATAPI_COMMAND;

//
// Physical Region Descriptor Table includes up to 65535 entries
// The entry data structure is listed at the following.
// the actual entry number comes from the PRDTL field in the command
// list entry for this command slot. 
//
typedef struct {
  UINT32   AhciPrdtDba;       //Data Base Address
  UINT32   AhciPrdtDbau;      //Data Base Address Upper 32-BITs
  UINT32   AhciPrdtRsvd;
  UINT32   AhciPrdtDbc:22;    //Data Byte Count
  UINT32   AhciPrdtRsvd1:9;
  UINT32   AhciPrdtIoc:1;     //Interrupt on Completion
} EFI_AHCI_COMMAND_PRDT;

//
// Command table data strucute which is pointed to by the entry in the command list
//
typedef struct {
  EFI_AHCI_COMMAND_FIS      CommandFis;       // A software constructed FIS.
  EFI_AHCI_ATAPI_COMMAND    AtapiCmd;         // 12 or 16 bytes ATAPI cmd.
  UINT8                     Reserved[0x30];
  EFI_AHCI_COMMAND_PRDT     PrdtTable[65535];     // The scatter/gather list for data transfer
} EFI_AHCI_COMMAND_TABLE;

//
// Received FIS structure
//
typedef struct {
  UINT8    AhciDmaSetupFis[0x1C];         // Dma Setup Fis: offset 0x00
  UINT8    AhciDmaSetupFisRsvd[0x04];
  UINT8    AhciPioSetupFis[0x14];         // Pio Setup Fis: offset 0x20
  UINT8    AhciPioSetupFisRsvd[0x0C];     
  UINT8    AhciD2HRegisterFis[0x14];      // D2H Register Fis: offset 0x40
  UINT8    AhciD2HRegisterFisRsvd[0x04];
  UINT64   AhciSetDeviceBitsFis;          // Set Device Bits Fix: offset 0x58
  UINT8    AhciUnknownFis[0x40];          // Unkonwn Fis: offset 0x60
  UINT8    AhciUnknownFisRsvd[0x60];      
} EFI_AHCI_RECEIVED_FIS; 

typedef struct {
  UINT32  Lower32;
  UINT32  Upper32;
} DATA_32;

typedef union {
  DATA_32   Uint32;
  UINT64    Uint64;
} DATA_64;

//
// Each PRDT entry can point to a memory block up to 4M byte
//
#define EFI_AHCI_MAX_DATA_PER_PRDT             0x400000

#define EFI_AHCI_FIS_REGISTER_H2D              0x27      //Register FIS - Host to Device
#define   EFI_AHCI_FIS_REGISTER_H2D_LENGTH     20 

//
// Port register
//
#define EFI_AHCI_PORT_START                    0x0100
#define EFI_AHCI_PORT_REG_WIDTH                0x0080
#define EFI_AHCI_PORT_CMD                      0x0018

//
//
//
typedef struct _EFI_ATA_COMMAND_BLOCK {
  UINT8 Reserved1[2];
  UINT8 AtaCommand;
  UINT8 AtaFeatures;
  UINT8 AtaSectorNumber;
  UINT8 AtaCylinderLow;
  UINT8 AtaCylinderHigh;
  UINT8 AtaDeviceHead;
  UINT8 AtaSectorNumberExp;
  UINT8 AtaCylinderLowExp;
  UINT8 AtaCylinderHighExp; 
  UINT8 AtaFeaturesExp;
  UINT8 AtaSectorCount;
  UINT8 AtaSectorCountExp;
  UINT8 Reserved2[6];
} EFI_ATA_COMMAND_BLOCK;

typedef struct _EFI_ATA_STATUS_BLOCK {
  UINT8 Reserved1[2];
  UINT8 AtaStatus;
  UINT8 AtaError;
  UINT8 AtaSectorNumber;
  UINT8 AtaCylinderLow;
  UINT8 AtaCylinderHigh;
  UINT8 AtaDeviceHead;
  UINT8 AtaSectorNumberExp;
  UINT8 AtaCylinderLowExp;
  UINT8 AtaCylinderHighExp; 
  UINT8 Reserved2;
  UINT8 AtaSectorCount;
  UINT8 AtaSectorCountExp;
  UINT8 Reserved3[6];
} EFI_ATA_STATUS_BLOCK;

// Class 1: PIO Data-In Commands
//
#define ATA_CMD_IDENTIFY_DRIVE                          0xec   ///< defined from ATA-3
//
// Class 4: DMA Command
//
#define ATA_CMD_READ_DMA_EXT                            0x25   ///< defined from ATA-6
#define ATA_CMD_WRITE_DMA_EXT                           0x35   ///< defined from ATA-6

///
/// *******************************************************
/// EFI_PCI_IO_PROTOCOL_OPERATION
/// *******************************************************
///
typedef enum {
  ///
  /// A read operation from system memory by a bus master.
  ///
  EfiPciIoOperationBusMasterRead,
  ///
  /// A write operation from system memory by a bus master.
  ///
  EfiPciIoOperationBusMasterWrite,
  ///
  /// Provides both read and write access to system memory by both the processor and a
  /// bus master. The buffer is coherent from both the processor's and the bus master's point of view.
  ///
  EfiPciIoOperationBusMasterCommonBuffer,
  EfiPciIoOperationMaximum
} EFI_PCI_IO_PROTOCOL_OPERATION;
