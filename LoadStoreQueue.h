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

        // find oldest non-executed entry (all preceding must be executed already)
        LSQEntry* to_execute = nullptr;
        for(auto& entry : queue) {
            if(!entry.executed) {
                if(entry.ready1 && entry.ready2) to_execute = &entry;
                break; // only the oldest non-executed can run
            }
        }
        if(to_execute == nullptr) return nullptr;

        to_execute->num_cycles_executed++;
        if(to_execute->num_cycles_executed < latency) return nullptr;

        // final cycle: compute address and result
        to_execute->address = to_execute->value1 + to_execute->imm;

        if(to_execute->address < 0 || to_execute->address >= (int)Memory.size()) {
            to_execute->exception = true;
            to_execute->executed = true;
            return to_execute;
        }

        if(to_execute->op == OpCode::LW) {
            // store-to-load forwarding: find most recent preceding SW with same address
            int forwarded = Memory[to_execute->address];
            for(auto& entry : queue) {
                if(&entry == to_execute) break;
                if(entry.op == OpCode::SW && entry.address == to_execute->address)
                    forwarded = entry.value2;
            }
            to_execute->result = forwarded;
        } else { // SW
            to_execute->result = to_execute->value2;
        }

        to_execute->executed = true;
        return to_execute;
    }

    void freeHead() {
        if(!queue.empty()) queue.erase(queue.begin());
    }
};
