//==- MyScratchpadChecker.cpp - Check for stores to dead variables -*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines a DeadStores, a flow-sensitive checker that looks for
//  stores to variables that are no longer live.
//
// Author: Anshuman Dhuliya [AD] (dhuliya@cse.iitb.ac.in)
//
// AD If MyScratchpadChecker class name is added or changed, then also edit,
// AD ../../../include/clang/StaticAnalyzer/Checkers/Checkers.td
//===----------------------------------------------------------------------===//

// #include "ClangSACheckers.h"
#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Attr.h"
#include "clang/AST/ParentMap.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Analysis/Analyses/LiveVariables.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugReporter.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/AnalysisManager.h"
#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/SaveAndRestore.h"
#include "llvm/Support/CommandLine.h"

using namespace clang;
using namespace ento;

// static llvm::cl::opt<bool> SlangReport ("slang-report", llvm::cl::Required,
// llvm::cl::desc("Generates slang report from file."));

//===----------------------------------------------------------------------===//
// MyScratchpadChecker
//===----------------------------------------------------------------------===//

namespace {
class MyScratchpadChecker : public Checker<check::ASTCodeBody> {
  public:
    void checkASTCodeBody(const Decl *D, AnalysisManager &mgr, BugReporter &BR) const {

        llvm::errs() << "Slang: Hello, World!\n";
    }
};
} // namespace

void ento::registerMyScratchpadChecker(CheckerManager &mgr) {
    // llvm::errs() << "A scratchpad to learn.\n";
    mgr.registerChecker<MyScratchpadChecker>();
}
