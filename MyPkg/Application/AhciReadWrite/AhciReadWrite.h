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
// #include <Library/IoLib.h>

#include <Library/PciSegmentLib.h>

