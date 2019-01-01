//==- DebugCheckers.cpp - Debugging Checkers ---------------------*- C++ -*-==//
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

#include "ClangSACheckers.h"
#include "clang/AST/Decl.h"      //AD
#include "clang/AST/Expr.h"      //AD
#include "clang/AST/ParentMap.h" //AD
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
#include <unordered_set>              //AD

using namespace clang;
using namespace ento;

//===----------------------------------------------------------------------===//
// MyCFGDumper
//===----------------------------------------------------------------------===//

namespace {
class MyCFGDumper : public Checker<check::ASTCodeBody> {
public:
  void checkASTCodeBody(const Decl *D, AnalysisManager &mgr,
                        BugReporter &BR) const;

  void handleBinaryOperator(const Expr *ES,
                            std::unordered_set<Stmt *> visited) const;

}; // namespace

void MyCFGDumper::handleBinaryOperator(
    const Expr *ES, std::unordered_set<Stmt *> visited) const {
  if (isa<BinaryOperator>(ES)) {
    BinaryOperator *bin_op =
        const_cast<BinaryOperator *>(cast<BinaryOperator>(ES));
    visited.insert(bin_op);
    Expr *LHS = bin_op->getLHS();
    if (isa<BinaryOperator>(LHS)) {
      handleBinaryOperator(LHS, visited);

    } else {
      handleBinaryOperator(LHS, visited);
    }

    llvm::errs() << bin_op->getOpcodeStr() << " ";

    Expr *RHS = bin_op->getRHS();
    if (isa<BinaryOperator>(RHS)) {
      handleBinaryOperator(RHS, visited);
    } else {
      handleBinaryOperator(RHS, visited);
    }
    llvm::errs() << "\n";
  }

  else if (isa<DeclRefExpr>(ES)) {
    const ValueDecl *ident = cast<DeclRefExpr>(ES)->getDecl();
    llvm::errs() << ident->getName() << " ";
  }

  else if (isa<IntegerLiteral>(ES)) {
    const IntegerLiteral *int_literal = cast<IntegerLiteral>(ES);
    bool is_signed = int_literal->getType()->isSignedIntegerType();
    llvm::errs() << int_literal->getValue().toString(10, is_signed) << " ";
  }
}

void MyCFGDumper::checkASTCodeBody(const Decl *D, AnalysisManager &mgr,
                                   BugReporter &BR) const {
  llvm::errs() << "----------------\n";

  PrintingPolicy Policy(mgr.getLangOpts());
  Policy.TerseOutput = false;
  Policy.PolishForDeclaration = true;
  D->print(llvm::errs(), Policy);

  llvm::errs() << "--------\n";

  // STEP 1.1: Print function name
  llvm::errs() << "FuncName: ";
  const FunctionDecl *func_decl = dyn_cast<FunctionDecl>(D);
  llvm::errs() << func_decl->getNameInfo().getAsString() << "\n";

  // STEP 1.2: Print function parameters.
  std::string Proto;
  llvm::errs() << "Params  : ";
  if (func_decl
          ->doesThisDeclarationHaveABody()) { //& !func_decl->hasPrototype())
                                              //{
    for (unsigned i = 0, e = func_decl->getNumParams(); i != e; ++i) {
      if (i)
        Proto += ", ";
      const ParmVarDecl *paramVarDecl = func_decl->getParamDecl(i);
      const VarDecl *varDecl = dyn_cast<VarDecl>(paramVarDecl);

      // Parameter type
      QualType T = varDecl->getTypeSourceInfo()
                       ? varDecl->getTypeSourceInfo()->getType()
                       : varDecl->getASTContext().getUnqualifiedObjCPointerType(
                             varDecl->getType());
      Proto += T.getAsString();

      // Parameter name
      Proto += " ";
      Proto += varDecl->getNameAsString();
    }
  }
  llvm::errs() << Proto << "\n";

  // STEP 1.3: Print function return type.
  const QualType returnQType = func_decl->getReturnType();
  llvm::errs() << "ReturnT : " << returnQType.getAsString() << "\n";

  // STEP 2: Print the CFG.
  if (CFG *cfg = mgr.getCFG(D)) {
    for (auto bb : *cfg) {

      std::unordered_set<Stmt *> visited_nodes;

      unsigned bb_id = bb->getBlockID();
      llvm::errs() << "BB" << bb_id << ":\n";
      for (auto elem : *bb) {
        // ref: https://clang.llvm.org/doxygen/CFG_8h_source.html#l00056
        // ref for printing block:
        // https://clang.llvm.org/doxygen/CFG_8cpp_source.html#l05234

        // Dump partial AST for each basic block
        Optional<CFGStmt> CS = elem.getAs<CFGStmt>();
        const Stmt *S = CS->getStmt();
        std::string stmt_class = S->getStmtClassName();
        Expr *ES = nullptr;

        if (isa<Expr>(S)) {
          // llvm::errs() << "Ignoring implicit casts in AST...\n";
          ES = const_cast<Expr *>(
              cast<Expr>(S)); // const pointer to normal pointer
          ES = ES->IgnoreImplicit();
        }

        if (stmt_class == "DeclStmt") {
          // VarDecl *ident = catsgith<VarDecl>(S);
          llvm::errs() << "Variable declaration for \n";
        }

        if (stmt_class == "BinaryOperator") { // this is an Expr, so cast again

          if (visited_nodes.find(ES) == visited_nodes.end()) {
            handleBinaryOperator(ES, visited_nodes);
          }
        }

        else {
          //llvm::errs() << "found " << stmt_class << "\n";
        }
        // llvm::errs() << "Partial AST info \n";
        // S->dump(); // Dumps partial AST
        llvm::errs() << "\n";
      }
    }
  }
}

} // anonymous namespace

void ento::registerMyCFGDumper(CheckerManager &mgr) {
  mgr.registerChecker<MyCFGDumper>();
}