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
#ifndef __AHCI_READ_WRITE_H__
#define __AHCI_READ_WRITE_H__

#include "AhciMode.h"

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <IndustryStandard/Atapi.h>
#include <IndustryStandard/Pci.h>
#include <IndustryStandard/Atapi.h>
#include <Protocol/PciIo.h>
#include <Library/PciSegmentLib.h>
#include <Library/IoLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
// #include <Include/IndustryStandard/Pci22.h>  // Note: Include <Pci.h> for use #define PCI_XXXXX_OFFSET
#include <Library/MemoryAllocationLib.h>
///
/// The default PCH PCI bus number
///
#define DEFAULT_PCI_SEGMENT_NUMBER_PCH 0
#define DEFAULT_PCI_BUS_NUMBER_PCH 0
//
//  SATA Controller Registers (D23:F0)
//
#define PCI_DEVICE_NUMBER_PCH_SATA 23
#define PCI_FUNCTION_NUMBER_PCH_SATA 0
//
//  SATA Controller common Registers
//
#define R_SATA_CFG_AHCI_BAR 0x24

//define signature
#define ATADRIVE 0x00000101
#define ATAPIDRIVE 0xEB140101

typedef struct
{
  UINT32 det : 4; // 0x00 Device Detection (DET)
  UINT32 spd : 4; // 0x04 Current Interface Speed (SPD)
  UINT32 ipm : 4; // 0x08 Interface Power Management (IPM)
  UINT32 rsv;     //0x12 Reserved
} SATA_Status;

//Port Ctrl Register
typedef struct tagSM_PORT
{
  UINT32 clb;       // 0x00, command list base address, 1K-UINT8 aligned
  UINT32 clbu;      // 0x04, command list base address upper 32 bits
  UINT32 fb;        // 0x08, FIS base address, 256-UINT8 aligned
  UINT32 fbu;       // 0x0C, FIS base address upper 32 bits
  UINT32 is;        // 0x10, interrupt status
  UINT32 ie;        // 0x14, interrupt enable
  UINT32 cmd;       // 0x18, command and status
  UINT32 rsv0;      // 0x1C, Reserved
  UINT32 tfd;       // 0x20, task file data
  UINT32 sig;       // 0x24, signature
  SATA_Status ssts; // 0x28, SATA status (SCR0:SStatus)
  UINT32 sctl;      // 0x2C, SATA control (SCR2:SControl)
  UINT32 serr;      // 0x30, SATA error (SCR1:SError)
  UINT32 sact;      // 0x34, SATA active (SCR3:SActive)
  UINT32 ci;        // 0x38, command issue
  UINT32 sntf;      // 0x3C, SATA notification (SCR4:SNotification)
  UINT32 fbs;       // 0x40, FIS-based switch control
  UINT32 rsv1[11];  // 0x44 ~ 0x6F, Reserved
  UINT32 vendor[4]; // 0x70 ~ 0x7F, vendor specific
} SM_PORT;

//HBA Memory Registers
typedef struct tagSM_MEM
{
  // 0x00 - 0x2B, Generic Host Control
  UINT32 cap;     // 0x00, Host capability
  UINT32 ghc;     // 0x04, Global host control
  UINT32 is;      // 0x08, Interrupt status
  UINT32 pi;      // 0x0C, Port implemented
  UINT32 vs;      // 0x10, Version
  UINT32 ccc_ctl; // 0x14, Command completion coalescing control
  UINT32 ccc_pts; // 0x18, Command completion coalescing ports
  UINT32 em_loc;  // 0x1C, Enclosure management location
  UINT32 em_ctl;  // 0x20, Enclosure management control
  UINT32 cap2;    // 0x24, Host capabilities extended
  UINT32 bohc;    // 0x28, BIOS/OS handoff control and status

  // 0x2C - 0x9F, Reserved
  UINT8 rsv[0xA0 - 0x2C];

  // 0xA0 - 0xFF, Vendor specific registers
  UINT8 vendor[0x100 - 0xA0];

  // 0x100 - 0x10FF, Port control registers, SM_CMD_LIST
  SM_PORT ports_ctl_reg[1]; // 1 ~ 32
} SM_MEM;


typedef enum
{
  FIS_TYPE_REG_H2D = 0x27,   // Register FIS - host to device
  FIS_TYPE_REG_D2H = 0x34,   // Register FIS - device to host
  FIS_TYPE_DMA_ACT = 0x39,   // DMA activate FIS - device to host
  FIS_TYPE_DMA_SETUP = 0x41, // DMA setup FIS - bidirectional
  FIS_TYPE_DATA = 0x46,      // Data FIS - bidirectional
  FIS_TYPE_BIST = 0x58,      // BIST activate FIS - bidirectional
  FIS_TYPE_PIO_SETUP = 0x5F, // PIO setup FIS - device to host
  FIS_TYPE_DEV_BITS = 0xA1,  // Set device bits FIS - device to host
} FIS_TYPE;

typedef struct tagCFIS_REG
{
  // UINT32 0
  UINT8 fis_type; // FIS_TYPE_REG_H2D

  UINT8 pmport : 4; // Port multiplier
  UINT8 rsv0 : 3;   // Reserved
  UINT8 c : 1;      // 1: Command, 0: Control

  UINT8 command;  // Command register
  UINT8 featurel; // Feature register, 7:0

  // UINT32 1
  UINT8 lba_low;   // LBA low register, 7:0
  UINT8 lba_mid;   // LBA mid register, 15:8
  UINT8 lba_high;   // LBA high register, 23:16
  UINT8 device; // Device register

  // UINT32 2
  UINT8 lba_low_ex;     // LBA register, 31:24
  UINT8 lba_mid_ex;     // LBA register, 39:32
  UINT8 lba_high_ex;     // LBA register, 47:40
  UINT8 featurel_ex; // Feature register, 15:8

  // UINT32 3
  UINT8 sector_count;  // Count register, 7:0
  UINT8 sector_count_ex;  // Count register, 15:8
  UINT8 rsv1;     // Isochronous command completion
  UINT8 control; // Control register

  // UINT32 4
  UINT8 rsv2[4]; // Reserved
} CFIS_REG;


//Command Table
typedef struct tagPRDT_REG
{
  // DW0
  UINT32 dba;  // Data base address
  // DW1
  UINT32 dbau; // Data base address upper 32 bits
  // DW2
  UINT32 rsv0; // Reserved

  // DW3
  UINT32 dbc : 22; // 21:00 UINT8 count, 4M max
  UINT32 rsv1 : 9; // 30:22 Reserved
  UINT32 i : 1;    // 31:30 Interrupt on completion
} PRDT_REG;

typedef struct tagSM_CMD_TBL
{
  // 0x00 -> CFIS_REG
  UINT8 cfis[64]; // Command FIS (2 to 16 Dwords)

  // 0x40
  UINT8 acmd[16]; // ATAPI command, 12 or 16 UINT8s

  // 0x50
  UINT8 rsv[48]; // Reserved

  // 0x80
  PRDT_REG prdt[1]; // Physical region descriptor table entries, 0 ~ 65535
} SM_CMD_TBL;


//Port Command List
typedef struct tagSM_CMD_LIST
{
  // DW0
  UINT8 cfl : 5; // Command FIS length in UINT32S, 2 ~ 16
  UINT8 a : 1;   // ATAPI
  UINT8 w : 1;   // Write, 1: H2D, 0: D2H
  UINT8 p : 1;   // Prefetchable

  UINT8 r : 1;    // Reset
  UINT8 b : 1;    // BIST
  UINT8 c : 1;    // Clear busy upon R_OK
  UINT8 rsv0 : 1; // Reserved
  UINT8 pmp : 4;  // Port multiplier port

  UINT16 prdtl; // Physical region descriptor table length in entries

  // DW1
  UINT32 prdbc; // Physical region descriptor UINT8 count transferred

  // DW2, 3
  UINT32 ctba;  // Command table descriptor base address
  UINT32 ctbau; // Command table descriptor base address upper 32 bits

  // DW4 - 7
  UINT32 rsv1[4]; // Reserved
} SM_CMD_LIST;

#endif
