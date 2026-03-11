#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <llvm-c/Core.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Types.h>

#define prt(x) if(x) { printf("%s\n", x); }

using namespace std;
#include <vector>
#include <set>
#include <unordered_map>
#include <fstream>
#include <iostream>

// declare data structures
unordered_map<LLVMBasicBlockRef, set<LLVMValueRef>> genSets;	// GEN set for each BB
unordered_map<LLVMBasicBlockRef, set<LLVMValueRef>> killSets;	// KILL set for each BB
set<LLVMValueRef> allStores;
unordered_map<LLVMValueRef, set<LLVMValueRef>> ptrsToStores;	// stores to each pointer
unordered_map<LLVMBasicBlockRef, set<LLVMBasicBlockRef>> preds;	// predecessors for each block
unordered_map<LLVMBasicBlockRef, set<LLVMValueRef>> inSets;	// IN sets
unordered_map<LLVMBasicBlockRef, set<LLVMValueRef>> outSets;	// OUT sets

/* This function reads the given llvm file and loads the LLVM IR into
	 data-structures that we can works on for optimization phase.
*/ 
LLVMModuleRef createLLVMModel(char * filename){
	char *err = 0;

	LLVMMemoryBufferRef ll_f = 0;
	LLVMModuleRef m = 0;

	LLVMCreateMemoryBufferWithContentsOfFile(filename, &ll_f, &err);

	if (err != NULL) {
		prt(err);
		return NULL;
	}

	LLVMParseIRInContext(LLVMGetGlobalContext(), ll_f, &m, &err);

	if (err != NULL) {
		prt(err);
	}

	return m;
}

/* COMMON SUBEXPR ELIMINATION */
bool cse(LLVMBasicBlockRef bb) {
	bool changes = false;
	vector<LLVMValueRef> instrs;
	set<LLVMValueRef> stores;

	for (LLVMValueRef instr = LLVMGetFirstInstruction(bb); instr; instr = LLVMGetNextInstruction(instr)) {
		if (LLVMIsATerminatorInst(instr)) continue;

		LLVMOpcode op = LLVMGetInstructionOpcode(instr);
		if (op == LLVMCall || op == LLVMAlloca) continue;
		if (op == LLVMStore) {
			// record locations of stores for safety check
			stores.insert(LLVMGetOperand(instr, 1));
			continue;
		}

		// check against all prev instructions
		bool replaced = false;
		for (LLVMValueRef prev : instrs) {
			if (LLVMGetInstructionOpcode(prev) == op) {
				int c = LLVMGetNumOperands(instr);
				int cprev = LLVMGetNumOperands(prev);
				if (c == cprev) {
					bool match = true;
					for (int i = 0; i < c; i++) {
						if (LLVMGetOperand(instr, i) != LLVMGetOperand(prev, i)) {
							match = false;
							break;
						}
					}
					if (match) {
						if (op != LLVMLoad) {
							LLVMReplaceAllUsesWith(instr, prev);
							replaced = true;
							changes = true;
							break;
						}
						// safety check for load instructions
						else {
							LLVMValueRef loc = LLVMGetOperand(instr, 0);
							if (stores.count(loc) == 0) {
								LLVMReplaceAllUsesWith(instr, prev);
								replaced = true;
								changes = true;
								break;
							}
						}
					}
				}
			}
		}

		instrs.push_back(instr);
		if (op == LLVMLoad && !replaced) stores.clear();
	}
	return changes;
}

/* DEAD CODE ELIMINATION */
bool dce(LLVMBasicBlockRef bb) {
	vector<LLVMValueRef> del;

	for (LLVMValueRef instr = LLVMGetFirstInstruction(bb); instr; instr = LLVMGetNextInstruction(instr)) {
		if (LLVMIsATerminatorInst(instr)) continue;

		LLVMOpcode op = LLVMGetInstructionOpcode(instr);
		if (op == LLVMCall || op == LLVMStore || op == LLVMAlloca) continue;

		if (LLVMGetFirstUse(instr) == NULL) {
			del.push_back(instr);
		}
	}
	for (LLVMValueRef instr : del) {
		LLVMInstructionEraseFromParent(instr);
	}
	return !del.empty();
}

/* CONSTANT FOLDING */
bool cfold(LLVMBasicBlockRef bb) {
	bool changes = false;
	for (LLVMValueRef instr = LLVMGetFirstInstruction(bb); instr; instr = LLVMGetNextInstruction(instr)) {
		LLVMOpcode op = LLVMGetInstructionOpcode(instr);
		// binary bexpr & rexpr ops
		if (op == LLVMAdd || op == LLVMSub || op == LLVMMul || op == LLVMICmp) {
			LLVMValueRef lhs = LLVMGetOperand(instr, 0);
			LLVMValueRef rhs = LLVMGetOperand(instr, 1);
			if (LLVMIsConstant(lhs) && LLVMIsConstant(rhs)) {
				changes = true;
				switch (op) {
					case LLVMAdd:
						LLVMReplaceAllUsesWith(instr, LLVMConstAdd(lhs, rhs));
						break;
					case LLVMSub:
						LLVMReplaceAllUsesWith(instr, LLVMConstSub(lhs, rhs));
						break;
					case LLVMMul:
						LLVMReplaceAllUsesWith(instr, LLVMConstMul(lhs, rhs));
						break;
					case LLVMICmp:
						LLVMIntPredicate pred = LLVMGetICmpPredicate(instr);
						LLVMReplaceAllUsesWith(instr, LLVMConstICmp(pred, lhs, rhs));
						break;
				}
			}
		}
		// TODO unary ops
	}
	return changes;
}

bool localOpts(LLVMBasicBlockRef bb) {
	return cse(bb) | cfold(bb) | dce(bb);
}

// CONSTANT PROPAGATION
void getGenSet(LLVMBasicBlockRef bb) {
	unordered_map<LLVMValueRef, LLVMValueRef> gen;
	for (LLVMValueRef instr = LLVMGetFirstInstruction(bb); instr; instr = LLVMGetNextInstruction(instr)) {
		LLVMOpcode op = LLVMGetInstructionOpcode(instr);
		if (op == LLVMStore) {
			LLVMValueRef ptr = LLVMGetOperand(instr, 1);
			gen[ptr] = instr;

			// for bookkeeping, also insert to
			// 1) set of all stores to this pointer
			// 2) set of all stores
			ptrsToStores[ptr].insert(instr);
			allStores.insert(instr);
		}
	}
	set<LLVMValueRef> genSet;
	for (const auto& [key, value] : gen) {
		genSet.insert(value);
	}
	genSets[bb] = genSet;
}

void getKillSet(LLVMBasicBlockRef bb) {
	set<LLVMValueRef> killSet;
	for (LLVMValueRef instr = LLVMGetFirstInstruction(bb); instr; instr = LLVMGetNextInstruction(instr)) {
		LLVMOpcode op = LLVMGetInstructionOpcode(instr);
		if (op == LLVMStore) {
			LLVMValueRef ptr = LLVMGetOperand(instr, 1);
			for (LLVMValueRef storeInstr : ptrsToStores[ptr]) {
				if (storeInstr != instr) {
					killSet.insert(storeInstr);
				}
			}
		}
	}
	killSets[bb] = killSet;
}

void getPredecessors(LLVMValueRef func) {
	for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(func); bb; bb = LLVMGetNextBasicBlock(bb)) {
		LLVMValueRef terminator = LLVMGetBasicBlockTerminator(bb);
		if (!terminator) continue;

		int numSuccessors = LLVMGetNumSuccessors(terminator);
		for (int i = 0; i < numSuccessors; i++) {
			LLVMBasicBlockRef succ = LLVMGetSuccessor(terminator, i);
			preds[succ].insert(bb);
		}
	}
}

void getInAndOut(LLVMValueRef func) {
	// init IN to empty (default)
	// init OUT to GEN
	for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(func); bb; bb = LLVMGetNextBasicBlock(bb)) {
		outSets[bb] = genSets[bb];
	}

	bool changes = true;
	while (changes) {
		changes = false;

		for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(func); bb; bb = LLVMGetNextBasicBlock(bb)) {
			set<LLVMValueRef> newIn;
			for (LLVMBasicBlockRef pred : preds[bb]) {
				for (LLVMValueRef instr : outSets[pred]) {
					newIn.insert(instr);
				}
			}
			inSets[bb] = newIn;

			// save old OUT
			set<LLVMValueRef> oldOut = outSets[bb];
			set<LLVMValueRef> newOut = genSets[bb];
			for (LLVMValueRef instr : inSets[bb]) {
				if (killSets[bb].find(instr) == killSets[bb].end()) {
					newOut.insert(instr);
				}
			}
			outSets[bb] = newOut;

			if (oldOut != newOut) {
				changes = true;
			}
		}
	}
}

void dataflowAnalysis(LLVMValueRef func) {
	// compute GEN, KILL, IN, OUT sets
	for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(func); bb; bb = LLVMGetNextBasicBlock(bb)) {
		getGenSet(bb);
		getKillSet(bb);
	}
	getPredecessors(func);
	getInAndOut(func);
}

bool cprop(LLVMBasicBlockRef bb) {
	bool changes = false;
	set<LLVMValueRef> del;

	// R = IN[B]
	set<LLVMValueRef> r = inSets[bb];
	for (LLVMValueRef instr = LLVMGetFirstInstruction(bb); instr; instr = LLVMGetNextInstruction(instr)) {
		LLVMOpcode op = LLVMGetInstructionOpcode(instr);

		// if I is store instr, add I to R
		if (op == LLVMStore) {
			r.insert(instr);

			// remove killed stores in R
			LLVMValueRef ptr = LLVMGetOperand(instr, 1);
			set<LLVMValueRef> toKill;
			for (LLVMValueRef storeInstr : r) {
				if (storeInstr != instr && LLVMGetInstructionOpcode(storeInstr) == LLVMStore) {
					if (LLVMGetOperand(storeInstr, 1) == ptr) {
						toKill.insert(storeInstr);
					}
				}
			}
			for (LLVMValueRef k : toKill) { r.erase(k); }
		}

		// if I is a load instr
		if (op == LLVMLoad) {
			LLVMValueRef ptr = LLVMGetOperand(instr, 0);

			// find all stores in R that write here
			set<LLVMValueRef> reachingStores;
			for (LLVMValueRef storeInstr : r) {
				if (LLVMGetInstructionOpcode(storeInstr) == LLVMStore) {
					if (LLVMGetOperand(storeInstr, 1) == ptr) {
						reachingStores.insert(storeInstr);
					}
				}
			}

			// check if all reaching stores are constants with same value
			if (!reachingStores.empty()) {
				LLVMValueRef firstConst = NULL;
				bool same = true;
				for (LLVMValueRef storeInstr : reachingStores) {
					LLVMValueRef val = LLVMGetOperand(storeInstr, 0);
					if (!LLVMIsConstant(val)) {
						same = false;
						break;
					}

					if (firstConst == NULL) firstConst = val;
					else if (LLVMConstIntGetSExtValue(firstConst) != LLVMConstIntGetSExtValue(val)) {
						same = false;
						break;
					}
				}

				// replace load with const & delete
				if (same && firstConst != NULL) {
					LLVMReplaceAllUsesWith(instr, firstConst);
					del.insert(instr);
					changes = true;
				}
			}
		}
	}

	for (LLVMValueRef instr : del) {
		LLVMInstructionEraseFromParent(instr);
	}
	return changes;
}

void walkBasicblocks(LLVMValueRef function){
	while (true) {
		bool anyChanges = false;

		// local opts
		for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(function); bb; bb = LLVMGetNextBasicBlock(bb)) {
			anyChanges |= localOpts(bb);
		}

		// clear data structures & recompute
		genSets.clear();
		killSets.clear();
		allStores.clear();
		ptrsToStores.clear();
		preds.clear();
		inSets.clear();
		outSets.clear();

		dataflowAnalysis(function);

		// run constant prop
		bool cpChanges = false;
		for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(function); bb; bb = LLVMGetNextBasicBlock(bb)) {
			anyChanges |= cprop(bb);
		}

		if (!anyChanges) break;
	}
}

void walkFunctions(LLVMModuleRef module){
	for (LLVMValueRef function =  LLVMGetFirstFunction(module); function; function = LLVMGetNextFunction(function)) {
		const char* funcName = LLVMGetValueName(function);
		walkBasicblocks(function);
 	}
}

void walkGlobalValues(LLVMModuleRef module){
	for (LLVMValueRef gVal =  LLVMGetFirstGlobal(module); gVal; gVal = LLVMGetNextGlobal(gVal)) {
                const char* gName = LLVMGetValueName(gVal);
        }
}

void sanityCheck(char output[], char key[]) {
	ifstream f1(output);
	ifstream f2(key);
	string line1, line2;
	while (getline(f1, line1) && getline(f2, line2)) {
		printf("%s\n%s\n\n", line1.c_str(), line2.c_str());
	}
	f1.close();
	f2.close();
}

LLVMModuleRef optimize(LLVMModuleRef mod){
	walkGlobalValues(mod);
	walkFunctions(mod);
	LLVMPrintModuleToFile(mod, "out/out_opt.ll", NULL);

	return mod;
}
