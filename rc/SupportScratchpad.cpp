//===----------------------------------------------------------------------===//
//  MIT License.
//  Copyright (c) 2019 The SLANG Authors.
//
//  Author: Ronak Chauhan (r.chauhan@somaiya.edu)
//  Author: Anshuman Dhuliya (dhuliya@cse.iitb.ac.in)
//
// AD If MyCFGDumper class name is added or changed, then also edit,
// AD ../../../include/clang/StaticAnalyzer/Checkers/Checkers.td
//
//===----------------------------------------------------------------------===//
//
// Expanding the MyDebugCheckers.cpp from 323a0b8 to support more constructs

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
#include <stack>                      // RC
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

private:
  void handleFunction(const FunctionDecl *D) const;

  void handleCfg(const CFG *cfg) const;

  void handleBBInfo(const CFGBlock *bb, const CFG *cfg) const;

  void handleBBStmts(const CFGBlock *bb) const;

  void handleDeclStmt(std::stack<const Stmt *> &helper_stack, int &temp_counter,
                      unsigned &block_id) const;

  void handleIntegerLiteral(const Stmt *IL_Stmt) const;

  void handleDeclRefExpr(const Stmt *DRE_Stmt) const;

  void handleBinaryOperator(std::stack<const Stmt *> &helper_stack,
                            int &temp_counter, unsigned &block_id) const;

  void subHandleBinaryOperator(const Stmt *expression, int &temp_counter,
                                 unsigned &block_id, bool is_assignment) const;

  void handleTerminator(const Stmt *terminator,
                        std::stack<const Stmt *> &helper_stack,
                        int &temp_counter, unsigned &block_id) const;

}; // class MyCFGDumper

// Main Entry Point. Invokes top level Function and Cfg handlers.
// Invoked once for each source translation unit function.
void MyCFGDumper::checkASTCodeBody(const Decl *D, AnalysisManager &mgr,
                                   BugReporter &BR) const {
  llvm::errs() << "\nBOUND START: SLANG_Generated_Output.\n";

  // PrintingPolicy Policy(mgr.getLangOpts());
  // Policy.TerseOutput = false;
  // Policy.PolishForDeclaration = true;
  // D->print(llvm::errs(), Policy);

  const FunctionDecl *func_decl = dyn_cast<FunctionDecl>(D);
  handleFunction(func_decl);

  if (const CFG *cfg = mgr.getCFG(D)) {
    handleCfg(cfg);
  } else {
    llvm::errs() << "SLANG: ERROR: No CFG for function.\n";
  }

  llvm::errs() << "\nBOUND END  : SLANG_Generated_Output.\n";
} // checkASTCodeBody()

void MyCFGDumper::handleCfg(const CFG *cfg) const {
  for (const CFGBlock *bb : *cfg) {
    handleBBInfo(bb, cfg);
    handleBBStmts(bb);
  }
} // handleCfg

// Gets the function name, paramaters and return type.
void MyCFGDumper::handleFunction(const FunctionDecl *func_decl) const {
  // STEP 1.1: Print function name
  llvm::errs() << "FuncName: ";
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
} // handleFunction()

void MyCFGDumper::handleBBInfo(const CFGBlock *bb, const CFG *cfg) const {
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
} // handleBBInfo()

void MyCFGDumper::handleDeclStmt(std::stack<const Stmt *> &helper_stack,
                                 int &temp_counter, unsigned &block_id) const {
  const DeclStmt *DS = cast<DeclStmt>(helper_stack.top());
  helper_stack.pop();

  const Decl *decl = DS->getSingleDecl();
  const NamedDecl *named_decl = cast<NamedDecl>(decl);
  QualType T = (cast<ValueDecl>(decl))->getType();

  llvm::errs() << T.getAsString() << " " << named_decl->getNameAsString();

  if (helper_stack.empty()) {
    llvm::errs() << "\n";
    return;
  }

  const Stmt *S = helper_stack.top();
  helper_stack.pop();

  llvm::errs() << " = ";
  switch (S->getStmtClass()) {
  case Stmt::BinaryOperatorClass:
    llvm::errs() << "B" << block_id << "." << temp_counter - 1;
    break;

  case Stmt::IntegerLiteralClass:
    handleIntegerLiteral(S);
    break;

  case Stmt::DeclRefExprClass:
    handleDeclRefExpr(S);
    break;

  default:
    llvm::errs() << "Unhandled " << S->getStmtClassName();
    break;
  }
  llvm::errs() << "\n";
} // handleDeclStmt()

// Handle subexpressions
void MyCFGDumper::subHandleBinaryOperator(const Stmt *expression_stmt,
                                            int &temp_counter,
                                            unsigned &block_id,
                                            bool is_assignment) const {
  switch (expression_stmt->getStmtClass()) {
  case Stmt::IntegerLiteralClass:
    handleIntegerLiteral(const_cast<Stmt *>(expression_stmt));
    break;

  case Stmt::DeclRefExprClass:
    handleDeclRefExpr(const_cast<Stmt *>(expression_stmt));
    break;

  case Stmt::BinaryOperatorClass:
    llvm::errs() << "B" << block_id << ".";
    if (is_assignment)
      llvm::errs() << temp_counter - 1;
    else
      llvm::errs() << temp_counter - 2;
    break;

  default:
    llvm::errs() << "Unhandled "
                 << const_cast<Stmt *>(expression_stmt)->getStmtClassName();
    break;
  }
} // subHandleBinaryOperator()

void MyCFGDumper::handleBinaryOperator(std::stack<const Stmt *> &helper_stack,
                                       int &temp_counter,
                                       unsigned &block_id) const {
  const Stmt *bin_op_stmt = helper_stack.top();
  const BinaryOperator *bin_op = cast<BinaryOperator>(bin_op_stmt);
  helper_stack.pop();

  Stmt *RHS = const_cast<Stmt *>(helper_stack.top());
  helper_stack.pop();

  Stmt *LHS = const_cast<Stmt *>(helper_stack.top());
  helper_stack.pop();

  // don't assign temporary variable to assignments
  if (bin_op->isAssignmentOp()) {

    // Assignments can be an issue in presence of temporaries when using a
    // stack. If LHS (3rd last element in the stack) is not a DeclRefExpr (i.e
    // a varable), it will cause a runtime error. Hence, we check and swap
    // them
    if (!isa<DeclRefExpr>(LHS)) {
      auto tmp = LHS;
      LHS = RHS;
      RHS = tmp;
    }

    handleDeclRefExpr(LHS);
    llvm::errs() << " " << bin_op->getOpcodeStr() << " ";
    subHandleBinaryOperator(RHS, temp_counter, block_id, true);
  }

  // Assign temporaries otherwise
  else {
    llvm::errs() << "B" << block_id << "." << temp_counter << " = ";
    temp_counter++;

    subHandleBinaryOperator(LHS, temp_counter, block_id, false);
    llvm::errs() << " " << bin_op->getOpcodeStr() << " ";
    subHandleBinaryOperator(RHS, temp_counter, block_id, false);

    helper_stack.push(bin_op_stmt);
  } // else

  llvm::errs() << "\n";
} // handleBinaryOperator()

void MyCFGDumper::handleBBStmts(const CFGBlock *bb) const {
  std::unordered_map<const Expr *, int> visited_nodes;

  unsigned bb_id = bb->getBlockID();

  std::stack<const Stmt *> helper_stack;
  int temp_counter = 1; // for naming temporaries

  for (auto elem : *bb) {
    // ref: https://clang.llvm.org/doxygen/CFG_8h_source.html#l00056
    // ref for printing block:
    // https://clang.llvm.org/doxygen/CFG_8cpp_source.html#l05234

    Optional<CFGStmt> CS = elem.getAs<CFGStmt>();
    const Stmt *S = CS->getStmt();

    switch (S->getStmtClass()) {
      // default:
      //   llvm::errs() << S->getStmtClassName() << ".\n";
      //   S->dump();
      //   llvm::errs() << "\n";
      //   break;

    case Stmt::DeclStmtClass:
      helper_stack.push(S);
      handleDeclStmt(helper_stack, temp_counter, bb_id);
      break;

    case Stmt::BinaryOperatorClass:
      helper_stack.push(S);
      handleBinaryOperator(helper_stack, temp_counter, bb_id);
      break;

    default:
      if (!isa<ImplicitCastExpr>(S)) {
        // llvm::errs() << S->getStmtClassName() << ".\n";
        helper_stack.push(S);
        // S->dump();
      }
      // llvm::errs() << "\n";
      break;
    } // switch()
  }   // for

  // get terminator
  const Stmt *terminator = (bb->getTerminator()).getStmt();
  handleTerminator(terminator, helper_stack, temp_counter, bb_id);
  llvm::errs() << "\n\n";
} // handleBBStmts()

void MyCFGDumper::handleIntegerLiteral(const Stmt *IL_Stmt) const {
  const IntegerLiteral *IL = cast<IntegerLiteral>(IL_Stmt);
  bool is_signed = IL->getType()->isSignedIntegerType();
  llvm::errs() << IL->getValue().toString(10, is_signed);
}

void MyCFGDumper::handleDeclRefExpr(const Stmt *DRE_Stmt) const {
  const DeclRefExpr *DRE = cast<DeclRefExpr>(DRE_Stmt);
  const ValueDecl *ident = DRE->getDecl();
  llvm::errs() << ident->getName();
}

void MyCFGDumper::handleTerminator(const Stmt *terminator,
                                   std::stack<const Stmt *> &helper_stack,
                                   int &temp_counter,
                                   unsigned &block_id) const {
  if (!terminator)
    return;

  Stmt::StmtClass terminator_class = terminator->getStmtClass();

  switch (terminator_class) {
  case Stmt::IfStmtClass:
    llvm::errs() << "if ";
    break;

  case Stmt::WhileStmtClass:
    llvm::errs() << "while ";
    break;

  default:
    llvm::errs() << "Unhandled terminator - " << terminator->getStmtClassName()
                 << "\n\n";
    terminator->dump();
    break;
  } // switch
  llvm::errs() << "B" << block_id << "." << temp_counter - 1 << "\n";
} // handleTerminator()

} // anonymous namespace

// Register the Checker
void ento::registerMyCFGDumper(CheckerManager &mgr) {
  mgr.registerChecker<MyCFGDumper>();
}