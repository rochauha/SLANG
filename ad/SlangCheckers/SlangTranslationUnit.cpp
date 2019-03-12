//===----------------------------------------------------------------------===//
//  MIT License.
//  Copyright (c) 2019 The SLANG Authors.
//
//  Author: Anshuman Dhuliya (dhuliya@cse.iitb.ac.in)
//
//===----------------------------------------------------------------------===//
// Store translation unit information.
//===----------------------------------------------------------------------===//

#include "SlangUtil.h"
#include "SlangExpr.h"
#include "SlangTranslationUnit.h"
#include "clang/Analysis/CFG.h"

using namespace slang;

std::string slang::SlangVar::convertToString() {
    std::stringstream ss;
    ss << "\"" << name << "\": " << typeStr << ",";
    return ss.str();
}

void slang::SlangVar::setLocalVarName(std::string varName, std::string funcName) {
    name = VAR_NAME_PREFIX;
    name += funcName + ":" + varName;
}

void slang::SlangVar::setGlobalVarName(std::string varName) {
    name = VAR_NAME_PREFIX;
    name += varName;
}

slang::SlangFunc::SlangFunc() {
    paramNames = std::vector<std::string>{};
    tmpVarCount = 0;
}

slang::SlangTranslationUnit::SlangTranslationUnit(): currFunc{nullptr}, varMap{}, funcMap{},
                                         mainStack{}, dirtyVars{}, edgeLabels{3} {
    fileName = "";
    nextBbId = 0;
    edgeLabels[FalseEdge] = "FalseEdge";
    edgeLabels[TrueEdge] = "TrueEdge";
    edgeLabels[UnCondEdge] = "UnCondEdge";
}

void slang::SlangTranslationUnit::clearMainStack() {
    mainStack.clear();
}

// clear the buffer for the next function.
void slang::SlangTranslationUnit::clear() {
    varMap.clear();
    dirtyVars.clear();
    clearMainStack();
} // clear()

void slang::SlangTranslationUnit::pushBackFuncParams(std::string paramName) {
    SLANG_TRACE("AddingParam: " << paramName << " to func " << currFunc->name)
    currFunc->paramNames.push_back(paramName);
}

void slang::SlangTranslationUnit::setFuncReturnType(std::string& retType) {
    currFunc->retType = retType;
}

void slang::SlangTranslationUnit::setVariadicness(bool variadic) {
    currFunc->variadic = variadic;
}

std::string slang::SlangTranslationUnit::getCurrFuncName() {
    return currFunc->name; // not fullName
}

void slang::SlangTranslationUnit::setCurrBb(const CFGBlock *bb) {
    currBbId = bb->getBlockID();
    currBb = bb;
}

int32_t slang::SlangTranslationUnit::getCurrBbId() {
    return currBbId;
}

void slang::SlangTranslationUnit::setNextBbId(int32_t nextBbId) {
    this->nextBbId = nextBbId;
}

int32_t slang::SlangTranslationUnit::genNextBbId() {
    nextBbId += 1;
    return nextBbId;
}

const CFGBlock* slang::SlangTranslationUnit::getCurrBb() {
    return currBb;
}

SlangVar& slang::SlangTranslationUnit::getVar(uint64_t varAddr) {
    //FIXME: there is no check
    return varMap[varAddr];
}

bool slang::SlangTranslationUnit::isNewVar(uint64_t varAddr) {
    return varMap.find(varAddr) == varMap.end();
}

uint32_t slang::SlangTranslationUnit::nextTmpId() {
    currFunc->tmpVarCount += 1;
    return currFunc->tmpVarCount;
}

/// Add a new basic block with bbId, and set currBbId
void slang::SlangTranslationUnit::addBb(int32_t bbId) {
    std::vector<std::string> emptyVector;
    currFunc->bbStmts[bbId] = emptyVector;
}

void slang::SlangTranslationUnit::setCurrBbId(int32_t bbId) {
    currBbId = bbId;
}

// bb must already be added
void slang::SlangTranslationUnit::addBbStmt(std::string stmt) {
    currFunc->bbStmts[currBbId].push_back(stmt);
}

// bb must already be added
void slang::SlangTranslationUnit::addBbStmts(std::vector<std::string>& slangStmts) {
    for (std::string slangStmt: slangStmts) {
        currFunc->bbStmts[currBbId].push_back(slangStmt);
    }
}

// bb must already be added
void slang::SlangTranslationUnit::addBbStmt(int32_t bbId, std::string slangStmt) {
    currFunc->bbStmts[bbId].push_back(slangStmt);
}

// bb must already be added
void slang::SlangTranslationUnit::addBbStmts(int32_t bbId, std::vector<std::string>& slangStmts) {
    for (std::string slangStmt: slangStmts) {
        currFunc->bbStmts[bbId].push_back(slangStmt);
    }
}

void slang::SlangTranslationUnit::addBbEdge(std::pair<int32_t,
        std::pair<int32_t, EdgeLabel>> bbEdge) {
    currFunc->bbEdges.push_back(bbEdge);
}

void slang::SlangTranslationUnit::addVar(uint64_t varId, SlangVar& slangVar) {
    varMap[varId] = slangVar;
}

//BOUND START: dirtyVars

void slang::SlangTranslationUnit::setDirtyVar(uint64_t varId, SlangExpr slangExpr) {
    // Clear the value for varId to an empty SlangExpr.
    // This forces the creation of a new tmp var,
    // whenever getTmpVarForDirtyVar() is called.
    dirtyVars[varId] = slangExpr;
}

// If this function is called dirtyVar dict should already have the entry.
SlangExpr slang::SlangTranslationUnit::getTmpVarForDirtyVar(uint64_t varId) {
    return dirtyVars[varId];
}

bool slang::SlangTranslationUnit::isDirtyVar(uint64_t varId) {
    return !(dirtyVars.find(varId) == dirtyVars.end());
}

void slang::SlangTranslationUnit::clearDirtyVars() {
    dirtyVars.clear();
}

//BOUND END  : dirtyVars

//BOUND START: conversion_routines 1 to SPAN Strings

std::string slang::SlangTranslationUnit::convertFuncName(std::string funcName) {
    std::stringstream ss;
    ss << FUNC_NAME_PREFIX << funcName;
    return ss.str();
}

std::string slang::SlangTranslationUnit::convertVarExpr(uint64_t varAddr) {
    // if here, var should already be in varMap
    std::stringstream ss;

    auto slangVar = varMap[varAddr];
    ss << slangVar.name;

    return ss.str();
}

std::string slang::SlangTranslationUnit::convertBbEdges(SlangFunc& slangFunc) {
    std::stringstream ss;

    for (auto p: slangFunc.bbEdges) {
        ss << NBSP10 << "(" << std::to_string(p.first);
        ss << ", " << std::to_string(p.second.first) << ", ";
        ss << "types." << edgeLabels[p.second.second] << "),\n";
    }

    return ss.str();
} // convertBbEdges()

//BOUND END  : conversion_routines 1 to SPAN Strings

//BOUND START: helper_functions for tib

// used only for debugging purposes
void slang::SlangTranslationUnit::printMainStack() const {
    SLANG_DEBUG("MAIN_STACK: [");
    for (const Stmt* stmt: mainStack) {
        SLANG_DEBUG(stmt->getStmtClassName() << ", ");
    }
    SLANG_DEBUG("]\n");
} // printMainStack()

void slang::SlangTranslationUnit::pushToMainStack(const Stmt *stmt) {
    mainStack.push_back(stmt);
} // pushToMainStack()

const Stmt* slang::SlangTranslationUnit::popFromMainStack() {
    if(mainStack.size()) {
        auto stmt = mainStack[mainStack.size() - 1];
        mainStack.pop_back();
        return stmt;
    }
    return nullptr;
} // popFromMainStack()

bool slang::SlangTranslationUnit::isMainStackEmpty() const {
    return mainStack.empty();
} // isMainStackEmpty()

//BOUND END  : helper_functions for tib

//BOUND START: dumping_routines

// dump entire span ir module for the translation unit.
void slang::SlangTranslationUnit::dumpSlangIr() {
    std::stringstream ss;

    dumpHeader(ss);
    dumpVariables(ss);
    dumpObjs(ss);
    dumpFooter(ss);

    //TODO: print the content to a file.
    llvm::errs() << ss.str();
} // dumpSlangIr()

void slang::SlangTranslationUnit::dumpHeader(std::stringstream& ss) {
    ss << "\n";
    ss << "# START: A_SPAN_translation_unit.\n";
    ss << "\n";
    ss << "# eval() the contents of this file.\n";
    ss << "# Keep the following imports in effect when calling eval.\n";
    ss << "\n";
    ss << "# import span.ir.types as types\n";
    ss << "# import span.ir.expr as expr\n";
    ss << "# import span.ir.instr as instr\n";
    ss << "# import span.ir.obj as obj\n";
    ss << "# import span.ir.tunit as irTUnit\n";
    ss << "\n";
    ss << "# An instance of span.ir.tunit.TUnit class.\n";
    ss << "irTUnit.TUnit(\n";
    ss << NBSP2 << "name = \"" << fileName << "\",\n";
    ss << NBSP2 << "description = \"Auto-Translated from Clang AST.\",\n";
} // dumpHeader()

void slang::SlangTranslationUnit::dumpFooter(std::stringstream& ss) {
    ss << ") # irTUnit.TUnit() ends\n";
    ss << "\n# END  : A_SPAN_translation_unit.\n";
} // dumpFooter()

void slang::SlangTranslationUnit::dumpVariables(std::stringstream& ss) {
    ss << NBSP2 << "allVars = {\n";
    for (const auto& var: varMap) {
        ss << NBSP4;
        ss << "\"" << var.second.name << "\":"
                   << var.second.typeStr << ",\n";
    }
    ss << NBSP2 << "}, # end allVars dict\n\n";
} // dumpVariables()

void slang::SlangTranslationUnit::dumpObjs(std::stringstream& ss) {
    ss << NBSP2 << "allObjs = {\n";
    dumpFunctions(ss);
    ss << "}, # end allObjs dict\n";
}

void slang::SlangTranslationUnit::dumpFunctions(std::stringstream& ss) {
    std::string prefix;
    for (auto slangFunc: funcMap) {
        ss << NBSP4; // indent
        ss << "\"" << slangFunc.second.fullName << "\":\n";
        ss << NBSP6 << "obj.Func(\n";

        // fields
        ss << NBSP8 << "name = " << "\"" << slangFunc.second.fullName << "\",\n";
        ss << NBSP8 << "paramsNames = [";
        prefix = "";
        for (std::string& paramName: slangFunc.second.paramNames) {
            ss << prefix << paramName;
            if (prefix.size() == 0) {
                prefix = ", ";
            }
        }
        ss << "]\n";
        ss << NBSP8 << "variadic = "
           << (slangFunc.second.variadic? "True" : "False") << ",\n";

        // ss << NBSP8 << "paramTypes = [";
        // prefix = "";
        // for (std::string& paramType: slangFunc.second.sig.paramTypes) {
        //     ss << prefix << paramType;
        //     if (prefix.size() == 0) {
        //         prefix = ", ";
        //     }
        // }
        // ss << "]\n";

        ss << NBSP8 << "returnType = " << slangFunc.second.retType << ",\n";

        // field: basicBlocks
        ss << "\n";
        ss << NBSP8 << "# Note: -1 is always start/entry BB. (REQUIRED)\n";
        ss << NBSP8 << "# Note: 0 is always end/exit BB (REQUIRED)\n";
        ss << NBSP8 << "basicBlocks = {\n";
        for (auto bb : slangFunc.second.bbStmts) {
            ss << NBSP10 << bb.first << ": [\n";
            if (bb.second.size()) {
                for (auto& stmt: bb.second) {
                    ss << NBSP12 << stmt << ",\n";
                }
            } else {
                ss << NBSP12 << "instr.NopI()" << ",\n";
            }
            ss << NBSP10 << "],\n";
            ss << "\n";
        }
        ss << NBSP8 << "}, # basicBlocks end.\n";

        // fields: bbEdges
        ss << "\n";
        ss << NBSP8 << "bbEdges= {\n";
        ss << convertBbEdges(slangFunc.second);
        ss << NBSP8 << "}, # bbEdges end\n";

        // close this function object
        ss << NBSP6 << "), # " << slangFunc.second.fullName
           << "() end. \n\n";
    }
} // dumpFunctions()

//BOUND END  : dumping_routines

