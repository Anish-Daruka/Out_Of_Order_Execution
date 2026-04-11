#pragma once
#include <iostream>
#include <vector>
#include <string>
#include "Basics.h"

struct LSQEntry {
    int tag;
    OpCode op;
    int dest_reg;  // for LW
    int imm;
    int tag1 = -1; // tag for base register (src1)
    int tag2 = -1; // tag for store data (src2, SW only)
    bool ready1 = false;
    bool ready2 = false;
    int value1 = 0; // base register value
    int value2 = 0; // store data (SW)
    int address = 0;
    int result = 0;
    bool exception = false;
    bool executed = false; // true after broadcasting result
    int num_cycles_executed = 0;
};

class LoadStoreQueue {
public:
    int latency;
    int max_size;
    std::vector<LSQEntry> queue; // in program order: front = oldest

    LoadStoreQueue(int latency, int max_size) : latency(latency), max_size(max_size) {}

    bool canAccept() { return (int)queue.size() < max_size; }

    void addEntry(Instruction& instr, int tag, std::vector<RATEntry>& RAT, std::vector<int>& ARF) {
        LSQEntry entry;
        entry.tag = tag;
        entry.op = instr.op;
        entry.dest_reg = (instr.op == OpCode::LW) ? instr.dest : 0;
        entry.imm = instr.imm;

        // lookup base register (src1)
        if(RAT[instr.src1].tag != -1) {
            entry.tag1 = RAT[instr.src1].tag;
            entry.ready1 = false;
        } else if(RAT[instr.src1].valid) {
            entry.value1 = RAT[instr.src1].value;
            entry.ready1 = true;
        } else {
            entry.value1 = ARF[instr.src1];
            entry.ready1 = true;
        }

        // SW: lookup store data (src2)
        if(instr.op == OpCode::SW) {
            if(RAT[instr.src2].tag != -1) {
                entry.tag2 = RAT[instr.src2].tag;
                entry.ready2 = false;
            } else if(RAT[instr.src2].valid) {
                entry.value2 = RAT[instr.src2].value;
                entry.ready2 = true;
            } else {
                entry.value2 = ARF[instr.src2];
                entry.ready2 = true;
            }
        } else {
            entry.ready2 = true; // LW has no src2
        }

        queue.push_back(entry);
    }

    void capture(int tag, int value) {
        for(auto& entry : queue) {
            if(entry.tag1 == tag) { entry.ready1 = true; entry.value1 = value; }
            if(entry.tag2 == tag) { entry.ready2 = true; entry.value2 = value; }
        }
    }

    // execute the oldest non-executed entry (in-order); returns it if complete
    LSQEntry* executeCycle(std::vector<int>& Memory) {
        if(queue.empty()) return nullptr;

        LSQEntry* completed = nullptr;

        // Step 1: advance all in-flight (started, not yet executed) entries
        for(auto& entry : queue) {
            if(entry.num_cycles_executed == 0 || entry.executed) continue;
            entry.num_cycles_executed++;
            if(entry.num_cycles_executed >= latency) {
                entry.address = entry.value1 + entry.imm;
                if(entry.address < 0 || entry.address >= (int)Memory.size()) {
                    entry.exception = true;
                } else if(entry.op == OpCode::LW) {
                    int forwarded = Memory[entry.address];
                    for(auto& prev : queue) {
                        if(&prev == &entry) break;
                        if(prev.op == OpCode::SW && prev.address == entry.address)
                            forwarded = prev.value2;
                    }
                    entry.result = forwarded;
                } else {
                    entry.result = entry.value2;
                }
                entry.executed = true;
                completed = &entry;
            }
        }

        // Step 2: start one new entry per cycle (oldest first, in-order)
        // Next entry can start only after the predecessor has been running for >= 2 cycles
        for(int i = 0; i < (int)queue.size(); i++) {
            auto& entry = queue[i];
            if(entry.executed) continue;
            if(entry.num_cycles_executed > 0) continue; // already started
            if(i > 0 && !queue[i-1].executed && queue[i-1].num_cycles_executed == 0) break;
            if(entry.ready1 && entry.ready2) {
                entry.num_cycles_executed = 1;
                if(latency == 1) {
                    entry.address = entry.value1 + entry.imm;
                    if(entry.address < 0 || entry.address >= (int)Memory.size()) {
                        entry.exception = true;
                    } else if(entry.op == OpCode::LW) {
                        int forwarded = Memory[entry.address];
                        for(auto& prev : queue) {
                            if(&prev == &entry) break;
                            if(prev.op == OpCode::SW && prev.address == entry.address)
                                forwarded = prev.value2;
                        }
                        entry.result = forwarded;
                    } else {
                        entry.result = entry.value2;
                    }
                    entry.executed = true;
                    if(completed == nullptr) completed = &entry;
                }
            }
            break; // in-order: stop at first non-started entry (ready or not)
        }

        return completed;
    }

    void freeHead() {
        if(!queue.empty()) queue.erase(queue.begin());
    }
};
