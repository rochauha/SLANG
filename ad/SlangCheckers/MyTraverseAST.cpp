//===----------------------------------------------------------------------===//
//  MIT License.
//  Copyright (c) 2019 The SLANG Authors.
//
//  Author: Anshuman Dhuliya (dhuliya@cse.iitb.ac.in)
//
// AD If MyTraverseAST class name is added or changed, then also edit,
// AD ../../../include/clang/StaticAnalyzer/Checkers/Checkers.td
//
//===----------------------------------------------------------------------===//
//

// #include "ClangSACheckers.h"
#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/AST/Decl.h" //AD
#include "clang/AST/Expr.h" //AD
#include "clang/AST/Stmt.h" //AD
#include "clang/Analysis/Analyses/Dominators.h"
#include "clang/Analysis/Analyses/LiveVariables.h"
#include "clang/Analysis/CallGraph.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugReporter.h" //AD
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

#include "SlangUtil.h"

using namespace clang;
using namespace ento;

// int span_add_nums(int a, int b);

// #define LOG_ME(X) if (Utility::debug_mode) Utility::log((X), __FILE__, __LINE__)
#define LOG_ME(X) llvm::errs() << __FILE__ << __func__ << __LINE__ << X;

//===----------------------------------------------------------------------===//
// SlangGenChecker
//===----------------------------------------------------------------------===//

namespace {
class Location {
  public:
    uint32_t col;
    uint32_t line;
    std::string fileName;

    void printLocation() {
        llvm::errs() << "Loc(" << fileName << ":" << line << ":" << col << ")\n";
    }
};
} // namespace

namespace {
class MyTraverseAST : public Checker<check::ASTCodeBody> {
  public:
    static Decl *D;

    // mainstart
    void checkASTCodeBody(const Decl *D, AnalysisManager &mgr, BugReporter &BR) const;

    // handling_routines
    void handleCfg(const CFG *cfg) const;
    void handleBb(const CFGBlock *bb, const CFG *cfg) const;
    void handleBbStmts(const CFGBlock *bb) const;
    void handleLocation(const Stmt *stmt) const;
    void printParent(const Stmt *stmt) const;
}; // class MyTraverseAST
} // anonymous namespace

Decl *MyTraverseAST::D = nullptr;

// mainstart, Main Entry Point. Invokes top level Function and Cfg handlers.
// Invoked once for each source translation unit function.
void MyTraverseAST::checkASTCodeBody(const Decl *D, AnalysisManager &mgr, BugReporter &BR) const {
    SLANG_EVENT("Starting the AST Trace print.")
    // SLANG_TRACE(span_add_nums(1,2));

    MyTraverseAST::D = const_cast<Decl *>(D); // the world is ending

    if (const CFG *cfg = mgr.getCFG(D)) {
        handleCfg(cfg);
    } else {
        llvm::errs() << "SLANG: ERROR: No CFG for function.\n";
    }

    llvm::errs() << "\nBOUND END  : SLANG_Generated_Output.\n";
} // checkASTCodeBody()

// BOUND START: handling_routines

void MyTraverseAST::handleCfg(const CFG *cfg) const {
    for (const CFGBlock *bb : *cfg) {
        handleBb(bb, cfg);
        handleBbStmts(bb);
    }
} // handleCfg()

// Print the successors in correct order...
void MyTraverseAST::handleBb(const CFGBlock *bb, const CFG *cfg) const {
    int32_t succId, bbId;

    bbId = bb->getBlockID();
    llvm::errs() << "BB" << bbId << ".\n";

    if (bb == &cfg->getEntry()) {
        llvm::errs() << "ENTRY BB\n";
    } else if (bb == &cfg->getExit()) {
        llvm::errs() << "EXIT BB\n";
    }

    // print predecessors
    llvm::errs() << "Preds: ";
    for (CFGBlock::const_pred_iterator I = bb->pred_begin(); I != bb->pred_end(); ++I) {
        CFGBlock *pred = *I;
        llvm::errs() << "|";
        if (pred) {
            llvm::errs() << "BB" << pred->getBlockID() << ", ";
        }
    }
    llvm::errs() << "\n";

    // print successors
    llvm::errs() << "Succs: ";
    for (CFGBlock::const_succ_iterator I = bb->succ_begin(); I != bb->succ_end(); ++I) {
        CFGBlock *succ = *I;
        llvm::errs() << "|";
        if (succ) {
            llvm::errs() << "BB" << succ->getBlockID() << ", ";
        }
    }
    llvm::errs() << "\n";
    llvm::errs() << "\n";
} // handleBb()

void MyTraverseAST::handleBbStmts(const CFGBlock *bb) const {
    for (auto elem : *bb) {
        // ref: https://clang.llvm.org/doxygen/CFG_8h_source.html#l00056
        // ref for printing block:
        // https://clang.llvm.org/doxygen/CFG_8cpp_source.html#l05234

        Optional<CFGStmt> CS = elem.getAs<CFGStmt>();
        const Stmt *stmt = CS->getStmt();
        // Stmt::StmtClass stmt_cls = stmt->getStmtClass();

        llvm::errs() << "Visiting: " << stmt->getStmtClassName() << "\n";
        stmt->dump();

        printParent(stmt);
        handleLocation(stmt);

        llvm::errs() << "\n";
    } // for (auto elem : *bb)

    // get terminator
    const Stmt *terminator = (bb->getTerminator()).getStmt();
    if (terminator) {
        llvm::errs() << "Visiting Terminator: " << terminator->getStmtClassName() << "\n";
        terminator->dump();
        printParent(terminator);
        handleLocation(terminator);
        llvm::errs() << "\n";
    }

    llvm::errs() << "\n\n";
} // handleBbStmts()

void MyTraverseAST::printParent(const Stmt *stmt) const {
    const auto &parents = D->getASTContext().getParents(*stmt);
    if (!parents.empty()) {
        const Stmt *stmt1 = parents[0].get<Stmt>();
        if (stmt1) {
            llvm::errs() << "Parent: " << stmt1->getStmtClassName() << "\n";
        } else {
            llvm::errs() << "Parent: Cannot print.\n";
        }
    } else {
        llvm::errs() << "Parent: None\n";
    }
}

void MyTraverseAST::handleLocation(const Stmt *stmt) const {
    Location loc;

    loc.line = D->getASTContext().getSourceManager().getExpansionLineNumber(stmt->getBeginLoc());
    loc.col = D->getASTContext().getSourceManager().getExpansionColumnNumber(stmt->getBeginLoc());
    loc.fileName = D->getASTContext().getSourceManager().getFilename(stmt->getBeginLoc()).str();

    loc.printLocation();
}
// Register the Checker
void ento::registerMyTraverseAST(CheckerManager &mgr) { mgr.registerChecker<MyTraverseAST>(); }
