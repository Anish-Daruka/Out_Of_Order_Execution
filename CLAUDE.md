# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.


## Important Instruction
Always follow the coding style already in the repository
If you are implementing anything, let it be minimal and try to make minimal changes to the existing codebase while implementing/debugging the functionality

## Build & Run

```bash
# Compile the simulator
make compile FILE=main.cpp

# Preprocess a test case (e.g., code1.txt → preprocessed/processed1.txt)
python3 preprocessor.py 1

# Run a test case directly (after preprocessing)
./main preprocessed/processed1.txt

# Run with a cycle limit
./main preprocessed/processed1.txt -cycles 100

# Diff against expected output
./main preprocessed/processed1.txt > out.txt && diff out.txt programs/ans1.txt
```

Test cases are `programs/code1.txt` through `programs/code7.txt`; expected outputs are `programs/ans1.txt` through `programs/ans7.txt`.

## Architecture Overview

This is a Tomasulo-style out-of-order 32-bit RISC-V processor simulator with precise exceptions. The pipeline has 4 stages: Fetch → Decode → Execute/Broadcast → Commit.

### Key Invariant
All architectural state changes (writes to `ARF`, `Memory`, exception raising, branch resolution) happen **only in the Commit stage**, in program order via the ROB head. Execution units produce results speculatively.

### File Structure
- **`Basics.h`** — Core data structures: `Instruction`, `ProcessorConfig`, `ROBEntry`, `RSEntry`, `RATEntry`, `OpCode` enum, `UnitType` enum.
- **`Processor.h`** — Main `Processor` class containing the full pipeline implementation. Owns `ARF`, `Memory`, `ROB`, `RAT`, `AddRS`, `MulRS`, `units` (vector of `ExecutionUnit`), `lsq`, and `bp`.
- **`ExecutionUnit.h`** — `ExecutionUnit` class (pipelined; one per unit type: Adder, Multiplier, Divider, Branch, Logic). Each unit has its own RS.
- **`LoadStoreQueue.h`** — `LoadStoreQueue` class. Executes **in-order only** (no out-of-order memory). Supports store-to-load forwarding.
- **`BranchPredictor.h`** — Per-instruction 2-bit saturating counter. States 0–1 predict taken, 2–3 predict not taken. Updated only at Commit.
- **`preprocessor.py`** — Two-pass Python preprocessor: resolves memory labels (`.A: 1 2 3`), branch/jump labels, strips comments. Outputs a flat instruction file with an optional `MEM_INIT ...` header line for memory initialization.
- **`main.cpp`** — Autograder entry point. Calls `loadProgram()`, loops `step()`, prints `dumpArchitecturalState()` and `Memory` contents.

### Instruction-to-Unit Mapping
| Unit | Instructions |
|------|-------------|
| Adder | `add`, `sub`, `addi`, `slt`, `slti` |
| Multiplier | `mul` |
| Divider | `div`, `rem` |
| Logic | `and`, `or`, `xor`, `andi`, `ori`, `xori` |
| Branch | `beq`, `bne`, `blt`, `ble` (latency = adder latency) |
| LoadStoreQueue | `lw`, `sw` |
| None (Fetch only) | `j` (unconditional jump, never dispatched to RS/ROB) |

### ROB / RAT Lifecycle
- `rob_head`/`rob_tail`/`rob_count` manage circular ROB indexing.
- `RAT[reg].valid` + `RAT[reg].ROB_tag` tracks in-flight register writes.
- On Decode: allocate ROB entry, allocate RS entry, update RAT.
- On CDB Broadcast: update ROB entry value + ready bit; wake up waiting RS entries.
- On Commit (ROB head ready): write to `ARF`, free ROB entry, invalidate RAT entry if still pointing to this ROB slot.

### Exceptions
- Detected on the last cycle of execution unit processing (overflow, div-by-zero, out-of-bounds memory).
- `has_exception` flag flows from unit → ROB entry → `Processor::exception` only at Commit of the faulting instruction.
- On exception commit: set `pc` to faulting instruction's PC, set `exception = true`, flush pipeline, halt.

### Branch Misprediction Flush
- Detected at Commit stage.
- Must flush ROB tail back to the branch, flush all RS entries, flush LSQ, and reconstruct RAT from ARF (or walk the remaining valid ROB entries).

### `x0` Register
Always zero. Any write committed to `x0` must be suppressed — `ARF[0]` stays 0.

### Preprocessed File Format
After preprocessing, the file has:
1. Optional first line: `MEM_INIT val1 val2 ...` (space-separated integers to fill `Memory` sequentially from index 0).
2. Remaining lines: one instruction per line, commas removed, labels replaced with integer PC values or memory offsets.
