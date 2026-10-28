#!/bin/bash
# Is the stale-load bug CVA6's own? Runs the invalidate test with the standard cbo.inval, on a model built
# WITHOUT the store-buffer fix (README, "Is the bug CVA6's own?"). Inside the cva6 container, from the repo root:
#   bash verif/tests/custom/pim/run_cbo_check.sh work-ver-nofix
set -e
cd "$(dirname "$0")/../../../.."
export RISCV="${RISCV:-$PWD/tools/riscv-toolchain}"
MODEL=${1:-work-ver-nofix}
T=verif/tests/custom
OUT=work-pim
[ -x $MODEL/Variane_testharness ] || { echo "no model in $MODEL/"; exit 1; }
mkdir -p $OUT

# -fno-tree-loop-distribute-patterns: else -O2 turns memset's loop in syscalls.c into a call to memset itself
CC="$RISCV/bin/riscv32-unknown-elf-gcc -march=rv64gc -mabi=lp64d -static -mcmodel=medany -fvisibility=hidden \
    -nostdlib -nostartfiles -O2 -fno-tree-loop-distribute-patterns -g -Wl,--no-warn-rwx-segments -I$T/env -I$T/common -I$T/pim \
    -T config/gen_from_riscv_config/linker/link.ld $T/common/syscalls.c $T/common/crt.S"
NM=$RISCV/bin/riscv32-unknown-elf-nm

run() {  # name, source, extra flags, expected
  local elf=$OUT/$1.elf
  $CC $3 $T/pim/$2 -o $elf
  local tohost=$($NM -B $elf | grep -w tohost | cut -d' ' -f1)
  rm -f trace_rvfi_hart_00.dasm
  if timeout 600 $MODEL/Variane_testharness $elf +debug_disable=1 +elf_file=$elf +tohost_addr=$tohost \
       +time_out=500000 < /dev/null > $OUT/$1.log 2>&1; then got=PASS; else got=FAIL; fi
  if [ -f trace_rvfi_hart_00.dasm ]; then mv trace_rvfi_hart_00.dasm $OUT/$1.dasm; fi
  printf '%-20s got %s, expected %s  %-14s %s\n' $1 $got $4 "$([ $got = $4 ] && echo ok || echo '<-- MISMATCH')" \
    "$(grep -o '\*\*\* [A-Z]* \*\*\* (tohost = [0-9]*' $OUT/$1.log | head -1)"
}

# expected values are for the model WITHOUT the fix
run nofix_cbo_inval      pim_cbo_inval.c  ""         FAIL  # standard cbo.inval reads the stale copy too
run nofix_cbo_inval_same pim_cbo_inval.c  -DSAME_WORD PASS  # control: same 8-byte word
run nofix_fence_inv      pim_fence_inv.c  ""         FAIL  # the 10-08 result, for comparison
