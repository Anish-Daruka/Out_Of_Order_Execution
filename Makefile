# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++17 -Wall -g

# ==========================================
# make compile FILE=<filename.cpp>
# ==========================================
# This target should compile your files with the provided 
# main.cpp. The main.cpp will always #include "Processor.h" 
# and will have its own main() function.
compile:
	@echo "Compiling simulator:"
	$(CXX) $(CXXFLAGS) main.cpp Processor.cpp -o main
	@echo "Build successful, 'main' created."

# ==========================================
# make run FILE=<filename.s>
# ==========================================
# Update this target to run whatever script or 
# program you wrote to preprocess the assembly labels. 
# Example below assumes a Python script named 'compiler.py'.
run: 
	@echo "Preprocessing main..."
	python3 preprocessor.py $(FILE)
	@echo "Preprocessing complete."
	mkdir -p outputCustom/$(dir $(FILE)) && ./main $(FILE) > outputCustom/$(FILE)

run1:compile
	@echo "Preprocessing main..."
	python3 preprocessor.py programs/code$(TC).txt
	@echo "Preprocessing complete."
	./main programs/code$(TC).txt > output/output$(TC).txt
