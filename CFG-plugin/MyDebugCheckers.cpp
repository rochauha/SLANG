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

  void handleIntegerLiteral(const IntegerLiteral *IL) const;
  void handleDeclRefExpr(const DeclRefExpr *DRE) const;

  // TODO: make use of the visited set to refer to previously computed
  // binary operators
  void handleBinaryOperator(const Expr *ES,
                            std::unordered_set<Expr *> visited) const;

}; // namespace

void MyCFGDumper::handleIntegerLiteral(const IntegerLiteral *IL) const {
  bool is_signed = IL->getType()->isSignedIntegerType();
  llvm::errs() << IL->getValue().toString(10, is_signed) << " ";
}

void MyCFGDumper::handleDeclRefExpr(const DeclRefExpr *DRE) const {
  const ValueDecl *ident = DRE->getDecl();
  llvm::errs() << ident->getName() << " ";
}

void MyCFGDumper::handleBinaryOperator(
    const Expr *ES, std::unordered_set<Expr *> visited) const {
  if (isa<BinaryOperator>(ES)) {
    const BinaryOperator *bin_op = cast<BinaryOperator>(ES);

    Expr *LHS = bin_op->getLHS();
    handleBinaryOperator(LHS, visited);

    llvm::errs() << bin_op->getOpcodeStr() << " ";

    Expr *RHS = bin_op->getRHS();
    handleBinaryOperator(RHS, visited);
  }

  else if (isa<DeclRefExpr>(ES)) {
    const DeclRefExpr *decl_ref_expr = cast<DeclRefExpr>(ES);
    handleDeclRefExpr(decl_ref_expr);
  }

  else if (isa<IntegerLiteral>(ES)) {
    const IntegerLiteral *int_literal = cast<IntegerLiteral>(ES);
    handleIntegerLiteral(int_literal);
  }

  else if (isa<ImplicitCastExpr>(ES)) {
    auto ES2 = ES->IgnoreParenImpCasts();
    handleBinaryOperator(ES2, visited);
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

      std::unordered_set<Expr *> visited_nodes;

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
        }

        if (stmt_class == "DeclStmt") {
          const DeclStmt *DS = cast<DeclStmt>(S);
          const DeclGroupRef DG = DS->getDeclGroup();

          for (auto decl : DG) { // DG contains all Decls
            auto named_decl = cast<NamedDecl>(decl);
            llvm::errs() << "Variable : " << named_decl->getNameAsString()
                         << "\n";
          }

          // Now evaluate expressions for the variables
          for (auto decl : DG) { // DG contains all VarDecls
            const VarDecl *var_decl = cast<VarDecl>(decl);
            const Expr *value = var_decl->getInit();
            if (value) {
              if (isa<IntegerLiteral>(value))
                handleIntegerLiteral(cast<IntegerLiteral>(value));
              else
                handleBinaryOperator(value, visited_nodes);
            }
          }
        }

        if (stmt_class == "BinaryOperator") { // this is an Expr, so cast again
          if (visited_nodes.find(ES) == visited_nodes.end()) {
            handleBinaryOperator(ES, visited_nodes);
          }
        }

        else {
          // llvm::errs() << "found " << stmt_class << "\n";
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