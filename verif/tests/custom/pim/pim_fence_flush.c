// pim.fence.flush = clean + invalidate. Checks both on one line:
// exit 1 = the stub's DRAM read missed the CPU's new value (clean failed),
// exit 2 = a later load missed the stub's DRAM write (invalidate failed). Pass = exit 0.
// -DNOFENCE swaps in a nop (must fail, exit 1); -DCLEAN_ONLY uses pim.fence.cl (must fail, exit 2).
#include "pim_stub.h"

// pim.fence.flush: custom-0 (0x0b), funct3 4, address in rs1
#if defined(NOFENCE)
#define PIM_FENCE_FLUSH(p) asm volatile("nop" ::: "memory")
#elif defined(CLEAN_ONLY)
#define PIM_FENCE_FLUSH(p) asm volatile(".insn i 0x0b, 1, x0, %0, 0" :: "r"(p) : "memory")
#else
#define PIM_FENCE_FLUSH(p) asm volatile(".insn i 0x0b, 4, x0, %0, 0" :: "r"(p) : "memory")
#endif

static volatile uint64_t buf[2] __attribute__((aligned(16))) = {0x0101010101010101ULL, 0x0202020202020202ULL};

int main(void) {
  const uint64_t new_val = 0x5a5a5a5a12345678ULL;
  const uint64_t pim_val = 0xa5a5a5a587654321ULL;

  (void)buf[0];                   // line into the cache
  buf[0] = new_val;               // dirty in the cache; DRAM still holds the old value
  PIM_FENCE_FLUSH(&buf[0]);
  PIM_ADDR = (uint64_t)&buf[0];
  PIM_READ = 1;                   // doorbell: the stub reads DRAM now
  asm volatile("fence" ::: "memory");
  if (PIM_READ != new_val) return 1;

  PIM_ADDR  = (uint64_t)&buf[1];
  PIM_WRITE = pim_val;            // the PIM writes the other half of the line in DRAM
  asm volatile("fence" ::: "memory");
  (void)PIM_ADDR;                 // read back: the stub has finished its write (stands in for pim.wait)
#ifndef NO_WAIT_FENCE
  asm volatile("fence" ::: "memory");  // else RVWMO lets the load below go before the read-back
#endif
  return buf[1] == pim_val ? 0 : 2;
}
