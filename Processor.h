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
    int seq_num=0; // tag for ROB_entry ,always incremented whenever instruction added to RS or ROB

    // pipeline registers
    std::vector<Instruction> inst_memory;
    std::vector<ExecutionUnit> ExecutionUnits; // list of execution units

    // architectural state (do not change)
    std::vector<int> ARF; // regFile

    std::vector<int> Memory; // Memory
    bool exception = false; // exception bit
    std::vector<RATEntry> RAT; // register alias table
    int fetch_instr_idx = -1; // index of the instruction in the inst_memory will be in fetch stage in next cycle
    int decode_instr_idx = -1; // index of the instruction in the inst_memory that will be in decode stage in next cycle


    LoadStoreQueue* lsq = nullptr;
    BranchPredictor bp;

    Processor(ProcessorConfig& config);

    void loadProgram(const std::string& filename);

    // -----Helper functions------
    void flush();
    void broadcastOnCDB(RSEntry* entry);
    ExecutionUnit& getExecutionUnitForOp(OpCode op);

    // --- Pipeline Stages ---
    void stageFetch();
    void stageDecode();
    void stageExecuteAndBroadcast();
    void stageCommit();

    // --- ROB Helpers ---
    int rob_head = 0;
    int rob_tail = 0;
    int rob_count = 0;
    std::vector<ROBEntry> ROB;
    ROBEntry& getROBHead();
    ROBEntry& getROBbyTag(int tag);
    bool isROBFull();
    void pushToROB(OpCode op, int tag,int dest_reg, int value);
    void popROBHead();
    void commitInstruction(ROBEntry& entry);



    bool step();
    void dumpArchitecturalState();
};