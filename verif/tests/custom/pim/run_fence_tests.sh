#!/bin/bash
# Build CVA6 (write-back HPDcache config) once, then run the PIM fence tests and their nop variants.
# Inside the cva6 container, from the repo root:  bash verif/tests/custom/pim/run_fence_tests.sh
# Add --no-build to reuse the last model.
set -e
cd "$(dirname "$0")/../../../.."
export RISCV="${RISCV:-$PWD/tools/riscv-toolchain}"
export VERILATOR_INSTALL_DIR="${VERILATOR_INSTALL_DIR:-$PWD/tools/verilator-v5.008}"
export PATH="$VERILATOR_INSTALL_DIR/bin:$PATH"
TARGET=cv64a6_imafdc_sv39_hpdcache_wb
T=verif/tests/custom
OUT=work-pim
mkdir -p $OUT

if [ "$1" != "--no-build" ]; then
  echo "building the model (log: $OUT/verilate.log) ..."
  make verilate verilator="verilator --no-timing" target=$TARGET NUM_JOBS=${NUM_JOBS:-8} > $OUT/verilate.log 2>&1 \
    || { tail -40 $OUT/verilate.log; exit 1; }
fi

# -fno-tree-loop-distribute-patterns: else -O2 turns memset's loop in syscalls.c into a call to memset itself
CC="$RISCV/bin/riscv32-unknown-elf-gcc -march=rv64gc -mabi=lp64d -static -mcmodel=medany -fvisibility=hidden \
    -nostdlib -nostartfiles -O2 -fno-tree-loop-distribute-patterns -g -Wl,--no-warn-rwx-segments -I$T/env -I$T/common -I$T/pim \
    -T config/gen_from_riscv_config/linker/link.ld $T/common/syscalls.c $T/common/crt.S"
NM=$RISCV/bin/riscv32-unknown-elf-nm

run() {  # name, source, extra flags, expected
  local elf=$OUT/$1.elf
  $CC $3 $T/pim/$2 -o $elf
  local tohost=$($NM -B $elf | grep -w tohost | cut -d' ' -f1)
  # </dev/null: a terminal read would freeze it under timeout. +tohost_addr: the testbench ends the test when the program stores its exit code there (as CI does)
  if timeout 600 work-ver/Variane_testharness $elf +debug_disable=1 +elf_file=$elf +tohost_addr=$tohost \
       +time_out=500000 < /dev/null > $OUT/$1.log 2>&1; then got=PASS; else got=FAIL; fi
  if [ -f trace_rvfi_hart_00.dasm ]; then mv trace_rvfi_hart_00.dasm $OUT/$1.dasm; fi
  printf '%-15s got %s, expected %s  %-14s %s\n' $1 $got $4 "$([ $got = $4 ] && echo ok || echo '<-- MISMATCH')" \
    "$(grep -o '\*\*\* [A-Z]* \*\*\* (tohost = [0-9]*' $OUT/$1.log | head -1)"
}

run fence_cl      pim_fence_cl.c  ""        PASS
run fence_cl_nop  pim_fence_cl.c  -DNOFENCE FAIL
run fence_inv     pim_fence_inv.c ""        PASS
run fence_inv_nop pim_fence_inv.c -DNOFENCE FAIL
run fence_inv_same pim_fence_inv.c -DSAME_WORD PASS  # control: same 8-byte word
