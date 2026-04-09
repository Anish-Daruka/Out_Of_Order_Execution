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
    int ROB_tag;
    bool ready=false;
    int value;
    int dest; // Destination register
    OpCode op;
    bool has_exception = false;
    int address; // for load/store instructions
};

struct RSEntry {
    int RS_tag;
    int ROB_tag;
    OpCode op;
    int dest; // Destination register
    int src1; // Source register 1
    int src2; // Source register 2
    bool ready1 = false;
    bool ready2 = false;
    int value1 = 0;
    int value2 = 0;
};


struct RATEntry{
    bool valid = false;
    int RS_tag = -1; //RS tag
    int value = -1;
}