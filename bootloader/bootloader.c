#include "elf_loader.h"
#include "memory_map.h"
#include "gop_setup.h"
#include "paging.h"

#define KERNEL_VIRTUAL_BASE 0xFFFF800000000000

struct KernelEntryParams {
    void* GopFramebufferBase;
    void* GopFramebufferSize;
    unsigned int GopFramebufferWidth;
    unsigned int GopFramebufferHeight;
    unsigned int GopPixelsPerScanLine;
};

struct page_table* create_pml4(uint64_t TotalSystemMemory, VOID* KernelPhysicalBase, uint64_t KernelSize, VOID* GopBufferBase, UINTN GopBufferSize) {
    // Allocate page tables
    struct page_table *pml4 = (struct page_table*)krequest_page();
    kmemset(pml4, 0, PAGE_SIZE);

    // Identity map the first 2GB of memory
    for (uint64_t i = 0; i < TotalSystemMemory; i += PAGE_SIZE) {
        MapPages((void*)i, (void*)i, pml4);
    }

    // Map the kernel to a higher half
    for (uint64_t i = 0; i < KernelSize; i += PAGE_SIZE) {
        void* paddr = (void*)(i + (uint64_t)KernelPhysicalBase);
        void* vaddr = (void*)(i + (uint64_t)KERNEL_VIRTUAL_BASE);

        MapPages(vaddr, paddr, pml4);
        Print(L"Mapping 0x%llx --> 0x%llx\n\r", vaddr, paddr);
    }

    // Identity mapping the GOP buffer
    for (uint64_t i = (uint64_t)GopBufferBase; i < (uint64_t)GopBufferBase + GopBufferSize; i += PAGE_SIZE) {
        MapPages((void*)i, (void*)i, pml4);
    }

    return pml4;
}

uint64_t GetTotalSystemMemory(
    EFI_MEMORY_DESCRIPTOR* MemoryMap,
    uint64_t Entries,
    uint64_t DescriptorSize
) {
    uint64_t TotalMemory = 0;

    for (uint64_t i = 0; i < Entries; ++i) {
        EFI_MEMORY_DESCRIPTOR* desc =
            (EFI_MEMORY_DESCRIPTOR*)((uint64_t)MemoryMap + (i * DescriptorSize));

        TotalMemory += desc->NumberOfPages * PAGE_SIZE;
    }

    return TotalMemory;
}

EFI_STATUS efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable) {
    InitializeLib(ImageHandle, SystemTable);
    Print(L"Stellux Bootloader - V%u.%u DEBUG ON\n\r", 0, 1);

    EFI_STATUS Status;
    EFI_HANDLE* Handles;
    UINTN HandleCount;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* FileSystem;

    // Locate handle(s) that support EFI_SIMPLE_FILE_SYSTEM_PROTOCOL
    Status = uefi_call_wrapper(BS->LocateHandleBuffer, 5, ByProtocol,
                &FileSystemProtocol, NULL, &HandleCount, &Handles);

    if (EFI_ERROR(Status)) {
        Print(L"Error locating file system: %r\n\r", Status);
        return Status;
    }

    // Usually, the file system is on the first handle, additional error checking is advised for production
    Status = uefi_call_wrapper(BS->HandleProtocol, 3, Handles[0],
                &FileSystemProtocol, (VOID**)&FileSystem);

    if (EFI_ERROR(Status)) {
        Print(L"Error obtaining file system: %r\n\r", Status);
        return Status;
    }

    // Open the root volume
    EFI_FILE* RootDir;
    Status = uefi_call_wrapper(FileSystem->OpenVolume, 2, FileSystem, &RootDir);
    if (EFI_ERROR(Status)) {
        Print(L"Error opening root volume: %r\n\r", Status);
        return Status;
    }

    VOID* EntryPoint = NULL;
    VOID* KernelBase = NULL;
    uint64_t KernelSize;
    Status = LoadElfKernel(RootDir, L"kernel.elf", &EntryPoint, &KernelBase, &KernelSize);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to load kernel.\n\r");
        return Status;
    }
    
    EFI_GRAPHICS_OUTPUT_PROTOCOL* Gop;
    Status = InitializeGOP(ImageHandle, &Gop);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to initialize GOP.\n\r");
        return Status;
    }

    // Initialize params
    struct KernelEntryParams params;
    params.GopFramebufferBase = (void*)Gop->Mode->FrameBufferBase;
    params.GopFramebufferSize = (void*)Gop->Mode->FrameBufferSize;
    params.GopFramebufferWidth = Gop->Mode->Info->HorizontalResolution;
    params.GopFramebufferHeight = Gop->Mode->Info->VerticalResolution;
    params.GopPixelsPerScanLine = Gop->Mode->Info->PixelsPerScanLine;

    // Cast the physical entry point to a function pointer
    void (*KernelEntryPoint)(struct KernelEntryParams*) = ((__attribute__((sysv_abi)) void(*)(struct KernelEntryParams*))((uint64_t)EntryPoint));
    
    // Check if kernel load was successful
    if (KernelEntryPoint == NULL) {
        Print(L"Kernel entry point is NULL. Exiting.\n\r");
        return -1;
    }

    Print(L"Kernel entry is at 0x%llx\n\r", KernelEntryPoint);

    // Read system memory map
    EFI_MEMORY_DESCRIPTOR* MemoryMap = NULL;
    UINTN MMapSize, MMapKey;
    UINTN DescriptorSize;
    UINT32 DescriptorVersion;

    // First call will just give us the map size
    uefi_call_wrapper(
        SystemTable->BootServices->GetMemoryMap,
        5,
        &MMapSize,
        MemoryMap,
        &MMapKey,
        &DescriptorSize,
        &DescriptorVersion
    );

    // Allocate enough space for the memory map
    MemoryMap = AllocateZeroPool(MMapSize);

    // Actually read in the memory map
    uefi_call_wrapper(
        SystemTable->BootServices->GetMemoryMap,
        5,
        &MMapSize,
        MemoryMap,
        &MMapKey,
        &DescriptorSize,
        &DescriptorVersion
    );

    uint64_t DescriptorCount = MMapSize / DescriptorSize;
    uint64_t TotalSystemMemory = GetTotalSystemMemory(MemoryMap, DescriptorCount, DescriptorSize);
    Print(L"Total System Memory: 0x%llx\n\r", TotalSystemMemory);

    struct page_table* pml4 = create_pml4(TotalSystemMemory, KernelBase, KernelSize, (void*)Gop->Mode->FrameBufferBase, (UINTN)Gop->Mode->FrameBufferSize);
    if (pml4 == NULL) {
        Print(L"Error occured while creating page table\n\r");
        return -1;
    }
    Print(L"Page Table PML4 Created\n\r");
    Print(L"%llu pages allocated for the page table\n\r", GetAllocatedPageCount());
    Print(L"Page table size: %llu KB\n\r", GetAllocatedMemoryCount() / 1024);

    // Actually read in the memory map
    uefi_call_wrapper(
        SystemTable->BootServices->GetMemoryMap,
        5,
        &MMapSize,
        MemoryMap,
        &MMapKey,
        &DescriptorSize,
        &DescriptorVersion
    );

    // Exit boot services.
    Status = gBS->ExitBootServices(ImageHandle, MMapKey);
    if (EFI_ERROR(Status)) {
        Print(L"Failed to exit boot services\n\r");
        return Status;
    }

    // Load PML4 address into CR3
    asm volatile ("mov %0, %%cr3" : : "r"((uint64_t)pml4));

    // Transfer control to the kernel
    KernelEntryPoint(&params);

    return EFI_SUCCESS;
}
