//===----------------------------------------------------------------------===//
//  MIT License.
//  Copyright (c) 2019 The SLANG Authors.
//
//  Author: Anshuman Dhuliya (dhuliya@cse.iitb.ac.in)
//
// AD If SlangBugReporterChecker class name is added or changed, then also edit,
// AD ../../../include/clang/StaticAnalyzer/Checkers/Checkers.td
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
#include <algorithm>                  //AD

#include "SlangUtil.h"

using namespace clang;
using namespace ento;

// #define LOG_ME(X) if (Utility::debug_mode) Utility::log((X), __FUNCTION__, __LINE__)

// Each bug has to be reported using following necessary six lines
// ----------------------
// LINE 32
// COLUMN 24
// BUG_NAME Dead Store
// BUG_CATEGORY Dead Store
// BUG_MSG Value assigned is not used
// ----------------------
// Note that UNIQUE_ID should not be same for two bugs.

namespace {
class SlangBugReporterChecker : public Checker<check::ASTCodeBody, check::EndAnalysis> {
  public:
    static Decl *D;
    static BugReporter *BR;
    static AnalysisDeclContext *AC;
    
    void checkASTCodeBody(const Decl *D, AnalysisManager &mgr, BugReporter &BR) const;
    void checkEndAnalysis(ExplodedGraph &G, BugReporter &BR, ExprEngine &Eng) const;

    // handling_routines
    void handleCfg(const CFG *cfg) const;
    void handleBBStmts(const CFGBlock *bb) const;
}; // class SlangBugReporterChecker
} // anonymous namespace

Decl *SlangBugReporterChecker::D = nullptr;
BugReporter *SlangBugReporterChecker::BR = nullptr;
AnalysisDeclContext *SlangBugReporterChecker::AC = nullptr;

// mainstart, Main Entry Point. Invokes top level Function and Cfg handlers.
// Invoked once for each source translation unit function.
void SlangBugReporterChecker::checkASTCodeBody(const Decl *D, AnalysisManager &mgr,
                                               BugReporter &BR) const {
    SlangBugReporterChecker::D = const_cast<Decl *>(D); // the world is ending
    SlangBugReporterChecker::BR = &BR;
    AC = mgr.getAnalysisDeclContext(D);

    if (const CFG *cfg = mgr.getCFG(D)) {
        handleCfg(cfg);
    }
} // checkASTCodeBody()

// BOUND START: handling_routines

void SlangBugReporterChecker::handleCfg(const CFG *cfg) const {
    for (const CFGBlock *bb : *cfg) {
        handleBBStmts(bb);
    }
} // handleCfg()

void SlangBugReporterChecker::handleBBStmts(const CFGBlock *bb) const {
    for (auto elem : *bb) {
        Optional<CFGStmt> CS = elem.getAs<CFGStmt>();
        const Stmt *stmt = CS->getStmt();

        if (isa<BinaryOperator>(stmt)) {
            PathDiagnosticLocation ExLoc =
                PathDiagnosticLocation::createBegin(stmt, BR->getSourceManager(), AC);
            BugType *bt = new BugType(this->getCheckName(), llvm::StringRef("SlangBug"),
                       llvm::StringRef("SlangBugCategory"));
            llvm::errs() << "===================== BugType Created! =================\n";
            BR->Register(bt);
            llvm::errs() << "===================== BugType Registered! =================\n";
            auto R =
                llvm::make_unique<BugReport>(*bt, llvm::StringRef("SlangBugReport final"), ExLoc);
            llvm::errs() << "===================== BugReport Created! =================\n";
            R->addNote(llvm::StringRef("Extra stuff"), ExLoc);
         
            // BR->emitReport(std::move(R));
            // llvm::errs() << "===================== BugReport Emitted! =================\n";
            // BR->EmitBasicReport(D, this, "SlangTest::AssignmentBug", "SlangTest", "Slang: test
            // assignment", ExLoc, stmt->getSourceRange());
        }
    }
} // handleBBStmts()

void SlangBugReporterChecker::checkEndAnalysis(ExplodedGraph &G, BugReporter &BR, ExprEngine &Eng) const {
    llvm::errs() << "DONE!!\n";
}

// Register the Checker
void ento::registerSlangBugReporterChecker(CheckerManager &mgr) {
    mgr.registerChecker<SlangBugReporterChecker>();
}
