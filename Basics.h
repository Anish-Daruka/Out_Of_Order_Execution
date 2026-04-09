#pragma once
#include <string>

enum class OpCode { ADD, SUB, ADDI, MUL, DIV, REM, LW, SW, BEQ, BNE, BLT, BLE, J, SLT, SLTI, AND, OR, XOR, ANDI, ORI, XORI };
enum class UnitType { ADDER, MULTIPLIER, DIVIDER, LOADSTORE, BRANCH, LOGIC };

struct Instruction {
    OpCode op;
    int dest;
    int src1;
    int src2;
    int imm;
    int pc;
};

struct ProcessorConfig {
    int num_regs = 32;
    int rob_size = 64;
    int mem_size = 1024;

    int logic_lat = 1;
    int add_lat = 2;
    int mul_lat = 4;
    int div_lat = 5;
    int mem_lat = 4;

    int logic_rs_size = 4;
    int adder_rs_size = 4;
    int mult_rs_size = 2;
    int div_rs_size = 2;
    int br_rs_size = 2;
    int lsq_rs_size = 32;
};

struct ROBEntry {
    // valid bit, ready bit, architectural register ID
    // other fields as required
    bool valid = false;
    bool ready = false;
    int arch_reg_id = -1;
    int value = 0;
    int pc = -1;

    bool exception_found = false;

    bool is_branch = false;
    bool mispredict = false;
    int corrected_pc = -1;
};

struct RSEntry {
    // value, tag, ready ... for both operands
    // other fields as required\

    bool busy = false;
    OpCode op;
    int val1 ,val2 = 0;
    int tag1 = -1, tag2 = -1;

    int dest_rob_tag = -1;
    int latency_counter = 0;
    int imm = 0;
    int pc = -1;
    int mem_addr = 0; 
    
    
};


struct RATEntry{
    bool
}