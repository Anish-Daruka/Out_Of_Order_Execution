import sys
import re

def preprocess(input_file, output_file=None):
    with open(input_file, 'r') as f:
        lines = f.readlines()

    clean_instructions = []
    labels = {}         # Jump labels -> PC (instruction index)
    mem_labels = {}     # Memory labels -> Memory array index
    
    current_pc = 0
    current_mem_ptr = 0
    initial_mem_data = []

    # PASS 1: Build Symbol Tables
    for line in lines:
        line = line.split('#')[0].strip()
        if not line: continue

        if line.startswith('.'):
            match = re.match(r'\.(\w+):\s*(.*)', line)
            if match:
                name, vals = match.groups()
                mem_labels[name] = current_mem_ptr
                val_list = vals.split()
                initial_mem_data.extend(val_list)
                current_mem_ptr += len(val_list)
            continue

        if ':' in line and not line.startswith('.'):
            label_name = line.split(':')[0].strip()
            labels[label_name] = current_pc
            remaining = line.split(':')[1].strip()
            if remaining:
                clean_instructions.append(remaining)
                current_pc += 1
            continue

        clean_instructions.append(line)
        current_pc += 1

    # PASS 2: Substitution
    final_output = []
    
    # NEW LOGIC: Put the Memory Data at the top of the file!
    # We prefix it with "MEM_INIT" so C++ knows what it is.
    if initial_mem_data:
        # Create a single line: MEM_INIT 1 2 3 4 5 ...
        mem_str = "MEM_INIT " + " ".join(initial_mem_data)
        final_output.append(mem_str)
        
    for inst in clean_instructions:
        for m_label, addr in mem_labels.items():
            inst = re.sub(fr'\b{m_label}\(', f'{addr}(', inst)
        
        for b_label, target_pc in labels.items():
            inst = re.sub(fr'\b{b_label}\b', str(target_pc), inst)
        
        inst = inst.replace(',', ' ')
        final_output.append(inst)

    with open(output_file, 'w') as f:
        f.write('\n'.join(final_output))

if __name__ == "__main__":
    input_file="programs/"+f"code{sys.argv[1]}.txt"
    output_file="preprocessed/"+f"processed{sys.argv[1]}.txt"
    preprocess(input_file, output_file)