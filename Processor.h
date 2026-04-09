#pragma once
#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <string>
#include "Basics.h"
#include "BranchPredictor.h"
#include "ExecutionUnit.h"
#include "LoadStoreQueue.h"

class Processor {
public:
    int pc;
    int clock_cycle;

    // pipeline registers

    std::vector<Instruction> inst_memory;

    // architectural state (do not change)
    std::vector<int> ARF; // regFile


    std::vector<int> Memory; // Memory
    bool exception = false; // exception bit
    std::vector<RSEntry> AddRS;
    std::vector<RSEntry> MulRS;
    std::vector<RATEntry> RAT; // register alias table
    std::vector<ROBEntry> ROB;


    // register alias table / reorder buffer

    std::vector<ExecutionUnit> units;
    LoadStoreQueue* lsq;
    BranchPredictor bp;

    Processor(ProcessorConfig& config) {
        pc = 0;
        clock_cycle = 0;
        
        Memory.resize(config.mem_size);

        ARF.resize(config.num_regs, 0);
        RAT.resize(config.num_regs);
        ROB.resize(config.rob_size);

        //arithmatic
        AddRS.resize(config.adder_rs_size);
        MulRS.resize(config.multiplier_rs_size);

        // Instantiate Hardware Units
        // Adder
        // Multiplier
        // Divider
        // Branch Computation
        // Bitwise Logic
        // Load-Store Unit
    }

    void loadProgram(const std::string& filename) {
        std::ifstream file(filename);
        std::string instr;

        std::string first_line;
        bool mem_initialised = false;
        std::getline(file,first_line);

        // check for memory allocation
        if(first_line.length()>8){
            std::string temp;
            temp = first_line.substr(0,8);
            // if first line starts with MEM_INIT
            if(temp == "MEM_INIT"){
                for(int j = 8; j<first_line.length(); j = j+2){
                    Memory.push_back(first_line[j]-48);     //convert char to int and push to memory
                }
                mem_initialised = true;
            }
        }
        // if not allocated, first line should be an instruction
        if(!mem_initialised){
            Instruction first_instr = stringToInstr(first_line);
            first_instr.pc = 0;
            inst_memory.push_back(first_instr);
        }
        while(std::getline(file,instr)){
            Instruction new_instr = stringToInstr(instr);
            new_instr.pc = inst_memory.size();
            inst_memory.push_back(new_instr);
        }
    }

    void flush() {// during exceptions
    //reload the full RAT from the ARF

    }; 

    void broadcastOnCDB() {}; 

    void stageFetch() {};

    void stageDecode() {};

    void stageExecuteAndBroadcast() {};

    void stageCommit() {};

    bool step() { //assuming no jump statements for now
        if(pc>=inst_memory.size())
        return false;
        
        
        //if RS and RAT is empty , then stageFetch
        clock_cycle++;
        return true; 
    }

    void dumpArchitecturalState() {
        std::cout << "\n=== ARCHITECTURAL STATE (CYCLE " << clock_cycle << ") ===\n";
        for (int i = 0; i < ARF.size(); i++) {
            std::cout << "x" << i << ": " << std::setw(4) << ARF[i] << " | ";
            if ((i+1) % 8 == 0) std::cout << std::endl;
        }
        if (exception) {
            std::cout << "EXCEPTION raised by instruction " << pc + 1 << std::endl;
        }
        std::cout << "Branch Predictor Stats: " << bp.correct_predictions << "/" << bp.total_branches << " correct.\n";
    }
};