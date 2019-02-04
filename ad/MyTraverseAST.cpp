//===----------------------------------------------------------------------===//
//  MIT License.
//  Copyright (c) 2019 The SLANG Authors.
//
//  Author: Anshuman Dhuliya (dhuliya@cse.iitb.ac.in)
//
//AD If MyTraverseAST class name is added or changed, then also edit,
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
#include "clang/StaticAnalyzer/Core/BugReporter/BugReporter.h"  //AD
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
}

namespace {
    class MyTraverseAST : public Checker<check::ASTCodeBody> {
    public:
        static Decl *D;

        void checkASTCodeBody(const Decl *D, AnalysisManager &mgr,
                              BugReporter &BR) const;

        // handling_routines
        void handleCfg(const CFG *cfg) const;

        void handleBBStmts(const CFGBlock *bb) const;
        void handleLocation(const Stmt *stmt) const;
        void printParent(const Stmt *stmt) const;
    }; // class MyTraverseAST
} // anonymous namespace

Decl* MyTraverseAST::D = nullptr;

// Main Entry Point. Invokes top level Function and Cfg handlers.
// Invoked once for each source translation unit function.
void MyTraverseAST::checkASTCodeBody(const Decl *D, AnalysisManager &mgr,
                                       BugReporter &BR) const {
    MyTraverseAST::D = const_cast<Decl*>(D); // the world is ending

    if (const CFG *cfg = mgr.getCFG(D)) {
        handleCfg(cfg);
    } else {
        llvm::errs() << "SLANG: ERROR: No CFG for function.\n";
    }

    llvm::errs() << "\nBOUND END  : SLANG_Generated_Output.\n";
} // checkASTCodeBody()

//BOUND START: handling_routines

void MyTraverseAST::handleCfg(const CFG *cfg) const {
    for (const CFGBlock *bb : *cfg) {
        llvm::errs() << "\n\nBB" << bb->getBlockID() << "\n";
        handleBBStmts(bb);
    }
} // handleCfg()

void MyTraverseAST::handleBBStmts(const CFGBlock *bb) const {
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
    const Stmt *terminator = nullptr;
    if(terminator = (bb->getTerminator()).getStmt()) {
        llvm::errs() << "Visiting Terminator: " << terminator->getStmtClassName() << "\n";
        terminator->dump();
        printParent(terminator);
        handleLocation(terminator);
        llvm::errs() << "\n";
    }

    llvm::errs() << "\n\n";
} // handleBBStmts()

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

    loc.line = D->getASTContext().getSourceManager().getExpansionLineNumber(stmt->getLocStart());
    loc.col = D->getASTContext().getSourceManager().getExpansionColumnNumber(stmt->getLocStart());
    loc.fileName = D->getASTContext().getSourceManager().getFilename(stmt->getLocStart()).str();

    loc.printLocation();
}
// Register the Checker
void ento::registerMyTraverseAST(CheckerManager &mgr) {
    mgr.registerChecker<MyTraverseAST>();
}

