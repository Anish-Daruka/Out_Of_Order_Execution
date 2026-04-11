#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <climits>
#include "Basics.h"

class ExecutionUnit {
public:
    UnitType name;
    int latency;
    std::vector<RSEntry*> RS; // reservation station entries for this unit
    std::vector<RSEntry*> in_flight; // pipelined: entries currently executing

    bool has_result = false;

    ExecutionUnit(UnitType name, int latency, int rs_size) : name(name), latency(latency) {
        RS.resize(rs_size, nullptr);
    }

    void capture(int tag, int value) {
        for(auto &rs_entry : RS) {
            if(rs_entry == nullptr) continue;
            if(rs_entry->tag1 == tag) { rs_entry->ready1 = true; rs_entry->value1 = value; }
            if(rs_entry->tag2 == tag) { rs_entry->ready2 = true; rs_entry->value2 = value; }
        }
    }

    int evaluate(RSEntry* entry) {
        if(entry == nullptr) return 0;

        // branch: compute actual next PC
        if(entry->op == OpCode::BEQ || entry->op == OpCode::BNE ||
           entry->op == OpCode::BLT || entry->op == OpCode::BLE){
            bool taken = false;
            if(entry->op == OpCode::BEQ) taken = (entry->value1 == entry->value2);
            else if(entry->op == OpCode::BNE) taken = (entry->value1 != entry->value2);
            else if(entry->op == OpCode::BLT) taken = (entry->value1 <  entry->value2);
            else if(entry->op == OpCode::BLE) taken = (entry->value1 <= entry->value2);
            return taken ? entry->imm : (entry->instr_pc + 1);
        }

        // div/rem by zero or INT_MIN/-1 overflow
        if((entry->op == OpCode::DIV || entry->op == OpCode::REM) && entry->value2 == 0){
            entry->exception = true;
            return 0;
        }
        if((entry->op == OpCode::DIV || entry->op == OpCode::REM) && entry->value1 == INT_MIN && entry->value2 == -1){
            if (entry->op == OpCode::DIV) {
                entry->exception = true;
                return 0;
            } else {
                return 0; // INT_MIN % -1 = 0
            }
        }

        long long res = 0;
        switch(entry->op) {
            case OpCode::ADD: case OpCode::ADDI: res = (long long)entry->value1 + entry->value2; break;
            case OpCode::SUB:  res = (long long)entry->value1 - entry->value2; break;
            case OpCode::MUL:  res = (long long)entry->value1 * entry->value2; break;
            case OpCode::DIV:  return entry->value1 / entry->value2;
            case OpCode::REM:  return entry->value1 % entry->value2;
            case OpCode::SLT: case OpCode::SLTI: return (entry->value1 < entry->value2) ? 1 : 0;
            case OpCode::AND: case OpCode::ANDI: return entry->value1 & entry->value2;
            case OpCode::OR:  case OpCode::ORI:  return entry->value1 | entry->value2;
            case OpCode::XOR: case OpCode::XORI: return entry->value1 ^ entry->value2;
            default: return 0;
        }
        // overflow check for arithmetic ops
        if(res > INT_MAX || res < INT_MIN) entry->exception = true;
        return (int)res;
    }
};
