// pim.fence.inv: after the PIM writes DRAM, a load anywhere in the line must see the PIM's value.
// Loads buf[1], the other 8-byte half of the line, right after the fence (one asm block, back to back).
// Pass (exit 0) = it sees the stub's value. Built with -DNOFENCE it must fail (stale cached copy).
// -DSAME_WORD (control): uses buf[0], the same 8-byte word as the fence's address.
#include "pim_stub.h"

#ifdef SAME_WORD
#define OFF 0
#else
#define OFF 8
#endif
#define STR_(x) #x
#define STR(x) STR_(x)

static volatile uint64_t buf[2] __attribute__((aligned(16))) = {0x0101010101010101ULL, 0x0202020202020202ULL};

int main(void) {
  const uint64_t pim_val = 0xa5a5a5a587654321ULL;
  uint64_t v;

  (void)buf[0];                   // line into the cache (clean)
  PIM_ADDR  = (uint64_t)&buf[OFF / 8];
  PIM_WRITE = pim_val;            // the PIM writes DRAM; the cached copy is now stale
  asm volatile("fence" ::: "memory");
  (void)PIM_ADDR;                 // read back: the stub has finished its write (stands in for pim.wait)
#ifdef NOFENCE
  asm volatile("nop\n\tld %0, " STR(OFF) "(%1)" : "=r"(v) : "r"(&buf[0]) : "memory");
#else
  asm volatile(".insn i 0x0b, 2, x0, %1, 0\n\tld %0, " STR(OFF) "(%1)" : "=r"(v) : "r"(&buf[0]) : "memory");
#endif
  return v == pim_val ? 0 : 1;
}
