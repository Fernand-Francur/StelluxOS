#include "paging.h"

UINT64 GlobalAllocatedMemory = 0;
UINT64 GlobalAllocatedPageCount = 0;

UINT64 GetAllocatedMemoryCount() {
	return GlobalAllocatedMemory;
}

UINT64 GetAllocatedPageCount() {
	return GlobalAllocatedPageCount;
}

void* krequest_page() {
    EFI_PHYSICAL_ADDRESS page;
    EFI_STATUS status = uefi_call_wrapper(
        BS->AllocatePages, 4,
        AllocateAnyPages,
        EfiLoaderData,
        1,
        &page
    );

    if (EFI_ERROR(status)) {
        Print(L"Could not allocate page: %r\n", status);
        return NULL;
    }

	GlobalAllocatedMemory += PAGE_SIZE;
	GlobalAllocatedPageCount++;
    return (void*)page;
}

void _kvaddr_to_page_offsets(
    UINT64 addr,
    struct page_index_dict* dict
) {
    addr >>= 12;
    dict->pt_lvl_1 = addr & 0x1ff;
    addr >>= 9;
    dict->pt_lvl_2 = addr & 0x1ff;
    addr >>= 9;
    dict->pt_lvl_3 = addr & 0x1ff;
    addr >>= 9;
    dict->pt_lvl_4 = addr & 0x1ff;
}

void kmemset(void* base, uint8_t val, UINT64 size) {
    unsigned char* start = base;
    unsigned char* end = start + size;

    for (unsigned char* ptr = start; ptr < end; ptr++) {
        *ptr = val;
    }
}

void MapPages(void* vaddr, void* paddr, struct page_table* pml4) {
    struct page_index_dict pdict;
	_kvaddr_to_page_offsets((UINT64)vaddr, &pdict);

	struct page_table *PDL3 = NULL, *PDL2 = NULL, *PageTable = NULL;

	struct page_table_entry* PTE_L4 = &pml4->entries[pdict.pt_lvl_4];

	if (PTE_L4->present == 0) {
		PDL3 = (struct page_table*)krequest_page();
		kmemset(PDL3, 0, PAGE_SIZE);

		PTE_L4->present = 1;
		PTE_L4->read_write = 1;
		PTE_L4->page_frame_number = (UINT64)PDL3 >> 12;
	} else {
		PDL3 = (struct page_table*)((UINT64)PTE_L4->page_frame_number << 12);
	}

	struct page_table_entry* PTE_L3 = &PDL3->entries[pdict.pt_lvl_3];
	
	if (PTE_L3->present == 0) {
		PDL2 = (struct page_table*)krequest_page();
		kmemset(PDL2, 0, PAGE_SIZE);

		PTE_L3->present = 1;
		PTE_L3->read_write = 1;
		PTE_L3->page_frame_number = (UINT64)PDL2 >> 12;
	} else {
		PDL2 = (struct page_table*)((UINT64)PTE_L3->page_frame_number << 12);
	}

	struct page_table_entry* PTE_L2 = &PDL2->entries[pdict.pt_lvl_2];
	
	if (PTE_L2->present == 0) {
		PageTable = (struct page_table*)krequest_page();
		kmemset(PageTable, 0, PAGE_SIZE);

		PTE_L2->present = 1;
		PTE_L2->read_write = 1;
		PTE_L2->page_frame_number = (UINT64)PageTable >> 12;
	} else {
		PageTable = (struct page_table*)((UINT64)PTE_L2->page_frame_number << 12);
	}

	struct page_table_entry* PTE_L1 = &PageTable->entries[pdict.pt_lvl_1];
	PTE_L1->present = 1;
	PTE_L1->read_write = 1;
	PTE_L1->page_frame_number = (UINT64)paddr >> 12;
}
