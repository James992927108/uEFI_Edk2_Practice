#include <Uefi.h>

EFI_STATUS UefiMain(IN EFI_HANDLE ImageHandle, IN EFI_SYSTEM_TABLE *SystemTable)
{
    _asm int 3;
    SystemTable->ConOut->OutputString(SystemTable->ConOut, L"[Uefi Version] Love uefi Hello World\n");
    return EFI_SUCCESS;
}