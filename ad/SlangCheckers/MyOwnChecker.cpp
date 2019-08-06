//==- MyOwnChecker.cpp - Debugging Checkers ---------------------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  This file defines checkers that display debugging information.
// AD Modified to do custom things.
// AD If class name added or changed also edit,
// AD ../../../include/clang/StaticAnalyzer/Checkers/Checkers.td
//
//===----------------------------------------------------------------------===//

// #include "ClangSACheckers.h"
#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/Analysis/Analyses/Dominators.h"
#include "clang/Analysis/Analyses/LiveVariables.h"
#include "clang/Analysis/CallGraph.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/AnalysisManager.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/CheckerContext.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ExplodedGraph.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/ExprEngine.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/raw_ostream.h" //AD
#include <string>                     //AD
#include "clang/AST/Decl.h"           //AD
#include <fstream>
#include <iostream>

using namespace clang;
using namespace ento;

//===----------------------------------------------------------------------===//
// MyOwnChecker
//===----------------------------------------------------------------------===//

namespace {
class MyState {
  public:
    MyState() {
        x = new int32_t;
        *x = 0;
    }

    int32_t *x;

    // Overload the == operator
    bool operator==(const MyState &X) const { return x == X.x; }

    // LLVMs equivalent of a hash function
    void Profile(llvm::FoldingSetNodeID &ID) const { ID.AddInteger(*x); }
};
} // anonymous namespace

REGISTER_MAP_WITH_PROGRAMSTATE(MyStateData, int, MyState)

namespace {
class MyOwnChecker : public Checker<check::ASTCodeBody> {
    static int x;         // see USING_X
    static int obj_count; // counts num of object created.
  public:
    MyOwnChecker() {
        obj_count += 1;
        llvm::errs() << "MyOwnChecker object " << obj_count << " initialized.\n";
    }

    void checkASTCodeBody(const Decl *D, AnalysisManager &mgr, BugReporter &BR) const {
        // USING_X
        x += 1;
        llvm::errs() << "Function count: " << x << "\n";

        // ProgramStateRef state;
        //   llvm::errs() << "Hello, There!\n";
        // if(!state->contains<MyStateData>(1)) {
        //     llvm::errs() << "AD: Setting up state. 1\n";
        //     auto d = MyState();
        //     state = state->set<MyStateData>(1, d);
        //     llvm::errs() << "AD: Setting up state.\n";
        // } else{
        //     llvm::errs() << "AD: Setting up state.2\n";
        //     auto d = state->get<MyStateData>(1);
        //     *(d->x) += 1;
        //     llvm::errs() << "AD: " << d->x << "\n";
        // }

        // AD START Read a local file.
        // std::ifstream inputTxtFile;
        // std::string line;
        // inputTxtFile.open("/home/codeman/.itsoflife/local/tmp/checker-input.txt");
        // if (inputTxtFile.is_open()) {
        //  while(getline(inputTxtFile, line)) {
        //      llvm::errs() << line << "\n";
        //  }
        //  inputTxtFile.close();
        //}
        // AD END   Read a local file.
    }
};
} // anonymous namespace

// USING_X : initialization is required.
// wihtout initialization: Undefined reference to anonymous::MyOwnChecker::x ???
int MyOwnChecker::x = 0;
int MyOwnChecker::obj_count = 0;

void ento::registerMyOwnChecker(CheckerManager &mgr) { mgr.registerChecker<MyOwnChecker>(); }
