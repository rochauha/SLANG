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
#include "clang/AST/Expr.h" //AD
#include "clang/AST/Stmt.h" //AD
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
#include <vector>                     // RC

using namespace clang;
using namespace ento;

//===----------------------------------------------------------------------===//
// MyCFGDumper
//===----------------------------------------------------------------------===//

namespace {
class MyCFGDumper : public Checker<check::ASTCodeBody> {
    typedef std::vector<const Stmt *> ElementListTy;

  public:
    void checkASTCodeBody(const Decl *D, AnalysisManager &mgr, BugReporter &BR) const;

  private:
    void getElements(const Stmt *expression_top, ElementListTy &elem_list) const;
    void handleCase(const CaseStmt *case_stmt) const;
    void dump_list(ElementListTy &elem_list) const;
};

void MyCFGDumper::checkASTCodeBody(const Decl *D, AnalysisManager &mgr, BugReporter &BR) const {
    llvm::errs() << "FuncName: ";
    const FunctionDecl *func_decl = dyn_cast<FunctionDecl>(D);
    llvm::errs() << func_decl->getNameInfo().getAsString() << "\n";

    if (CFG *cfg = mgr.getCFG(D)) {
        for (auto bb : *cfg) {
            unsigned bb_id = bb->getBlockID();
            llvm::errs() << "BB" << bb_id << ":\n";
            auto term = (bb->getTerminator()).getStmt();

            if (term) {
                switch (term->getStmtClass()) {
                case Stmt::IfStmtClass: {
                    auto if_stmt = cast<IfStmt>(term);
                    auto condition = if_stmt->getCond();
                    condition->dump();
                    ElementListTy elem_list;
                    getElements(condition, elem_list);
                    break;
                }

                case Stmt::SwitchStmtClass: {
                    auto switch_stmt = cast<SwitchStmt>(term);
                    auto body = cast<CompoundStmt>(switch_stmt->getBody());
                    // body->dump();
                    for (auto it = body->body_begin(); it != body->body_end(); ++it) {
                        if (isa<CaseStmt>(*it))
                            handleCase(cast<CaseStmt>(*it));
                    }
                    break;
                }
                }
            }
        }
    }
}

void MyCFGDumper::handleCase(const CaseStmt *case_stmt) const {
    // for (auto it = case_stmt->child_begin(); it !=
    // case_stmt->child_end(); ++it) {
    // }
    const Expr *raw_condition = cast<Expr>(*(case_stmt->child_begin()));
    // raw_condition->dump();

    const Expr *condition = (isa<ImplicitCastExpr>(raw_condition) || isa<ParenExpr>(raw_condition))
                                ? raw_condition->IgnoreParenImpCasts()
                                : raw_condition;
    ElementListTy elem_list;
    getElements(condition, elem_list);
    dump_list(elem_list);
} // handleCase()

void MyCFGDumper::dump_list(ElementListTy &elem_list) const {
    llvm::errs() << "case list starts here :\n\n";
    for (auto it = elem_list.begin(); it != elem_list.end(); ++it) {
        (*it)->dump();
        llvm::errs() << "\n";
    }
    llvm::errs() << "case list ends here :\n\n\n";
} // dump_list

void MyCFGDumper::getElements(const Stmt *expression_top, ElementListTy &elem_list) const {
    switch (expression_top->getStmtClass()) {
    case Stmt::BinaryOperatorClass: {
        const BinaryOperator *bin_op = cast<BinaryOperator>(expression_top);
        getElements(bin_op->getLHS(), elem_list);
        getElements(bin_op->getRHS(), elem_list);
        break;
    }
    case Stmt::UnaryOperatorClass: {
        const UnaryOperator *un_op = cast<UnaryOperator>(expression_top);
        getElements(un_op->getSubExpr(), elem_list);
        break;
    }
    case Stmt::ImplicitCastExprClass: {
        const ImplicitCastExpr *imp_cast = cast<ImplicitCastExpr>(expression_top);
        getElements(imp_cast->getSubExpr(), elem_list);
        break;
    }
    }
    elem_list.push_back(expression_top);
} // getElements()

} // anonymous namespace

void ento::registerMyCFGDumper(CheckerManager &mgr) { mgr.registerChecker<MyCFGDumper>(); }