# Out-of-Order Processor Implementation & Testing Map

This document serves as the master execution plan, detailing the implementation phases, the corresponding test cases for verification, and strategies for debugging hardware simulation.

## 1. Implementation Phases & Test Case Mapping

### Phase 1: Pre-processing & Instruction Parsing (The Compiler)
*   **Goal**: Properly read the RISC-V assembly files, parse memory initializations (e.g., `.A: 1 2 3`), strip comments/empty lines, and translate labels into usable offsets for branches and memory commands.
*   **Verification**: 
    *   All files (`code1.txt` through `code7.txt`) should be correctly read into `inst_memory` and `Memory` without throwing parsing errors.
    *   Verify by printing out the `inst_memory` vector and `Memory` layout upon initialization.

### Phase 2: Core Tomasulo Architecture (Arithmetic & Logic)
*   **Goal**: Implement the Register Alias Table (RAT), Reorder Buffer (ROB), Reservation Stations (RS), and functional Execution Units (Adder, Multiplier, Divider, Logic). Get the core Pipeline stages (Fetch, Decode, Execute/Broadcast, Commit) running continuously.
*   **Relevant Tests**:
    *   **`code1.txt`**: Tests sequential data dependencies (RAW - Read After Write hazards). Verifies that instructions wait in the RS until CDB broadcast wakes them up.
    *   **`code2.txt`**: Tests Write-After-Write (WAW) hazards. Multiple `add` instructions write to `x1`. Verifies that the RAT correctly shadows older registers and the ROB correctly commits them in sequential order.

### Phase 3: Exceptions & Halting
*   **Goal**: Implement bounds checking and mathematical rules (like Divide-by-Zero). Guarantee that the pipeline flushes and the processor halts correctly when a faulting instruction reaches the *Commit* stage (Precise Exceptions).
*   **Relevant Tests**:
    *   **`code5.txt`**: Performs various basic math operations before executing `div x6, x2, x3` where `x3 = 0`. Verifies that the exception flag is raised organically on the Execution block, propagated to the ROB, and halts the simulation strictly at retirement without committing invalid data.

### Phase 4: Control Flow, Branches & Branch Prediction
*   **Goal**: Add the Branch Predictor (2-bit saturating counter) and handling for jumps/branches. On mispredictions, completely flush the speculative executions from the pipeline (RS, LSQ, and ROB tail) and reset the correct `pc`.
*   **Relevant Tests**:
    *   **`code3.txt`**: Includes `j` (Unconditional jump), `blt`, and `ble`. Also includes memory basics.
    *   **`code4.txt`**: A standard `for`-loop mapping to `blt` at the bottom. Tests tight loop prediction handling. Predictor should train to "Taken" mostly.

### Phase 5: Memory Disambiguation & Load Store Queue (LSQ)
*   **Goal**: Enable Load/Store instructions to execute. Ensure the LSQ observes in-order memory computation semantics and features load-forwarding (where a pending store passes its data directly to a subsequent load looking at the same address).
*   **Relevant Tests**:
    *   **`code4.txt`**: Heavy array vector addition (`C[i] = A[i] + B[i]`). Heavily utilizes the LSQ with `lw` and `sw` tied together.
    *   **`code3.txt`**: Memory read/write loops interacting closely with conditional loops.

### Phase 6: Full Integration & Hardware Stress Testing
*   **Goal**: Combine everything. Pipeline must seamlessly handle massive Register Aliasing, structural hazards (RS/ROB fill ups), dense LSQ operations, and intense Branch Prediction training simultaneously without deadlocking.
*   **Relevant Tests**:
    *   **`code6.txt`**: Complex nested logic including `rem`, loops, memory tracking, and complex mathematical dependencies.
    *   **`code7.txt`**: Matrix Multiplication ($O(N^3)$ loops). Extremely heavy hardware usage. Tests structural hazard handling—if sizes are small, the pipeline should stall perfectly without data loss.

---

## 2. Verification & Debugging Strategy

### 2.1 Checking Correctness
For every `code(X).txt` file, there is an equivalent `ans(X).txt` file containing expected output.
*   **Automated Execution Check**: At the end of each test, the `main` loop prints the `Architectural State` and the fully modified `Memory`. Diffing this directly against `ans(X).txt` will ensure cycle-accurate memory precision.
*   **Cycle Counting Check**: Once logic is completely solid, check `processor.clock_cycle`. If the pipeline is functioning with maximum parallelism, the cycle count should decrease compared to a purely in-order processor.

### 2.2 Trace Logging Techniques (For Debugging)
When a test case fails (usually meaning the simulation hangs up or produces garbage data), instrument the pipeline stages:

1.  **Decode Dump**: Print `[Cycle] Decode: Inst <Name> tagged as ROB_ID=<id>`.
2.  **Execute Dump**: Print `[Cycle] ExecMsg: Unit <X> starting calculation for Tag=<id>`.
3.  **CDB Dump**: Print `[Cycle] Broadcast: Tag=<id> computed Value=<val>`. 
4.  **Commit Dump**: Print `[Cycle] Commit: Retiring ROB_ID=<id>. ARF[dest] updated to <val>.`
5.  **Stall Output**: If `Processor::step()` doesn't seem to be retiring any instructions for consecutive cycles, print a message `[Cycle] STALL - ROB Head: <status>, LSQ: <status>`. This usually reveals deadlocks resulting from missing broadcasts.

### 2.3 Edge Case Checklists
Keep an eye out for these subtle hardware bug triggers:
*   **The x0 register**: Attempting to commit a write to `x0` must be swallowed up and remain `0` forever to prevent invalid memory reads in standard ISA logic.
*   **Branch Misprediction Flush**: Be sure the `RAT` recovers the correct mapping. You cannot just clear the RAT; you must reconstruct it to the point *right before* the speculative branch execution (or clear it all out to map strictly to the ARF up to the last valid instruction).
*   **Stale Broadcasts**: Don't let an execution unit broadcast its tag more than once, or it might falsely wake up an instruction waiting on a reused tag in a subsequent loop. Wait, capture, clear RS entry.