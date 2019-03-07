//===----------------------------------------------------------------------===//
//  MIT License.
//  Copyright (c) 2019 The SLANG Authors.
//
//  Author: Ronak Chauhan (r.chauhan@somaiya.edu)
//  Author: Anshuman Dhuliya [AD] (dhuliya@cse.iitb.ac.in)
//
//AD If SlangGenChecker class name is added or changed, then also edit,
//AD ../../../../include/clang/StaticAnalyzer/Checkers/Checkers.td
//
//===----------------------------------------------------------------------===//
//

#include "ClangSACheckers.h"
#include "clang/AST/Expr.h" //AD
#include "clang/AST/Stmt.h" //AD
#include "clang/AST/Type.h" //AD
#include "clang/AST/Decl.h" //AD
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/AnalysisManager.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/raw_ostream.h" //AD
#include <string>                     //AD
#include <vector>                     //AD
#include <utility>                    //AD
#include <unordered_map>              //AD
#include <fstream>                    //AD
#include <sstream>                    //AD

#include "SlangUtil.h"
#include "SlangExpr.h"
#include "SlangTranslationUnit.h"

using namespace clang;
using namespace ento;
using namespace slang;

// int span_add_nums(int a, int b); // for testing linking with external lib (success)

//===----------------------------------------------------------------------===//
// SlangGenChecker
//===----------------------------------------------------------------------===//

namespace {
    /**
     * Generate the SLANG (SPAN IR) from Clang AST.
     */
    class SlangGenChecker : public Checker<check::ASTCodeBody, check::EndOfTranslationUnit> {
        static SlangTranslationUnit stu;
        static const FunctionDecl *FD;  // funcDecl

    public:
        // mainentry
        void checkASTCodeBody(const Decl *D, AnalysisManager &mgr,
                              BugReporter &BR) const;
        void checkEndOfTranslationUnit(const TranslationUnitDecl *TU,
                AnalysisManager &Mgr, BugReporter &BR) const;

        // handling_routines
        void handleFunctionDef(const FunctionDecl *D) const;
        void handleCfg(const CFG *cfg) const;
        void handleBbInfo(const CFGBlock *bb, const CFG *cfg) const;
        void handleBbStmts(const CFGBlock *bb) const;
        void handleStmt(const Stmt *stmt) const;
        void handleVariable(const ValueDecl *valueDecl) const;
        void handleDeclStmt(const DeclStmt *declStmt) const;
        void handleDeclRefExpr(const DeclRefExpr *DRE) const;
        void handleBinaryOperator(const BinaryOperator *binOp) const;
        void handleReturnStmt() const;
        void handleIfStmt() const;

        // conversion_routines
        SlangExpr convertAssignment(bool compound_receiver) const;
        SlangExpr convertIntegerLiteral(const IntegerLiteral *IL) const;
        // a function, if stmt, *y on lhs, arr[i] on lhs are examples of a compound_receiver.
        SlangExpr convertExpr(bool compound_receiver) const;
        SlangExpr convertDeclRefExpr(const DeclRefExpr *dre) const;
        SlangExpr convertVarDecl(const VarDecl *varDecl) const;
        SlangExpr convertUnaryOp(const UnaryOperator *unOp, bool compound_receiver) const;
        SlangExpr convertUnaryIncDec(const UnaryOperator *unOp, bool compound_receiver) const;
        SlangExpr convertBinaryOp(const BinaryOperator *binOp, bool compound_receiver) const;
        void adjustDirtyVar(SlangExpr& slangExpr) const;
        std::string convertClangType(QualType qt) const;

        // helper_functions
        SlangExpr genTmpVariable(QualType qt) const;
        SlangExpr getTmpVarForDirtyVar(uint64_t varId,
                QualType qualType, bool& newTmp) const;
        bool isTopLevel(const Stmt* stmt) const;
        uint64_t getLocationId(const Stmt *stmt) const;

    }; // class SlangGenChecker
} // anonymous namespace

SlangTranslationUnit SlangGenChecker::stu = SlangTranslationUnit();
const FunctionDecl* SlangGenChecker::FD = nullptr;

// mainentry, main entry point. Invokes top level Function and Cfg handlers.
// It is invoked once for each source translation unit function.
void SlangGenChecker::checkASTCodeBody(const Decl *D, AnalysisManager &mgr,
                                   BugReporter &BR) const {
    SLANG_EVENT("BOUND START: SLANG_Generated_Output.\n")

    // SLANG_DEBUG("slang_add_nums: " << slang_add_nums(1,2) << "only\n"; // lib testing

    const FunctionDecl *funcDecl = dyn_cast<FunctionDecl>(D);
    handleFunctionDef(funcDecl);

    if (const CFG *cfg = mgr.getCFG(D)) {
        handleCfg(cfg);
        stu.dumpSlangIr();
    } else {
        SLANG_ERROR("No CFG for function.")
    }

    SLANG_EVENT("BOUND END  : SLANG_Generated_Output.\n")
} // checkASTCodeBody()

void SlangGenChecker::checkEndOfTranslationUnit(const TranslationUnitDecl *TU,
                               AnalysisManager &Mgr, BugReporter &BR) const {
    SLANG_EVENT("Translation Unit Converted.")
}

//BOUND START: handling_routines

// Gets the function name, parameters and return type.
void SlangGenChecker::handleFunctionDef(const FunctionDecl *funcDecl) const {
    // STEP 1.1: Get function name.
    std::string funcName = funcDecl->getNameInfo().getAsString();
    SLANG_DEBUG("AddingFunction: " << funcName)
    stu.addFunction(funcName);
    FD = funcDecl;

    // STEP 1.2: Get function parameters.
    std::stringstream ss;
    if (funcDecl->doesThisDeclarationHaveABody()) { //& !funcDecl->hasPrototype())
        for (unsigned i = 0, e = funcDecl->getNumParams(); i != e; ++i) {
            const ParmVarDecl *paramVarDecl = funcDecl->getParamDecl(i);
            handleVariable(paramVarDecl); // adds the var too
            stu.pushBackFuncParams(stu.getVar((uint64_t)paramVarDecl).name);
        }
    }

    // STEP 1.3: Get function return type.
    const QualType returnQType = funcDecl->getReturnType();
    std::string retTypeStr = convertClangType(returnQType);
    stu.setFuncReturnType(retTypeStr);
} // handleFunctionDef()

void SlangGenChecker::handleCfg(const CFG *cfg) const {
    for (const CFGBlock *bb : *cfg) {
        handleBbInfo(bb, cfg);
        handleBbStmts(bb);
    }
} // handleCfg()

void SlangGenChecker::handleBbInfo(const CFGBlock *bb, const CFG *cfg) const {
    int32_t succId, bbId;
    unsigned entryBbId = cfg->getEntry().getBlockID();

    if ((bbId = bb->getBlockID()) == (int32_t)entryBbId) {
        bbId = -1; //entry block is ided -1.
    }
    stu.addBb(bbId);

    SLANG_DEBUG("BB" << bbId)

    if (bb == &cfg->getEntry()) {
        SLANG_DEBUG("ENTRY BB")
    } else if (bb == &cfg->getExit()) {
        SLANG_DEBUG("EXIT BB")
    }

    // access and record successor blocks
    const Stmt *stmt = (bb->getTerminator()).getStmt();
    if (stmt && (isa<IfStmt>(stmt) || isa<WhileStmt>(stmt))) {
        // if here, then only conditional edges are present
        bool trueEdge = true;
        if (bb->succ_size() > 2) {
            SLANG_ERROR("'If' has more than two successors.")
        }

        for (CFGBlock::const_succ_iterator I = bb->succ_begin();
             I != bb->succ_end(); ++I) {

            CFGBlock *succ = *I;
            if ((succId = succ->getBlockID()) == (int32_t)entryBbId) {
                succId = -1;
            }

            if (trueEdge) {
                stu.addBbEdge(std::make_pair(bbId, std::make_pair(succId, TrueEdge)));
                trueEdge = false;
            } else {
                stu.addBbEdge(std::make_pair(bbId, std::make_pair(succId, FalseEdge)));
            }
        }
    } else {
        // if here, then only unconditional edges are present
        if (!bb->succ_empty()) {
            // num. of succ: bb->succ_size()
            for (CFGBlock::const_succ_iterator I = bb->succ_begin();
                    I != bb->succ_end(); ++I) {
                CFGBlock *succ = *I;
                if (!succ) {
                    // unreachable block ??
                    succ = I->getPossiblyUnreachableBlock();
                    SLANG_DEBUG("(Unreachable BB)")
                    continue;
                }

                if ((succId = succ->getBlockID()) == (int32_t)entryBbId) {
                    succId = -1;
                }

                stu.addBbEdge(std::make_pair(bbId, std::make_pair(succId, UnCondEdge)));
            }
        }
    }
} // handleBbInfo()

void SlangGenChecker::handleBbStmts(const CFGBlock *bb) const {
    for (auto elem : *bb) {
        // ref: https://clang.llvm.org/doxygen/CFG_8h_source.html#l00056
        // ref for printing block:
        // https://clang.llvm.org/doxygen/CFG_8cpp_source.html#l05234

        Optional<CFGStmt> CS = elem.getAs<CFGStmt>();
        const Stmt *stmt = CS->getStmt();
        handleStmt(stmt);

        if (isTopLevel(stmt)) {
            stu.clearDirtyVars();
            // stu.clearMainStack();
        }
    } // for (auto elem : *bb)

    // get terminator
    const Stmt *stmt = (bb->getTerminator()).getStmt();
    if (stmt) { handleStmt(stmt); }

} // handleBbStmts()

void SlangGenChecker::handleStmt(const Stmt *stmt) const {
    Stmt::StmtClass stmtClass = stmt->getStmtClass();

    stu.printMainStack();
    SLANG_DEBUG("Processing: " << stmt->getStmtClassName())

    // to handle each kind of statement/expression.
    switch (stmtClass) {
        default: {
            // push to stack by default.
            stu.pushToMainStack(stmt);

            SLANG_DEBUG("SLANG: DEFAULT: Pushed to stack: " << stmt->getStmtClassName())
            stmt->dump();
            break;
        }

        case Stmt::DeclRefExprClass: {
            handleDeclRefExpr(cast<DeclRefExpr>(stmt));
            break;
        }

        case Stmt::DeclStmtClass: {
            handleDeclStmt(cast<DeclStmt>(stmt));
            break;
        }

        case Stmt::BinaryOperatorClass: {
            handleBinaryOperator(cast<BinaryOperator>(stmt));
            break;
        }

        case Stmt::ReturnStmtClass: { handleReturnStmt(); break; }

        case Stmt::WhileStmtClass: // same as Stmt::IfStmtClass
        case Stmt::IfStmtClass: { handleIfStmt(); break; }

        case Stmt::ImplicitCastExprClass: {
            // do nothing
            break;
        }
    }
} // handleStmt()

// record the variable name and type
void SlangGenChecker::handleVariable(const ValueDecl *valueDecl) const {
    uint64_t varAddr = (uint64_t) valueDecl;
    std::string varName;
    if (stu.isNewVar(varAddr)) {
        // seeing the variable for the first time.
        SlangVar slangVar{};
        slangVar.id = varAddr;
        const VarDecl *varDecl = dyn_cast<VarDecl>(valueDecl);
        if (varDecl) {
            varName = valueDecl->getNameAsString();
            if (varDecl->hasLocalStorage()) {
                slangVar.setLocalVarName(varName, stu.getCurrFuncName());
            } else if(varDecl->hasGlobalStorage()) {
                slangVar.setGlobalVarName(varName);
            } else if (varDecl->hasExternalStorage()) {
                SLANG_ERROR("External Storage Not Handled.")
            } else {
                SLANG_ERROR("Unknown variable storage.")
            }
        } else {
            SLANG_ERROR("ValueDecl not a VarDecl!")
        }
        slangVar.typeStr = convertClangType(valueDecl->getType());
        stu.addVar(slangVar.id, slangVar);
        SLANG_DEBUG("NEW_VAR: " << slangVar.convertToString())
    } else {
        SLANG_DEBUG("SEEN_VAR: " << stu.getVar(varAddr).convertToString())
    }
} // handleVariable()

void SlangGenChecker::handleDeclStmt(const DeclStmt *declStmt) const {
    // assumes there is only single decl inside (the likely case).
    std::stringstream ss;

    const VarDecl *varDecl = cast<VarDecl>(declStmt->getSingleDecl());
    handleVariable(varDecl);

    if (!stu.isMainStackEmpty()) {
        // there is smth on the stack, hence on the rhs.
        SlangExpr slangExpr{};
        auto exprLhs = convertVarDecl(varDecl);
        exprLhs.locId = getLocationId(declStmt);
        auto exprRhs = convertExpr(exprLhs.compound);

        // order_correction for DeclStmt
        slangExpr.addSlangStmts(exprRhs.slangStmts);
        slangExpr.addSlangStmts(exprLhs.slangStmts);

        // slangExpr.qualType = exprLhs.qualType;
        ss << "instr.AssignI(" << exprLhs.expr << ", " << exprRhs.expr << ")";
        slangExpr.addSlangStmt(ss.str());

        stu.addBbStmts(slangExpr.slangStmts);
    }
} // handleDeclStmt()

void SlangGenChecker::handleIfStmt() const {
    std::stringstream ss;

    auto exprArg = convertExpr(true);
    ss << "instr.CondI(" << exprArg.expr << ")";

    // order_correction for if stmt
    exprArg.addSlangStmt(ss.str());
    stu.addBbStmts(exprArg.slangStmts);
} // handleIfStmt()

void SlangGenChecker::handleReturnStmt() const {
    std::stringstream ss;

    if (!stu.isMainStackEmpty()) {
        // return has an argument
        auto exprArg = convertExpr(true);
        ss << "instr.ReturnI(" << exprArg.expr << ")";

        // order_correction for return stmt
        exprArg.addSlangStmt(ss.str());
        stu.addBbStmts(exprArg.slangStmts);
    } else {
        ss << "instr.ReturnI()";
        stu.addBbStmt(ss.str()); //okay
    }
} // handleReturnStmt()

void SlangGenChecker::handleDeclRefExpr(const DeclRefExpr *declRefExpr) const {
    stu.pushToMainStack(declRefExpr);

    const ValueDecl *valueDecl = declRefExpr->getDecl();
    if (isa<VarDecl>(valueDecl)) {
        handleVariable(valueDecl);
    } else {
        SLANG_DEBUG("handleDeclRefExpr: unhandled " << declRefExpr->getStmtClassName())
    }
} // handleDeclRefExpr()

void SlangGenChecker::handleBinaryOperator(const BinaryOperator *binOp) const {
    if (binOp->isAssignmentOp() && isTopLevel(binOp)) {
        SlangExpr slangExpr = convertAssignment(false); // top level is never compound
        stu.addBbStmts(slangExpr.slangStmts);
    } else {
        stu.pushToMainStack(binOp);
    }
} // handleBinaryOperator()

//BOUND END  : handling_routines

//BOUND START: conversion_routines to SlangExpr

// convert top of stack to SPAN IR.
// returns converted string, and false if the converted string is only a simple const/var expression.
SlangExpr SlangGenChecker::convertExpr(bool compound_receiver) const {
    std::stringstream ss;

    const Stmt *stmt = stu.popFromMainStack();

    switch(stmt->getStmtClass()) {
        case Stmt::IntegerLiteralClass: {
            const IntegerLiteral *il = cast<IntegerLiteral>(stmt);
            return convertIntegerLiteral(il);
        }

        case Stmt::DeclRefExprClass: {
            const DeclRefExpr *declRefExpr = cast<DeclRefExpr>(stmt);
            return convertDeclRefExpr(declRefExpr);
        }

        case Stmt::BinaryOperatorClass: {
            const BinaryOperator *binOp = cast<BinaryOperator>(stmt);
            return convertBinaryOp(binOp, compound_receiver);
        }

        case Stmt::UnaryOperatorClass: {
            auto unOp = cast<UnaryOperator>(stmt);
            return convertUnaryOp(unOp, compound_receiver);
        }

        default: {
            // error state
            SLANG_ERROR("UnknownStmt: " << stmt->getStmtClassName())
            stmt->dump();
            return SlangExpr("ERROR:convertExpr", false, QualType());
        }
    }
    // return SlangExpr("ERROR:convertExpr", false, QualType());
} // convertExpr()

SlangExpr SlangGenChecker::convertIntegerLiteral(const IntegerLiteral *il) const {
    std::stringstream ss;

    bool is_signed = il->getType()->isSignedIntegerType();
    ss << "expr.Lit(" << il->getValue().toString(10, is_signed) << ")";
    SLANG_TRACE(ss.str())

    return SlangExpr(ss.str(), false, il->getType());
} // convertIntegerLiteral()

SlangExpr SlangGenChecker::convertAssignment(bool compound_receiver) const {
    std::stringstream ss;
    SlangExpr slangExpr{};

    auto exprLhs = convertExpr(false); // unconditionally false
    auto exprRhs = convertExpr(exprLhs.compound);

    if (compound_receiver && exprLhs.compound) {
        slangExpr = genTmpVariable(exprLhs.qualType);

        // order_correction for assignment
        slangExpr.addSlangStmts(exprRhs.slangStmts);
        slangExpr.addSlangStmts(exprLhs.slangStmts);

        ss << "instr.AssignI(" << exprLhs.expr << ", " << exprRhs.expr << ")";
        slangExpr.addSlangStmt(ss.str());

        ss.str(""); // empty the stream
        ss << "instr.AssignI(" << slangExpr.expr << ", " << exprLhs.expr << ")";
        slangExpr.addSlangStmt(ss.str());
    } else {
        // order_correction for assignment
        slangExpr.addSlangStmts(exprRhs.slangStmts);
        slangExpr.addSlangStmts(exprLhs.slangStmts);

        ss << "instr.AssignI(" << exprLhs.expr << ", " << exprRhs.expr << ")";
        slangExpr.addSlangStmt(ss.str());

        slangExpr.expr = exprLhs.expr;
        slangExpr.qualType = exprLhs.qualType;
        slangExpr.compound = exprLhs.compound;
        slangExpr.nonTmpVar = exprLhs.nonTmpVar;
        slangExpr.varId = exprLhs.varId;
    }

    if (slangExpr.nonTmpVar) {
        SlangExpr emptySlangExpr = SlangExpr{};
        stu.setDirtyVar(slangExpr.varId, emptySlangExpr);
    }

    return slangExpr;
}

void SlangGenChecker::adjustDirtyVar(SlangExpr& slangExpr) const {
    std::stringstream ss;
    bool newTmp;
    if (slangExpr.isNonTmpVar() && stu.isDirtyVar(slangExpr.varId)) {
        SlangExpr sp = getTmpVarForDirtyVar(slangExpr.varId,
                slangExpr.qualType, newTmp);
        if (newTmp) {
            // only add if a new temporary was generated
            ss << "instr.AssignI(" << sp.expr << ", " << slangExpr.expr << ")";
            slangExpr.addSlangStmt(ss.str());
        }
        slangExpr.expr = sp.expr;
        slangExpr.nonTmpVar = false;
    }
}

SlangExpr SlangGenChecker::convertBinaryOp(const BinaryOperator *binOp,
        bool compound_receiver) const {
    std::stringstream ss;
    std::string op;
    SlangExpr varExpr{};

    if (binOp->isAssignmentOp()) {
        return convertAssignment(compound_receiver);
    }

    SlangExpr exprR = convertExpr(true);
    SlangExpr exprL = convertExpr(true);
    adjustDirtyVar(exprL);

    if (compound_receiver) {
        varExpr = genTmpVariable(exprL.qualType);
        ss << "instr.AssignI(" << varExpr.expr << ", ";
    }

    // order_correction binary operator
    varExpr.addSlangStmts(exprL.slangStmts);
    varExpr.addSlangStmts(exprR.slangStmts);

    varExpr.qualType = exprL.qualType;

    switch(binOp->getOpcode()) {
        default: {
            SLANG_DEBUG("convertBinaryOp: " << binOp->getOpcodeStr())
            return SlangExpr("ERROR:convertBinaryOp", false, QualType());
        }

        case BO_Rem: { op = "op.Modulo"; break; }
        case BO_Add: { op = "op.Add"; break; }
        case BO_Sub: { op = "op.Sub"; break;}
        case BO_Mul: { op = "op.Mul"; break;}
        case BO_Div: { op = "op.Div"; break;}
    }

    ss << "expr.BinaryE(" << exprL.expr << ", " << op << ", " << exprR.expr << ")";

    if (compound_receiver) {
        ss << ")"; // close instr.AssignI(...
        varExpr.addSlangStmt(ss.str());
    } else {
        varExpr.expr = ss.str();
        varExpr.compound = true; // since a binary expression
    }

    return varExpr;
}

SlangExpr SlangGenChecker::convertUnaryOp(const UnaryOperator *unOp,
        bool compound_receiver) const {
    std::stringstream ss;
    std::string op;
    SlangExpr varExpr{};
    QualType qualType;

    switch (unOp->getOpcode()) {
        // special handling
        case UO_PreInc:
        case UO_PreDec:
        case UO_PostInc:
        case UO_PostDec: {
            return convertUnaryIncDec(unOp, compound_receiver);
        }

        default: {
            break;
        }
    }

    SlangExpr exprArg = convertExpr(true);
    adjustDirtyVar(exprArg);
    qualType = exprArg.qualType;

    switch(unOp->getOpcode()) {
        default: {
            SLANG_DEBUG("convertUnaryOp: " << unOp->getOpcodeStr(unOp->getOpcode()))
            return SlangExpr("ERROR:convertUnaryOp", false, QualType());
        }

        case UO_AddrOf: {
            qualType = FD->getASTContext().getPointerType(exprArg.qualType);
            op = "op.AddrOf";
            break;
        }

        case UO_Deref: {
            auto ptr_type = cast<PointerType>(exprArg.qualType.getTypePtr());
            qualType = ptr_type->getPointeeType();
            op = "op.Deref";
            break;
        }

        case UO_Minus: { op = "op.Minus"; break;}
        case UO_Plus: { op = "op.Plus"; break;}
    }

    ss << "expr.UnaryE(" << op << ", " << exprArg.expr << ")";

    if (compound_receiver) {
        varExpr = genTmpVariable(qualType);
        ss << "instr.AssignI(" << varExpr.expr << ", ";
    }

    // order_correction unary operator
    varExpr.addSlangStmts(exprArg.slangStmts);

    if (compound_receiver) {
        ss << ")"; // close instr.AssignI(...
        varExpr.addSlangStmt(ss.str());
    } else {
        varExpr.expr = ss.str();
        varExpr.compound = true;
        varExpr.qualType = qualType;
    }
    return varExpr;
} // convertUnaryOp()

SlangExpr SlangGenChecker::convertUnaryIncDec(const UnaryOperator *unOp,
        bool compound_receiver) const {
    std::stringstream ss;
    SlangExpr exprArg = convertExpr(true);
    SlangExpr emptySlangExpr = SlangExpr{};

    switch(unOp->getOpcode()) {
        case UO_PreInc: {
            ss << "instr.AssignI(" << exprArg.expr << ", ";
            ss << "expr.BinaryE(" << exprArg.expr << ", op.Add, expr.LitE(1)))";
            exprArg.addSlangStmt(ss.str());

            if (exprArg.nonTmpVar && stu.isDirtyVar(exprArg.varId)) {
                adjustDirtyVar(exprArg);
            }
            stu.setDirtyVar(exprArg.varId, emptySlangExpr);
            break;
        }

        case UO_PostInc: {
            ss << "instr.AssignI(" << exprArg.expr << ", ";
            ss << "expr.BinaryE(" << exprArg.expr << ", op.Add, expr.LitE(1)))";

            if (exprArg.nonTmpVar) {
                stu.setDirtyVar(exprArg.varId, emptySlangExpr);
                adjustDirtyVar(exprArg);
            }
            // add increment after storing in temporary
            exprArg.addSlangStmt(ss.str());
            break;
        }

        default: {
            SLANG_ERROR("UnknownOp")
            break;
        }
    }

    return exprArg;
} // convertUnaryIncDec()

SlangExpr SlangGenChecker::convertVarDecl(const VarDecl *varDecl) const {
    std::stringstream ss;
    SlangExpr slangExpr{};

    ss << "expr.VarE(\"" << stu.convertVarExpr((uint64_t)varDecl) << "\")";
    slangExpr.expr = ss.str();
    slangExpr.compound = false;
    slangExpr.qualType = varDecl->getType();
    slangExpr.nonTmpVar = true;
    slangExpr.varId = (uint64_t)varDecl;

    return slangExpr;
}

SlangExpr SlangGenChecker::convertDeclRefExpr(const DeclRefExpr *dre) const {
    std::stringstream ss;

    const ValueDecl *valueDecl = dre->getDecl();
    if (isa<VarDecl>(valueDecl)) {
        auto varDecl = cast<VarDecl>(valueDecl);
        SlangExpr slangExpr = convertVarDecl(varDecl);
        slangExpr.locId = getLocationId(dre);
        return slangExpr;
    } else {
        SLANG_ERROR("Not_a_VarDecl.")
        return SlangExpr("ERROR:convertDeclRefExpr", false, QualType());
    }
}

std::string SlangGenChecker::convertClangType(QualType qt) const {
    std::stringstream ss;
    const Type *type = qt.getTypePtr();
    if (type->isBuiltinType()) {
        if(type->isIntegerType()) {
            if(type->isCharType()) {
                ss << "types.Char";
            } else {
                ss << "types.Int";
            }
        } else if(type->isFloatingType()) {
            ss << "types.Float";
        } else if(type->isVoidType()) {
            ss << "types.Void";
        } else {
            ss << "UnknownBuiltinType.";
        }
    } else if(type->isPointerType()) {
        ss << "types.Ptr(to=";
        QualType pqt = type->getPointeeType();
        ss << convertClangType(pqt);
        ss << ")";
    } else {
        ss << "UnknownType.";
    }

    return ss.str();
} // convertClangType()

//BOUND END  : conversion_routines to SlangExpr

//BOUND START: helper_functions

// returns an empty SlangExpr if var is not dirty
SlangExpr SlangGenChecker::getTmpVarForDirtyVar(uint64_t varId,
        QualType qualType,
        bool& newTmp) const {
    SlangExpr slangExpr;
    newTmp = false;

    if (!stu.isDirtyVar(varId)) {
        return slangExpr;
    }

    slangExpr = stu.getTmpVarForDirtyVar(varId);
    if (slangExpr.expr.size() == 0) {
        newTmp = true;
        // allocate tmp var on demand
        slangExpr = genTmpVariable(qualType);
        stu.setDirtyVar(varId, slangExpr);
    }

    return slangExpr;
}

SlangExpr SlangGenChecker::genTmpVariable(QualType qt) const {
    std::stringstream ss;
    SlangExpr slangExpr{};

    // STEP 1: Populate a SlangVar object with unique name.
    SlangVar slangVar{};
    slangVar.id = stu.nextTmpId();
    ss << "t." << slangVar.id;
    slangVar.setLocalVarName(ss.str(), stu.getCurrFuncName());
    slangVar.name = ss.str();
    slangVar.typeStr = convertClangType(qt);

    // STEP 2: Add to the var map.
    // FIXME: The var's 'id' here should be small enough to not interfere with uint64_t addresses.
    stu.addVar(slangVar.id, slangVar);

    // STEP 3: generate var expression.
    ss.str(""); // empty the stream
    ss << "expr.VarE(\"" << slangVar.name << "\")";

    slangExpr.expr = ss.str();
    slangExpr.compound = false;
    slangExpr.qualType = qt;
    slangExpr.nonTmpVar = false;

    return slangExpr;
} // genTmpVariable()

// get and encode the location of a statement element
uint64_t SlangGenChecker::getLocationId(const Stmt *stmt) const {
    uint64_t locId = 0;
    uint32_t line = 0;
    uint32_t col = 0;

    line = FD->getASTContext().getSourceManager().getExpansionLineNumber(stmt->getLocStart());
    col  = FD->getASTContext().getSourceManager().getExpansionColumnNumber(stmt->getLocStart());

    locId = line;
    locId <<= 32;
    locId |= col;

    return locId; // line_32 | col_32
} // getLocationId()

// If an element is top level, return true.
// e.g. in statement "x = y = z = 10;" the first "=" from left is top level.
bool SlangGenChecker::isTopLevel(const Stmt* stmt) const {
    // see MyTraverseAST.cpp for more details.
    const auto &parents = FD->getASTContext().getParents(*stmt);
    if (!parents.empty()) {
        const Stmt *stmt1 = parents[0].get<Stmt>();
        if (stmt1) {
            // SLANG_DEBUG("Parent: " << stmt1->getStmtClassName())
            switch (stmt1->getStmtClass()) {
                default: {
                    return false;
                    break;
                }
                case Stmt::WhileStmtClass:
                case Stmt::ForStmtClass:
                case Stmt::CaseStmtClass:
                case Stmt::DefaultStmtClass:
                case Stmt::CompoundStmtClass: {
                    return true; // top level
                }
            }
        } else {
            // SLANG_DEBUG("Parent: Cannot print")
            return false;
        }
    } else {
        return true; // top level
    }
} // isTopLevel()

//BOUND END  : helper_functions

// Register the Checker
void ento::registerSlangGenChecker(CheckerManager &mgr) {
    mgr.registerChecker<SlangGenChecker>();
}

