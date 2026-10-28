#!/bin/bash
# Run the pim.fence.flush test and its controls on the model run_fence_tests.sh built (no RTL change needed).
# Inside the cva6 container, from the repo root:  bash verif/tests/custom/pim/run_flush_tests.sh
set -e
cd "$(dirname "$0")/../../../.."
export RISCV="${RISCV:-$PWD/tools/riscv-toolchain}"
T=verif/tests/custom
OUT=work-pim
[ -x work-ver/Variane_testharness ] || { echo "no model in work-ver/: run run_fence_tests.sh first"; exit 1; }
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
  # </dev/null: a terminal read would freeze it under timeout. +tohost_addr: the testbench ends the test when the program stores its exit code there (as CI does)
  if timeout 600 work-ver/Variane_testharness $elf +debug_disable=1 +elf_file=$elf +tohost_addr=$tohost \
       +time_out=500000 < /dev/null > $OUT/$1.log 2>&1; then got=PASS; else got=FAIL; fi
  if [ -f trace_rvfi_hart_00.dasm ]; then mv trace_rvfi_hart_00.dasm $OUT/$1.dasm; fi
  printf '%-20s got %s, expected %s  %-14s %s\n' $1 $got $4 "$([ $got = $4 ] && echo ok || echo '<-- MISMATCH')" \
    "$(grep -o '\*\*\* [A-Z]* \*\*\* (tohost = [0-9]*' $OUT/$1.log | head -1)"
}

run fence_flush         pim_fence_flush.c ""           PASS
run fence_flush_nop     pim_fence_flush.c -DNOFENCE    FAIL  # expect tohost = 1
run fence_flush_cl_only pim_fence_flush.c -DCLEAN_ONLY FAIL  # control: clean without invalidate, expect tohost = 2
run fence_flush_nowait  pim_fence_flush.c -DNO_WAIT_FENCE FAIL  # first version, no fence after the read-back: failed 10-28 with tohost = 2
