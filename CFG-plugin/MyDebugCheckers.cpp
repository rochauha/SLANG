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
#include "clang/AST/Decl.h" //AD
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

using namespace clang;
using namespace ento;

//===----------------------------------------------------------------------===//
// MyCFGDumper
//===----------------------------------------------------------------------===//

namespace {
class MyCFGDumper : public Checker<check::ASTCodeBody> {
public:
  void checkASTCodeBody(const Decl *D, AnalysisManager &mgr,
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
        QualType T =
            varDecl->getTypeSourceInfo()
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
        unsigned bb_id = bb->getBlockID();
        llvm::errs() << "BB" << bb_id << ":\n";
        for (auto elem : *bb) {
          // ref: https://clang.llvm.org/doxygen/CFG_8h_source.html#l00056
          // ref for printing block:
          // https://clang.llvm.org/doxygen/CFG_8cpp_source.html#l05234

          // Dump partial AST for each basic block
          Optional<CFGStmt> CS = elem.getAs<CFGStmt>();
          const Stmt *S = CS->getStmt();
          // S->dump(); // Dumps partial AST

          switch (S->getStmtClass()) {
          case Stmt::DeclStmtClass:
            llvm::errs() << "DeclStmt\n";
            cast<DeclStmt>(S)->getSingleDecl();
            break;
          case Stmt::IfStmtClass: {
            llvm::errs() << "IfStmt\n";
            const VarDecl *var = cast<IfStmt>(S)->getConditionVariable();
            break;
          }
          case Stmt::ForStmtClass: {
            llvm::errs() << "ForStmt\n";
            const VarDecl *var = cast<ForStmt>(S)->getConditionVariable();
            break;
          }
          case Stmt::WhileStmtClass: {
            llvm::errs() << "WhileStmt\n";
            const VarDecl *var = cast<WhileStmt>(S)->getConditionVariable();
            break;
          }
          case Stmt::SwitchStmtClass: {
            llvm::errs() << "SwitchStmt\n";
            const VarDecl *var = cast<SwitchStmt>(S)->getConditionVariable();
            break;
          }
          default:
            break;
          }
        }
      }
    }
  }
};
} // anonymous namespace

void ento::registerMyCFGDumper(CheckerManager &mgr) {
  mgr.registerChecker<MyCFGDumper>();
}

// if (elem.getKind() == CFGElement::Kind::Statement) {
//   auto cfgstmt = elem.getAs<CFGStmt>();
//   //auto stmt = cfgstmt.getStmt();
//   //stmt->dump();
//   //llvm::errs() << "AD: " << cfgstmt << "\n";
// }

// void DeclPrinter::printDeclType(QualType T, StringRef DeclName, bool Pack) {
//   // Normally, a PackExpansionType is written as T[3]... (for instance, as a
//   // template argument), but if it is the type of a declaration, the ellipsis
//   // is placed before the name being declared.
//   if (auto *PET = T->getAs<PackExpansionType>()) {
//     Pack = true;
//     T = PET->getPattern();
//   }
//   T.print(Out, Policy, (Pack ? "..." : "") + DeclName, Indentation);
// }

// void DeclPrinter::VisitVarDecl(VarDecl *D) {
//   prettyPrintPragmas(D);
//
//   QualType T = D->getTypeSourceInfo()
//     ? D->getTypeSourceInfo()->getType()
//     : D->getASTContext().getUnqualifiedObjCPointerType(D->getType());
//
//  ...
//  printDeclType(T, D->getName());
// AD If class name added or changed also edit,
// AD ../../../include/clang/StaticAnalyzer/Checkers/Checkers.td

// void DeclPrinter::VisitParmVarDecl(ParmVarDecl *D) {
//   VisitVarDecl(D);
// }
