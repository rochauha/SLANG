//===----------------------------------------------------------------------===//
//  MIT License.
//  Copyright (c) 2019 The SLANG Authors.
//
//  Author: Ronak Chauhan (r.chauhan@somaiya.edu)
//  Author: Anshuman Dhuliya (dhuliya@cse.iitb.ac.in)
//
//AD If SlangGenChecker class name is added or changed, then also edit,
//AD ../../../include/clang/StaticAnalyzer/Checkers/Checkers.td
//
//===----------------------------------------------------------------------===//
//

#include "ClangSACheckers.h"
#include "clang/AST/Expr.h" //AD
#include "clang/AST/Stmt.h" //AD
#include "clang/AST/Type.h" //AD
#include "clang/AST/Decl.h" //AD
//AD #include "clang/AST/ASTContext.h" //AD
//AD #include "clang/Analysis/Analyses/Dominators.h"
//AD #include "clang/Analysis/Analyses/LiveVariables.h"
//AD #include "clang/Analysis/CallGraph.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/AnalysisManager.h"
//AD #include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
//AD #include "clang/StaticAnalyzer/Core/PathSensitive/ExplodedGraph.h"
//AD #include "clang/StaticAnalyzer/Core/PathSensitive/ExprEngine.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/raw_ostream.h" //AD
#include <string>                     //AD
#include <vector>                     //AD
#include <utility>                    //AD
#include <unordered_map>              //AD
#include <fstream>                    //AD
#include <sstream>                    //AD

using namespace clang;
using namespace ento;

// non-breaking space
#define NBSP2  "  "
#define NBSP4  "    "
#define NBSP6  "      "
#define NBSP8  "        "
#define NBSP10 "          "

#define VAR_NAME_PREFIX "v:"
#define FUNC_NAME_PREFIX "f:"

// #define LOG_ME(X) if (Utility::LS) Utility::log((X), __FUNCTION__, __LINE__)

namespace {
    // Expr class to store converted expression info.
    class SpanExpr {
    public:
        std::string expr;
        bool compound;
        QualType qualType;
        std::vector<std::string> spanStmts;

        bool nonTmpVar;
        uint64_t varId;

        uint64_t locId; // line_32 | col_32

        SpanExpr() {
            expr = "";
            compound = true;

            nonTmpVar = false;
            varId = 0;
        }

        SpanExpr(std::string e, bool compnd, QualType qt) {
            expr = e;
            compound = compnd;
            qualType = qt;
        }

        void printExpr() {
            llvm::errs() << "SpanExpr(";
            llvm::errs() << expr << ", " << compound << ", ";
            qualType.dump();
            llvm::errs() << ")\n";
        }

        void addSpanStmt(std::string spanStmt) {
            spanStmts.push_back(spanStmt);
        }

        void addSpanStmts(std::vector<std::string>& spanStmts) {
            for (std::string spanStmt: spanStmts) {
                this->spanStmts.push_back(spanStmt);
            }
        }

        bool isNonTmpVar() {
            return nonTmpVar;
        }
    };
}

//===----------------------------------------------------------------------===//
// FIXME: Utility Class
// Some useful static utility functions in a class.
//===----------------------------------------------------------------------===//
namespace {
    class Utility {
    public:
        static void readFile1();
        static bool LS; // Log Switch
        // static void log(std::string msg, std::string func_name, uint32_t line);
    };
}

bool Utility::LS = true;

// void Utility::log(std::string msg, std::string func_name, uint32_t line) {
//     llvm::errs() << "SLANG: " << func_name << ":" << line << " " << msg << "\n";
// }

// code to read file, just for reference
void Utility::readFile1() {
    std::ifstream inputTxtFile;
    std::string line;
    std::string fileName("/home/codeman/.itsoflife/local/tmp/checker-input.txt");
    inputTxtFile.open(fileName);
    if (inputTxtFile.is_open()) {
      while(std::getline(inputTxtFile, line)) {
          llvm::errs() << line << "\n";
      }
      inputTxtFile.close();
    } else {
        llvm::errs() << "SLANG: ERROR: Cannot open file '" << fileName << "'\n";
    }
}

//===----------------------------------------------------------------------===//
// TraversedInfoBuffer
// Information is stored while traversing the function's CFG. Specifically,
// 1. The information of the variables used.
// 2. The CFG and the basic block structure.
// 3. The three address code representation.
//===----------------------------------------------------------------------===//
namespace {
    enum EdgeLabel {FalseEdge = 0, TrueEdge = 1, UnCondEdge = 2};

    class VarInfo {
    public:
        uint64_t id;
        // variable name: e.g. a variable 'x' in main function, is "v:main:x".
        std::string var_name;
        std::string type_str;

        std::string convertToString() {
            std::stringstream ss;
            ss << "\"" << var_name << "\": " << type_str << ",";
            return ss.str();
        }
    };

    class TraversedInfoBuffer {
    public:
        int id;
        uint32_t tmp_var_counter;
        int32_t curr_bb_id;

        Decl *D;

        std::string func_name;
        std::string func_ret_t;
        std::string func_params;

        // stack to help convert ast structure to 3-address code.
        std::vector<const Stmt*> main_stack;

        // maps a unique variable id to its VarInfo.
        std::unordered_map<uint64_t , VarInfo> var_map;
        std::unordered_map<uint64_t, SpanExpr> dirtyVars;
        // stores bb_edge(s); entry bb id is mapped to -1
        std::vector<std::pair<int32_t, std::pair<int32_t , EdgeLabel>>> bb_edges;
        // stmts in bb; entry bb id is mapped to -1
        std::unordered_map<int32_t , std::vector<std::string>> bb_stmts;

        std::vector<std::string> edge_labels;

        TraversedInfoBuffer();
        void clear();

        uint32_t nextTmpCount();
        SpanExpr genTmpVariable(QualType qt);

        // conversion_routines 1 to SPAN Strings
        std::string convertClangType(QualType qt);
        std::string convertFuncName(std::string func_name);
        std::string convertVarExpr(uint64_t var_addr);
        std::string convertLocalVarName(std::string var_name);
        std::string convertGlobalVarName(std::string var_name);
        std::string convertBbEdges();

        // dirtyVars convenience functions
        void setDirtyVar(uint64_t varId);
        bool isDirtyVar(uint64_t varId);
        SpanExpr getTmpVarForDirtyVar(uint64_t varId, QualType qualType, bool& newTmp);
        void clearDirtyVars();
        void clearMainStack();

        // SPAN IR dumping_routines
        void dumpSpanIr();
        void dumpHeader();
        void dumpVariables();
        void dumpFunctions();
        void dumpFooter();

        // helper_functions for tib
        void printMainStack() const;
        void pushToMainStack(const Stmt *stmt);
        const Stmt* popFromMainStack();
        bool isMainStackEmpty() const;

        // // For Program state purpose.
        // // Overload the == operator
        // bool operator==(const TraversedInfoBuffer &tib) const;
        // // LLVMs equivalent of a hash function
        // void Profile(llvm::FoldingSetNodeID &ID) const;
    };
}

TraversedInfoBuffer::TraversedInfoBuffer(): id{1}, tmp_var_counter{}, main_stack{},
    var_map{}, dirtyVars{}, bb_edges{}, bb_stmts{}, edge_labels{3} {

    edge_labels[FalseEdge] = "FalseEdge";
    edge_labels[TrueEdge] = "TrueEdge";
    edge_labels[UnCondEdge] = "UnCondEdge";
}

void TraversedInfoBuffer::clearMainStack() {
    main_stack.clear();
}

uint32_t TraversedInfoBuffer::nextTmpCount() {
    tmp_var_counter += 1;
    return tmp_var_counter;
}

// clear the buffer for the next function.
void TraversedInfoBuffer::clear() {
    func_name = "";
    func_ret_t = "";
    func_params = "";
    curr_bb_id = 0;
    tmp_var_counter = 0;

    var_map.clear();
    dirtyVars.clear();
    bb_edges.clear();
    bb_stmts.clear();
    clearMainStack();
} // clear()

SpanExpr TraversedInfoBuffer::genTmpVariable(QualType qt) {
    std::stringstream ss;
    SpanExpr spanExpr{};
    // STEP 1: generate the name.
    uint32_t var_id = nextTmpCount();
    ss << VAR_NAME_PREFIX << func_name << ":t." << var_id;

    // STEP 2: register the tmp var and its type.
    VarInfo varInfo{};
    varInfo.var_name = ss.str();
    varInfo.type_str = convertClangType(qt);

    // STEP 3: Add to the var map.
    // FIXME: The 'var_id' here should be small enough to not interfere with uint64_t addresses.
    var_map[var_id] = varInfo;

    // STEP 4: generate var expression.
    ss.str(""); // empty the stream
    ss << "expr.VarE(\"" << varInfo.var_name << "\")";

    spanExpr.expr = ss.str();
    spanExpr.compound = false;
    spanExpr.qualType = qt;
    spanExpr.nonTmpVar = false;

    return spanExpr;
} // genTmpVariable()

//BOUND START: dirtyVars

void TraversedInfoBuffer::setDirtyVar(uint64_t varId) {
    // Clear the value for varId to an empty SpanExpr.
    // This forces the creation of a new tmp var,
    // whenever getTmpVarForDirtyVar() is called.
    SpanExpr spanExpr{};
    dirtyVars[varId] = spanExpr;
}

bool TraversedInfoBuffer::isDirtyVar(uint64_t varId) {
    if (dirtyVars.find(varId) == dirtyVars.end()) {
        return false;
    }
    return true;
}

// returns an empty SpanExpr if var is not dirty
SpanExpr TraversedInfoBuffer::getTmpVarForDirtyVar(uint64_t varId, QualType qualType,
        bool& newTmp) {
    SpanExpr spanExpr;
    newTmp = false;

    if (!isDirtyVar(varId)) {
        return spanExpr;
    } else if(dirtyVars[varId].expr.size() == 0) {
        newTmp = true;
        // allocate tmp var on demand
        dirtyVars[varId] = genTmpVariable(qualType);
    }

    spanExpr = dirtyVars[varId];
    return spanExpr;
}

void TraversedInfoBuffer::clearDirtyVars() {
    dirtyVars.clear();
}

//BOUND END  : dirtyVars

//BOUND START: conversion_routines 1 to SPAN Strings

std::string TraversedInfoBuffer::convertFuncName(std::string func_name) {
    std::stringstream ss;
    ss << FUNC_NAME_PREFIX << func_name;
    return ss.str();
}

std::string TraversedInfoBuffer::convertGlobalVarName(std::string var_name) {
    std::stringstream ss;
    ss << VAR_NAME_PREFIX << var_name;
    return ss.str();
}

std::string TraversedInfoBuffer::convertLocalVarName(std::string var_name) {
    // assumes func_name is set
    std::stringstream ss;
    ss << VAR_NAME_PREFIX << func_name << ":" << var_name;
    return ss.str();
}

std::string TraversedInfoBuffer::convertVarExpr(uint64_t var_addr) {
    // if here, var should already be in var_map
    std::stringstream ss;

    auto vinfo = var_map[var_addr];
    ss << vinfo.var_name;

    return ss.str();
}

// Converts Clang Types into SPAN parsable type strings:
// Examples,
//   for `int` it returns `types.Int`.
//   for `int*` it returns `types.Ptr(to=types.Int)`.
// TODO: handle all possible types
std::string TraversedInfoBuffer::convertClangType(QualType qt) {
    std::stringstream ss;
    const Type *type = qt.getTypePtr();
    if (type->isBuiltinType()) {
        if(type->isIntegerType()) {
            if(type->isCharType()) {
                ss << "types.Char";
            } else {
                ss << "types.Int";
            }
        } else if(type->isFloatingType()) {
            ss << "types.Float";
        } else if(type->isVoidType()) {
            ss << "types.Void";
        } else {
            ss << "UnknownBuiltinType.";
        }
    } else if(type->isPointerType()) {
        ss << "types.Ptr(to=";
        QualType pqt = type->getPointeeType();
        ss << convertClangType(pqt);
        ss << ")";
    } else {
        ss << "UnknownType.";
    }

    return ss.str();
} // convertClangType()

std::string TraversedInfoBuffer::convertBbEdges() {
    std::stringstream ss;

    for (auto p: bb_edges) {
        ss << NBSP8 << "graph.BbEdge(" << std::to_string(p.first);
        ss << ", " << std::to_string(p.second.first) << ", ";
        ss << "graph." << edge_labels[p.second.second] << "),\n";
    }

    return ss.str();
} // convertBbEdges()

//BOUND END  : conversion_routines 1 to SPAN Strings

//BOUND START: helper_functions for tib

// used only for debugging purposes
void TraversedInfoBuffer::printMainStack() const {
    if (Utility::LS) {
        llvm::errs() << "MAIN_STACK: [";
        for (auto stmt: main_stack) {
            llvm::errs() << stmt->getStmtClassName() << ", ";
        }
        llvm::errs() << "]\n";
    }
} // printMainStack()

void TraversedInfoBuffer::pushToMainStack(const Stmt *stmt) {
    main_stack.push_back(stmt);
} // pushToMainStack()

const Stmt* TraversedInfoBuffer::popFromMainStack() {
    if(main_stack.size()) {
        auto stmt = main_stack[main_stack.size() - 1];
        main_stack.pop_back();
        return stmt;
    }
    return nullptr;
} // popFromMainStack()

bool TraversedInfoBuffer::isMainStackEmpty() const {
    return main_stack.empty();
} // isMainStackEmpty()

//BOUND END  : helper_functions for tib

//BOUND START: dumping_routines

// dump entire span ir module for the translation unit.
void TraversedInfoBuffer::dumpSpanIr() {
    dumpHeader();
    dumpVariables();
    dumpFunctions();
    dumpFooter();
} // dumpSpanIr()

void TraversedInfoBuffer::dumpVariables() {
    llvm::errs() << "all_vars: Dict[types.VarNameT, types.ReturnT] = {\n";
    for (auto var: var_map) {
        // with indent of two spaces
        llvm::errs() << "  ";
        llvm::errs() << "\"" << var.second.var_name
                     << "\": " << var.second.type_str << ",\n";
    }
    llvm::errs() << "} # end all_vars dict\n\n";
} // dumpVariables()

void TraversedInfoBuffer::dumpHeader() {
    std::stringstream ss;

    ss << "#!/usr/bin/env python3\n";
    ss << "\n";
    ss << "# MIT License.\n";
    ss << "# Copyright (c) 2019 The SLANG Authors.\n";
    ss << "\n";
    ss << "\"\"\"\n";
    ss << "Slang (SPAN IR) program.\n";
    ss << "\"\"\"\n";
    ss << "\n";
    ss << "from typing import Dict\n";
    ss << "\n";
    ss << "import span.ir.types as types\n";
    ss << "import span.ir.expr as expr\n";
    ss << "import span.ir.instr as instr\n";
    ss << "\n";
    ss << "import span.sys.graph as graph\n";
    ss << "import span.sys.universe as universe\n";
    ss << "\n";
    ss << "# analysis unit name\n";
    ss << "name = \"SLANG\"\n";
    ss << "description = \"Auto-Translated from Clang AST.\"\n";
    ss << "\n";

    llvm::errs() << ss.str();
} // dumpHeader()

void TraversedInfoBuffer::dumpFooter() {
    std::stringstream ss;
    ss << "\n";
    ss << "# Always build the universe from a 'program module'.\n";
    ss << "# Initialize the universe with program in this module.\n";
    ss << "universe.build(name, description, all_vars, all_func)\n";
    llvm::errs() << ss.str();
} // dumpFooter()

void TraversedInfoBuffer::dumpFunctions() {
    llvm::errs() << "all_func: Dict[types.FuncNameT, graph.FuncNode] = {\n";

    llvm::errs() << NBSP2; // indent
    llvm::errs() << "\"" << convertFuncName(func_name) << "\":\n";
    llvm::errs() << NBSP4 << "graph.FuncNode(\n";

    // fields
    llvm::errs() << NBSP6 << "name= " << "\"" << convertFuncName(func_name) << "\",\n";
    llvm::errs() << NBSP6 << "params= [" << func_params << "],\n";
    llvm::errs() << NBSP6 << "returns= " << func_ret_t << ",\n";

    // fields: basic_blocks
    llvm::errs() << "\n";
    llvm::errs() << NBSP6 << "# if -1, its start_block. (REQUIRED)\n";
    llvm::errs() << NBSP6 << "# if  0, its end_block. (REQUIRED)\n";
    llvm::errs() << NBSP6 << "basic_blocks= {\n";
    for (auto bb : bb_stmts) {
        llvm::errs() << NBSP8 << bb.first << ": graph.BB([\n";
        if (bb.second.size()) {
            for (auto stmt: bb.second) {
                llvm::errs() << NBSP10 << stmt << ",\n";
            }
        } else {
            llvm::errs() << NBSP10 << "instr.NopI()" << ",\n";
        }
        llvm::errs() << NBSP8 << "]),\n";
    }
    llvm::errs() << NBSP6 << "}, # basic_blocks end.\n";

    // fields: bb_edges
    llvm::errs() << "\n";
    llvm::errs() << NBSP6 << "bb_edges= [\n";
    llvm::errs() << convertBbEdges();
    llvm::errs() << NBSP6 << "],\n";

    // close this function data structure
    llvm::errs() << NBSP4 << "), # " << convertFuncName(func_name) << "() end. \n\n";

    // close all_func
    llvm::errs() << "} # end all_func dict.\n";
} // dumpFunctions()

//BOUND END  : dumping_routines

// // For Program state's purpose: not in use currently.
// // Overload the == operator
// bool TraversedInfoBuffer::operator==(const TraversedInfoBuffer &tib) const { return id == tib.id; }
// // LLVMs equivalent of a hash function
// void TraversedInfoBuffer::Profile(llvm::FoldingSetNodeID &ID) const { ID.AddInteger(1); }


//===----------------------------------------------------------------------===//
// SlangGenChecker
//===----------------------------------------------------------------------===//

namespace {
    class SlangGenChecker : public Checker<check::ASTCodeBody> {
        static TraversedInfoBuffer tib;

    public:
        // mainstart
        void checkASTCodeBody(const Decl *D, AnalysisManager &mgr,
                              BugReporter &BR) const;

        // handling_routines
        void handleFunctionDef(const FunctionDecl *D) const;
        void handleCfg(const CFG *cfg) const;
        void handleBbInfo(const CFGBlock *bb, const CFG *cfg) const;
        void handleBbStmts(const CFGBlock *bb) const;
        void handleStmt(const Stmt *stmt) const;
        void handleVariable(const ValueDecl *valueDecl) const;
        void handleDeclStmt(const DeclStmt *declStmt) const;
        void handleDeclRefExpr(const DeclRefExpr *DRE) const;
        void handleBinaryOperator(const BinaryOperator *binOp) const;
        void handleReturnStmt() const;
        void handleIfStmt() const;

        // helper_functions
        void addStmtToCurrBlock(std::string stmt) const;
        void addSpanStmtsToCurrBlock(std::vector<std::string>& spanStmts) const;
        bool isTopLevel(const Stmt* stmt) const;
        uint64_t getLocationId(const Stmt *stmt) const;

        // conversion_routines to SpanExpr
        SpanExpr convertAssignment(bool compound_receiver) const;
        SpanExpr convertIntegerLiteral(const IntegerLiteral *IL) const;
        // a function, if stmt, *y on lhs, arr[i] on lhs are examples of a compound_receiver.
        SpanExpr convertExpr(bool compound_receiver) const;
        SpanExpr convertDeclRefExpr(const DeclRefExpr *dre) const;
        SpanExpr convertVarDecl(const VarDecl *varDecl) const;
        SpanExpr convertUnaryOp(const UnaryOperator *unOp, bool compound_receiver) const;
        SpanExpr convertUnaryIncDec(const UnaryOperator *unOp, bool compound_receiver) const;
        SpanExpr convertBinaryOp(const BinaryOperator *binOp, bool compound_receiver) const;
        void adjustDirtyVar(SpanExpr& spanExpr) const;

    }; // class SlangGenChecker
} // anonymous namespace

TraversedInfoBuffer SlangGenChecker::tib = TraversedInfoBuffer();

// mainstart, main entry point. Invokes top level Function and Cfg handlers.
// It is invoked once for each source translation unit function.
void SlangGenChecker::checkASTCodeBody(const Decl *D, AnalysisManager &mgr,
                                   BugReporter &BR) const {
    Utility::readFile1();
    llvm::errs() << "\nBOUND START: SLANG_Generated_Output.\n";

    tib.clear(); // clear the buffer for this function.
    tib.D = const_cast<Decl*>(D);

    const FunctionDecl *func_decl = dyn_cast<FunctionDecl>(D);
    handleFunctionDef(func_decl);

    if (const CFG *cfg = mgr.getCFG(D)) {
        handleCfg(cfg);
        tib.dumpSpanIr();
    } else {
        llvm::errs() << "SLANG: ERROR: No CFG for function.\n";
    }

    llvm::errs() << "\nBOUND END  : SLANG_Generated_Output.\n";
} // checkASTCodeBody()

//BOUND START: handling_routines

void SlangGenChecker::handleCfg(const CFG *cfg) const {
    for (const CFGBlock *bb : *cfg) {
        handleBbInfo(bb, cfg);
        handleBbStmts(bb);
    }
} // handleCfg()

// Gets the function name, paramaters and return type.
void SlangGenChecker::handleFunctionDef(const FunctionDecl *func_decl) const {
    // STEP 1.1: Get function name.
    tib.func_name = func_decl->getNameInfo().getAsString();

    // STEP 1.2: Get function parameters.
    std::stringstream ss;
    std::string prefix = "";
    if (func_decl->doesThisDeclarationHaveABody()) { //& !func_decl->hasPrototype())
        for (unsigned i = 0, e = func_decl->getNumParams(); i != e; ++i) {
            const ParmVarDecl *paramVarDecl = func_decl->getParamDecl(i);
            handleVariable(paramVarDecl);
            if (i != 0) {
                prefix = ", ";
            }
            ss << prefix << "\"" << tib.convertVarExpr((uint64_t)paramVarDecl) << "\"";
        }
        tib.func_params = ss.str();
    }

    // STEP 1.3: Get function return type.
    const QualType returnQType = func_decl->getReturnType();
    tib.func_ret_t = tib.convertClangType(returnQType);
} // handleFunctionDef()

void SlangGenChecker::handleBbInfo(const CFGBlock *bb, const CFG *cfg) const {
    int32_t succ_id, bb_id;
    unsigned entry_bb_id = cfg->getEntry().getBlockID();

    if ((bb_id = bb->getBlockID()) == (int32_t)entry_bb_id) {
        bb_id = -1; //entry block is ided -1.
    }

    tib.curr_bb_id = bb_id;
    std::vector<std::string> empty_vector;
    tib.bb_stmts[bb_id] = empty_vector;

    llvm::errs() << "BB" << bb_id << "\n";

    if (bb == &cfg->getEntry()) {
        llvm::errs() << "ENTRY BB\n";
    } else if (bb == &cfg->getExit()) {
        llvm::errs() << "EXIT BB\n";
    }

    // access and record successor blocks
    const Stmt *stmt = (bb->getTerminator()).getStmt();
    if (stmt && (isa<IfStmt>(stmt) || isa<WhileStmt>(stmt))) {
        // if here, then only conditional edges are present
        bool true_edge = true;
        if (bb->succ_size() > 2) {
            llvm::errs() << "SPAN: ERROR: 'If' has more than two successors.\n";
        }

        for (CFGBlock::const_succ_iterator I = bb->succ_begin();
             I != bb->succ_end(); ++I) {

            CFGBlock *succ = *I;
            if ((succ_id = succ->getBlockID()) == (int32_t)entry_bb_id) {
                succ_id = -1;
            }

            if (true_edge) {
                tib.bb_edges.push_back(std::make_pair(bb_id, std::make_pair(succ_id, TrueEdge)));
                true_edge = false;
            } else {
                tib.bb_edges.push_back(std::make_pair(bb_id, std::make_pair(succ_id, FalseEdge)));
            }
        }
    } else {
        // if here, then only unconditional edges are present
        if (!bb->succ_empty()) {
            // num. of succ: bb->succ_size()
            for (CFGBlock::const_succ_iterator I = bb->succ_begin();
                    I != bb->succ_end(); ++I) {
                CFGBlock *succ = *I;
                if (!succ) {
                    // unreachable block ??
                    succ = I->getPossiblyUnreachableBlock();
                    llvm::errs() << "(Unreachable BB)";
                    continue;
                }

                if ((succ_id = succ->getBlockID()) == (int32_t)entry_bb_id) {
                    succ_id = -1;
                }

                tib.bb_edges.push_back(std::make_pair(bb_id, std::make_pair(succ_id, UnCondEdge)));
            }
        }
    }
} // handleBbInfo()

void SlangGenChecker::handleBbStmts(const CFGBlock *bb) const {
    for (auto elem : *bb) {
        // ref: https://clang.llvm.org/doxygen/CFG_8h_source.html#l00056
        // ref for printing block:
        // https://clang.llvm.org/doxygen/CFG_8cpp_source.html#l05234

        Optional<CFGStmt> CS = elem.getAs<CFGStmt>();
        const Stmt *stmt = CS->getStmt();
        handleStmt(stmt);

        if (isTopLevel(stmt)) {
            tib.clearDirtyVars();
            // tib.clearMainStack();
        }
    } // for (auto elem : *bb)

    // get terminator
    const Stmt *stmt = (bb->getTerminator()).getStmt();
    if (stmt) { handleStmt(stmt); }

    llvm::errs() << "\n\n";
} // handleBbStmts()

void SlangGenChecker::handleStmt(const Stmt *stmt) const {
    Stmt::StmtClass stmt_cls = stmt->getStmtClass();

    tib.printMainStack();
    llvm::errs() << "Processing: " << stmt->getStmtClassName() << "\n";

    // to handle each kind of statement/expression.
    switch (stmt_cls) {
        default: {
            // push to stack by default.
            tib.pushToMainStack(stmt);

            llvm::errs() << "SLANG: DEFAULT: Pushed to stack: "
                         << stmt->getStmtClassName() << ".\n";
            stmt->dump();
            llvm::errs() << "\n";
            break;
        }

        case Stmt::DeclRefExprClass: {
            handleDeclRefExpr(cast<DeclRefExpr>(stmt));
            break;
        }

        case Stmt::DeclStmtClass: {
            handleDeclStmt(cast<DeclStmt>(stmt));
            break;
        }

        case Stmt::BinaryOperatorClass: {
            handleBinaryOperator(cast<BinaryOperator>(stmt));
            break;
        }

        case Stmt::ReturnStmtClass: { handleReturnStmt(); break; }

        case Stmt::WhileStmtClass: // same as Stmt::IfStmtClass
        case Stmt::IfStmtClass: { handleIfStmt(); break; }

        case Stmt::ImplicitCastExprClass: {
            // do nothing
            break;
        }
    }
} // handleStmt()

// record the variable name and type
void SlangGenChecker::handleVariable(const ValueDecl *valueDecl) const {
    uint64_t var_id = (uint64_t) valueDecl;
    if (tib.var_map.find(var_id) == tib.var_map.end()) {
        // seeing the variable for the first time.
        VarInfo varInfo{};
        varInfo.id = var_id;
        const VarDecl *varDecl = dyn_cast<VarDecl>(valueDecl);
        if (varDecl) {
            if (varDecl->hasLocalStorage()) {
                varInfo.var_name = tib.convertLocalVarName(valueDecl->getNameAsString());
            } else if(varDecl->hasGlobalStorage()) {
                varInfo.var_name = tib.convertGlobalVarName(valueDecl->getNameAsString());
            } else if (varDecl->hasExternalStorage()) {
                llvm::errs() << "SLANG: ERROR: External Storage Not Handled.\n";
            } else {
                llvm::errs() << "SLANG: ERROR: Unknown variable storage.\n";
            }
        } else {
            llvm::errs() << "SLANG: ERROR: ValueDecl not a VarDecl!\n";
        }
        varInfo.type_str = tib.convertClangType(valueDecl->getType());
        tib.var_map[var_id] = varInfo;
        llvm::errs() << "NEW_VAR: " << varInfo.convertToString() << "\n";
    } else {
        llvm::errs() << "SEEN_VAR: " << tib.var_map[var_id].convertToString() << "\n";
    }
} // handleVariable()

void SlangGenChecker::handleDeclStmt(const DeclStmt *declStmt) const {
    // assumes there is only single decl inside (the likely case).
    std::stringstream ss;

    const VarDecl *varDecl = cast<VarDecl>(declStmt->getSingleDecl());
    handleVariable(varDecl);

    if (!tib.isMainStackEmpty()) {
        // there is smth on the stack, hence on the rhs.
        SpanExpr spanExpr{};
        auto exprLhs = convertVarDecl(varDecl);
        exprLhs.locId = getLocationId(declStmt);
        auto exprRhs = convertExpr(exprLhs.compound);

        // order_correction for DeclStmt
        spanExpr.addSpanStmts(exprRhs.spanStmts);
        spanExpr.addSpanStmts(exprLhs.spanStmts);

        // spanExpr.qualType = exprLhs.qualType;
        ss << "instr.AssignI(" << exprLhs.expr << ", " << exprRhs.expr << ")";
        spanExpr.addSpanStmt(ss.str());

        addSpanStmtsToCurrBlock(spanExpr.spanStmts);
    }
} // handleDeclStmt()

void SlangGenChecker::handleIfStmt() const {
    std::stringstream ss;

    auto exprArg = convertExpr(true);
    ss << "instr.CondI(" << exprArg.expr << ")";

    // order_correction for if stmt
    exprArg.addSpanStmt(ss.str());
    addSpanStmtsToCurrBlock(exprArg.spanStmts);
} // handleIfStmt()

void SlangGenChecker::handleReturnStmt() const {
    std::stringstream ss;

    if (!tib.isMainStackEmpty()) {
        // return has an argument
        auto exprArg = convertExpr(true);
        ss << "instr.ReturnI(" << exprArg.expr << ")";

        // order_correction for return stmt
        exprArg.addSpanStmt(ss.str());
        addSpanStmtsToCurrBlock(exprArg.spanStmts);
    } else {
        ss << "instr.ReturnI()";
        addStmtToCurrBlock(ss.str()); // okay
    }
} // handleReturnStmt()

void SlangGenChecker::handleDeclRefExpr(const DeclRefExpr *declRefExpr) const {
    tib.pushToMainStack(declRefExpr);

    const ValueDecl *valueDecl = declRefExpr->getDecl();
    if (isa<VarDecl>(valueDecl)) {
        handleVariable(valueDecl);
    } else {
        llvm::errs() << "SLANG: ERROR: handleDeclRefExpr: unhandled "
                     << declRefExpr->getStmtClassName() << "\n";
    }
} // handleDeclRefExpr()

void SlangGenChecker::handleBinaryOperator(const BinaryOperator *binOp) const {
    if (binOp->isAssignmentOp() && isTopLevel(binOp)) {
        SpanExpr spanExpr = convertAssignment(false); // top level is never compound
        addSpanStmtsToCurrBlock(spanExpr.spanStmts);
    } else {
        tib.pushToMainStack(binOp);
    }
} // handleBinaryOperator()

//BOUND END  : handling_routines

//BOUND START: conversion_routines to SpanExpr

// convert top of stack to SPAN IR.
// returns converted string, and false if the converted string is only a simple const/var expression.
SpanExpr SlangGenChecker::convertExpr(bool compound_receiver) const {
    std::stringstream ss;

    const Stmt *stmt = tib.popFromMainStack();

    switch(stmt->getStmtClass()) {
        case Stmt::IntegerLiteralClass: {
            const IntegerLiteral *il = cast<IntegerLiteral>(stmt);
            return convertIntegerLiteral(il);
        }

        case Stmt::DeclRefExprClass: {
            const DeclRefExpr *declRefExpr = cast<DeclRefExpr>(stmt);
            return convertDeclRefExpr(declRefExpr);
        }

        case Stmt::BinaryOperatorClass: {
            const BinaryOperator *binOp = cast<BinaryOperator>(stmt);
            return convertBinaryOp(binOp, compound_receiver);
        }

        case Stmt::UnaryOperatorClass: {
            auto unOp = cast<UnaryOperator>(stmt);
            return convertUnaryOp(unOp, compound_receiver);
        }

        default: {
            // error state
            llvm::errs() << "SLANG: ERROR: convertExpr: " << stmt->getStmtClassName() << "\n";
            stmt->dump();
            return SpanExpr("ERROR:convertExpr", false, QualType());
        }
    }
    // return SpanExpr("ERROR:convertExpr", false, QualType());
} // convertExpr()

SpanExpr SlangGenChecker::convertIntegerLiteral(const IntegerLiteral *il) const {
    std::stringstream ss;

    bool is_signed = il->getType()->isSignedIntegerType();
    ss << "expr.Lit(" << il->getValue().toString(10, is_signed) << ")";
    llvm::errs() << ss.str() << "\n";

    return SpanExpr(ss.str(), false, il->getType());
} // convertIntegerLiteral()

SpanExpr SlangGenChecker::convertAssignment(bool compound_receiver) const {
    std::stringstream ss;
    SpanExpr spanExpr{};

    auto exprLhs = convertExpr(false); // unconditionally false
    auto exprRhs = convertExpr(exprLhs.compound);

    if (compound_receiver && exprLhs.compound) {
        spanExpr = tib.genTmpVariable(exprLhs.qualType);

        // order_correction for assignment
        spanExpr.addSpanStmts(exprRhs.spanStmts);
        spanExpr.addSpanStmts(exprLhs.spanStmts);

        ss << "instr.AssignI(" << exprLhs.expr << ", " << exprRhs.expr << ")";
        spanExpr.addSpanStmt(ss.str());

        ss.str(""); // empty the stream
        ss << "instr.AssignI(" << spanExpr.expr << ", " << exprLhs.expr << ")";
        spanExpr.addSpanStmt(ss.str());
    } else {
        // order_correction for assignment
        spanExpr.addSpanStmts(exprRhs.spanStmts);
        spanExpr.addSpanStmts(exprLhs.spanStmts);

        ss << "instr.AssignI(" << exprLhs.expr << ", " << exprRhs.expr << ")";
        spanExpr.addSpanStmt(ss.str());

        spanExpr.expr = exprLhs.expr;
        spanExpr.qualType = exprLhs.qualType;
        spanExpr.compound = exprLhs.compound;
        spanExpr.nonTmpVar = exprLhs.nonTmpVar;
        spanExpr.varId = exprLhs.varId;

    }

    if (spanExpr.nonTmpVar) {
        tib.setDirtyVar(spanExpr.varId);
    }

    return spanExpr;
}

void SlangGenChecker::adjustDirtyVar(SpanExpr& spanExpr) const {
    std::stringstream ss;
    bool newTmp;
    if (spanExpr.isNonTmpVar() && tib.isDirtyVar(spanExpr.varId)) {
        SpanExpr sp = tib.getTmpVarForDirtyVar(spanExpr.varId, spanExpr.qualType, newTmp);
        if (newTmp) {
            // only add if a new temporary was generated
            ss << "instr.AssignI(" << sp.expr << ", " << spanExpr.expr << ")";
            spanExpr.addSpanStmt(ss.str());
        }
        spanExpr.expr = sp.expr;
        spanExpr.nonTmpVar = false;
    }
}

SpanExpr SlangGenChecker::convertBinaryOp(const BinaryOperator *binOp,
        bool compound_receiver) const {
    std::stringstream ss;
    std::string op;
    SpanExpr varExpr{};

    if (binOp->isAssignmentOp()) {
        return convertAssignment(compound_receiver);
    }

    SpanExpr exprR = convertExpr(true);
    SpanExpr exprL = convertExpr(true);
    adjustDirtyVar(exprL);

    if (compound_receiver) {
        varExpr = tib.genTmpVariable(exprL.qualType);
        ss << "instr.AssignI(" << varExpr.expr << ", ";
    }

    // order_correction binary operator
    varExpr.addSpanStmts(exprL.spanStmts);
    varExpr.addSpanStmts(exprR.spanStmts);

    varExpr.qualType = exprL.qualType;

    switch(binOp->getOpcode()) {
        default: {
            llvm::errs() << "SLANG: ERROR: convertBinaryOp: " << binOp->getOpcodeStr() << "\n";
            return SpanExpr("ERROR:convertBinaryOp", false, QualType());
        }

        case BO_Rem: { op = "op.Modulo"; break; }
        case BO_Add: { op = "op.Add"; break; }
        case BO_Sub: { op = "op.Sub"; break;}
        case BO_Mul: { op = "op.Mul"; break;}
        case BO_Div: { op = "op.Div"; break;}
    }

    ss << "expr.BinaryE(" << exprL.expr << ", " << op << ", " << exprR.expr << ")";

    if (compound_receiver) {
        ss << ")"; // close instr.AssignI(...
        varExpr.addSpanStmt(ss.str());
    } else {
        varExpr.expr = ss.str();
        varExpr.compound = true; // since a binary expression
    }

    return varExpr;
}

SpanExpr SlangGenChecker::convertUnaryOp(const UnaryOperator *unOp,
        bool compound_receiver) const {
    std::stringstream ss;
    std::string op;
    SpanExpr varExpr{};
    QualType qualType;

    switch (unOp->getOpcode()) {
        // special handling
        case UO_PreInc:
        case UO_PreDec:
        case UO_PostInc:
        case UO_PostDec: {
            return convertUnaryIncDec(unOp, compound_receiver);
        }

        default: {
            break;
        }
    }

    SpanExpr exprArg = convertExpr(true);
    adjustDirtyVar(exprArg);
    qualType = exprArg.qualType;

    switch(unOp->getOpcode()) {
        default: {
            llvm::errs() << "SLANG: ERROR: convertUnaryOp: "
                         << unOp->getOpcodeStr(unOp->getOpcode()) << "\n";
            return SpanExpr("ERROR:convertUnaryOp", false, QualType());
        }

        case UO_AddrOf: {
            qualType = tib.D->getASTContext().getPointerType(exprArg.qualType);
            op = "op.AddrOf";
            break;
        }

        case UO_Deref: {
            auto ptr_type = cast<PointerType>(exprArg.qualType.getTypePtr());
            qualType = ptr_type->getPointeeType();
            op = "op.Deref";
            break;
        }

        case UO_Minus: { op = "op.Minus"; break;}
        case UO_Plus: { op = "op.Plus"; break;}
    }

    ss << "expr.UnaryE(" << op << ", " << exprArg.expr << ")";

    if (compound_receiver) {
        varExpr = tib.genTmpVariable(qualType);
        ss << "instr.AssignI(" << varExpr.expr << ", ";
    }

    // order_correction unary operator
    varExpr.addSpanStmts(exprArg.spanStmts);

    if (compound_receiver) {
        ss << ")"; // close instr.AssignI(...
        varExpr.addSpanStmt(ss.str());
    } else {
        varExpr.expr = ss.str();
        varExpr.compound = true;
        varExpr.qualType = qualType;
    }
    return varExpr;
} // convertUnaryOp()

SpanExpr SlangGenChecker::convertUnaryIncDec(const UnaryOperator *unOp,
                                             bool compound_receiver) const {
    std::stringstream ss;
    SpanExpr exprArg = convertExpr(true);

    switch(unOp->getOpcode()) {
        case UO_PreInc: {
            ss << "instr.AssignI(" << exprArg.expr << ", ";
            ss << "expr.BinaryE(" << exprArg.expr << ", op.Add, expr.LitE(1)))";
            exprArg.addSpanStmt(ss.str());

            uint64_t varId = exprArg.varId;
            if (exprArg.nonTmpVar && tib.isDirtyVar(exprArg.varId)) {
                adjustDirtyVar(exprArg);
            }
            tib.setDirtyVar(varId);
            break;
        }

        case UO_PostInc: {
            ss << "instr.AssignI(" << exprArg.expr << ", ";
            ss << "expr.BinaryE(" << exprArg.expr << ", op.Add, expr.LitE(1)))";

            if (exprArg.nonTmpVar) {
                tib.setDirtyVar(exprArg.varId);
                adjustDirtyVar(exprArg);
            }
            // add increment after storing in temporary
            exprArg.addSpanStmt(ss.str());
            break;
        }

        default: {
            llvm::errs() << "SLANG: ERROR.convertUnaryIncDec unknown op\n";
            break;
        }
    }

    return exprArg;
} // convertUnaryIncDec()

SpanExpr SlangGenChecker::convertVarDecl(const VarDecl *varDecl) const {
    std::stringstream ss;
    SpanExpr spanExpr{};

    ss << "expr.VarE(\"" << tib.convertVarExpr((uint64_t)varDecl) << "\")";
    spanExpr.expr = ss.str();
    spanExpr.compound = false;
    spanExpr.qualType = varDecl->getType();
    spanExpr.nonTmpVar = true;
    spanExpr.varId = (uint64_t)varDecl;

    return spanExpr;
}

SpanExpr SlangGenChecker::convertDeclRefExpr(const DeclRefExpr *dre) const {
    std::stringstream ss;

    const ValueDecl *valueDecl = dre->getDecl();
    if (isa<VarDecl>(valueDecl)) {
        auto varDecl = cast<VarDecl>(valueDecl);
        SpanExpr spanExpr = convertVarDecl(varDecl);
        spanExpr.locId = getLocationId(dre);
        return spanExpr;
    } else {
        llvm::errs() << "SLANG: ERROR: " << __func__ << ": Not a VarDecl.";
        return SpanExpr("ERROR:convertDeclRefExpr", false, QualType());
    }
}

//BOUND END  : conversion_routines to SpanExpr

//BOUND START: helper_functions

void SlangGenChecker::addStmtToCurrBlock(std::string stmt) const {
    tib.bb_stmts[tib.curr_bb_id].push_back(stmt);
}

void SlangGenChecker::addSpanStmtsToCurrBlock(std::vector<std::string>& spanStmts) const {
    for (std::string spanStmt: spanStmts) {
        tib.bb_stmts[tib.curr_bb_id].push_back(spanStmt);
    }
}

// get and encode the location of a statement element
uint64_t SlangGenChecker::getLocationId(const Stmt *stmt) const {
    uint64_t locId = 0;

    locId |= tib.D->getASTContext().getSourceManager().getExpansionLineNumber(stmt->getLocStart());
    locId <<= 32;
    locId |= tib.D->getASTContext().getSourceManager().getExpansionColumnNumber(stmt->getLocStart());

    return locId; // line_32 | col_32
} // getLocationId()

// If an element is top level, return true.
// e.g. in statement "x = y = z = 10;" the first "=" from left is top level.
bool SlangGenChecker::isTopLevel(const Stmt* stmt) const {
    // see MyTraverseAST.cpp for more details.
    const auto &parents = tib.D->getASTContext().getParents(*stmt);
    if (!parents.empty()) {
        const Stmt *stmt1 = parents[0].get<Stmt>();
        if (stmt1) {
            // llvm::errs() << "Parent: " << stmt1->getStmtClassName() << "\n";
            switch (stmt1->getStmtClass()) {
                default: {
                    return false;
                    break;
                }
                case Stmt::CaseStmtClass:
                case Stmt::DefaultStmtClass:
                case Stmt::CompoundStmtClass: {
                    return true; // top level
                }
            }
        } else {
            // llvm::errs() << "Parent: Cannot print.\n";
            return false;
        }
    } else {
        return true; // top level
    }
} // isTopLevel()

//BOUND END  : helper_functions

// Register the Checker
void ento::registerSlangGenChecker(CheckerManager &mgr) {
    mgr.registerChecker<SlangGenChecker>();
}

