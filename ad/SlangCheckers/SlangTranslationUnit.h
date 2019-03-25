//===----------------------------------------------------------------------===//
//  MIT License.
//  Copyright (c) 2019 The SLANG Authors.
//
//  Author: Ronak Chauhan (r.chauhan@somaiya.edu)
//  Author: Anshuman Dhuliya (dhuliya@cse.iitb.ac.in)
//
//===----------------------------------------------------------------------===//
// Store translation unit information.
//===----------------------------------------------------------------------===//

#ifndef SLANG_TRANSLATIONUNIT_H
#define SLANG_TRANSLATIONUNIT_H

#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include "clang/AST/Stmt.h"
#include "clang/Analysis/CFG.h"

// int span_add_nums(int a, int b);

// non-breaking space
#define NBSP1 " "
#define NBSP2 NBSP1 NBSP1
#define NBSP4 NBSP2 NBSP2
#define NBSP6 NBSP2 NBSP4
#define NBSP8 NBSP4 NBSP4
#define NBSP10 NBSP4 NBSP6
#define NBSP12 NBSP6 NBSP6

#define VAR_NAME_PREFIX "v:"
#define FUNC_NAME_PREFIX "f:"

// using namespace slang;

//===----------------------------------------------------------------------===//
// SlangTranslationUnit
// Information is stored while traversing the function's CFG. Specifically,
// 1. The information of the variables used.
// 2. The CFG and the basic block structure.
// 3. The three address code representation.
//===----------------------------------------------------------------------===//
namespace slang {
// the numbering 0,1,2 is important.
enum EdgeLabel { FalseEdge = 0, TrueEdge = 1, UnCondEdge = 2 };
enum SlangRecordKind { Struct = 0, Union = 1 };

class SlangVar {
  public:
    uint64_t id;
    // variable name: e.g. a variable 'x' in main function, is "v:main:x".
    std::string name;
    std::string typeStr;

    SlangVar();
    SlangVar(uint64_t id, std::string name);
    std::string convertToString();
    void setLocalVarName(std::string varName, std::string funcName);
    void setGlobalVarName(std::string varName);
};

class SlangFuncSig {
  public:
    std::string retType;
    std::vector<std::string> paramTypes;
};

class SlangFunc {
  public:
    std::string name;     // e.g. 'main'
    std::string fullName; // e.g. 'f:main'
    std::string retType;
    std::vector<std::string> paramNames;
    bool variadic;

    uint32_t tmpVarCount;
    int32_t currBbId;
    int32_t nextBbId;
    const CFGBlock *currBb; // the current bb being converted
    const Stmt *lastDeclStmt;

    // stores bbEdges(s); entry bb id is mapped to -1
    std::vector<std::pair<int32_t, std::pair<int32_t, EdgeLabel>>> bbEdges;
    // stmts in bb; entry bb id is mapped to -1, others remain the same
    std::unordered_map<int32_t, std::vector<std::string>> bbStmts;

    SlangFunc();
};

class SlangRecordField {
  public:
    bool anonymous;
    std::string name;
    std::string typeStr;
    QualType type;

    // SlangRecordField_functions
    SlangRecordField();
    std::string getName() const;
    std::string toString();
    void clear();
};

class SlangRecord {
  public:
    SlangRecordKind recordKind; // Struct, or Union
    bool anonymous;
    std::string name;
    std::vector<SlangRecordField> fields;
    std::string locStr;
    int32_t nextAnonymousFieldId;

    // SlangRecord_functions
    SlangRecord();
    std::vector<SlangRecordField> getFields() const;
    std::string getNextAnonymousFieldIdStr();
    std::string toShortString();
    std::string toString();
};

class SlangTranslationUnit {
  public:
    /**
     * Points to the current function object, to which all instructions are
     * to be added.
     */
    std::string fileName;

    SlangFunc *currFunc;
    int32_t recordId; // used to generate names for anonymous records (see getNextRecordId())

    // maps a unique variable id to its SlangVar.
    std::unordered_map<uint64_t, SlangVar> varMap;
    // map of var-name to a count: used in case two local variables have same name (blocks)
    std::unordered_map<std::string, int32_t> varNameMap;
    // contains functions
    std::unordered_map<uint64_t, SlangFunc> funcMap;
    // contains structs
    std::unordered_map<uint64_t, SlangRecord> recordMap;

    // stack to help convert ast structure to 3-address code.
    std::vector<const Stmt *> mainStack;
    // tracks variables that become dirty in an expression
    std::unordered_map<uint64_t, SlangExpr> dirtyVars;

    std::vector<std::string> edgeLabels;

    SlangTranslationUnit();
    void clear();

    /**
     * Adds function to the funcMap and sets currFunc pointer to the new object.
     * All instructions are added to the currFunc after this call.
     * @param funcName
     */
    // void addFunction(std::string funcName);

    /**
     * @return name of the current function.
     */
    std::string getCurrFuncName();

    /**
     * Appends the function parameters.
     */
    void pushBackFuncParams(std::string param);

    /**
     * @param varAddr
     * @return True if var is not in varMap.
     */
    bool isNewVar(uint64_t varAddr);

    void addBb(int32_t bbId);
    void addBbStmt(std::string stmt);
    void addBbStmt(int32_t bbId, std::string stmt);
    void addBbStmts(std::vector<std::string> &slangStmts);
    void addBbStmts(int32_t bbId, std::vector<std::string> &slangStmts);
    void addBbEdge(std::pair<int32_t, std::pair<int32_t, EdgeLabel>> bbEdge);
    void setNextBbId(int32_t nextBbId);
    int32_t genNextBbId();
    void addVar(uint64_t varId, SlangVar &slangVar);
    SlangVar &getVar(uint64_t varAddr);

    uint32_t nextTmpId();
    void setFuncReturnType(std::string &retType);
    void setVariadicness(bool variadic);

    void setCurrBbId(int32_t bbId);
    int32_t getCurrBbId();
    void setCurrBb(const CFGBlock *bb);
    const CFGBlock *getCurrBb();

    // for handling locations for temporaries in InitListExprs
    void setLastDeclStmtTo(const Stmt *declStmt);
    const Stmt *getLastDeclStmt() const;

    // record_related_routines
    bool isRecordPresent(uint64_t recordAddr);
    void addRecord(uint64_t recordAddr, SlangRecord slangRecord);
    SlangRecord &getRecord(uint64_t recordAddr);
    int32_t getNextRecordId();
    std::string getNextRecordIdStr();

    // conversion_routines 1 to SPAN Strings
    std::string convertFuncName(std::string funcName);
    std::string convertVarExpr(uint64_t varAddr);
    std::string convertLocalVarName(std::string varName);
    std::string convertGlobalVarName(std::string varName);
    std::string convertBbEdges(SlangFunc &slangFunc);

    // dirtyVars convenience functions
    void setDirtyVar(uint64_t varId, SlangExpr slangExpr);
    bool isDirtyVar(uint64_t varId);
    SlangExpr getTmpVarForDirtyVar(uint64_t varId);
    void clearDirtyVars();
    void clearMainStack();

    // SPAN IR dumping_routines
    void dumpSlangIr();
    void dumpHeader(std::stringstream &ss);
    void dumpFooter(std::stringstream &ss);
    void dumpVariables(std::stringstream &ss);
    void dumpObjs(std::stringstream &ss);
    void dumpFunctions(std::stringstream &ss);
    void dumpRecords(std::stringstream &ss);

    // helper_functions for tui
    void printMainStack() const;
    void pushToMainStack(const Stmt *stmt);
    const Stmt *popFromMainStack();
    bool isMainStackEmpty() const;

    // // For Program state purpose.
    // // Overload the == operator
    // bool operator==(const SlangTranslationUnit &tui) const;
    // // LLVMs equivalent of a hash function
    // void Profile(llvm::FoldingSetNodeID &ID) const;
}; // SlangTranslationUnit class
} // namespace slang

// // For Program state's purpose: not in use currently.
// // Overload the == operator
// bool SlangTranslationUnit::operator==(const SlangTranslationUnit &tui) const { return id ==
// tui.id; }
// // LLVMs equivalent of a hash function
// void SlangTranslationUnit::Profile(llvm::FoldingSetNodeID &ID) const { ID.AddInteger(1); }

#endif // SLANG_TRANSLATIONUNIT_H
