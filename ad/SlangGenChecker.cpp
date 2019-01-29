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
#include "clang/AST/Decl.h" //AD
#include "clang/AST/Expr.h" //AD
#include "clang/AST/Stmt.h" //AD
#include "clang/Analysis/Analyses/Dominators.h"
#include "clang/Analysis/Analyses/LiveVariables.h"
#include "clang/Analysis/CallGraph.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/AnalysisManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ExplodedGraph.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ExprEngine.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/raw_ostream.h" //AD
#include <string>                     //AD
#include <vector>                     //AD
#include <utility>                    //AD
#include <unordered_map>              //AD
#include <fstream>                    //AD
#include <sstream>                    //AD
#include <stack>                      //AD

using namespace clang;
using namespace ento;

// #define LOG_ME(X) if (Utility::debug_mode) Utility::log((X), __FUNCTION__, __LINE__)

//===----------------------------------------------------------------------===//
// FIXME: Utility Class
// Some useful static utility functions in a class.
//===----------------------------------------------------------------------===//
namespace {
    class Utility {
    public:
        static void readFile1();
        static bool debug_mode;
        static bool LS; // Log Switch
        static void log(std::string msg, std::string func_name, uint32_t line);
    };
}

bool Utility::debug_mode = false;
bool Utility::LS = false;

void Utility::log(std::string msg, std::string func_name, uint32_t line) {
    llvm::errs() << "SLANG: " << func_name << ":" << line << " " << msg << "\n";
}

void Utility::readFile1() {
    //Read a specific local file.
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
// FIXME: Test TraversedInfoBuffer functionality.
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
        int tmp_var_counter;

        std::string func_name;
        std::string func_ret_t;
        std::string func_params;

        // stack to help convert ast structure to 3-address code.
        std::stack<const Stmt*> main_stack;
        // contains names of the temporary vars for expressions.
        std::stack<std::string> sub_stack;

        // maps a unique variable id to its string representation.
        std::unordered_map<uint64_t , VarInfo> var_map;
        // maps bb_edge
        std::vector<std::pair<int32_t, std::pair<int32_t , EdgeLabel>>> bb_edges;
        // entry bb id is mapped to -1
        std::unordered_map<uint32_t , std::vector<std::string>> bb_stmts;

        std::vector<std::string> edge_labels;

        TraversedInfoBuffer();
        void clear();

        std::string genTmpVariable(QualType qt);
        int nextTmpCount();

        // conversion_routines to SPAN Strings
        std::string convertClangType(QualType qt);
        std::string convertFuncName(std::string func_name);
        std::string convertVarExpr(uint64_t var_addr);
        std::string convertLocalVarName(std::string var_name);
        std::string convertGlobalVarName(std::string var_name);
        std::string convertBbStmts(const std::vector<std::string>& stmts);

        // SPAN IR Printing routines
        TraversedInfoBuffer& dumpSpanIr();
        // void dumpHeader();
        // void dumpVariables();
        // void dumpFunctions();
        // void dumpFooter();
        std::string convertBbEdges();

        // // For Program state purpose.
        // // Overload the == operator
        // bool operator==(const TraversedInfoBuffer &tib) const;
        // // LLVMs equivalent of a hash function
        // void Profile(llvm::FoldingSetNodeID &ID) const;
    };
}

TraversedInfoBuffer::TraversedInfoBuffer(): id{1}, tmp_var_counter{}, main_stack{}, sub_stack{},
    var_map{}, bb_edges{}, bb_stmts{}, edge_labels{3} {

    edge_labels[FalseEdge] = "FalseEdge";
    edge_labels[TrueEdge] = "TrueEdge";
    edge_labels[UnCondEdge] = "UnCondEdge";
}

int TraversedInfoBuffer::nextTmpCount() {
    tmp_var_counter += 1;
    return tmp_var_counter;
}

std::string TraversedInfoBuffer::convertFuncName(std::string func_name) {
    std::stringstream ss;
    ss << "f:" << func_name;
    return ss.str();
}

std::string TraversedInfoBuffer::convertGlobalVarName(std::string var_name) {
    std::stringstream ss;
    ss << "v:" << var_name;
    return ss.str();
}

std::string TraversedInfoBuffer::convertLocalVarName(std::string var_name) {
    // assumes func_name is set
    std::stringstream ss;
    ss << "v:" << func_name << ":" << var_name;
    return ss.str();
}

std::string TraversedInfoBuffer::genTmpVariable(const QualType qt) {
    std::stringstream ss;
    ss << "t." << nextTmpCount();

    return ss.str();
}

// Converts Clang Types into SPAN parsable type strings:
// Examples,
// for `int` it returns `types.Int`.
// for `int*` it returns `types.Ptr(to=types.Int)`.
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

//BOUND START: conversion_routines 1

std::string TraversedInfoBuffer::convertVarExpr(uint64_t var_addr) {
    // if here, var should already be in var_map
    std::stringstream ss;

    auto vinfo = var_map[var_addr];
    ss << "expr.VarE(\"" << vinfo.var_name << "\")";

    return ss.str();
}

std::string TraversedInfoBuffer::convertBbStmts(const std::vector<std::string>& stmts) {
    std::stringstream ss;

    for (auto stmt : stmts) {
        ss << stmt << ",\n";
    }

    return ss.str();
} // convertBbStmts()

//BOUND END  : conversion_routines 1

// clear the buffer for the next function.
void TraversedInfoBuffer::clear() {
    func_name = "";
    func_ret_t = "";
    func_params = "";

    var_map.clear();
    bb_edges.clear();
    bb_stmts.clear();
    while (!main_stack.empty()) {
        main_stack.pop();
    }
    while (!sub_stack.empty()) {
        sub_stack.pop();
    }
}

TraversedInfoBuffer& TraversedInfoBuffer::dumpSpanIr() {
    llvm::errs() << "Printing the TraversedInfoBuffer as SPAN IR:\n";
    llvm::errs() << "FuncName: " << convertFuncName(func_name) << "\n";
    llvm::errs() << convertBbEdges();

    for (auto bb: bb_stmts) {
        llvm::errs() << "  " << bb.first << ": [\n";
        llvm::errs() << convertBbStmts(bb.second) << "\n],\n";
    }

    return *this;
} // dumpSpanIr()

std::string TraversedInfoBuffer::convertBbEdges() {
    std::stringstream ss;

    for (auto p: bb_edges) {
        ss << "graph.BbEdge(" << std::to_string(p.first);
        ss << ", " << std::to_string(p.second.first) << ", ";
        ss << "graph." << edge_labels[p.second.second] << "),\n";
    }

    return ss.str();
}

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
        void checkASTCodeBody(const Decl *D, AnalysisManager &mgr,
                              BugReporter &BR) const;

        // handling_routines
        void handleFunctionDef(const FunctionDecl *D) const;

        void handleCfg(const CFG *cfg) const;

        void handleBBInfo(const CFGBlock *bb, const CFG *cfg) const;

        void handleBBStmts(const CFGBlock *bb) const;

        void handleVariable(const ValueDecl *val_decl) const;

        void handleDeclStmt(const DeclStmt *declStmt) const;

        std::string convertIntegerLiteral(const IntegerLiteral *IL) const;

        void handleDeclRefExpr(const DeclRefExpr *DRE) const;

        std::string convertBinaryOperator(const BinaryOperator *binOp) const;

        // void handleTerminator(const Stmt *terminator,
        //                       std::unordered_map<const Expr *, int> &visited,
        //                       unsigned int block_id) const;

        // conversion_routines
        std::string convertRValue() const;
        std::string convertDeclRefExpr(const DeclRefExpr *dre) const;
        std::string convertLhs() const;
        std::string convertRhs() const;

    }; // class SlangGenChecker
} // anonymous namespace

TraversedInfoBuffer SlangGenChecker::tib = TraversedInfoBuffer();

// Main Entry Point. Invokes top level Function and Cfg handlers.
// Invoked once for each source translation unit function.
void SlangGenChecker::checkASTCodeBody(const Decl *D, AnalysisManager &mgr,
                                   BugReporter &BR) const {
    Utility::readFile1();
    llvm::errs() << "\nBOUND START: SLANG_Generated_Output.\n";

    tib.clear(); // clear the buffer for this function.

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
        handleBBInfo(bb, cfg);
        handleBBStmts(bb);
    }
} // handleCfg()

// Gets the function name, paramaters and return type.
void SlangGenChecker::handleFunctionDef(const FunctionDecl *func_decl) const {
    // STEP 1.1: Get function name.
    tib.func_name = func_decl->getNameInfo().getAsString();

    // STEP 1.2: Get function parameters.
    if (func_decl->doesThisDeclarationHaveABody()) { //& !func_decl->hasPrototype())
        for (unsigned i = 0, e = func_decl->getNumParams(); i != e; ++i) {
            const ParmVarDecl *paramVarDecl = func_decl->getParamDecl(i);
            handleVariable(paramVarDecl);
        }
    }

    // STEP 1.3: Get function return type.
    const QualType returnQType = func_decl->getReturnType();
    tib.func_ret_t = tib.convertClangType(returnQType);
} // handleFunctionDef()

void SlangGenChecker::handleBBInfo(const CFGBlock *bb, const CFG *cfg) const {
    int32_t succ_id, bb_id;
    unsigned entry_bb_id = cfg->getEntry().getBlockID();

    if ((bb_id = bb->getBlockID()) == (int32_t)entry_bb_id) {
        bb_id = -1; //entry block is ided -1.
    }

    llvm::errs() << "BB" << bb_id << "\n";

    if (bb == &cfg->getEntry()) {
        llvm::errs() << "ENTRY BB\n";
    } else if (bb == &cfg->getExit()) {
        llvm::errs() << "EXIT BB\n";
    }

    // access and record successor blocks
    const Stmt *terminator = (bb->getTerminator()).getStmt();
    if (terminator && isa<IfStmt>(terminator)) {
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
} // handleBBInfo()

void SlangGenChecker::handleBBStmts(const CFGBlock *bb) const {
    std::unordered_map<const Expr *, int> visited_nodes;

    unsigned bb_id = bb->getBlockID();

    for (auto elem : *bb) {
        // ref: https://clang.llvm.org/doxygen/CFG_8h_source.html#l00056
        // ref for printing block:
        // https://clang.llvm.org/doxygen/CFG_8cpp_source.html#l05234

        Optional<CFGStmt> CS = elem.getAs<CFGStmt>();
        const Stmt *stmt = CS->getStmt();
        Stmt::StmtClass stmt_cls = stmt->getStmtClass();

        llvm::errs() << "Processing: " << stmt->getStmtClassName() << "\n";

        // the main statement selection conditions.
        // Ronak and Manav: Create a separate function,
        // to handle each kind of statement/expression.
        if (Utility::debug_mode) {
            llvm::errs() << "Unhandled " << stmt->getStmtClassName() << "\n";
            stmt->dump();
            llvm::errs() << "\n";
        } else {
            switch (stmt_cls) {
                default: {
                    llvm::errs() << "SLANG: ERROR: Unhandled Stmt Class: " <<
                                 stmt->getStmtClassName() << ".\n";
                    stmt->dump();
                    llvm::errs() << "\n";
                    break;
                }

                case Stmt::DeclRefExprClass: {
                    tib.main_stack.push(stmt);
                    // const DeclRefExpr *dre = cast<DeclRefExpr>(stmt);
                    // const ValueDecl *val_decl = dre->getDecl();
                    // handleVariable(val_decl);
                    break;
                }

                case Stmt::DeclStmtClass: {
                    const DeclStmt *declStmt = cast<DeclStmt>(stmt);
                    handleDeclStmt(declStmt);
                    break;
                }

                case Stmt::IntegerLiteralClass: {
                    tib.main_stack.push(stmt);
                    break;
                }

                case Stmt::BinaryOperatorClass: {
                    auto binOp = cast<BinaryOperator>(stmt);
                    convertBinaryOperator(binOp);
                    break;
                }
            }
        }
        // if (stmt_class == "DeclStmt") {
        // } else if (stmt_class == "BinaryOperator") {
        //     convertBinaryOperator(ES, visited_nodes, bb_id);
        //     llvm::errs() << "\n";
        // } else {
        //     // llvm::errs() << "found " << stmt_class << "\n";
        // }
        // llvm::errs() << "Partial AST info \n";
        // S->dump(); // Dumps partial AST
        // llvm::errs() << "\n";
    } // for (auto elem : *bb)

    // get terminator
    const Stmt *terminator = (bb->getTerminator()).getStmt();
    // handleTerminator(terminator, visited_nodes, bb_id);

    llvm::errs() << "\n\n";
} // handleBBStmts()

void SlangGenChecker::handleVariable(const ValueDecl *val_decl) const {
    uint64_t var_id = (uint64_t) val_decl;
    if (tib.var_map.find(var_id) == tib.var_map.end()) {
        // seeing the variable for the first time.
        VarInfo varInfo{};
        varInfo.id = var_id;
        const VarDecl *varDecl = dyn_cast<VarDecl>(val_decl);
        if (varDecl) {
            if (varDecl->hasLocalStorage()) {
                varInfo.var_name = tib.convertLocalVarName(val_decl->getNameAsString());
            } else if(varDecl->hasGlobalStorage()) {
                varInfo.var_name = tib.convertGlobalVarName(val_decl->getNameAsString());
            } else if (varDecl->hasExternalStorage()) {
                llvm::errs() << "SLANG: ERROR: External Storage Not Handled.\n";
            } else {
                llvm::errs() << "SLANG: ERROR: Unknown variable storage.\n";
            }
        } else {
            llvm::errs() << "SLANG: ERROR: ValueDecl not a VarDecl!\n";
        }
        varInfo.type_str = tib.convertClangType(val_decl->getType());
        tib.var_map[var_id] = varInfo;
        llvm::errs() << "NEW_VAR: " << varInfo.convertToString() << "\n";
    } else {
        llvm::errs() << "SEEN_VAR: " << tib.var_map[var_id].convertToString() << "\n";
    }
}

void SlangGenChecker::handleDeclStmt(const DeclStmt *declStmt) const {
    // assumes there is only single decl inside (the likely case).
    std::stringstream ss;

    const VarDecl *varDecl = cast<VarDecl>(declStmt->getSingleDecl());
    handleVariable(varDecl);

    if (!tib.main_stack.empty()) {
        // there is smth on the stack, hence on the rhs.
        ss << convertRValue();
    }
}

std::string SlangGenChecker::convertIntegerLiteral(const IntegerLiteral *il) const {
    std::stringstream ss;

    bool is_signed = il->getType()->isSignedIntegerType();
    ss << "expr.Lit(" << il->getValue().toString(10, is_signed) << ")";
    llvm::errs() << ss.str() << "\n";

    return ss.str();
}

void SlangGenChecker::handleDeclRefExpr(const DeclRefExpr *DRE) const {
    const ValueDecl *ident = DRE->getDecl();
    llvm::errs() << ident->getName() << " ";
}

std::string SlangGenChecker::convertBinaryOperator(const BinaryOperator *binOp) const {
    std::stringstream ss;

    if (binOp->isAssignmentOp()) {
        // pull the lhs and rhs from the stack(s).
        ss << "instr.AssignI(" << convertLhs() << ", " << convertRhs() << "),";
    }

    return ss.str();
}

// void SlangGenChecker::handleTerminator(
//         const Stmt *terminator, std::unordered_        auto rhs = tib.main_stack.top();map<const Expr *, int> &visited,
//         unsigned int block_id) const {
//
//     if (terminator && isa<IfStmt>(terminator)) {
//         const Expr *condition = (cast<IfStmt>(terminator))->getCond();
//         llvm::errs() << "if ";
//         handleBinaryOperator(condition, visited, block_id);
//         llvm::errs() << "\n";
//     }
// }

//BOUND END  : handling_routines

//BOUND START: conversion_routines

// convert top of stack to SPAN IR.
std::string SlangGenChecker::convertRValue() const {
    std::stringstream ss;

    const Stmt *stmt = tib.main_stack.top();
    tib.main_stack.pop();

    switch(stmt->getStmtClass()) {
        case Stmt::IntegerLiteralClass: {
            ss << convertIntegerLiteral(cast<IntegerLiteral>(stmt));
            break;
        }

        case Stmt::DeclRefExprClass: {
            ss << convertDeclRefExpr(cast<DeclRefExpr>(stmt));
            break;
        }

        case Stmt::BinaryOperatorClass: {
            break;
        }

        default: {
            // error state
        }
    }

    return ss.str();
} // convertRValue()

std::string SlangGenChecker::convertDeclRefExpr(const DeclRefExpr *dre) const {
    std::stringstream ss;

    const ValueDecl *val_decl = dre->getDecl();
    if (isa<VarDecl>(val_decl)) {
        auto var_decl = cast<VarDecl>(val_decl);
        ss << tib.convertVarExpr((uint64_t)var_decl);
    } else {
        llvm::errs() << "SLANG: ERROR: " << __func__ << ": Not a VarDecl.";
    }

    return ss.str();
}

std::string SlangGenChecker::convertLhs() const {
    std::stringstream ss;

    // const Stmt *stmt = tib.main_stack.top();
    // tib.main_stack.pop();

    // switch(stmt->getStmtClass()) {
    //     case Stmt::DeclRefExprClass: {
    //         auto var = cast<DeclRefExpr>(lhs)->getDecl();
    //         if (isa<VarDecl>(var)) {
    //             ss << tib.convertVarExpr((uint64_t)cast<VarDecl>(var));
    //         } else {
    //             llvm::errs() << "SLANG: Unandled Decl.";
    //         }
    //         break;
    //     }
    //     default: {
    //         llvm::errs() << "SLANG: Unhandled LHS.";
    //         break;
    //     }
    // }

    return ss.str();
}

std::string SlangGenChecker::convertRhs() const {
    std::stringstream ss;

    return ss.str();
}

//BOUND END  : conversion_routines

// Register the Checker
void ento::registerSlangGenChecker(CheckerManager &mgr) {
    mgr.registerChecker<SlangGenChecker>();
}
