/*
* codegen.c
*
* INPUT: (optimized) LLVM IR
* OUTPUT: Assembly code
*
* author: yawen x. (@diluvii)
* date: 03/10/2026
*/

#include "codegen.h"
#include <stdio.h>
#include <stdbool.h>
#include <llvm-c/Core.h>

using namespace std;
#include <unordered_map>
#include <set>
#include <vector>
#include <algorithm>

// enumerate registers
#define EBX 0
#define ECX 1
#define EDX 2
#define SPILL -1

// let us instantiate global data structures
unordered_map<LLVMValueRef, int> reg_map;
unordered_map<LLVMValueRef, int> inst_index;		// instructions -> index in BB
unordered_map<LLVMValueRef, pair<int, int>> live_range;	// instruction -> (index of def, index of last use)
set<int> available;					// set of available registers

/* ---------------------- *
* REGISTER ALLOCATION
*
* linear scan algorithm
* INPUT: optimized IR
* OUTPUT: reg_map (data structure: map<LLVMValueRef, physical register>)
* ---------------------- */
// helper to populate inst_index, live_range
void compute_liveness(LLVMBasicBlockRef bb) {
	inst_index.clear();
	live_range.clear();

	int idx = 0;
	for (LLVMValueRef instr = LLVMGetFirstInstruction(bb); instr; instr = LLVMGetNextInstruction(instr)) {
		// skip allocas
		if (LLVMGetInstructionOpcode(instr) != LLVMAlloca) {
			inst_index[instr] = idx;
			idx++;
		}
	}

	// find last use of each instruction
	for (auto entry : inst_index) {
		LLVMValueRef instr = entry.first;
		int start = entry.second;
		int end = start;

		// scan through other instructions that start after this one
		for (auto other : inst_index) {
			LLVMValueRef other_instr = other.first;
			int other_start = other.second;

			if (other_start >= start) {
				// go through operands to see if instr is used
				for (int i = 0; i < LLVMGetNumOperands(other_instr); i++) {
					if (LLVMGetOperand(other_instr, i) == instr)
						end = max(end, other_start);
				}
			}
		}

		live_range[instr] = {start, end};
	}

	// sanity check (print live_range)
	/*for (auto entry : live_range) {
		LLVMDumpValue(entry.first);
		printf(" : [%d,%d]\n", entry.second.first, entry.second.second);
	}*/
}

// helper to free registers of operands whose liveness ends at instr
void freeEndingOperands(LLVMValueRef instr) {
	for (int i = 0; i < LLVMGetNumOperands(instr); i++) {
		LLVMValueRef operand = LLVMGetOperand(instr, i);
		if (live_range.count(operand) && live_range[operand].second == inst_index[instr]) {
			if (reg_map.count(operand) && reg_map[operand] != SPILL) {
				available.insert(reg_map[operand]);
			}
		}
	}
}

// helper to find register to spill
LLVMValueRef find_spill(LLVMValueRef instr) {
	// sort by decreasing live range endpoint
	vector<LLVMValueRef> cand;
	for (auto entry : reg_map) {
		if (entry.second != SPILL && live_range.count(entry.first)) cand.push_back(entry.first);
	}

	sort(cand.begin(), cand.end(), [](LLVMValueRef a, LLVMValueRef b) {
		return live_range[a].second > live_range[b].second;
	});

	int start = inst_index[instr];
	int end = live_range[instr].second;

	for (LLVMValueRef v : cand) {
		int v_start = live_range[v].first;
		int v_end = live_range[v].second;
		if (v_start <= start && end <= v_end) {
			if (reg_map.count(v) && reg_map[v] != SPILL) return v;
		}
	}

	return NULL;
}

// linear scan main function!
void register_alloc(LLVMModuleRef mod) {
	// get the function; iterate until we find the one with basic blocks
	// since in the IR we often have @read or @print first
	LLVMValueRef func = LLVMGetFirstFunction(mod);
	while (LLVMGetFirstBasicBlock(func) == NULL) {
		func = LLVMGetNextFunction(func);
	}

	// for each BB
	for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(func); bb; bb = LLVMGetNextBasicBlock(bb)) {
		// populate set of available registers, inst_index, live_range
		available = {EBX, ECX, EDX};
		compute_liveness(bb);

		// for each instruction
		for (LLVMValueRef instr = LLVMGetFirstInstruction(bb); instr; instr = LLVMGetNextInstruction(instr)) {
			// skip allocas
			if (LLVMGetInstructionOpcode(instr) == LLVMAlloca) continue;

			LLVMOpcode op = LLVMGetInstructionOpcode(instr);

			// now check if this instruction produces a result
			bool hasResult = (op != LLVMStore && op != LLVMBr && op != LLVMRet && !(op == LLVMCall && LLVMGetTypeKind(LLVMTypeOf(instr)) == LLVMVoidTypeKind));
			if (!hasResult) {
				// free registers whose liveness end here
				for (int i = 0; i < LLVMGetNumOperands(instr); i++) {
					LLVMValueRef operand = LLVMGetOperand(instr, i);
					// if operand live range ends at this instruction
					if (live_range.count(operand) && live_range[operand].second == inst_index[instr]) {
						if (reg_map.count(operand) && reg_map[operand] != SPILL) {
							available.insert(reg_map[operand]);
						}
					}
				}
			} else {
				// CASE 1: add/sub/mul where first operand's register is free
				if (op == LLVMAdd || op == LLVMSub || op == LLVMMul) {
					LLVMValueRef op1 = LLVMGetOperand(instr, 0);
					if (reg_map.count(op1) && reg_map[op1] != SPILL && live_range.count(op1) && live_range[op1].second == inst_index[instr]) {
						int R = reg_map[op1];
						reg_map[instr] = R;

						// free second operand if it stops being live here
						LLVMValueRef op2 = LLVMGetOperand(instr, 1);
						if (live_range.count(op2) && live_range[op2].second == inst_index[instr]) {
							if (reg_map.count(op2) && reg_map[op2] != SPILL) {
								available.insert(reg_map[op2]);
							}
						}
						continue;
					}
				}

				// CASE 2: free register available
				if (!available.empty()) {
					int R = *available.begin();
					available.erase(available.begin());
					reg_map[instr] = R;
					freeEndingOperands(instr);
					continue;
				}

				// CASE 3: no register available, must spill
				LLVMValueRef V = find_spill(instr);
				if (V == NULL) reg_map[instr] = SPILL;
				else if (live_range[instr].second > live_range[V].second)
					// instr lives longer, so spill it
					reg_map[instr] = SPILL;
				else {
					// steal V's register
					int R = reg_map[V];
					reg_map[V] = SPILL;
					reg_map[instr] = R;
				}
				freeEndingOperands(instr);
			}
		}
	}
}

/* ---------------------- *
* ASSEMBLY CODE GENERATION
*
* XXX
* ---------------------- */
int codegen(LLVMModuleRef mod) {
	// first let's populate the register map, etc.
	register_alloc(mod);

	// sanity check
	for (auto entry : reg_map) {
		LLVMDumpValue(entry.first);
		const char* reg;
		switch (entry.second) {
			case EBX: reg = "ebx"; break;
			case ECX: reg = "ecx"; break;
			case EDX: reg = "edx"; break;
			default: reg = "SPILL"; break;
		}
		printf(" -> %s\n", reg);
	}

	return 0;
}
