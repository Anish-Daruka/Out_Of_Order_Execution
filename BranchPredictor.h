#pragma once
#include "Basics.h"
#include <iostream>
#include <vector>
#include <map>
#include <unordered_map>


class BranchPredictor {
public:
    int total_branches = 0;
    int correct_predictions = 0;
    std::unordered_map<int, int> pc_state;

    int getPcState(int pc){
        if(pc_state.find(pc) == pc_state.end()){
            pc_state[pc] = 0;
        }
        return pc_state[pc];
    }

    int predict(int current_pc, Instruction inst) {
        int pc = current_pc +1;
        OpCode op = inst.op;
        if(op == OpCode::J) return current_pc + inst.imm;

        if(op == OpCode::BEQ || op == OpCode::BNE || op == OpCode::BLT || op == OpCode::BLE){
            int state = getPcState(current_pc);

            if(state <=1) return current_pc + inst.imm;
            else return pc;
        }
        
        else return pc;


    }

    void update(int pc, int actual_target, bool taken, bool was_correct) {
        total_branches++;
        if (was_correct) {
            correct_predictions++;
        }
        int state = getPcState(pc);

        if(taken){
            if (state !=0) pc_state[pc] = state-1;
        }
        else{
            if(state != 3) pc_state[pc] = state+1;
        }
    }
};