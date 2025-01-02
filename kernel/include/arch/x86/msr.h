#ifdef ARCH_X86_64
#ifndef MSR_H
#define MSR_H
#include <types.h>

#define IA32_EFER       0xC0000080
#define IA32_EFER_SCE   0x00000001
#define IA32_STAR       0xC0000081
#define IA32_LSTAR      0xC0000082
#define IA32_FMASK      0xC0000084

#define IA32_GS_BASE        0xC0000101
#define IA32_KERNEL_GS_BASE 0xC0000102

namespace arch::x86::msr {
/**
 * @brief Reads the value of a Model Specific Register (MSR).
 * 
 * Retrieves the current value stored in the specified MSR. MSRs are used
 * to control various CPU-specific features and configurations. Access to
 * MSRs typically requires privileged CPU modes.
 * 
 * @param msr The identifier of the Model Specific Register to read.
 * @return uint64_t The value read from the specified MSR.
 */
__PRIVILEGED_CODE uint64_t read(uint32_t msr);

/**
 * @brief Writes a value to a Model Specific Register (MSR).
 * 
 * Sets the specified MSR to the provided value. MSRs are used to control
 * various CPU-specific features and configurations. Writing to MSRs typically
 * requires privileged CPU modes.
 * 
 * @param msr The identifier of the Model Specific Register to write to.
 * @param value The value to be written to the specified MSR.
 */
__PRIVILEGED_CODE void write(uint32_t msr, uint64_t value);
} // namespace arch::x86::msr

#endif // MSR_H
#endif // ARCH_X86_64

