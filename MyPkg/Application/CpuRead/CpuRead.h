#ifndef __CPUREAD_H__
#define __CPUREAD_H__
#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/UefiLib.h>
#include <Register/Cpuid.h>

#define PRINT_BIT_FIELD(Variable, FieldName) \
  Print (L"%5a%42a: %x\n", #Variable, #FieldName, Variable.Bits.FieldName);

#define PRINT_VALUE(Variable, Description) \
  Print (L"%5a%42a: %x\n", #Variable, #Description, Variable);
#define BASIC_FUNC_ADDR 0x00000000
#define EXTENTED_FUNC_ADDR 0x80000000
#endif
