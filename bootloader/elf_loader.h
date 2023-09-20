#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include <efi.h>
#include <efilib.h>

EFI_STATUS LoadElfKernel(EFI_FILE* RootDir, CHAR16* FileName, VOID** EntryPoint, VOID** KernelBase);

#endif // ELF_LOADER_H