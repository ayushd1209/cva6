// pim.dispatch / pim.gemv reach the stub's PIM port with the right fields, and rd holds distinct tokens.
// Exit 0 = pass, else the number of the failed check. -DNOP (no instructions) must fail.
#include "pim_offload.h"

static uint64_t opnd[8] __attribute__((aligned(64)));   // dispatch operand in DRAM
static uint64_t gdesc[4] __attribute__((aligned(32)));  // stands in for a pim.gemv descriptor

int main(void) {
  uint64_t n0 = PIM_CMD_COUNT;
  uint64_t t0 = pim_dispatch(&opnd[0], 5);          // mid-group MAC: rd = 0, no token
  uint64_t t1 = pim_dispatch_c(&opnd[4], 7);        // closes the group: first token
  uint64_t t2 = pim_gemv(gdesc, SHAPE(3, 48));
  uint64_t t3 = pim_gemv(gdesc, SHAPE(128, 64));
  asm volatile("fence" ::: "memory");               // all four have left the store buffer

  if (t0 != 0) return 2;
  if (t1 == t2 || t1 == t3 || t2 == t3) return 3;  // distinct tokens
  if (t1 != 0 || t2 != 1 || t3 != 2) return 4;     // round-robin from reset
  if (PIM_CMD_COUNT != n0 + 4) return 5;            // exactly four commands
  if (!log_is(n0 + 0, KIND_DISPATCH, &opnd[0], DESC(0, 5))) return 6;
  if (!log_is(n0 + 1, KIND_DISPATCH, &opnd[4], DESC(t1, (1u << 31) | 7))) return 7;
  if (!log_is(n0 + 2, KIND_GEMV, gdesc, DESC(t2, SHAPE(3, 48)))) return 8;
  if (!log_is(n0 + 3, KIND_GEMV, gdesc, DESC(t3, SHAPE(128, 64)))) return 9;
  return 0;
}
