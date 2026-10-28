// PIM offload tests: the stub's PIM port registers (corev_apu/tb/ariane_testharness.sv) and instruction macros
#include "pim_stub.h"

#define PIM_REG(off) (*(volatile uint64_t *)(PIM_STUB_BASE + (off)))
#define PIM_CMD_COUNT PIM_REG(0x38)  // commands the stub received
#define PIM_LOG_SEL   PIM_REG(0x40)  // log entry to read back (command number mod 16)
#define PIM_LOG_KIND  PIM_REG(0x48)  // 1 = pim.dispatch, 2 = pim.gemv
#define PIM_LOG_PA    PIM_REG(0x50)  // operand physical address (translated rs1)
#define PIM_LOG_DESC  PIM_REG(0x58)  // token[63:48] | asid[47:32] | payload[31:0]

#define KIND_DISPATCH 1
#define KIND_GEMV     2
#define DESC(tok, payload) (((uint64_t)(tok) << 48) | (uint64_t)(payload))
#define SHAPE(outputs, steps) (((uint64_t)(outputs) << 16) | (uint64_t)(steps))

// pim.dispatch rd, rs1, rs2: custom-0 (0x0b), funct3 3, R-type; funct7 = 1 sets bit 25, the completion bit
// pim.gemv rd, rs1, rs2: funct3 5; rs2 = num_outputs[31:16] | dot_steps[15:0]
// -DNOP swaps each for "li rd, 0" (no command), so the checks must fail
#ifdef NOP
#define PIM_INSN(f3, f7) "li %0, 0"
#else
#define PIM_INSN(f3, f7) ".insn r 0x0b, " #f3 ", " #f7 ", %0, %1, %2"
#endif

static inline uint64_t pim_dispatch(const void *addr, uint64_t num_macs) {
  uint64_t tok;
  asm volatile(PIM_INSN(3, 0) : "=r"(tok) : "r"(addr), "r"(num_macs) : "memory");
  return tok;
}
static inline uint64_t pim_dispatch_c(const void *addr, uint64_t num_macs) {
  uint64_t tok;
  asm volatile(PIM_INSN(3, 1) : "=r"(tok) : "r"(addr), "r"(num_macs) : "memory");
  return tok;
}
static inline uint64_t pim_gemv(const void *desc, uint64_t shape) {
  uint64_t tok;
  asm volatile(PIM_INSN(5, 0) : "=r"(tok) : "r"(desc), "r"(shape) : "memory");
  return tok;
}

// 1 if stub log entry i holds {kind, pa, desc}
static inline int log_is(uint64_t i, uint64_t kind, const void *pa, uint64_t desc) {
  PIM_LOG_SEL = i;
  asm volatile("fence" ::: "memory");
  return PIM_LOG_KIND == kind && PIM_LOG_PA == (uint64_t)pa && PIM_LOG_DESC == desc;
}
