#include "sem.h"
#include <stdio.h>
#include <stack>
#include <set>
#include <string>

// data structures
stack<set<string>> symbolTable;
set<string> symbolSet;
bool firstPass;	// allows param to be pushed onto first symbol table
int errs;
set<astNode*> visited;

// function decls
void dfs(astNode* node);
set<astNode*> getChildren(astNode* node);
void checkDecl(string varName);
void checkVar(string varName);

int semanticAnalysis(astNode* root) {
	// init data structures & flags
	symbolTable = stack<set<string>>();
	symbolSet = set<string>();
	symbolTable.push(symbolSet);
	firstPass = false;
	errs = 0;

	// graph traversal
	visited = set<astNode*>();
	dfs(root);

	return errs;
}

void dfs(astNode* node) {
	if (visited.count(node) == 0) {
		visited.insert(node);
		bool isBlock = (node->type == ast_stmt && node->stmt.type == ast_block);

		// push symbol table upon entering block
		if (isBlock) {
			if (!firstPass) {
				symbolTable.push(symbolSet);
				symbolSet.clear();
			} else { firstPass = false; }
		}

		// iterate through neighbors
		set<astNode*> neighbors = getChildren(node);
		for (astNode* n : neighbors) {
			dfs(n);
		}

		// pop symbol table upon exiting block
		if (isBlock) {
			symbolTable.pop();
		}
	}
}

set<astNode*> getChildren(astNode* node) {
	set<astNode*> rset = set<astNode*>();
	switch(node->type) {
		case ast_prog:
			rset.insert(node->prog.func);
			break;
		case ast_func:
			if (node->func.param != nullptr) {
				symbolTable.top().insert(node->func.param->var.name);
				firstPass = true;
			}
			rset.insert(node->func.body);
			break;
		case ast_stmt:
			switch (node->stmt.type) {
				case ast_block:
					for (astNode* n : *(node->stmt.block.stmt_list)) {
						rset.insert(n);
					}
					break;
				case ast_if:
					rset.insert(node->stmt.ifn.cond);
					rset.insert(node->stmt.ifn.if_body);
					if (node->stmt.ifn.else_body != nullptr) {
						rset.insert(node->stmt.ifn.else_body);
					}
					break;
				case ast_while:
					rset.insert(node->stmt.whilen.cond);
					rset.insert(node->stmt.whilen.body);
					break;
				case ast_decl:
					checkDecl(node->stmt.decl.name);
					symbolTable.top().insert(node->stmt.decl.name);					
					break;
				case ast_asgn:
					rset.insert(node->stmt.asgn.lhs);
					rset.insert(node->stmt.asgn.rhs);
					break;
				case ast_call:
					if (node->stmt.call.param != nullptr) {
						rset.insert(node->stmt.call.param);
					}				
					break;
				case ast_ret:
					rset.insert(node->stmt.ret.expr);
					break;
				default:
					break;
			}
			break;
		case ast_var:
			checkVar(node->var.name);
			break;
		case ast_bexpr:
			rset.insert(node->bexpr.lhs);
			rset.insert(node->bexpr.rhs);
			break;
		case ast_rexpr:
			rset.insert(node->rexpr.lhs);
			rset.insert(node->rexpr.rhs);
			break;
		default:
			break;
	}
	return rset;
}

// decls check (each var declared once per scope)
void checkDecl(string varName) {
	for (const string& sym : symbolTable.top()) {
		if (varName.compare(sym.c_str()) == 0) {
			errs += 1;
			printf("error: '%s' has already been declared within this scope\n", sym.c_str());
			return;
		}
	}
}

// var check (must be declared before use)
void checkVar(string varName) {
	stack<set<string>> tableCpy = symbolTable;
	while (!tableCpy.empty()) {
		set<string> currSet = tableCpy.top();
		for (const string& sym : currSet) {
			if (varName.compare(sym.c_str()) == 0) {
				return;
			}
		}
		tableCpy.pop();
	}
	errs += 1;
	printf("error: '%s' not declared before use\n", varName.c_str());
}
