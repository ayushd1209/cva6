# PIM fence on CVA6

This directory holds the RTL validation of `pim.fence` for the MSc thesis
"RISC-V Host for Asynchronous DRAM-PIM" (TU Dresden, 2026).

## What was built

The core config is `cv64a6_imafdc_sv39_hpdcache_wb` (RV64, write-back HPDcache).

- **Decoder (`core/decoder.sv`).** Custom-0 opcode with the address in rs1:
  - funct3 1 = `pim.fence.cl` (clean)
  - funct3 2 = `pim.fence.inv` (invalidate)
  - funct3 4 = `pim.fence.flush` (clean + invalidate)

  All three reuse the core's Zicbom CBO path through the store unit. The other
  funct3 values (`pim.wait`, `pim.dispatch`, `pim.gemv`) stay illegal: they are
  not in RTL.
- **Store-buffer fix (`core/store_buffer.sv`).** A load must not overtake a
  pending CBO anywhere in the CBO's cache line. Before the fix the check
  compared 8-byte blocks (bits [11:3]), but a line is 16 bytes in this config.
  A load to the other half of the line could overtake a pending invalidate and
  read stale data, which breaks the RISC-V CMO ordering rule. CBO entries now
  compare bits [11:4]. Normal stores still compare [11:3].
- **PIM stub (`corev_apu/tb/ariane_testharness.sv`, testbench only).** It sits
  in the unused GPIO slot at 0x4000_0000 and reads and writes DRAM directly,
  past the CPU caches. A fence that doesn't really clean or invalidate
  therefore fails the test. Registers are listed in `pim_stub.h`.

## Tests

Each fence test has a twin that replaces the fence with a nop. The twin must
fail, which shows the test can catch a missing fence. Exit 0 = pass.

| Test | What it checks | Expected |
|---|---|---|
| `fence_cl` | After `pim.fence.cl` and the doorbell, the stub reads the CPU's new value from DRAM | PASS |
| `fence_cl_nop` | Same with a nop | FAIL |
| `fence_inv` | After the stub writes DRAM, a load to the *other* 8-byte half of the line right after `pim.fence.inv` sees the stub's value | PASS (FAIL before the store-buffer fix) |
| `fence_inv_nop` | Same with a nop | FAIL |
| `fence_inv_same` | Control: the same check on the same 8-byte word | PASS |
| `fence_flush` | Both checks on one line: the clean (exit 1 if missing) and the invalidate (exit 2 if missing) | PASS |
| `fence_flush_nop` | Same with a nop | FAIL, tohost = 1 |
| `fence_flush_cl_only` | Control: `pim.fence.cl` in place of `.flush`, so the invalidate is missing | FAIL, tohost = 2 |
| `fence_flush_nowait` | The first version of `fence_flush`, which has no fence after the read-back | FAIL, tohost = 2 |

`fence_flush_nowait` is kept as a record of a test bug, not a hardware bug.
RVWMO lets the final load go before the read-back of the stub, so it can read
DRAM before the stub writes. Software that waits for the PIM and then reads the
PIM's results needs that ordering. Here a `fence` provides it; `pim.wait` would
have to.

`run_fence_tests.sh` runs the first five and `run_flush_tests.sh` runs the
last four. Each run leaves `<test>.log` and `<test>.dasm` in `work-pim/`; the
`.dasm` file is the retired-instruction trace.

## Is the bug CVA6's own?

Yes. On the core without the store-buffer fix, the standard Zicbom
`cbo.inval` (0x0005200f) reads the same stale copy as `pim.fence.inv`
(`pim_cbo_inval.c`, a copy of `pim_fence_inv.c` with the standard instruction).
In M-mode the decoder turns both into the same CBO_INVAL store-unit operation,
so the custom decode adds nothing to the bug.

| Test (core without the fix) | Expected | Got (2026-10-28) |
|---|---|---|
| `nofix_cbo_inval` | FAIL, tohost = 1 | FAIL, tohost = 1: `ld 8(a0)` returns 0x0202… after `cbo.inval` |
| `nofix_cbo_inval_same` | PASS (control: same 8-byte word) | PASS |
| `nofix_fence_inv` | FAIL, tohost = 1 (the 10-08 result) | FAIL, tohost = 1 |

Build the core without the fix into its own directory, restore the fix, run:

```sh
git show f7e3c457f:core/store_buffer.sv > core/store_buffer.sv
make verilate verilator="verilator --no-timing" target=cv64a6_imafdc_sv39_hpdcache_wb NUM_JOBS=8 ver-library=work-ver-nofix
git checkout HEAD -- core/store_buffer.sv
bash verif/tests/custom/pim/run_cbo_check.sh work-ver-nofix
```

(inside the container, with `RISCV`, `VERILATOR_INSTALL_DIR` and `PATH` set as in `run_fence_tests.sh`).

## Commits (branch `pim-ext`)

| Commit | Content | Result |
|---|---|---|
| `f7e3c457f` | Decoder, PIM stub, first five tests | `fence_inv` FAILS: the CVA6 bug |
| `1a4f2b887` | Store-buffer fix | All five as expected |
| `e627f085f` | `cva6.Dockerfile` | |

## Reproduce

Run these from a fresh clone. Ubuntu 24.04 (GCC 13) is used because it matches
CVA6's CI; a newer host GCC is too new for the pinned Verilator 5.008 and Spike.

```sh
git clone --recursive -b pim-ext git@github.com:ayushd1209/cva6.git ~/cva6
cd ~/cva6
docker build -t cva6-env -f cva6.Dockerfile .
docker run -it --name cva6 --user $(id -u):$(id -g) -e HOME=/tmp -v ~/cva6:/cva6 cva6-env bash
```

Inside the container, the first time only, install the toolchain, Verilator
and Spike. The last line also runs CVA6's own baseline tests (expect
`6 PASSED, 0 FAILED`):

```sh
export RISCV=/cva6/tools/riscv-toolchain NUM_JOBS=8
export DV_SIMULATORS=veri-testharness,spike DV_TARGET=cv64a6_imafdc_sv39_hpdcache_wb
bash ci/install-toolchain.sh && bash verif/regress/cv64a6_imafdc_tests.sh
```

Then run the tests. The first script builds the model:

```sh
bash verif/tests/custom/pim/run_fence_tests.sh
bash verif/tests/custom/pim/run_flush_tests.sh
```

To see the bug, check out `f7e3c457f` and run `run_fence_tests.sh` again. It
rebuilds the model, and `fence_inv` fails.

## Traps

- The testbench ends a test only when the program stores to `tohost`, so the
  scripts pass `+tohost_addr` (see `corev_apu/tb/rvfi_tracer.sv`).
- A timeout ends a run as `tohost = 2147483647`. A failed check is the
  program's exit code.
- Under `timeout`, the sim needs `< /dev/null`. Otherwise it stops on a
  terminal read and the log stays empty.
- At -O2, `syscalls.c` needs `-fno-tree-loop-distribute-patterns`. Otherwise
  memset calls itself.
