#pragma once
#include <iostream>
#include <vector>
#include <string>
#include "Basics.h"

class ExecutionUnit {
public:
    UnitType name;
    int latency;
    std::vector<RSEntry*> RS; // reservation station entries for this unit
    std::vector<RSEntry*> in_flight; // pipelined: entries currently executing

    bool has_result = false;
    // bool has_exception = false;

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
        if(entry->op == OpCode::DIV && entry->value2 == 0){
            entry->exception = true;
            return 0;
        }
        switch(entry->op) {
            case OpCode::ADD: case OpCode::ADDI: return entry->value1 + entry->value2;
            case OpCode::SUB:  return entry->value1 - entry->value2;
            case OpCode::MUL:  return entry->value1 * entry->value2;
            case OpCode::DIV:  return entry->value1 / entry->value2;
            case OpCode::REM:  return entry->value1 % entry->value2;
            case OpCode::SLT: case OpCode::SLTI: return (entry->value1 < entry->value2) ? 1 : 0;
            case OpCode::AND: case OpCode::ANDI: return entry->value1 & entry->value2;
            case OpCode::OR:  case OpCode::ORI:  return entry->value1 | entry->value2;
            case OpCode::XOR: case OpCode::XORI: return entry->value1 ^ entry->value2;
            default: return 0;
        }
    }
};
