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
#include "llvm/Support/raw_ostream.h" // AD
#include <list>                       // RC
#include <sstream>                    // RC
#include <stack>                      // RC
#include <string>                     // AD

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
  enum BehaviorTy { REDUCE_EXPR_TO_SINGLE_TEMP = 1, REDUCE_EXPR_TO_THREE_ADDR };
  typedef std::list<std::string> InstructionListTy;

  void handleFunction(const FunctionDecl *D) const;

  void handleCfg(const CFG *cfg) const;

  void handleBBInfo(const CFGBlock *bb, const CFG *cfg) const;

  void handleBBStmts(const CFGBlock *bb) const;

  void handleDeclStmt(std::stack<const Stmt *> &helper_stack,
                      InstructionListTy &instr_list, int &temp_counter,
                      unsigned &block_id) const;

  std::string getIntegerLiteralValue(const Stmt *IL_Stmt) const;

  std::string getDeclRefExprValue(const Stmt *DRE_Stmt) const;

  void handleAssignment(std::stack<const Stmt *> &helper_stack,
                        InstructionListTy &instr_list, int &temp_counter,
                        unsigned &block_id) const;

  std::string reduce(std::stack<const Stmt *> &helper_stack,
                     InstructionListTy &instr_list, int &temp_counter,
                     unsigned &block_id, BehaviorTy behavior) const;

  std::string getOperandWithSideEffects(std::string operand,
                                        const UnaryOperator *un_op,
                                        InstructionListTy &instr_list,
                                        int &temp_counter,
                                        unsigned &block_id) const;

  std::string getReducedTemporary(std::stack<const Stmt *> &helper_stack,
                                  InstructionListTy &instr_list,
                                  int &temp_counter, unsigned &block_id) const;

  std::string getDevelopedRValue(std::stack<const Stmt *> &helper_stack,
                                 InstructionListTy &instr_list,
                                 int &temp_counter, unsigned &block_id) const;

  std::string getPrimitiveValue(std::stack<const Stmt *> &helper_stack) const;

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
                                 InstructionListTy &instr_list,
                                 int &temp_counter, unsigned &block_id) const {
  const Decl *decl = cast<DeclStmt>(helper_stack.top())->getSingleDecl();
  const NamedDecl *named_decl = cast<NamedDecl>(decl);
  QualType T = (cast<ValueDecl>(decl))->getType();

  helper_stack.pop();
  if (helper_stack.empty())
    return;

  std::stringstream current_instr_stream;
  current_instr_stream << T.getAsString() << " "
                       << named_decl->getNameAsString() << " = "
                       << reduce(helper_stack, instr_list, temp_counter,
                                 block_id, REDUCE_EXPR_TO_THREE_ADDR);
  instr_list.push_back(current_instr_stream.str());
}

std::string
MyCFGDumper::getReducedTemporary(std::stack<const Stmt *> &helper_stack,
                                 InstructionListTy &instr_list,
                                 int &temp_counter, unsigned &block_id) const {
  if (isa<UnaryOperator>(helper_stack.top())) {
    return getDevelopedRValue(helper_stack, instr_list, temp_counter, block_id);
  }

  else if (!isa<BinaryOperator>(helper_stack.top())) {
    return getPrimitiveValue(helper_stack);
  }

  std::string reduced_value_str;
  const Stmt *current_stmt = helper_stack.top();
  helper_stack.pop();

  std::stringstream current_instr_stream;

  const BinaryOperator *bin_op = cast<BinaryOperator>(current_stmt);
  current_instr_stream << "B" << block_id << "." << temp_counter;
  reduced_value_str = current_instr_stream.str();
  ++temp_counter;

  std::string right_operand =
      getReducedTemporary(helper_stack, instr_list, temp_counter, block_id);
  std::string left_operand =
      getReducedTemporary(helper_stack, instr_list, temp_counter, block_id);
  current_instr_stream << " = " << left_operand << " "
                       << std::string(bin_op->getOpcodeStr()) << " "
                       << right_operand;

  instr_list.push_back(current_instr_stream.str());
  return reduced_value_str;
}

// Do additional work if UnaryOperator is present, otherwise simply go back to
// getReducedTemporary
std::string
MyCFGDumper::getDevelopedRValue(std::stack<const Stmt *> &helper_stack,
                                InstructionListTy &instr_list,
                                int &temp_counter, unsigned &block_id) const {
  const UnaryOperator *un_op = cast<UnaryOperator>(helper_stack.top());
  helper_stack.pop();
  std::stringstream current_instr_stream;
  std::string reduced_value_str;
  std::string operand;

  current_instr_stream << "B" << block_id << "." << temp_counter;
  reduced_value_str = current_instr_stream.str();

  ++temp_counter;
  operand =
      getReducedTemporary(helper_stack, instr_list, temp_counter, block_id);

  operand = getOperandWithSideEffects(operand, un_op, instr_list, temp_counter,
                                      block_id);

  current_instr_stream << " = " << operand;

  instr_list.push_back(current_instr_stream.str());
  return reduced_value_str;
}

std::string
MyCFGDumper::getPrimitiveValue(std::stack<const Stmt *> &helper_stack) const {
  const Stmt *current_stmt = helper_stack.top();
  helper_stack.pop();

  std::string reduced_value;
  switch (current_stmt->getStmtClass()) {
  case Stmt::IntegerLiteralClass:
    reduced_value = getIntegerLiteralValue(current_stmt);
    break;

  case Stmt::DeclRefExprClass:
    reduced_value = getDeclRefExprValue(current_stmt);
    break;

  default:
    reduced_value = "Unhandled type for reduced value";
    break;
  }
  return reduced_value;
}

// Deal with effects of ++ and --
std::string MyCFGDumper::getOperandWithSideEffects(
    std::string operand, const UnaryOperator *un_op,
    InstructionListTy &instr_list, int &temp_counter,
    unsigned &block_id) const {
  if (!un_op)
    return operand;

  std::stringstream current_instr_stream;
  std::string updated_operand;
  switch (un_op->getOpcode()) {
  case UO_PostInc:
    current_instr_stream << "B" << block_id << "." << temp_counter;
    ++temp_counter;
    updated_operand = current_instr_stream.str();
    current_instr_stream << " = " << operand;
    instr_list.push_back(current_instr_stream.str());
    instr_list.push_back(operand + " = " + operand + " + 1");
    return updated_operand;

  case UO_PreInc:
    instr_list.push_back(operand + " = " + operand + " + 1");
    return operand;

  case UO_PostDec:
    current_instr_stream << "B" << block_id << "." << temp_counter;
    ++temp_counter;
    updated_operand = current_instr_stream.str();
    current_instr_stream << " = " << operand;
    instr_list.push_back(current_instr_stream.str());
    instr_list.push_back(operand + " = " + operand + " - 1");
    return updated_operand;

  case UO_PreDec:
    instr_list.push_back(operand + " = " + operand + " - 1");
    return operand;

  case UO_AddrOf:
    return ("&" + operand);

  case UO_Deref:
    return ("*" + operand);

  case UO_Plus:
    return ("+" + operand);
  case UO_Minus:
    return ("-" + operand);

  case UO_Not:
  case UO_LNot:
  case UO_Coawait:
  default:
    llvm::errs() << "UNOP ";
    break;
  }
}

std::string MyCFGDumper::reduce(std::stack<const Stmt *> &helper_stack,
                                InstructionListTy &instr_list,
                                int &temp_counter, unsigned &block_id,
                                BehaviorTy behavior) const {
  std::stringstream current_instr_stream;
  const Stmt *current_stmt = helper_stack.top();

  if (isa<BinaryOperator>(current_stmt) &&
      behavior == REDUCE_EXPR_TO_THREE_ADDR) {
    const BinaryOperator *bin_op = cast<BinaryOperator>(current_stmt);
    helper_stack.pop();

    std::string right_operand =
        getReducedTemporary(helper_stack, instr_list, temp_counter, block_id);
    std::string left_operand =
        getReducedTemporary(helper_stack, instr_list, temp_counter, block_id);
    current_instr_stream << left_operand << " "
                         << std::string(bin_op->getOpcodeStr()) << " "
                         << right_operand;
  } else {
    current_instr_stream << getReducedTemporary(helper_stack, instr_list,
                                                temp_counter, block_id);
  }
  return current_instr_stream.str();
}

// Start handling LHS and RHS separately
void MyCFGDumper::handleAssignment(std::stack<const Stmt *> &helper_stack,
                                   InstructionListTy &instr_list,
                                   int &temp_counter,
                                   unsigned &block_id) const {
  helper_stack.pop(); // remove the assignment operator
  std::stringstream current_instr_stream;
  std::string LHS;
  UnaryOperator *un_op = nullptr;
  if (isa<UnaryOperator>(helper_stack.top())) {
    un_op =
        const_cast<UnaryOperator *>(cast<UnaryOperator>(helper_stack.top()));
    helper_stack.pop();
  }

  LHS = reduce(helper_stack, instr_list, temp_counter, block_id,
               REDUCE_EXPR_TO_SINGLE_TEMP);

  // in this case, only possible unary operators are * and &
  LHS =
      getOperandWithSideEffects(LHS, un_op, instr_list, temp_counter, block_id);
  current_instr_stream << LHS << " = ";
  current_instr_stream << reduce(helper_stack, instr_list, temp_counter,
                                 block_id, REDUCE_EXPR_TO_THREE_ADDR);
  instr_list.push_back(current_instr_stream.str());
}

void MyCFGDumper::handleBBStmts(const CFGBlock *bb) const {

  unsigned bb_id = bb->getBlockID();

  std::stack<const Stmt *> helper_stack;
  int temp_counter = 1; // for naming temporaries
  InstructionListTy instruction_list;

  for (auto elem : *bb) {
    // ref: https://clang.llvm.org/doxygen/CFG_8h_source.html#l00056
    // ref for printing block:
    // https://clang.llvm.org/doxygen/CFG_8cpp_source.html#l05234

    Optional<CFGStmt> CS = elem.getAs<CFGStmt>();
    const Stmt *S = CS->getStmt();

    if (isa<ImplicitCastExpr>(S))
      continue;

    helper_stack.push(S);

    // check for assignment
    switch (S->getStmtClass()) {
      // default:
      // llvm::errs() << S->getStmtClassName() << "\n";
      // S->dump();
      // llvm::errs() << "\n\n";
      // break;

    case Stmt::BinaryOperatorClass: {
      const BinaryOperator *bin_op = cast<BinaryOperator>(S);
      if (bin_op->isAssignmentOp()) {
        // start handling everything
        handleAssignment(helper_stack, instruction_list, temp_counter, bb_id);
      }
      break;
    }

    case Stmt::DeclStmtClass:
      handleDeclStmt(helper_stack, instruction_list, temp_counter, bb_id);
      break;

    } // switch
  }   // for

  for (auto it : instruction_list) {
    llvm::errs() << it << "\n";
  }
  // get terminator
  // const Stmt *terminator = (bb->getTerminator()).getStmt();
  // handleTerminator(terminator, helper_stack, temp_counter, bb_id);
  // llvm::errs() << "\n\n\n\n";
} // handleBBStmts()

std::string MyCFGDumper::getIntegerLiteralValue(const Stmt *IL_Stmt) const {
  const IntegerLiteral *IL = cast<IntegerLiteral>(IL_Stmt);
  bool is_signed = IL->getType()->isSignedIntegerType();
  return IL->getValue().toString(10, is_signed);
}

std::string MyCFGDumper::getDeclRefExprValue(const Stmt *DRE_Stmt) const {
  const DeclRefExpr *DRE = cast<DeclRefExpr>(DRE_Stmt);
  const ValueDecl *ident = DRE->getDecl();
  return ident->getName();
}

} // anonymous namespace

// Register the Checker
void ento::registerMyCFGDumper(CheckerManager &mgr) {
  mgr.registerChecker<MyCFGDumper>();
}