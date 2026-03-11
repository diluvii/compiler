#include "ir.h"
#include <string.h>
#include <stdio.h>
#include <unordered_map>
#include <set>
#include <string>
#include <vector>

using namespace std;

void collectDecls(astNode* stmt, set<string>& varNames);
void renameVarsExpr(astNode* expr, unordered_map<string, string>& renameMap);
void renameVarsStmt(astNode* stmt, unordered_map<string, string>& renameMap, int& counter);
void removeUnreachableBBs(LLVMValueRef func);
LLVMBasicBlockRef genIRStmt(astNode* stmt, LLVMBasicBlockRef startBB);
LLVMValueRef genIRExpr(astNode* expr);

unordered_map<string, LLVMValueRef> varMap;
LLVMValueRef retRef;
LLVMBasicBlockRef retBB;
LLVMBuilderRef builder;
LLVMValueRef printFunc;
LLVMValueRef readFunc;

void collectDecls(astNode* stmt, set<string>& varNames) {
    switch (stmt->stmt.type) {
        case ast_decl:
            varNames.insert(string(stmt->stmt.decl.name));
            break;
        case ast_block:
            for (auto s : *stmt->stmt.block.stmt_list)
                collectDecls(s, varNames);
            break;
        case ast_while:
            collectDecls(stmt->stmt.whilen.body, varNames);
            break;
        case ast_if:
            collectDecls(stmt->stmt.ifn.if_body, varNames);
            if (stmt->stmt.ifn.else_body != NULL)
                collectDecls(stmt->stmt.ifn.else_body, varNames);
            break;
        default: break;
    }
}

void renameVars(astNode* root) {
    int counter = 0;
    unordered_map<string, string> renameMap;
    astNode* func = root->prog.func;
    if (func->func.param != NULL) {
        string oldName = string(func->func.param->var.name);
        string newName = oldName + "_" + to_string(counter++);
        renameMap[oldName] = newName;
        func->func.param->var.name = strdup(newName.c_str());
    }
    renameVarsStmt(func->func.body, renameMap, counter);
}

void renameVarsStmt(astNode* stmt, unordered_map<string, string>& renameMap, int& counter) {
    switch (stmt->stmt.type) {
        case ast_decl: {
            string oldName = string(stmt->stmt.decl.name);
            string newName = oldName + "_" + to_string(counter++);
            renameMap[oldName] = newName;
            stmt->stmt.decl.name = strdup(newName.c_str());
            break;
        }
        case ast_asgn:
            renameVarsExpr(stmt->stmt.asgn.lhs, renameMap);
            renameVarsExpr(stmt->stmt.asgn.rhs, renameMap);
            break;
        case ast_call:
            if (stmt->stmt.call.param != NULL)
                renameVarsExpr(stmt->stmt.call.param, renameMap);
            break;
        case ast_while:
            renameVarsExpr(stmt->stmt.whilen.cond, renameMap);
            renameVarsStmt(stmt->stmt.whilen.body, renameMap, counter);
            break;
        case ast_if:
            renameVarsExpr(stmt->stmt.ifn.cond, renameMap);
            renameVarsStmt(stmt->stmt.ifn.if_body, renameMap, counter);
            if (stmt->stmt.ifn.else_body != NULL)
                renameVarsStmt(stmt->stmt.ifn.else_body, renameMap, counter);
            break;
        case ast_ret:
            renameVarsExpr(stmt->stmt.ret.expr, renameMap);
            break;
        case ast_block:
            for (auto s : *stmt->stmt.block.stmt_list)
                renameVarsStmt(s, renameMap, counter);
            break;
        default: break;
    }
}

void renameVarsExpr(astNode* expr, unordered_map<string, string>& renameMap) {
    switch (expr->type) {
        case ast_var:
            if (renameMap.count(string(expr->var.name)))
                expr->var.name = strdup(renameMap[string(expr->var.name)].c_str());
            break;
        case ast_uexpr:
            renameVarsExpr(expr->uexpr.expr, renameMap);
            break;
        case ast_bexpr:
            renameVarsExpr(expr->bexpr.lhs, renameMap);
            renameVarsExpr(expr->bexpr.rhs, renameMap);
            break;
        case ast_rexpr:
            renameVarsExpr(expr->rexpr.lhs, renameMap);
            renameVarsExpr(expr->rexpr.rhs, renameMap);
            break;
        default: break;
    }
}

void removeUnreachableBBs(LLVMValueRef func) {
    // BFS from entry to find all reachable blocks
    LLVMBasicBlockRef entry = LLVMGetFirstBasicBlock(func);
    set<LLVMBasicBlockRef> visited;
    vector<LLVMBasicBlockRef> queue;

    queue.push_back(entry);
    visited.insert(entry);

    while (!queue.empty()) {
        LLVMBasicBlockRef bb = queue.front();
        queue.erase(queue.begin());

        LLVMValueRef terminator = LLVMGetBasicBlockTerminator(bb);
        if (terminator == NULL) continue;

        unsigned numSucc = LLVMGetNumSuccessors(terminator);
        for (unsigned i = 0; i < numSucc; i++) {
            LLVMBasicBlockRef succ = LLVMGetSuccessor(terminator, i);
            if (visited.find(succ) == visited.end()) {
                visited.insert(succ);
                queue.push_back(succ);
            }
        }
    }

    // collect unreachable blocks
    vector<LLVMBasicBlockRef> toRemove;
    for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(func);
         bb != NULL; bb = LLVMGetNextBasicBlock(bb)) {
        if (visited.find(bb) == visited.end()) {
            toRemove.push_back(bb);
        }
    }

    // delete them
    for (LLVMBasicBlockRef bb : toRemove) {
        LLVMValueRef inst = LLVMGetFirstInstruction(bb);
        while (inst != NULL) {
            LLVMValueRef next = LLVMGetNextInstruction(inst);
            LLVMInstructionEraseFromParent(inst);
            inst = next;
        }
        LLVMDeleteBasicBlock(bb);
    }
}

LLVMModuleRef genIR(astNode* root) {
	renameVars(root);
	astNode* currNode;

	LLVMModuleRef mod = LLVMModuleCreateWithName("");
	// TODO set target architecture
	LLVMSetDataLayout(mod, "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128");
	LLVMSetTarget(mod, "x86_64-pc-linux-gnu");

	// extern nodes for print, read
	if (root->prog.ext1 != NULL) {
		LLVMTypeRef param[] = { LLVMInt32Type() };
		LLVMTypeRef funcType = LLVMFunctionType(LLVMVoidType(), param, 1, 0);
		printFunc = LLVMAddFunction(mod, "print", funcType);
	}
	if (root->prog.ext2 != NULL) {
		LLVMTypeRef funcType = LLVMFunctionType(LLVMInt32Type(), NULL, 0, 0);
		readFunc = LLVMAddFunction(mod, "read", funcType);
	}

	// now go to function node
	currNode = root->prog.func;
	LLVMTypeRef funcType;
	if (currNode->func.param == NULL) {
		funcType = LLVMFunctionType(LLVMInt32Type(), NULL, 0, 0);
	} else {
		LLVMTypeRef param[] = { LLVMInt32Type() };
		funcType = LLVMFunctionType(LLVMInt32Type(), param, 1, 0);
	}
	LLVMValueRef func = LLVMAddFunction(mod, currNode->func.name, funcType);

	builder = LLVMCreateBuilder();
	LLVMBasicBlockRef entryBB = LLVMAppendBasicBlock(func, "");

	// set with names of params, local vars
	set<string> varNames;
	if (currNode->func.param != NULL) {
		varNames.insert(string(currNode->func.param->var.name));
	}
	/*for (auto stmt : *currNode->func.body->stmt.block.stmt_list) {
		if (stmt->stmt.type == ast_decl) {
			varNames.insert(string(stmt->stmt.decl.name));
		}
	}*/
	collectDecls(currNode->func.body, varNames);
	LLVMPositionBuilderAtEnd(builder, entryBB);

	// map of allocas from set above
	varMap = unordered_map<string, LLVMValueRef>();
	for (auto name : varNames) {
		varMap[name] = LLVMBuildAlloca(builder, LLVMInt32Type(), name.c_str());
	}

	// return instruction
	retRef = LLVMBuildAlloca(builder, LLVMInt32Type(), "return");

	if (currNode->func.param != NULL) {
		string paramName = string(currNode->func.param->var.name);
		LLVMBuildStore(builder, LLVMGetParam(func, 0), varMap[paramName]);
	}

	// return basic block
	retBB = LLVMAppendBasicBlock(func, "");
	LLVMPositionBuilderAtEnd(builder, retBB);
	LLVMValueRef l1 = LLVMBuildLoad2(builder, LLVMInt32Type(), retRef, "");
	LLVMBuildRet(builder, l1);

	// IR for function body; exit as return value
	LLVMBasicBlockRef exitBB = genIRStmt(currNode->func.body, entryBB);
	if (LLVMGetBasicBlockTerminator(exitBB) == NULL) {
		LLVMPositionBuilderAtEnd(builder, exitBB);
		LLVMBuildBr(builder, retBB);
	}

	removeUnreachableBBs(func);
	varMap.clear();

	// write out
	// LLVMDumpModule(mod);
	char* err = NULL;
	LLVMPrintModuleToFile(mod, "out/out.ll", &err);
	if (err) { LLVMDisposeMessage(err); }
	// cleanup
	LLVMDisposeBuilder(builder);

	return mod;
}

LLVMBasicBlockRef genIRStmt(astNode* stmt, LLVMBasicBlockRef startBB) {
	switch(stmt->stmt.type) {
		case (ast_decl): return startBB;
		case (ast_asgn): {
			LLVMPositionBuilderAtEnd(builder, startBB);
			LLVMValueRef rhs = genIRExpr(stmt->stmt.asgn.rhs);
			string lhsName = string(stmt->stmt.asgn.lhs->var.name);
			// printf("asgn lhs name: %s, in map: %d\n", lhsName.c_str(), (int)varMap.count(lhsName));
			LLVMBuildStore(builder, rhs, varMap[lhsName]);
			return startBB;
		}
		case (ast_call): {
			LLVMPositionBuilderAtEnd(builder, startBB);
			LLVMValueRef param = genIRExpr(stmt->stmt.call.param);
			LLVMTypeRef paramType[] = { LLVMInt32Type() };
			LLVMTypeRef funcType = LLVMFunctionType(LLVMVoidType(), paramType, 1, 0);
			LLVMValueRef args[] = { param };
			LLVMBuildCall2(builder, funcType, printFunc, args, 1, "");
			return startBB;
		}
		case (ast_while): {
			LLVMPositionBuilderAtEnd(builder, startBB);

			// generate condition check BB & branch to it
			LLVMBasicBlockRef condBB = LLVMAppendBasicBlock(LLVMGetBasicBlockParent(startBB), "");
			LLVMBuildBr(builder, condBB);

			// write to condBB; branch to true & false
			LLVMPositionBuilderAtEnd(builder, condBB);
			LLVMValueRef cond = genIRExpr(stmt->stmt.whilen.cond);
			LLVMBasicBlockRef trueBB = LLVMAppendBasicBlock(LLVMGetBasicBlockParent(startBB), "");
			LLVMBasicBlockRef falseBB = LLVMAppendBasicBlock(LLVMGetBasicBlockParent(startBB), "");
			LLVMBuildCondBr(builder, cond, trueBB, falseBB);

			// loop!
			LLVMBasicBlockRef trueExitBB = genIRStmt(stmt->stmt.whilen.body, trueBB);
			LLVMPositionBuilderAtEnd(builder, trueExitBB);
			LLVMBuildBr(builder, condBB);
			return falseBB;
		}
		case (ast_if): {
			LLVMPositionBuilderAtEnd(builder, startBB);

			// condition check & branching
			LLVMValueRef cond = genIRExpr(stmt->stmt.ifn.cond);
			LLVMBasicBlockRef trueBB = LLVMAppendBasicBlock(LLVMGetBasicBlockParent(startBB), "");
			LLVMBasicBlockRef falseBB = LLVMAppendBasicBlock(LLVMGetBasicBlockParent(startBB), "");
			LLVMBuildCondBr(builder, cond, trueBB, falseBB);

			// check if there exists an else body
			if (stmt->stmt.ifn.else_body == NULL) {
				LLVMBasicBlockRef ifExitBB = genIRStmt(stmt->stmt.ifn.if_body, trueBB);
				LLVMPositionBuilderAtEnd(builder, ifExitBB);
				LLVMBuildBr(builder, falseBB);
				return falseBB;
			} else {
				LLVMBasicBlockRef ifExitBB = genIRStmt(stmt->stmt.ifn.if_body, trueBB);
				LLVMBasicBlockRef elseExitBB = genIRStmt(stmt->stmt.ifn.else_body, falseBB);
				LLVMBasicBlockRef endBB = LLVMAppendBasicBlock(LLVMGetBasicBlockParent(startBB), "");
				LLVMPositionBuilderAtEnd(builder, ifExitBB);
				LLVMBuildBr(builder, endBB);
				LLVMPositionBuilderAtEnd(builder, elseExitBB);
				LLVMBuildBr(builder, endBB);
				return endBB;
			}
		}
		case (ast_ret): {
			LLVMPositionBuilderAtEnd(builder, startBB);
			LLVMValueRef retVal = genIRExpr(stmt->stmt.ret.expr);
			LLVMBuildStore(builder, retVal, retRef);
			LLVMBuildBr(builder, retBB);
			LLVMBasicBlockRef endBB = LLVMAppendBasicBlock(LLVMGetBasicBlockParent(startBB), "");
			return endBB;
		}
		case (ast_block): {
			LLVMBasicBlockRef prevBB = startBB;
			for (auto s: *stmt->stmt.block.stmt_list) {
				prevBB = genIRStmt(s, prevBB);
			}
			return prevBB;
			break;
		}
		default:
			printf("not supposed to get here :(\n");
			return NULL;
	}
	return startBB;
}

LLVMValueRef genIRExpr(astNode* expr) {
	switch (expr->type) {
		case (ast_cnst): {
			return LLVMConstInt(LLVMInt32Type(), expr->cnst.value, 1);
		}
		case (ast_var): {
			return LLVMBuildLoad2(builder, LLVMInt32Type(), varMap[string(expr->var.name)], "");
		}
		case (ast_uexpr): {
			LLVMValueRef operand = genIRExpr(expr->uexpr.expr);
			return LLVMBuildSub(builder, LLVMConstInt(LLVMInt32Type(), 0, 1), operand, "");
		}
		case (ast_bexpr): {
			LLVMValueRef lhs = genIRExpr(expr->bexpr.lhs);
			LLVMValueRef rhs = genIRExpr(expr->bexpr.rhs);
			switch (expr->bexpr.op) {
				case add: return LLVMBuildAdd(builder, lhs, rhs, "");
				case sub: return LLVMBuildSub(builder, lhs, rhs, "");
				case mul: return LLVMBuildMul(builder, lhs, rhs, "");
				case divide: return LLVMBuildSDiv(builder, lhs, rhs, "");
				default: return NULL;
			}
		}
		case (ast_rexpr): {
			LLVMValueRef lhs = genIRExpr(expr->rexpr.lhs);
			LLVMValueRef rhs = genIRExpr(expr->rexpr.rhs);
			LLVMIntPredicate pred;
			switch (expr->rexpr.op) {
				case eq: pred = LLVMIntEQ; break;
				case neq: pred = LLVMIntNE; break;
				case lt: pred = LLVMIntSLT; break;
				case gt: pred = LLVMIntSGT; break;
				case le: pred = LLVMIntSLE; break;
				case ge: pred = LLVMIntSGE; break;
				default: pred = LLVMIntEQ; break;
			}
			return LLVMBuildICmp(builder, pred, lhs, rhs, "");
		}
		case (ast_call): {
			LLVMTypeRef funcType = LLVMFunctionType(LLVMInt32Type(), NULL, 0, 0);
			return LLVMBuildCall2(builder, funcType, readFunc, NULL, 0, "");
		}
		case (ast_stmt): {
			if (expr->stmt.type == ast_call) {
				return LLVMBuildCall2(builder, LLVMFunctionType(LLVMInt32Type(), NULL, 0, 0), readFunc, NULL, 0, "");
			}
			return NULL;
		}
		default: {
			printf("error! invalid ast node type; we are not supposed to be here :(\n");
			printf("error! invalid ast node type: %d\n", expr->type);
			return NULL;
		}
	}
}
