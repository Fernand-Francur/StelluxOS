#include <acpi/madt.h>
#include <serial/serial.h>
#include <arch/percpu.h>

namespace acpi {
madt g_madt;

madt& madt::get() {
    return g_madt;
}

void madt::init(acpi_sdt_header* acpi_madt_table) {
    madt_table* table = reinterpret_cast<madt_table*>(acpi_madt_table);

    // Print basic MADT table information
    serial::printf("MADT Table:\n");
    serial::printf("  LAPIC Address  : 0x%08x\n", table->lapic_address);
    serial::printf("  Flags          : 0x%08x\n", table->flags);

    uint8_t* entry = table->entries;
    uint8_t* table_end = reinterpret_cast<uint8_t*>(table) + table->header.length;

    while (entry < table_end) {
        uint8_t entry_type = *entry;
        uint8_t entry_length = *(entry + 1);

        switch (entry_type) {
        case MADT_DESCRIPTOR_TYPE_LAPIC: {
            // Check if maximum supported number of local apics has been tracked
            if (m_local_apics.size() >= MAX_SYSTEM_CPUS) {
                break;
            }

            lapic_desc* desc = reinterpret_cast<lapic_desc*>(entry);
            if (desc->flags & LAPIC_PROCESSOR_ENABLED_BIT) {
                m_local_apics.push_back(*desc);
            }
            break;
        }
        case MADT_DESCRIPTOR_TYPE_IOAPIC: {
            // ioapic_desc* desc = reinterpret_cast<ioapic_desc*>(entry);

            // serial::printf("IOAPIC Entry:\n");
            // serial::printf("  IOAPIC ID: %u\n", desc->ioapic_id);
            // serial::printf("  IOAPIC Address: 0x%08x\n", desc->ioapic_address);
            // serial::printf("  Global System Interrupt Base: %u\n", desc->global_system_interrupt_base);
            break;
        }
        default: {
            break;
        }
        }

        // Advance to the next entry
        entry += entry_length;
    }
}
} // namespace acpi
