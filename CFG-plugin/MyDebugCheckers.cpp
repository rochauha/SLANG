//==- MyDebugCheckers.cpp ------------------------------------------*- C99 -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//  Author: _________________  [__] (____________________)
//  Author: Anshuman Dhuliya [AD] (dhuliya@cse.iitb.ac.in)
//
//AD If MyCFGDumper class name is added or changed, then also edit,
//AD ../../../include/clang/StaticAnalyzer/Checkers/Checkers.td
//
//===----------------------------------------------------------------------===//
//

#include "ClangSACheckers.h"
#include "clang/AST/Decl.h" //AD
#include "clang/AST/Expr.h" //AD
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
#include <unordered_map>              //AD

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
  void handleBinaryOperator(const Expr *ES,
                            std::unordered_map<const Expr *, int> &visited,
                            unsigned int block_id) const;
}; // namespace

void MyCFGDumper::handleIntegerLiteral(const IntegerLiteral *IL) const {
  bool is_signed = IL->getType()->isSignedIntegerType();
  llvm::errs() << IL->getValue().toString(10, is_signed) << " ";
}

void MyCFGDumper::handleDeclRefExpr(const DeclRefExpr *DRE) const {
  const ValueDecl *ident = DRE->getDecl();
  llvm::errs() << ident->getName() << " ";
}

// Before accessing an expression, the CFG accesses its sub expressions in a
// bottom-up fashion. This can be seen when we dump the CFG using clang. To have
// similar access manually, we keep track of already traversed sub-expressions
// in the 'visited' hash table.
void MyCFGDumper::handleBinaryOperator(
    const Expr *ES, std::unordered_map<const Expr *, int> &visited,
    unsigned int block_id) const {

  static int count = 1;

  if (visited.empty()) {
    count = 1;
  }

  if (isa<BinaryOperator>(ES)) {
    const BinaryOperator *bin_op = cast<BinaryOperator>(ES);

    // Don't assign temporaries to assignments
    if (!bin_op->isAssignmentOp()) {
      // If the node visited, use it's temporary and return, otherwise store new
      // temporary and continue evaluating.
      if (visited.find(ES) != visited.end()) {
        llvm::errs() << "B" << block_id << "." << visited[ES] << " ";
        return;
      }

      else {
        visited[ES] = count;
        llvm::errs() << "B" << block_id << "." << visited[ES] << " = ";
        count++;
      }
    }

    Expr *LHS = bin_op->getLHS();
    handleBinaryOperator(LHS, visited, block_id);

    llvm::errs() << bin_op->getOpcodeStr() << " ";

    Expr *RHS = bin_op->getRHS();
    handleBinaryOperator(RHS, visited, block_id);
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
    handleBinaryOperator(ES2, visited, block_id);
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

      std::unordered_map<const Expr *, int> visited_nodes;

      unsigned bb_id = bb->getBlockID();
      llvm::errs() << "BB" << bb_id << " ";
      if (bb == &cfg->getEntry())
        llvm::errs() << "[ ENTRY BLOCK ]\n";
      else if (bb == &cfg->getExit())
        llvm::errs() << "[ EXIT BLOCK ]\n";
      else
        llvm::errs() << "\n";

      // details for predecessor blocks
      llvm::errs() << "Predecessors : ";
      if (!bb->pred_empty()) {
        llvm::errs() << bb->pred_size() << "\n              ";

        for (CFGBlock::const_pred_iterator I = bb->pred_begin();
             I != bb->pred_end(); ++I) {
          CFGBlock *B = *I;
          bool Reachable = true;
          if (!B) {
            Reachable = false;
            B = I->getPossiblyUnreachableBlock();
          }
          llvm::errs() << " B" << B->getBlockID();
          if (!Reachable)
            llvm::errs() << " (Unreachable)";
        }
        llvm::errs() << "\n";
      } else {
        llvm::errs() << "None\n";
      }

      // details for successor blocks
      llvm::errs() << "Successors : ";
      if (!bb->succ_empty()) {
        llvm::errs() << bb->succ_size() << "\n            ";

        for (CFGBlock::const_succ_iterator I = bb->succ_begin();
             I != bb->succ_end(); ++I) {
          CFGBlock *B = *I;
          bool Reachable = true;
          if (!B) {
            Reachable = false;
            B = I->getPossiblyUnreachableBlock();
          }

          llvm::errs() << " B" << B->getBlockID();
          if (!Reachable)
            llvm::errs() << "(Unreachable)";
        }
        llvm::errs() << "\n";
      } else {
        llvm::errs() << "None\n";
      }

      for (auto elem : *bb) {
        // ref: https://clang.llvm.org/doxygen/CFG_8h_source.html#l00056
        // ref for printing block:
        // https://clang.llvm.org/doxygen/CFG_8cpp_source.html#l05234

        Optional<CFGStmt> CS = elem.getAs<CFGStmt>();
        const Stmt *S = CS->getStmt();
        std::string stmt_class = S->getStmtClassName();
        Expr *ES = nullptr;

        if (isa<Expr>(S)) {
          ES = const_cast<Expr *>(cast<Expr>(S));
        }

        if (stmt_class == "DeclStmt") {
          const DeclStmt *DS = cast<DeclStmt>(S);
          const DeclGroupRef DG = DS->getDeclGroup();

          for (auto decl : DG) { // DG contains all Decls
            const NamedDecl *named_decl = cast<NamedDecl>(decl);
            QualType T = (cast<ValueDecl>(decl))->getType();
            llvm::errs() << T.getAsString() << " "
                         << named_decl->getNameAsString() << "\n";
          }

          // Now evaluate expressions for the variables
          for (auto decl : DG) {
            const VarDecl *var_decl = cast<VarDecl>(decl);
            const NamedDecl *named_decl = cast<NamedDecl>(decl);
            const Expr *value = var_decl->getInit();

            if (value) {
              llvm::errs() << named_decl->getNameAsString() << " = ";
              handleBinaryOperator(value, visited_nodes, bb_id);
              llvm::errs() << "\n";
            }
          }
        }

        if (stmt_class == "BinaryOperator") {
          handleBinaryOperator(ES, visited_nodes, bb_id);
          llvm::errs() << "\n";
        }

        else {
          // llvm::errs() << "found " << stmt_class << "\n";
        }
        // llvm::errs() << "Partial AST info \n";
        // S->dump(); // Dumps partial AST
        // llvm::errs() << "\n";
      }

      // get terminator
      const Stmt *terminator = (bb->getTerminator()).getStmt();
      if (terminator && isa<IfStmt>(terminator)) {
        const Expr *condition = (cast<IfStmt>(terminator))->getCond();
        llvm::errs() << "if ";
        handleBinaryOperator(condition, visited_nodes, bb_id);
        llvm::errs() << "\n";
      }

      llvm::errs() << "\n\n";
    }
  }
}

} // anonymous namespace

void ento::registerMyCFGDumper(CheckerManager &mgr) {
  mgr.registerChecker<MyCFGDumper>();
}
