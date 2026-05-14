# Implementation Notes — COL216 A2 Tomasulo Processor Simulator

This document describes the design choices and implementation approach taken for each component of the out-of-order processor simulator.

---

## Overall Pipeline Structure

The pipeline has four stages: Fetch, Decode, Execute/Broadcast, and Commit. Only one instruction can occupy Fetch or Decode at any time. Multiple instructions can be in-flight across different execution units simultaneously. Commit is strictly in-order via the ROB head.

Stages are processed each cycle in **reverse pipeline order** — Commit first, then Execute/Broadcast, then Decode, then Fetch. This ordering ensures that results produced by Execute this cycle can immediately wake up RS entries and be visible to Decode in the same cycle, which matches Tomasulo semantics without needing an extra forwarding pass.

The ROB head pop is deferred to the **end** of the cycle (after all stages run), so every stage sees a consistent ROB during its work window.

---

## Fetch Stage

The Fetch stage reads the instruction at `fetch_instr_idx` and moves it into the Decode slot. If Decode is already occupied, Fetch stalls.

For branches, the branch predictor is consulted and the predicted next PC is stored alongside the instruction in the Decode slot. This predicted PC is later written into the ROB entry at Decode so that Commit can compare it against the actual outcome.

Unconditional jumps (`j`) are resolved entirely at Fetch — the PC is redirected immediately and no execution unit is involved.

---

## Decode Stage

Decode allocates one ROB entry and one RS entry per instruction. If either the ROB is full or the target unit's RS is full, Decode stalls until space is available.

Operand values are looked up using a three-way priority: if the RAT entry has a pending tag (a prior in-flight instruction writes this register), the RS entry records that tag and waits; if the RAT holds a forwarded value (written by a completed-but-not-yet-committed instruction), that value is used directly; otherwise the value is read from the ARF.

The RAT is updated at Decode to point to the new ROB tag for the destination register. Branches and stores do not update the RAT since they produce no register result.

---

## Execution Units & Reservation Stations

Each functional unit (Adder, Multiplier, Divider, Logic, Branch) owns its own RS. Each cycle, the unit picks the **oldest ready entry** (the one with the smallest `seq_num` tag) to issue to its pipeline, ensuring age-ordered fairness.

Units are fully pipelined — they can have multiple instructions in-flight simultaneously, one per pipeline stage, up to their latency depth. Results are computed on the final pipeline cycle and queued for CDB broadcast in the same cycle they complete. RS slots are freed one cycle after broadcast to avoid aliasing between a result being read and the slot being reused.

Latency-1 units (Logic) complete in the same cycle they start, so their result is placed directly on the broadcast list without going through the in-flight queue.

---

## CDB Broadcast

When an instruction completes, its result is broadcast on the Common Data Bus. This does three things simultaneously:

1. Updates the corresponding ROB entry with the result and marks it ready.
2. Clears the RAT tag for the destination register and writes the forwarded value in-place so future Decode lookups can read it.
3. Wakes up all waiting RS entries and LSQ entries that had a dependency tag matching this result.

Multiple units can broadcast in the same cycle since a CDB of sufficient width is assumed.

---

## Load/Store Queue

The LSQ maintains memory operations in strict program order (FIFO). This is necessary because memory address aliasing cannot be resolved as cleanly as register aliasing via the RAT.

Execution within the LSQ is in-order: only the oldest non-started entry may begin executing each cycle, and it can only start once its predecessor has already begun (preventing two entries from starting in parallel). Once an entry has computed its address and result, it broadcasts on the CDB like any other unit.

**Store-to-load forwarding** is handled transparently within the LSQ: when a load's address matches a preceding store in the queue, the load reads the store's data value directly rather than going to memory. This forwarding happens at the end of the load's execution latency and does not reduce the latency.

Stores do not broadcast on the CDB — their address and value are recorded in their ROB entry and the actual memory write happens only at Commit. This is essential for precise semantics: a store becomes visible to memory only when it is the oldest instruction and known non-speculative.

---

## Commit Stage

Commit checks the ROB head. If it is not ready, the stage stalls. If ready:

- **Register-writing instructions**: write the result to `ARF`, then clear the RAT entry (only if it still points to this ROB tag — a newer instruction may have already taken ownership of that register).
- **Stores**: perform the actual `Memory` write using the address and value recorded in the ROB entry.
- **Branches**: compare the actual next PC (computed by the branch unit) against the predicted PC stored in the ROB entry. If they differ, a flush is triggered and the PC is corrected. If they match, only the branch predictor state is updated.
- **Exceptions**: if the ROB head carries an exception flag, `pc` is set to the faulting instruction's PC, the exception bit is raised, the pipeline is flushed, and simulation halts. The exception is never acted on before the faulting instruction reaches the head, ensuring preceding instructions are fully committed first.

The `x0` register is protected at commit — any write targeting register 0 is silently suppressed.

---

## Branch Misprediction Flush

On detecting a misprediction at Commit:

1. The RAT is rebuilt entirely from the current `ARF`. At this point all instructions older than the branch have already been committed, so the ARF represents the correct architectural state up to and including the branch.
2. All ROB entries are cleared and the ROB head/tail/count are reset.
3. All RS entries across every unit are freed (heap memory is released).
4. The LSQ is cleared.
5. The Decode slot is cleared.
6. `fetch_instr_idx` is set to the correct next PC.
7. A `flushed_this_cycle` flag is set to prevent Fetch from running again in the same cycle and fetching a stale instruction.

---

## Branch Predictor

A per-PC 2-bit saturating counter is used. The initial state (for any PC seen for the first time) is 0, which predicts taken. States 0–1 predict taken; states 2–3 predict not taken.

The predictor is updated **only at Commit**, not at Execute. This is important: if a branch is later flushed as mispredicted, we do not want a speculative update to have already corrupted the predictor state.

---

## Precise Exceptions

Exceptions are detected at the last cycle of an instruction's execution (e.g., overflow, divide-by-zero, out-of-bounds memory access) and the exception flag is set in the RS entry at that point. This flag flows to the ROB entry during CDB broadcast. However, the processor-level exception bit is only set when the faulting instruction reaches the ROB head at Commit.

This guarantees that no younger (speculative) instruction can cause an observable side-effect before the older faulting instruction is retired, which is the definition of precise exceptions.

---

## Preprocessor

A two-pass Python preprocessor handles all source-level translation before the C++ simulator sees the file:

- **Pass 1**: scans for memory label declarations and branch/jump labels, building a symbol table with resolved addresses.
- **Pass 2**: emits the flat instruction file with labels replaced by integer offsets, commas and comments stripped, and an optional `MEM_INIT` header line carrying the initial memory contents.

This clean separation means the C++ code only ever sees a simple, uniform instruction format and never has to deal with symbolic names or source-level syntax.

---

## Key Invariant

All architectural state changes — writes to registers, writes to memory, exception signalling, branch correction — happen **only in the Commit stage**, in program order. Everything in Fetch, Decode, and Execute is speculative and can be discarded by a flush.
