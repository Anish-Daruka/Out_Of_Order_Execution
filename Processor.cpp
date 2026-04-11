#include "Processor.h"
#include <climits>
#include <algorithm>
#include "utils.h"

//initializing the processor
Processor::Processor(ProcessorConfig& config) {
    pc = 0;
    clock_cycle = 0;
    fetch_instr_idx = 0; // start fetching instruction 0

    Memory.resize(config.mem_size, 0);
    ARF.resize(config.num_regs, 0);
    RAT.resize(config.num_regs);
    ROB.resize(config.rob_size);

    // Instantiate Execution Units
    ExecutionUnits.push_back(ExecutionUnit(UnitType::ADDER,      config.add_lat,    config.adder_rs_size));
    ExecutionUnits.push_back(ExecutionUnit(UnitType::MULTIPLIER, config.mul_lat,    config.mult_rs_size));
    ExecutionUnits.push_back(ExecutionUnit(UnitType::DIVIDER,    config.div_lat,    config.div_rs_size));
    ExecutionUnits.push_back(ExecutionUnit(UnitType::BRANCH,     config.add_lat,    config.br_rs_size));
    ExecutionUnits.push_back(ExecutionUnit(UnitType::LOGIC,      config.logic_lat,  config.logic_rs_size));

    //Instantiate LSQ
    lsq = new LoadStoreQueue(config.mem_lat, config.lsq_rs_size);

}

// load the program from the input file into instruction memory
void Processor::loadProgram(const std::string& filename) {
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
            std::istringstream iss(first_line.substr(9));
            int val, idx = 0;
            while(iss >> val && idx < (int)Memory.size()){
                Memory[idx++] = val;
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

//flush during exceptions, reload the RAT from the ARF
void Processor::flush() {
    //reload the full RAT from the ARF
    for(int i = 0; i < (int)RAT.size(); i++){
        RAT[i].valid = true;
        RAT[i].tag = -1;
        RAT[i].value = ARF[i];
    }

    //clear the ROB and RS
    for(auto &entry : ROB){
        entry.free = true;
        entry.ready = false;
        entry.tag = -1;
    }

    for(auto &unit : ExecutionUnits){
        for(auto &rs_entry : unit.RS){
            if(rs_entry != nullptr){
                delete rs_entry;
                rs_entry = nullptr;
            }
        }
        unit.in_flight.clear();
        unit.pending_free.clear();
    }
    if(lsq) lsq->queue.clear();

    // reset ROB pointers and decode stage
    for(auto &entry : ROB){ entry.free = true; entry.ready = false; entry.tag = -1; }
    rob_head = rob_tail = rob_count = 0;
    decode_instr_idx = -1;
    flushed_this_cycle = true;
}

//broadcast the result on the CDB to update the RS and ROB entries waiting for that result
void Processor::broadcastOnCDB(RSEntry* entry) {
    int tag = entry->tag;
    int value = entry->result;

    // update ROB entry
    ROBEntry& rob_entry = getROBbyTag(tag);
    rob_entry.value = value;
    rob_entry.ready = true;
    if(entry->exception) rob_entry.exception = true;

    // update RAT: clear the tag so future dispatches read from ARF or RAT.value
    for(int i = 0; i < (int)RAT.size(); i++){
        if(RAT[i].tag == tag){
            RAT[i].valid = true;
            RAT[i].tag = -1;
            RAT[i].value = value;
        }
    }

    // wake up waiting RS entries in all units and LSQ
    for(auto &unit : ExecutionUnits){
        unit.capture(tag, value);
    }
    if(lsq) lsq->capture(tag, value);
}



// --- Pipeline Stages ---

void Processor::stageFetch() {
    if(fetch_instr_idx == -1) return; // no instruction to fetch, stall

    Instruction& instr = inst_memory[fetch_instr_idx];
    if(instr.op == OpCode::J)//Jump completes in fetch stage itself
    {
        pc = instr.imm;
        if(pc < (int)inst_memory.size())
            fetch_instr_idx = pc;
        else
            fetch_instr_idx = -1; 
        return;
    }

    if(decode_instr_idx != -1) return; // decode stage is busy, stall
    // move the fetched instruction to decode stage
    decode_instr_idx = fetch_instr_idx;

    //decide the next instruction to be fetched in the next cycle
    if(instr.op == OpCode::BEQ || instr.op == OpCode::BNE ||
       instr.op == OpCode::BLT || instr.op == OpCode::BLE){
        pc = bp.predict(fetch_instr_idx, instr);
        decode_predicted_pc = pc; // store predicted next PC for this branch
    }
    else{
        pc = pc + 1;
        decode_predicted_pc = -1;
    }
    if(pc < (int)inst_memory.size())
        fetch_instr_idx = pc;
    else
        fetch_instr_idx = -1;
}

void Processor::stageDecode() {
    if(decode_instr_idx == -1) return;
    Instruction& instr = inst_memory[decode_instr_idx];
    OpCode op = instr.op;
    
    if(isROBFull()) return; // ROB is full, stall


    //if it is lw and sw
    if(op == OpCode::LW || op == OpCode::SW){
        if(!lsq->canAccept()) return; // LSQ is full, stall
        seq_num++;
        int tag = seq_num;
        lsq->addEntry(instr, tag, RAT, ARF);
        pushToROB(op, tag, (op == OpCode::LW ? instr.dest : 0), 0);
        getROBbyTag(tag).instr_pc = instr.pc;
        // LW writes to a register: update RAT
        if(op == OpCode::LW){
            RAT[instr.dest].tag = tag;
            RAT[instr.dest].valid = false;
        }
        decode_instr_idx = -1;
        return;
    }

    ExecutionUnit& target_unit = getExecutionUnitForOp(op);
    int free_rs_idx = -1;
    for(int i = 0; i < (int)target_unit.RS.size(); i++){
        if(target_unit.RS[i] == nullptr){ free_rs_idx = i; break; }
    }
    if(free_rs_idx == -1) return; // no free RS entry, stall

    seq_num++;
    int tag = seq_num;

    RSEntry* new_RS_entry = new RSEntry(op, instr.dest, instr.src1, instr.src2, tag);

    // lookup src1 in RAT
    if(RAT[instr.src1].tag != -1){
        new_RS_entry->tag1 = RAT[instr.src1].tag;
        new_RS_entry->ready1 = false;
    } else if(RAT[instr.src1].valid){
        new_RS_entry->value1 = RAT[instr.src1].value;
        new_RS_entry->ready1 = true;
    } else {
        new_RS_entry->value1 = ARF[instr.src1];
        new_RS_entry->ready1 = true;
    }

    // lookup src2 in RAT (immediate ops get value2 = imm directly)
    if(op == OpCode::ADDI || op == OpCode::SLTI ||
       op == OpCode::ANDI || op == OpCode::ORI || op == OpCode::XORI){
        new_RS_entry->value2 = instr.imm;
        new_RS_entry->ready2 = true;
    } else {
        if(RAT[instr.src2].tag != -1){
            new_RS_entry->tag2 = RAT[instr.src2].tag;
            new_RS_entry->ready2 = false;
        } else if(RAT[instr.src2].valid){
            new_RS_entry->value2 = RAT[instr.src2].value;
            new_RS_entry->ready2 = true;
        } else {
            new_RS_entry->value2 = ARF[instr.src2];
            new_RS_entry->ready2 = true;
        }
    }

    // update RAT for destination register (branches and SW don't write to registers)
    bool is_branch = (op == OpCode::BEQ || op == OpCode::BNE ||
                      op == OpCode::BLT || op == OpCode::BLE);
    bool writes_reg = !is_branch && (op != OpCode::SW);
    if(writes_reg && instr.dest != 0){
        RAT[instr.dest].tag = tag;
        RAT[instr.dest].valid = false;
    }

    // for branches: store imm (target) and instr_pc so evaluate can compute actual next PC
    if(is_branch){
        new_RS_entry->imm = instr.imm;
        new_RS_entry->instr_pc = instr.pc;
    }

    target_unit.RS[free_rs_idx] = new_RS_entry;
    pushToROB(op, tag, instr.dest, 0);

    // set instr_pc and predicted_pc in the ROB entry
    ROBEntry& rob_entry = getROBbyTag(tag);
    rob_entry.instr_pc = instr.pc;
    if(op == OpCode::BEQ || op == OpCode::BNE ||
       op == OpCode::BLT || op == OpCode::BLE)
        rob_entry.predicted_pc = decode_predicted_pc;

    decode_instr_idx = -1; // decode stage is now free
}

void Processor::stageExecuteAndBroadcast() {
    std::vector<RSEntry*> ToBeBroadcasted;

    for(auto &unit : ExecutionUnits){
        // free RS slots that completed in the previous cycle
        for(auto &rs_slot : unit.RS){
            for(auto &pf : unit.pending_free){
                if(rs_slot == pf){ rs_slot = nullptr; break; }
            }
        }
        unit.pending_free.clear();

        // advance all in-flight entries by one cycle, collect completions
        for(auto &entry : unit.in_flight){
            if(entry == nullptr) continue;
            entry->num_cycles_executed++;
            if(entry->num_cycles_executed == unit.latency){
                entry->result = unit.evaluate(entry);
                ToBeBroadcasted.push_back(entry);
                unit.pending_free.push_back(entry); // defer RS free to next cycle
                entry = nullptr;
            }
        }
        // clean up completed (nullptr) entries from in_flight
        unit.in_flight.erase(
            std::remove(unit.in_flight.begin(), unit.in_flight.end(), nullptr),
            unit.in_flight.end()
        );

        // start one new entry per cycle if pipeline not full
        if((int)unit.in_flight.size() < unit.latency){
            int min_tag = INT_MAX;
            RSEntry* oldest_ready = nullptr;
            for(auto &entry : unit.RS){
                if(entry == nullptr) continue;
                if(!entry->ready1 || !entry->ready2) continue;
                if(entry->num_cycles_executed > 0) continue; // already in flight
                if(entry->tag < min_tag){ min_tag = entry->tag; oldest_ready = entry; }
            }
            if(oldest_ready != nullptr){
                oldest_ready->num_cycles_executed = 1;
                if(unit.latency == 1){
                    // latency-1 unit completes in the same cycle it starts
                    oldest_ready->result = unit.evaluate(oldest_ready);
                    ToBeBroadcasted.push_back(oldest_ready);
                    unit.pending_free.push_back(oldest_ready); // defer RS free to next cycle
                } else {
                    unit.in_flight.push_back(oldest_ready);
                }
            }
        }
    }


    // LSQ: advance and start BEFORE ALU broadcasts so LSQ can't start new entries based on same-cycle ALU results
    LSQEntry* lsq_completed = lsq ? lsq->executeCycle(Memory) : nullptr;

    // broadcast ALU completions
    for(auto &entry : ToBeBroadcasted){
        broadcastOnCDB(entry);
    }

    if(lsq_completed){
        int tag = lsq_completed->tag;
        ROBEntry& rob_entry = getROBbyTag(tag);
        rob_entry.value   = lsq_completed->result;
        rob_entry.address = lsq_completed->address;
        rob_entry.ready   = true;
        rob_entry.exception = lsq_completed->exception;
        if(lsq_completed->op == OpCode::LW){
            for(int i = 0; i < (int)RAT.size(); i++){
                if(RAT[i].tag == tag){
                    RAT[i].valid = true;
                    RAT[i].tag = -1;
                    RAT[i].value = lsq_completed->result;
                }
            }
        }
        for(auto &unit : ExecutionUnits) unit.capture(tag, lsq_completed->result);
        lsq->capture(tag, lsq_completed->result);
    }
}

void Processor::stageCommit() {
    //check the topmost entry of the ROB, if it is ready, then commit it and update the ARF and RAT accordingly, else stall
    if(rob_count == 0) return; // ROB is empty, nothing to commit
    ROBEntry& head_entry = getROBHead();
    if(!head_entry.ready) return; // head of ROB is not ready, stall
    if(head_entry.exception){
        pc = head_entry.instr_pc; // set PC to faulting instruction
        exception = true;
        flush();
        return;
    }
    // commit the instruction at the head of the ROB
    commitInstruction(head_entry);

    // if flush was triggered (branch misprediction), ROB is already cleared — skip pop/freeHead
    if(rob_count == 0) return;

    // free LSQ head for memory instructions
    if(head_entry.op == OpCode::LW || head_entry.op == OpCode::SW)
        lsq->freeHead();

    //pop the head of the ROB
    popROBHead();
}



// ----- ROB helpers -----
ROBEntry& Processor::getROBHead() {
    return ROB[rob_head];
}

ROBEntry& Processor::getROBbyTag(int tag) {
    for(int i = 0; i < (int)ROB.size(); i++){
        if(!ROB[i].free && ROB[i].tag == tag) return ROB[i];
    }
    return ROB[0]; // shouldn't reach here
}

bool Processor::isROBFull() {
    return rob_count == (int)ROB.size();
}

void Processor::pushToROB(OpCode op, int tag, int dest_reg, int value) {
    ROB[rob_tail].tag = tag;
    ROB[rob_tail].op = op;
    ROB[rob_tail].dest_reg = dest_reg;
    ROB[rob_tail].value = value;
    ROB[rob_tail].ready = false;
    ROB[rob_tail].free = false;
    rob_tail = (rob_tail + 1) % ROB.size();
    rob_count++;
}

void Processor::popROBHead() {
    ROB[rob_head].free = true;
    ROB[rob_head].ready = false;
    ROB[rob_head].tag = -1;
    rob_head = (rob_head + 1) % ROB.size();
    rob_count--;
}

void Processor::commitInstruction(ROBEntry& entry) {
    if(entry.op == OpCode::BEQ || entry.op == OpCode::BNE ||
       entry.op == OpCode::BLT || entry.op == OpCode::BLE){
        int actual_next_pc = entry.value; // computed by branch unit in evaluate
        bool taken = (actual_next_pc != entry.instr_pc + 1);
        bool was_correct = (actual_next_pc == entry.predicted_pc);
        bp.update(entry.instr_pc, actual_next_pc, taken, was_correct);
        if(!was_correct){
            pc = actual_next_pc;
            fetch_instr_idx = (actual_next_pc < (int)inst_memory.size()) ? actual_next_pc : -1;
            flush(); // clears ROB, RS, LSQ, RAT, resets rob_count
        }
    }
    else if(entry.op== OpCode::SW){
        Memory[entry.address] = entry.value; // commit the store to memory
    }
    else
    {
        if(entry.dest_reg != 0) // x0 always stays 0
            ARF[entry.dest_reg] = entry.value;
        // clear RAT entry if no newer instruction is pending for this register
        if(RAT[entry.dest_reg].tag == entry.tag) { RAT[entry.dest_reg].valid = false; RAT[entry.dest_reg].tag = -1; } //
            //RAT[entry.dest_reg].valid = false;

    }
}



// ----- Helper functions -----
ExecutionUnit& Processor::getExecutionUnitForOp(OpCode op) {
    UnitType target;
    if(op == OpCode::ADD || op == OpCode::SUB || op == OpCode::ADDI ||
       op == OpCode::SLT || op == OpCode::SLTI)
        target = UnitType::ADDER;
    else if(op == OpCode::MUL)
        target = UnitType::MULTIPLIER;
    else if(op == OpCode::DIV || op == OpCode::REM)
        target = UnitType::DIVIDER;
    else if(op == OpCode::AND || op == OpCode::OR  || op == OpCode::XOR ||
            op == OpCode::ANDI|| op == OpCode::ORI || op == OpCode::XORI)
        target = UnitType::LOGIC;
    else if(op == OpCode::BEQ || op == OpCode::BNE ||
            op == OpCode::BLT || op == OpCode::BLE)
        target = UnitType::BRANCH;
    else
        target = UnitType::LOADSTORE;

    for(auto &unit : ExecutionUnits){
        if(unit.name == target) return unit;
    }
    return ExecutionUnits[0]; // shouldn't reach here
}



// ----- Main step function -----
bool Processor::step() {
    //stop when no further instructions to fetch, decode, execute or commit
    if(pc >= (int)inst_memory.size() && rob_count == 0 && fetch_instr_idx == -1 && decode_instr_idx == -1) return false;

    // process stages in reverse pipeline order
    flushed_this_cycle = false;
    stageCommit();
    stageExecuteAndBroadcast();
    if(decode_instr_idx != -1) stageDecode();
    if(fetch_instr_idx != -1 && !flushed_this_cycle) stageFetch();

    clock_cycle++;
    if(exception){
        return false;
    }
    return true;
}
//check architectural state
void Processor::dumpArchitecturalState() {
    std::cout << "\n=== ARCHITECTURAL STATE (CYCLE " << clock_cycle << ") ===\n";
    for (int i = 0; i < (int)ARF.size(); i++) {
        std::cout << "x" << i << ": " << std::setw(4) << ARF[i] << " | ";
        if ((i+1) % 8 == 0) std::cout << std::endl;
    }
    if (exception) {
        std::cout << "EXCEPTION raised by instruction " << pc + 1 << std::endl;
    }
    std::cout << "Branch Predictor Stats: " << bp.correct_predictions << "/" << bp.total_branches << " correct.\n";
}

// only one can be in fetch and decode at a time, but multiple instructions can be in execute and broadcast stages at the same time. commit is in-order, so only one instruction can be in commit stage at a time.
// after every cycle, I can change the status of each instruction
// the possible states are the 4 stages and "waiting" since some other instruction is in that stage.
// So should I go like, means I can go in reverse order
// can multiple instructions commit in the same cycle? No, only one instruction can commit in a cycle since commit is in-order and we need to check the head of the ROB for commit. So if the head of the ROB is not ready, then we cannot commit any instruction in that cycle. If the head of the ROB is ready, then we can commit that instruction and move the head of the ROB to the next entry. So only one instruction can be in commit stage at a time.
//means I should go like, after each clock cycle, I should store the status of each instruction
//and then in the clock cycle, I should check in reverse which all instructions can move to the next stage.
//rob stores the instructions to commit
//RS stores the instructions to execute
//I have to make to variable for the instruction in fetch and decode stage, since only one instruction can be in those stages at a time. So I can have a variable for the current instruction in fetch stage and another variable for the current instruction in decode stage. And then I can check if those variables are empty or not to determine if I can move the next instruction to those stages or not.
