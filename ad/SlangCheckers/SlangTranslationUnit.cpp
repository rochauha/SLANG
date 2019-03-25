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

#define DONT_PRINT "DONT_PRINT"

using namespace slang;

SlangVar::SlangVar() {}

SlangVar::SlangVar(uint64_t id, std::string name) {
    // specially for anonymous field names (needed in member expressions)
    this->id = id;
    this->name = name;
    this->typeStr = DONT_PRINT;
}

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
    currBbId = 0;
    nextBbId = 0;
}

slang::SlangTranslationUnit::SlangTranslationUnit()
    : currFunc{nullptr}, varMap{}, funcMap{}, mainStack{}, dirtyVars{}, edgeLabels{3} {
    fileName = "";
    edgeLabels[FalseEdge] = "FalseEdge";
    edgeLabels[TrueEdge] = "TrueEdge";
    edgeLabels[UnCondEdge] = "UnCondEdge";
}

void slang::SlangTranslationUnit::clearMainStack() { mainStack.clear(); }

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

void slang::SlangTranslationUnit::setFuncReturnType(std::string &retType) {
    currFunc->retType = retType;
}

void slang::SlangTranslationUnit::setVariadicness(bool variadic) { currFunc->variadic = variadic; }

std::string slang::SlangTranslationUnit::getCurrFuncName() {
    return currFunc->name; // not fullName
}

void slang::SlangTranslationUnit::setCurrBb(const CFGBlock *bb) {
    currFunc->currBbId = bb->getBlockID();
    currFunc->currBb = bb;
}

int32_t slang::SlangTranslationUnit::getCurrBbId() { return currFunc->currBbId; }

void slang::SlangTranslationUnit::setNextBbId(int32_t nextBbId) { currFunc->nextBbId = nextBbId; }

int32_t slang::SlangTranslationUnit::genNextBbId() {
    currFunc->nextBbId += 1;
    return currFunc->nextBbId;
}

const CFGBlock *slang::SlangTranslationUnit::getCurrBb() { return currFunc->currBb; }

SlangVar &slang::SlangTranslationUnit::getVar(uint64_t varAddr) {
    // FIXME: there is no check
    return varMap[varAddr];
}

void slang::SlangTranslationUnit::setLastDeclStmtTo(const Stmt *declStmt) {
    currFunc->lastDeclStmt = declStmt;
}

const Stmt *slang::SlangTranslationUnit::getLastDeclStmt() const { return currFunc->lastDeclStmt; }

bool slang::SlangTranslationUnit::isNewVar(uint64_t varAddr) {
    return varMap.find(varAddr) == varMap.end();
}

uint32_t slang::SlangTranslationUnit::nextTmpId() {
    currFunc->tmpVarCount += 1;
    return currFunc->tmpVarCount;
}

/// Add a new basic block with the given bbId
void slang::SlangTranslationUnit::addBb(int32_t bbId) {
    std::vector<std::string> emptyVector;
    currFunc->bbStmts[bbId] = emptyVector;
}

void slang::SlangTranslationUnit::setCurrBbId(int32_t bbId) { currFunc->currBbId = bbId; }

// bb must already be added
void slang::SlangTranslationUnit::addBbStmt(std::string stmt) {
    currFunc->bbStmts[currFunc->currBbId].push_back(stmt);
}

// bb must already be added
void slang::SlangTranslationUnit::addBbStmts(std::vector<std::string> &slangStmts) {
    for (std::string slangStmt : slangStmts) {
        currFunc->bbStmts[currFunc->currBbId].push_back(slangStmt);
    }
}

// bb must already be added
void slang::SlangTranslationUnit::addBbStmt(int32_t bbId, std::string slangStmt) {
    currFunc->bbStmts[bbId].push_back(slangStmt);
}

// bb must already be added
void slang::SlangTranslationUnit::addBbStmts(int32_t bbId, std::vector<std::string> &slangStmts) {
    for (std::string slangStmt : slangStmts) {
        currFunc->bbStmts[bbId].push_back(slangStmt);
    }
}

void slang::SlangTranslationUnit::addBbEdge(
    std::pair<int32_t, std::pair<int32_t, EdgeLabel>> bbEdge) {
    currFunc->bbEdges.push_back(bbEdge);
}

void slang::SlangTranslationUnit::addVar(uint64_t varId, SlangVar &slangVar) {
    varMap[varId] = slangVar;
}

// BOUND START: record_related_routines

bool SlangTranslationUnit::isRecordPresent(uint64_t recordAddr) {
    return !(recordMap.find(recordAddr) == recordMap.end());
}

void SlangTranslationUnit::addRecord(uint64_t recordAddr, SlangRecord slangRecord) {
    recordMap[recordAddr] = slangRecord;
}

SlangRecord &SlangTranslationUnit::getRecord(uint64_t recordAddr) { return recordMap[recordAddr]; }

int32_t SlangTranslationUnit::getNextRecordId() {
    recordId += 1;
    return recordId;
}

std::string SlangTranslationUnit::getNextRecordIdStr() {
    std::stringstream ss;
    ss << getNextRecordId();
    return ss.str();
}

// BOUND END  : record_related_routines

// BOUND START: SlangRecordField_functions

SlangRecordField::SlangRecordField() : anonymous{false}, name{""}, typeStr{""}, type{QualType()} {}

std::string SlangRecordField::getName() const { return name; }

std::string SlangRecordField::toString() {
    std::stringstream ss;
    ss << "("
       << "\"" << name << "\"";
    ss << ", " << typeStr << ")";
    return ss.str();
}

void SlangRecordField::clear() {
    anonymous = false;
    name = "";
    typeStr = "";
    type = QualType();
}

// BOUND END  : SlangRecordField_functions

// BOUND START: SlangRecord_functions

// SlangRecord_functions
SlangRecord::SlangRecord() {
    recordKind = Struct; // Struct, or Union
    anonymous = false;
    name = "";
    nextAnonymousFieldId = 0;
}

std::string SlangRecord::getNextAnonymousFieldIdStr() {
    std::stringstream ss;
    nextAnonymousFieldId += 1;
    ss << nextAnonymousFieldId;
    return ss.str();
}

std::vector<SlangRecordField> SlangRecord::getFields() const { return fields; }

std::string SlangRecord::toString() {
    std::stringstream ss;
    ss << NBSP6;
    ss << ((recordKind == Struct) ? "types.Struct(\n" : "types.Union(\n");

    ss << NBSP8 << "name = ";
    ss << "\"" << name << "\""
       << ",\n";

    std::string suffix = ",\n";
    ss << NBSP8 << "fields = [\n";
    for (auto field : fields) {
        ss << NBSP10 << field.toString() << suffix;
    }
    ss << NBSP8 << "],\n";

    ss << NBSP8 << "loc = " << locStr << ",\n";
    ss << NBSP6 << ")"; // close types.*(...

    return ss.str();
}

std::string SlangRecord::toShortString() {
    std::stringstream ss;

    if (recordKind == Struct) {
        ss << "types.Struct";
    } else {
        ss << "types.Union";
    }
    ss << "(\"" << name << "\")";

    return ss.str();
}

// BOUND END  : SlangRecord_functions

// BOUND START: dirtyVars

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

void slang::SlangTranslationUnit::clearDirtyVars() { dirtyVars.clear(); }

// BOUND END  : dirtyVars

// BOUND START: conversion_routines 1 to SPAN Strings

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

std::string slang::SlangTranslationUnit::convertBbEdges(SlangFunc &slangFunc) {
    std::stringstream ss;

    for (auto p : slangFunc.bbEdges) {
        ss << NBSP10 << "(" << std::to_string(p.first);
        ss << ", " << std::to_string(p.second.first) << ", ";
        ss << "types." << edgeLabels[p.second.second] << "),\n";
    }

    return ss.str();
} // convertBbEdges()

// BOUND END  : conversion_routines 1 to SPAN Strings

// BOUND START: helper_functions for tib

// used only for debugging purposes
void slang::SlangTranslationUnit::printMainStack() const {
    std::stringstream ss;
    ss << "MAIN_STACK: [";
    for (const Stmt *stmt : mainStack) {
        ss << stmt->getStmtClassName() << ", ";
    }
    ss << "]\n";
    SLANG_DEBUG(ss.str());
} // printMainStack()

void slang::SlangTranslationUnit::pushToMainStack(const Stmt *stmt) {
    mainStack.push_back(stmt);
} // pushToMainStack()

const Stmt *slang::SlangTranslationUnit::popFromMainStack() {
    if (mainStack.size()) {
        auto stmt = mainStack[mainStack.size() - 1];
        mainStack.pop_back();
        return stmt;
    }
    return nullptr;
} // popFromMainStack()

bool slang::SlangTranslationUnit::isMainStackEmpty() const {
    return mainStack.empty();
} // isMainStackEmpty()

// BOUND END  : helper_functions for tib

// BOUND START: dumping_routines

// dump entire span ir module for the translation unit.
void slang::SlangTranslationUnit::dumpSlangIr() {
    std::stringstream ss;

    dumpHeader(ss);
    dumpVariables(ss);
    dumpObjs(ss);
    dumpFooter(ss);

    // TODO: print the content to a file.
    std::string fileName = this->fileName + ".spanir";
    Util::writeToFile(fileName, ss.str());
    llvm::errs() << ss.str();
} // dumpSlangIr()

void slang::SlangTranslationUnit::dumpHeader(std::stringstream &ss) {
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
    ss << "# from span.ir.types import Loc\n";
    ss << "\n";
    ss << "# An instance of span.ir.tunit.TUnit class.\n";
    ss << "irTUnit.TUnit(\n";
    ss << NBSP2 << "name = \"" << fileName << "\",\n";
    ss << NBSP2 << "description = \"Auto-Translated from Clang AST.\",\n";
} // dumpHeader()

void slang::SlangTranslationUnit::dumpFooter(std::stringstream &ss) {
    ss << ") # irTUnit.TUnit() ends\n";
    ss << "\n# END  : A_SPAN_translation_unit.\n";
} // dumpFooter()

void slang::SlangTranslationUnit::dumpVariables(std::stringstream &ss) {
    ss << "\n";
    ss << NBSP2 << "allVars = {\n";
    for (const auto &var : varMap) {
        if (var.second.typeStr == DONT_PRINT)
            continue;
        ss << NBSP4;
        ss << "\"" << var.second.name << "\": " << var.second.typeStr << ",\n";
    }
    ss << NBSP2 << "}, # end allVars dict\n\n";
} // dumpVariables()

void slang::SlangTranslationUnit::dumpObjs(std::stringstream &ss) {
    ss << NBSP2 << "allObjs = {\n";
    dumpRecords(ss);
    dumpFunctions(ss);
    ss << NBSP2 << "}, # end allObjs dict\n";
}

void slang::SlangTranslationUnit::dumpRecords(std::stringstream &ss) {
    for (auto slangRecord : recordMap) {
        ss << NBSP4;
        ss << "\"" << slangRecord.second.name << "\":\n";
        ss << slangRecord.second.toString();
        ss << ",\n\n";
    }
    ss << "\n";
}

void slang::SlangTranslationUnit::dumpFunctions(std::stringstream &ss) {
    std::string prefix;
    for (auto slangFunc : funcMap) {
        ss << NBSP4; // indent
        ss << "\"" << slangFunc.second.fullName << "\":\n";
        ss << NBSP6 << "obj.Func(\n";

        // fields
        ss << NBSP8 << "name = "
           << "\"" << slangFunc.second.fullName << "\",\n";
        ss << NBSP8 << "paramNames = [";
        prefix = "";
        for (std::string &paramName : slangFunc.second.paramNames) {
            ss << prefix << "\"" << paramName << "\"";
            if (prefix.size() == 0) {
                prefix = ", ";
            }
        }
        ss << "],\n";
        ss << NBSP8 << "variadic = " << (slangFunc.second.variadic ? "True" : "False") << ",\n";

        // ss << NBSP8 << "paramTypes = [";
        // prefix = "";
        // for (std::string& paramType: slangFunc.second.sig.paramTypes) {
        //     ss << prefix << paramType;
        //     if (prefix.size() == 0) {
        //         prefix = ", ";
        //     }
        // }
        // ss << "],\n";

        ss << NBSP8 << "returnType = " << slangFunc.second.retType << ",\n";

        // field: basicBlocks
        ss << "\n";
        ss << NBSP8 << "# Note: -1 is always start/entry BB. (REQUIRED)\n";
        ss << NBSP8 << "# Note: 0 is always end/exit BB (REQUIRED)\n";
        ss << NBSP8 << "basicBlocks = {\n";
        for (auto bb : slangFunc.second.bbStmts) {
            ss << NBSP10 << bb.first << ": [\n";
            if (bb.second.size()) {
                for (auto &stmt : bb.second) {
                    ss << NBSP12 << stmt << ",\n";
                }
            } else {
                ss << NBSP12 << "instr.NopI()"
                   << ",\n";
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
        ss << NBSP6 << "), # " << slangFunc.second.fullName << "() end. \n\n";
    }
} // dumpFunctions()

// BOUND END  : dumping_routines
