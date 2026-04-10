#pragma once

#include "Basics.h"
#include <string>
#include <sstream>
#include <stdexcept>
#include <cctype>

inline int parseReg(const std::string& s) {
    if (s.empty()) return 0;
    if (s[0] == 'x' || s[0] == 'X') return std::stoi(s.substr(1));
    return std::stoi(s);
}

inline Instruction stringToInstr(const std::string& str) {
    Instruction instr{};
    std::stringstream ss(str);
    std::string opStr;
    if (!(ss >> opStr)) {
        throw std::runtime_error("Empty instruction string");
    }

    for (char& c : opStr) {
        c = std::tolower(static_cast<unsigned char>(c));
    }

    if (opStr == "add") instr.op = OpCode::ADD;
    else if (opStr == "sub") instr.op = OpCode::SUB;
    else if (opStr == "addi") instr.op = OpCode::ADDI;
    else if (opStr == "mul") instr.op = OpCode::MUL;
    else if (opStr == "div") instr.op = OpCode::DIV;
    else if (opStr == "rem") instr.op = OpCode::REM;
    else if (opStr == "lw") instr.op = OpCode::LW;
    else if (opStr == "sw") instr.op = OpCode::SW;
    else if (opStr == "beq") instr.op = OpCode::BEQ;
    else if (opStr == "bne") instr.op = OpCode::BNE;
    else if (opStr == "blt") instr.op = OpCode::BLT;
    else if (opStr == "ble") instr.op = OpCode::BLE;
    else if (opStr == "j") instr.op = OpCode::J;
    else if (opStr == "slt") instr.op = OpCode::SLT;
    else if (opStr == "slti") instr.op = OpCode::SLTI;
    else if (opStr == "and") instr.op = OpCode::AND;
    else if (opStr == "or") instr.op = OpCode::OR;
    else if (opStr == "xor") instr.op = OpCode::XOR;
    else if (opStr == "andi") instr.op = OpCode::ANDI;
    else if (opStr == "ori") instr.op = OpCode::ORI;
    else if (opStr == "xori") instr.op = OpCode::XORI;
    else throw std::runtime_error("Unknown OpCode: " + opStr);

    if (instr.op == OpCode::J) {
        std::string target;
        ss >> target;
        instr.imm = std::stoi(target);
        return instr;
    }

    if (instr.op == OpCode::LW || instr.op == OpCode::SW) {
        std::string regStr, offsetStr;
        ss >> regStr >> offsetStr;
        if (instr.op == OpCode::LW) {
            instr.dest = parseReg(regStr);
        } else {
            instr.src2 = parseReg(regStr); // src2 is the data to store
        }
        
        size_t parenOpen = offsetStr.find('(');
        size_t parenClose = offsetStr.find(')');
        if (parenOpen != std::string::npos && parenClose != std::string::npos) {
            instr.imm = std::stoi(offsetStr.substr(0, parenOpen));
            std::string brStr = offsetStr.substr(parenOpen + 1, parenClose - parenOpen - 1);
            instr.src1 = parseReg(brStr);
        } else {
            // Assume format without parens if they are completely missing (unlikely after preprocessor, but safe)
            instr.src1 = 0;
            instr.imm = 0;
        }
        return instr;
    }

    if (instr.op == OpCode::BEQ || instr.op == OpCode::BNE || 
        instr.op == OpCode::BLT || instr.op == OpCode::BLE) {
        std::string s1, s2, immStr;
        ss >> s1 >> s2 >> immStr;
        instr.src1 = parseReg(s1);
        instr.src2 = parseReg(s2);
        instr.imm = std::stoi(immStr);
        return instr;
    }

    if (instr.op == OpCode::ADDI || instr.op == OpCode::ANDI ||
        instr.op == OpCode::ORI || instr.op == OpCode::XORI || 
        instr.op == OpCode::SLTI) {
        std::string rd, rs1, immStr;
        ss >> rd >> rs1 >> immStr;
        instr.dest = parseReg(rd);
        instr.src1 = parseReg(rs1);
        instr.imm = std::stoi(immStr);
        return instr;
    }

    // Default R-type: ADD, SUB, MUL, DIV, REM, SLT, AND, OR, XOR
    std::string rd, rs1, rs2;
    ss >> rd >> rs1 >> rs2;
    instr.dest = parseReg(rd);
    instr.src1 = parseReg(rs1);
    instr.src2 = parseReg(rs2);

    return instr;
}


