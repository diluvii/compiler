/*
* codegen.c
*
* INPUT: (optimized) LLVM IR
* OUTPUT: Assembly code
*
* author: yawen x. (@diluvii)
* date: 03/10/2026 - 03/11/2026
*/

#include "codegen.h"
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <llvm-c/Core.h>

using namespace std;
#include <string>
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
FILE* outfile;

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
void free_ending_operands(LLVMValueRef instr) {
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
					free_ending_operands(instr);
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
				free_ending_operands(instr);
			}
		}
	}
}

/* ---------------------- *
* ASSEMBLY CODE GENERATION
*
* INPUT: optimized IR, information from register allocation
* OUTPUT: assembly code (written to output file)
* ---------------------- */
int local_mem;
unordered_map<LLVMValueRef, int> offset_map;		// instruction -> memory offset from %ebp
unordered_map<LLVMBasicBlockRef, string> bb_labels;	// BB -> label

// helper function to get register name from int
const char* reg_name(int reg) {
	switch (reg) {
		case EBX: return "%ebx";
		case ECX: return "%ecx";
		case EDX: return "%edx";
		default: return NULL;
	}
}

// populates map of BB -> char* label
void create_bb_labels(LLVMValueRef func) {
	int i = 0;
	for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(func); bb; bb = LLVMGetNextBasicBlock(bb)) {
		bb_labels[bb] = "BB" + to_string(i);
		i++;
	}
}

// emits required directives for function
void print_directives(LLVMValueRef func) {
	string name = LLVMGetValueName(func);
	fprintf(outfile, ".text\n");
	fprintf(outfile, ".globl %s\n", name.c_str());
	fprintf(outfile, ".type %s, @function\n", name.c_str());
	fprintf(outfile, "%s:\n", name.c_str());
}

// emits assembly instructions to restore %esp, %ebp, ret
void print_function_end() {
	fprintf(outfile, "\tleave\n");
	fprintf(outfile, "\tret\n");
}

// populates offsetMap
void get_offset_map(LLVMValueRef func) {
	local_mem = 4;
	LLVMValueRef param = NULL;
	if (LLVMCountParams(func) > 0) {
		param = LLVMGetParam(func, 0);
		offset_map[param] = 8;
	}

	// iterate through BBs & instrs
	for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(func); bb; bb = LLVMGetNextBasicBlock(bb)) {
		for (LLVMValueRef instr = LLVMGetFirstInstruction(bb); instr; instr = LLVMGetNextInstruction(instr)) {
			LLVMOpcode op = LLVMGetInstructionOpcode(instr);
			switch (op) {
				case LLVMAlloca:
					local_mem += 4;
					offset_map[instr] = -1 * local_mem;
					break;
				case LLVMStore: {
					LLVMValueRef op1 = LLVMGetOperand(instr, 0);
					LLVMValueRef op2 = LLVMGetOperand(instr, 1);
					if (op1 == param) {
						int x = offset_map[op1];
						offset_map[op2] = x;
					} else {
						if (!LLVMIsConstant(op1)) {
							int x = offset_map[op2];
							offset_map[op1] = x;
						}
					}
					break;
				}
				case LLVMLoad: {
					LLVMValueRef op1 = LLVMGetOperand(instr, 0);
					int x = offset_map[op1];
					offset_map[instr] = x;
					break;
				}
				default:
					continue;
			}
		}
	}
}

// codegen main function
int codegen(LLVMModuleRef mod) {
	outfile = fopen("out/out.s", "w");

	// first let's populate the register map, etc.
	register_alloc(mod);

	// for each function (i.e. for our only function), populate the appropriate
	// so let's get the function
	LLVMValueRef func = LLVMGetFirstFunction(mod);
	while (LLVMGetFirstBasicBlock(func) == NULL) {
		func = LLVMGetNextFunction(func);
	}

	// then populate appropriate data structures
	create_bb_labels(func);
	print_directives(func);
	get_offset_map(func);

	// emit prologue
	fprintf(outfile, "\tpushl %%ebp\n");
	fprintf(outfile, "\tmovl %%esp, %%ebp\n");
	fprintf(outfile, "\tsubl $%d, %%esp\n", local_mem);
	fprintf(outfile, "\tpushl %%ebx\n");

	// emit instructions
	for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(func); bb; bb = LLVMGetNextBasicBlock(bb)) {
		fprintf(outfile, "%s:\n", bb_labels[bb].c_str());
		for (LLVMValueRef instr = LLVMGetFirstInstruction(bb); instr; instr = LLVMGetNextInstruction(instr)) {
			LLVMOpcode op = LLVMGetInstructionOpcode(instr);
			switch (op) {
				case LLVMRet: {
					LLVMValueRef A = LLVMGetOperand(instr, 0);
					if (LLVMIsConstant(A)) {
						fprintf(outfile, "\tmovl $%lld, %%eax\n", LLVMConstIntGetSExtValue(A));
					} else if (reg_map.count(A) && reg_map[A] != SPILL) {
						const char* reg = reg_name(reg_map[A]);
						fprintf(outfile, "\tmovl %s, %%eax\n", reg);
					} else {
						int k = offset_map[A];
						fprintf(outfile, "\tmovl %d(%%ebp), %%eax\n", k);
					}
					fprintf(outfile, "\tpopl %%ebx\n");
					print_function_end();
					break;
				}
				case LLVMLoad: {
					if (reg_map.count(instr) && reg_map[instr] != SPILL) {
						int reg = reg_map[instr];
						LLVMValueRef b = LLVMGetOperand(instr, 0);
						int c = offset_map[b];
						fprintf(outfile, "\tmovl %d(%%ebp), %s\n", c, reg_name(reg));
					}
					break;
				}
				case LLVMStore: {
					LLVMValueRef A = LLVMGetOperand(instr, 0);
					LLVMValueRef b = LLVMGetOperand(instr, 1);
					LLVMValueRef param = NULL;
					if (LLVMCountParams(func) > 0) param = LLVMGetParam(func, 0);
					if (A == param) break;
					else if (LLVMIsConstant(A)) {
						int c = offset_map[b];
						fprintf(outfile, "\tmovl $%lld, %d(%%ebp)\n", LLVMConstIntGetSExtValue(A), c);
					} else if (reg_map.count(A)) {
						int reg = reg_map[A];
						if (reg != SPILL) {
							int c = offset_map[b];
							fprintf(outfile, "\tmovl %s, %d(%%ebp)\n", reg_name(reg), c);
						} else {
							int c1 = offset_map[A];
							fprintf(outfile, "\tmovl %d(%%ebp), %%eax\n", c1);
							int c2 = offset_map[b];
							fprintf(outfile, "\tmovl %%eax, %d(%%ebp)\n", c2);
						}
					}
					break;
				}
				case LLVMCall: {
					fprintf(outfile, "\tpushl %%ecx\n");
					fprintf(outfile, "\tpushl %%edx\n");
					int num_args = LLVMGetNumArgOperands(instr);
					if (num_args > 0) {
						LLVMValueRef P = LLVMGetOperand(instr, 0);
						if (LLVMIsConstant(P)) {
							fprintf(outfile, "\tpushl $%lld\n", LLVMConstIntGetSExtValue(P));
						} else if (reg_map.count(P)) {
							if (reg_map[P] != SPILL) {
								fprintf(outfile, "\tpushl %s\n", reg_name(reg_map[P]));
							} else {
								int k = offset_map[P];
								fprintf(outfile, "\tpushl %d(%%ebp)\n", k);
							}
						}
					}
					// emit call func
					fprintf(outfile, "\tcall %s\n", LLVMGetValueName(LLVMGetCalledValue(instr)));
					// if exists param
					if (num_args > 0) {
						fprintf(outfile, "\taddl $4, %%esp\n");
					}
					fprintf(outfile, "\tpopl %%edx\n");
					fprintf(outfile, "\tpopl %%ecx\n");
					// if instr is of form (%a = call type @func())
					if (LLVMGetTypeKind(LLVMTypeOf(instr)) != LLVMVoidTypeKind) {
						if (reg_map.count(instr) && reg_map[instr] != SPILL) {
							fprintf(outfile, "\tmovl %%eax, %s\n", reg_name(reg_map[instr]));
						} else {
							fprintf(outfile, "\tmovl %%eax, %d(%%ebp)\n", offset_map[instr]);
						}
					}
					break;
				}
				case LLVMBr: {
					// if unconditional
					if (LLVMGetNumOperands(instr) == 1) {
						LLVMBasicBlockRef b = LLVMValueAsBasicBlock(LLVMGetOperand(instr, 0));
						string L = bb_labels[b];
						fprintf(outfile, "\tjmp %s\n", L.c_str());
					}
					// if conditional
					else {
						LLVMBasicBlockRef falseBB = LLVMValueAsBasicBlock(LLVMGetOperand(instr, 1));
						LLVMBasicBlockRef trueBB = LLVMValueAsBasicBlock(LLVMGetOperand(instr, 2));
						LLVMValueRef cond = LLVMGetOperand(instr, 0);
						LLVMIntPredicate pred = LLVMGetICmpPredicate(cond);
						const char* jxx;
						switch (pred) {
							case LLVMIntEQ: jxx = "je"; break;
							case LLVMIntNE: jxx = "jne"; break;
							case LLVMIntSLT: jxx = "jl"; break;
							case LLVMIntSGT: jxx = "jg"; break;
							case LLVMIntSLE: jxx = "jle"; break;
							case LLVMIntSGE: jxx = "jge"; break;
							default: jxx = "jmp"; break;
						}
						fprintf(outfile, "\t%s %s\n", jxx, bb_labels[trueBB].c_str());
						fprintf(outfile, "\tjmp %s\n", bb_labels[falseBB].c_str());
					}
					break;
				}
				case LLVMAdd:
				case LLVMSub:
				case LLVMMul: {
					// register for instruction
					const char* R;
					if (reg_map.count(instr) && reg_map[instr] != SPILL) R = reg_name(reg_map[instr]);
					else R = "%eax";

					// deal with operand A
					LLVMValueRef A = LLVMGetOperand(instr, 0);
					LLVMValueRef B = LLVMGetOperand(instr, 1);
					if (LLVMIsConstant(A)) fprintf(outfile, "\tmovl $%lld, %s\n", LLVMConstIntGetSExtValue(A), R);
					else if (reg_map.count(A)) {
						if (reg_map[A] != SPILL) {
							if (strcmp(reg_name(reg_map[A]), R) != 0) 
								fprintf(outfile, "\tmovl %s, %s\n", reg_name(reg_map[A]), R);
						} else fprintf(outfile, "\tmovl %d(%%ebp), %s\n", offset_map[A], R);
					}

					// look at operation & deal with operand B
					const char* op_str;
					switch (op) {
						case LLVMAdd: op_str = "addl"; break;
						case LLVMSub: op_str = "subl"; break;
						default: op_str = "imull";
					}
					if (LLVMIsConstant(B)) fprintf(outfile, "\t%s $%lld, %s\n", op_str, LLVMConstIntGetSExtValue(B), R);
					else if (reg_map.count(B)) {
						if (reg_map[B] != SPILL) fprintf(outfile, "\t%s %s, %s\n", op_str, reg_name(reg_map[B]), R);
						else fprintf(outfile, "\t%s %d(%%ebp), %s\n", op_str, offset_map[B], R);
					}

					// if instr is in memory
					if (!(reg_map.count(instr) && reg_map[instr] != SPILL)) {
						fprintf(outfile, "\tmovl %%eax, %d(%%ebp)\n", offset_map[instr]);
					}
					break;
				}
				case LLVMICmp: {
					const char* R;
					if (reg_map.count(instr) && reg_map[instr] != SPILL) R = reg_name(reg_map[instr]);
					else R = "%eax";
					LLVMValueRef A = LLVMGetOperand(instr, 0);
					LLVMValueRef B = LLVMGetOperand(instr, 1);
					if (LLVMIsConstant(A)) fprintf(outfile, "\tmovl $%lld, %s\n", LLVMConstIntGetSExtValue(A), R);
					else if (reg_map.count(A)) {
						if (reg_map[A] != SPILL) {
							if (strcmp(reg_name(reg_map[A]), R) != 0)
								fprintf(outfile, "\tmovl %s, %s\n", reg_name(reg_map[A]), R);
						}
						else fprintf(outfile, "\tmovl %d(%%ebp), %s\n", offset_map[A], R);
					}
					if (LLVMIsConstant(B)) fprintf(outfile, "\tcmpl $%lld, %s\n", LLVMConstIntGetSExtValue(B), R);
					else if (reg_map.count(B)) {
						if (reg_map[B] != SPILL) fprintf(outfile, "\tcmpl %s, %s\n", reg_name(reg_map[B]), R);
						else fprintf(outfile, "\tcmpl %d(%%ebp), %s\n", offset_map[B], R);
					}
					break;
				}
				default:
					continue;
			}
		}
	}

	printf("assembly code generated! look for it in out/out.s :) :)\n");
	fclose(outfile);
	return 0;
}
