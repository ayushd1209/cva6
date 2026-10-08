// PIM fence tests: stub registers (GPIO slot, see corev_apu/tb/ariane_testharness.sv) and fence macros
#include <stdint.h>

#define PIM_STUB_BASE 0x40000000UL
#define PIM_ADDR  (*(volatile uint64_t *)(PIM_STUB_BASE + 0x00))  // DRAM address the stub acts on
#define PIM_READ  (*(volatile uint64_t *)(PIM_STUB_BASE + 0x08))  // write: doorbell, read: what the stub saw
#define PIM_WRITE (*(volatile uint64_t *)(PIM_STUB_BASE + 0x18))  // write: stub stores the value into DRAM

// pim.fence.{cl,inv}: custom-0 (0x0b), funct3 1 / 2, address in rs1; -DNOFENCE swaps in a nop
#ifdef NOFENCE
#define PIM_FENCE_CL(p)  asm volatile("nop" ::: "memory")
#define PIM_FENCE_INV(p) asm volatile("nop" ::: "memory")
#else
#define PIM_FENCE_CL(p)  asm volatile(".insn i 0x0b, 1, x0, %0, 0" :: "r"(p) : "memory")
#define PIM_FENCE_INV(p) asm volatile(".insn i 0x0b, 2, x0, %0, 0" :: "r"(p) : "memory")
#endif
