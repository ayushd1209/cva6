// pim.fence.cl: dirty data must be in DRAM before the doorbell reaches the PIM.
// Pass (exit 0) = the stub read the new value from DRAM. Built with -DNOFENCE it must fail.
#include "pim_stub.h"

static volatile uint64_t buf[2] __attribute__((aligned(16))) = {0x0101010101010101ULL, 0};

int main(void) {
  const uint64_t new_val = 0x5a5a5a5a12345678ULL;

  (void)buf[0];                   // line into the cache
  buf[0] = new_val;               // dirty in the cache; DRAM still holds the old value
  PIM_FENCE_CL(&buf[0]);
  PIM_ADDR = (uint64_t)&buf[0];
  PIM_READ = 1;                   // doorbell: the stub reads DRAM now
  asm volatile("fence" ::: "memory");
  return PIM_READ == new_val ? 0 : 1;
}
