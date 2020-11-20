/*
 *  SpdRead.h
 *
 *  Created on: 2020年11月16日
 *      Author: Anthony Teng
 */

//MdePkg
#include <Include/Uefi.h>
#include <Include/Library/UefiBootServicesTableLib.h>
#include <Include/Library/IoLib.h>
#include <Include/Library/UefiLIb.h>
#include <IndustryStandard/Pci22.h> // for use #define PCI_XXXXX_OFFSET
//ShellPkg
#include <Include/Library/ShellLib.h> // .inf:ShellLib, fot use ShellPromptForResponse()
#include <Include/Library/ShellCommandLib.h> //.inf: ShellCommandLib, for use DumpHex()

#include "SMBusOffset.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

#define BASE_ADDRESS 0x80000000
#define CLEAN_ADDRESS 0xFF000000
