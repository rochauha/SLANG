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
#include "clang/Analysis/CFG.h"
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
#include <iomanip>                    //AD for std::fixed

#include "SlangUtil.h"
#include "SlangExpr.h"
#include "SlangTranslationUnit.h"

using namespace clang;
using namespace ento;
using namespace slang;

typedef std::vector<const Stmt *> StmtVector;

// TODO: add location id to all expressions and object definitions.
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
        void handleFunction(const FunctionDecl *funcDecl) const;
        void handleCfg(const CFG *cfg) const;
        void handleBbInfo(const CFGBlock *bb, const CFG *cfg) const;
        void handleBbStmts(const CFGBlock *bb) const;
        void handleStmt(const Stmt *stmt) const;
        void handleVariable(const ValueDecl *valueDecl,
                            std::string funcName) const;
        void handleDeclStmt(const DeclStmt *declStmt) const;
        void handleDeclRefExpr(const DeclRefExpr *DRE) const;
        void handleBinaryOperator(const BinaryOperator *binOp) const;
        void handleCallExpr(const CallExpr *callExpr) const;
        void handleReturnStmt() const;
        void handleIfStmt() const;
        void handleSwitchStmt(const SwitchStmt *switchStmt) const;

        // conversion_routines
        SlangExpr convertAssignment(bool compound_receiver,
                std::string* locStr) const;
        SlangExpr convertIntegerLiteral(const IntegerLiteral *IL) const;
        SlangExpr convertFloatingLiteral(const FloatingLiteral *fl) const;
        SlangExpr convertStringLiteral(const StringLiteral *sl) const;
        // a function, if stmt, *y on lhs, arr[i] on lhs are examples of a compound_receiver.
        SlangExpr convertExpr(bool compound_receiver) const;
        SlangExpr convertDeclRefExpr(const DeclRefExpr *dre) const;
        SlangExpr convertVarDecl(const VarDecl *varDecl, std::string& locStr) const;
        SlangExpr convertUnaryOp(const UnaryOperator *unOp, bool compound_receiver) const;
        SlangExpr convertUnaryIncDec(const UnaryOperator *unOp, bool compound_receiver) const;
        SlangExpr convertBinaryOp(const BinaryOperator *binOp, bool compound_receiver) const;
        SlangExpr convertEnumConst(const EnumConstantDecl* ecd, uint64_t locId) const;
        SlangExpr convertCallExpr(const CallExpr *callExpr,
                bool compound_receiver) const;
        void adjustDirtyVar(SlangExpr& slangExpr, std::string& locStr) const;
        std::string convertClangType(QualType qt) const;

        // helper_functions
        SlangExpr genTmpVariable(QualType qt, std::string& locStr) const;
        SlangExpr genTmpVariable(std::string slangTypeStr,
                std::string& locStr) const;
        SlangExpr getTmpVarForDirtyVar(uint64_t varId,
                QualType qualType, bool& newTmp) const;
        bool isTopLevel(const Stmt* stmt) const;
        uint64_t getLocationId(const Stmt *stmt) const;
        std::string getLocationString(const Stmt *stmt) const;
        StmtVector getCaseExprElements(const CaseStmt *caseStmt) const;
        void getCaseExprElements(StmtVector& stmts, const Stmt *stmt) const;

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
    if (stu.fileName.size() == 0) {
        stu.fileName = D->getASTContext().getSourceManager()
                       .getFilename(D->getLocStart()).str();
    }

    const FunctionDecl *funcDecl = dyn_cast<FunctionDecl>(D);
    handleFunctionDef(funcDecl);

    if (const CFG *cfg = mgr.getCFG(D)) {
        handleCfg(cfg);
    } else {
        SLANG_ERROR("No CFG for function.")
    }
} // checkASTCodeBody()

void SlangGenChecker::checkEndOfTranslationUnit(const TranslationUnitDecl *TU,
                               AnalysisManager &Mgr, BugReporter &BR) const {
    stu.dumpSlangIr();
    SLANG_EVENT("Translation Unit Ended.\n")
    SLANG_EVENT("BOUND END  : SLANG_Generated_Output.\n")
} // checkEndOfTranslationUnit()

//BOUND START: handling_routines

// Gets the function name, parameters and return type.
void SlangGenChecker::handleFunctionDef(const FunctionDecl *funcDecl) const {
    FD = funcDecl;
    handleFunction(funcDecl);
    stu.currFunc = &stu.funcMap[(uint64_t)funcDecl];
} // handleFunctionDef()

void SlangGenChecker::handleFunction(const FunctionDecl *funcDecl) const {
    if (stu.funcMap.find((uint64_t)funcDecl) == stu.funcMap.end()) {
        // if here, function not already present. Add its details.
        SlangFunc slangFunc{};
        slangFunc.name = funcDecl->getNameInfo().getAsString();
        slangFunc.fullName = stu.convertFuncName(slangFunc.name);
        SLANG_DEBUG("AddingFunction: " << slangFunc.name)

        // STEP 1.2: Get function parameters.
        // if (funcDecl->doesThisDeclarationHaveABody()) { //& !funcDecl->hasPrototype())
        for (unsigned i = 0, e = funcDecl->getNumParams(); i != e; ++i) {
            const ParmVarDecl *paramVarDecl = funcDecl->getParamDecl(i);
            handleVariable(paramVarDecl, slangFunc.name); // adds the var too
            slangFunc.paramNames.push_back(
                    stu.getVar((uint64_t)paramVarDecl).name);
        }
        slangFunc.variadic = funcDecl->isVariadic();

        // STEP 1.3: Get function return type.
        slangFunc.retType = convertClangType(funcDecl->getReturnType());

        // STEP 2: Copy the function to the map.
        stu.funcMap[(uint64_t)funcDecl] = slangFunc;
    }
} // handleFunction()

void SlangGenChecker::handleCfg(const CFG *cfg) const {
    stu.setNextBbId(cfg->size() - 1);
    for (const CFGBlock *bb : *cfg) {
        handleBbInfo(bb, cfg);
        // stu.clearMainStack(); // FIXME: a hack
        handleBbStmts(bb);
    }
} // handleCfg()

void SlangGenChecker::handleBbInfo(const CFGBlock *bb, const CFG *cfg) const {
    int32_t succId, bbId;

    stu.setCurrBb(bb);

    unsigned entryBbId = cfg->getEntry().getBlockID();
    if ((bbId = bb->getBlockID()) == (int32_t)entryBbId) {
        bbId = -1; //entry block is ided -1.
    }
    stu.addBb(bbId);
    stu.setCurrBbId(bbId);

    // Don't handle info in presence of SwitchStmt
    const Stmt *terminatorStmt = (bb->getTerminator()).getStmt();
    if (terminatorStmt && isa<SwitchStmt>(terminatorStmt)) {
        // all is done when we finally see the switch stmt.
        SLANG_DEBUG("BB" << bbId << ". Has switch terminator.");
        return;
    }

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
        default:
            // push to stack by default.
            stu.pushToMainStack(stmt);
            SLANG_DEBUG("SLANG: DEFAULT: Pushed to stack: " << stmt->getStmtClassName())
            stmt->dump();
            break;

        case Stmt::DeclRefExprClass:
            handleDeclRefExpr(cast<DeclRefExpr>(stmt)); break;

        case Stmt::DeclStmtClass:
            handleDeclStmt(cast<DeclStmt>(stmt)); break;

        case Stmt::BinaryOperatorClass:
            handleBinaryOperator(cast<BinaryOperator>(stmt)); break;

        case Stmt::ReturnStmtClass:
            handleReturnStmt(); break;

        case Stmt::WhileStmtClass: // same as Stmt::IfStmtClass
        case Stmt::IfStmtClass: handleIfStmt(); break;

        case Stmt::SwitchStmtClass:
            handleSwitchStmt(cast<SwitchStmt>(stmt)); break;

        case Stmt::CallExprClass:
            handleCallExpr(cast<CallExpr>(stmt)); break;

        case Stmt::ImplicitCastExprClass:
            break; // do nothing
    }
    stu.printMainStack();
} // handleStmt()

// record the variable name and type
void SlangGenChecker::handleVariable(const ValueDecl *valueDecl,
        std::string funcName) const {
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
                slangVar.setLocalVarName(varName, funcName);
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
    handleVariable(varDecl, stu.getCurrFuncName());

    std::string locStr = getLocationString(declStmt);

    if (!stu.isMainStackEmpty()) {
        // there is smth on the stack, hence on the rhs.
        SlangExpr slangExpr{};
        auto exprLhs = convertVarDecl(varDecl, locStr);
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
    const ValueDecl *valueDecl = declRefExpr->getDecl();
    if (isa<VarDecl>(valueDecl)) {
        stu.pushToMainStack(declRefExpr);
        handleVariable(valueDecl, stu.getCurrFuncName());
    } else if (isa<FunctionDecl>(valueDecl)) {
        handleFunction(cast<FunctionDecl>(valueDecl));
    } else {
        SLANG_DEBUG("handleDeclRefExpr: unhandled " << declRefExpr->getStmtClassName())
    }
} // handleDeclRefExpr()

void SlangGenChecker::handleBinaryOperator(const BinaryOperator *binOp) const {
    if (binOp->isAssignmentOp() && isTopLevel(binOp)) {
        std::string locStr = getLocationString(binOp)
        SlangExpr slangExpr = convertAssignment(false, locStr); // top level is never compound
        stu.addBbStmts(slangExpr.slangStmts);
    } else {
        stu.pushToMainStack(binOp);
    }
} // handleBinaryOperator()

void SlangGenChecker::handleCallExpr(const CallExpr *callExpr) const {
    stu.pushToMainStack(callExpr);
    if (isTopLevel(callExpr)) {
        SlangExpr slangExpr = convertExpr(false); // top level is never compound
        stu.addBbStmts(slangExpr.slangStmts);
        std::stringstream ss;
        ss << "instr.CallI(" << slangExpr.expr << ")";
        stu.addBbStmt(ss.str());
    }
} // handleCallExpr()

// Convert switch to if-else ladder
void SlangGenChecker::handleSwitchStmt(const SwitchStmt *switchStmt) const {
    SlangExpr switchCondVar;
    SlangExpr caseCondVar;
    SlangExpr newIfCondVar;

    switchStmt->dump();

    switchCondVar = convertExpr(true);
    stu.addBbStmts(switchCondVar.slangStmts);

    // Get all successor ids
    std::vector<int32_t> succIds;
    llvm::errs() << "successor ids : ";
    for (CFGBlock::const_succ_iterator I = stu.getCurrBb()->succ_begin();
         I != stu.getCurrBb()->succ_end(); ++I) {
        succIds.push_back((*I)->getBlockID());
    }
    llvm::errs() << "\n";

    if (succIds.size() == 1) {
        // only default case present
        stu.addBbEdge(std::make_pair(stu.getCurrBbId(),
                                     std::make_pair(succIds[0], UnCondEdge)));
        return;
    }

    // Get full expressions used in all case stmts in Clang-style order
    std::vector<StmtVector> stmtVecVec;
    std::vector<std::string> locStrs;
    const CompoundStmt *body = cast<CompoundStmt>(switchStmt->getBody());
    for (auto it = body->body_begin(); it != body->body_end(); ++it) {
        if (isa<CaseStmt>(*it)) {
            StmtVector stmtVec = getCaseExprElements(cast<CaseStmt>(*it));
            stmtVecVec.push_back(stmtVec);
            locStrs.push_back(getLocationString(*it));
        }
    }
    llvm::errs() << "#new blocks = " << stmtVecVec.size() << "\n";

    // start creating and adding if-condition blocks for each case stmt
    int32_t ifBbId;
    int32_t oldIfBbId = 0; // init with zero is necessary
    std::stringstream ss;
    uint32_t index = 0;
    std::string locStr;
    // reverse iterating to correct the order
    for (auto stmtVecPtr = stmtVecVec.end() - 1;
            stmtVecPtr != stmtVecVec.begin() - 1;
            --stmtVecPtr, ++index) {
        ss.str(""); // clear the stream

        // convert case expression
        // Push case expression stmts to the mainStack
        for (const Stmt *stmt : *stmtVecPtr) {
            stu.pushToMainStack(stmt);
        }
        caseCondVar = convertExpr(true);
        newIfCondVar = genTmpVariable("types.Int", locStrs[index]);

        ss << "instr.AssignI(" << newIfCondVar.expr << ", ";
        ss << "expr.BinaryE(";
        ss << switchCondVar.expr << ", op.BO_EQ, " << caseCondVar.expr << "))";
        newIfCondVar.addSlangStmt(ss.str());

        ss.str("");
        ss << "instr.CondI(" << newIfCondVar.expr << ")";

        if (index == 0) {
            // the first if-stmt can be put in the current block itself
            ifBbId = stu.getCurrBbId(); // i.e. no new bb for if-stmt
            stu.addBbStmts(newIfCondVar.slangStmts);
            stu.addBbStmt(ss.str());
        } else {
            ifBbId = stu.genNextBbId(); // i.e. a new bb for if-stmt
            stu.addBb(ifBbId);
            stu.addBbStmts(ifBbId, newIfCondVar.slangStmts);
            stu.addBbStmt(ifBbId, ss.str());
        }

        // add true edge to this case stmt
        stu.addBbEdge(std::make_pair(ifBbId,
                                     std::make_pair(succIds[index], TrueEdge)));

        if (oldIfBbId != 0) {
            // add false edge from previous if-stmt bb to this if-stmt bb
            stu.addBbEdge(std::make_pair(oldIfBbId,
                                         std::make_pair(ifBbId, FalseEdge)));
        }

        oldIfBbId = ifBbId;
    } // for

    // for the last ifBbId add the last successor as FalseEdge successor
    int32_t lastSuccBbId = succIds[succIds.size()-1];
    stu.addBbEdge(std::make_pair(ifBbId,
                                 std::make_pair(lastSuccBbId, FalseEdge)));
} // handleSwitchStmt()

//BOUND END  : handling_routines

//BOUND START: conversion_routines to SlangExpr

// convert top of stack to SPAN IR.
// returns converted string, and false if the converted string is only a simple const/var expression.
SlangExpr SlangGenChecker::convertExpr(bool compound_receiver) const {
    std::stringstream ss;

    const Stmt *stmt = stu.popFromMainStack();

    switch(stmt->getStmtClass()) {
        case Stmt::IntegerLiteralClass:
            return convertIntegerLiteral(cast<IntegerLiteral>(stmt));

        case Stmt::FloatingLiteralClass:
            return convertFloatingLiteral(cast<FloatingLiteral>(stmt));

        case Stmt::StringLiteralClass:
            return convertStringLiteral(cast<StringLiteral>(stmt));

        case Stmt::DeclRefExprClass:
            return convertDeclRefExpr(cast<DeclRefExpr>(stmt));

        case Stmt::BinaryOperatorClass:
            return convertBinaryOp(cast<BinaryOperator>(stmt),
                    compound_receiver);

        case Stmt::UnaryOperatorClass:
            return convertUnaryOp(cast<UnaryOperator>(stmt), compound_receiver);

        case Stmt::CallExprClass:
            return convertCallExpr(cast<CallExpr>(stmt), compound_receiver);

        default: {
            // error state
            SLANG_ERROR("UnknownStmt: " << stmt->getStmtClassName())
            stmt->dump();
            return SlangExpr("ERROR:convertExpr", false, QualType());
        }
    }
    // return SlangExpr("ERROR:convertExpr", false, QualType());
} // convertExpr()

SlangExpr SlangGenChecker::convertIntegerLiteral(
        const IntegerLiteral *il) const {
    std::stringstream ss;
    std::string suffix = ""; // helps make int appear float

    // check if int is implicitly casted to floating
    const auto &parents = FD->getASTContext().getParents(*il);
    if (!parents.empty()) {
        const Stmt *stmt1 = parents[0].get<Stmt>();
        if (stmt1) {
            switch (stmt1->getStmtClass()) {
                default: break;
                case Stmt::ImplicitCastExprClass: {
                    const ImplicitCastExpr *ice = cast<ImplicitCastExpr>(stmt1);
                    switch(ice->getCastKind()) {
                        default: break;
                        case CastKind::CK_IntegralToFloating:
                            suffix = ".0";
                            break;
                    }
                }
            }
        }
    }

    bool is_signed = il->getType()->isSignedIntegerType();
    ss << "expr.LitE(" << il->getValue().toString(10, is_signed);
    ss << suffix << ")";
    SLANG_TRACE(ss.str())

    return SlangExpr(ss.str(), false, il->getType());
} // convertIntegerLiteral()

SlangExpr SlangGenChecker::convertFloatingLiteral(
        const FloatingLiteral *fl) const {
    std::stringstream ss;

    ss << "expr.LitE(" << std::fixed << fl->getValue().convertToDouble() << ")";
    SLANG_TRACE(ss.str())

    return SlangExpr(ss.str(), false, fl->getType());
}

SlangExpr SlangGenChecker::convertStringLiteral(
        const StringLiteral *sl) const {
    std::stringstream ss;

    ss << "expr.LitE(\"\"\"" << sl->getBytes().data() << "\"\"\")";
    SLANG_TRACE(ss.str())

    return SlangExpr(ss.str(), false, sl->getType());
}

SlangExpr SlangGenChecker::convertCallExpr(const CallExpr *callExpr,
        bool compound_receiver) const {
    std::stringstream ss;
    std::vector<SlangExpr> args;
    SlangExpr slangExpr;
    const FunctionDecl *callee;
    std::string calleeName;

    slangExpr.compound = true;

    uint32_t numOfArgs = callExpr->getNumArgs();
    slangExpr.qualType = callExpr->getType();
    callee = callExpr->getDirectCallee();
    calleeName = callee->getNameInfo().getAsString();

    // convert each argument
    for (uint32_t i = 0; i < numOfArgs; ++i) {
        args.push_back(convertExpr(true));
    }

    ss.str("");
    std::string prefix = "";
    for (auto argIter = args.end() - 1;
            argIter != args.begin() - 1;
            --argIter) {
        slangExpr.addSlangStmts(argIter->slangStmts);
        ss << prefix << argIter->expr;
        if (prefix.size() == 0) {
            prefix = ", ";
        }
    }
    std::string argString = ss.str();

    ss.str("");
    ss << "expr.CallE(\"" << stu.convertFuncName(calleeName) << "\"";
    ss << ", [" << argString << "])";
    slangExpr.expr = ss.str();

    if (compound_receiver) {
        ss.str("");
        std::string locStr = getLocationString(callExpr);
        SlangExpr tmpVar = genTmpVariable(slangExpr.qualType, locStr);
        ss << "instr.AssignI(" << tmpVar.expr << ", ";
        ss << slangExpr.expr << ")";
        tmpVar.addSlangStmts(slangExpr.slangStmts);
        tmpVar.addSlangStmt(ss.str());
        return tmpVar;
    }

    return slangExpr;
} // convertCallExpr()

SlangExpr SlangGenChecker::convertAssignment(bool compound_receiver,
        std::string& locStr) const {
    std::stringstream ss;
    SlangExpr slangExpr{};

    auto exprLhs = convertExpr(false); // unconditionally false
    auto exprRhs = convertExpr(exprLhs.compound);

    if (compound_receiver && exprLhs.compound) {
        slangExpr = genTmpVariable(exprLhs.qualType, locStr);

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

void SlangGenChecker::adjustDirtyVar(SlangExpr& slangExpr,
        std::string& locStr) const {
    std::stringstream ss;
    bool newTmp;
    if (slangExpr.isNonTmpVar() && stu.isDirtyVar(slangExpr.varId)) {
        SlangExpr sp = getTmpVarForDirtyVar(slangExpr.varId,
                slangExpr.qualType, newTmp, locStr);
        if (newTmp) {
            // only add if a new temporary was generated
            ss << "instr.AssignI(" << sp.expr << ", " << slangExpr.expr << ")";
            slangExpr.addSlangStmt(ss.str());
        }
        slangExpr.expr = sp.expr;
        slangExpr.nonTmpVar = false;
    }
}

SlangExpr SlangGenChecker::convertEnumConst(const EnumConstantDecl* ecd, uint64_t locId) const {
    std::stringstream ss;
    ss << "expr.LitE(" << (ecd->getInitVal()).toString(10);
    ss << ", " << locId << ")";
    return SlangExpr(ss.str(), false, QualType());
}

SlangExpr SlangGenChecker::convertBinaryOp(const BinaryOperator *binOp,
        bool compound_receiver) const {
    std::stringstream ss;
    std::string op;
    SlangExpr varExpr{};

    std::string locStr = getLocationString(binOp);

    if (binOp->isAssignmentOp()) {
        return convertAssignment(compound_receiver, locStr);
    }

    SlangExpr exprR = convertExpr(true);
    SlangExpr exprL = convertExpr(true);
    adjustDirtyVar(exprL, locStr);

    if (compound_receiver) {
        varExpr = genTmpVariable(exprL.qualType, locStr);
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

        case BO_Add: { op = "op.BO_ADD"; break; }
        case BO_Sub: { op = "op.BO_SUB"; break;}
        case BO_Mul: { op = "op.BO_MUL"; break;}
        case BO_Div: { op = "op.BO_DIV"; break;}
        case BO_Rem: { op = "op.BO_MOD"; break; }
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

    std::string locStr = getLocationString(unOp);

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
    adjustDirtyVar(exprArg, locStr);
    qualType = exprArg.qualType;

    switch(unOp->getOpcode()) {
        default: {
            SLANG_DEBUG("convertUnaryOp: " << unOp->getOpcodeStr(unOp->getOpcode()))
            return SlangExpr("ERROR:convertUnaryOp", false, QualType());
        }

        case UO_AddrOf: {
            qualType = FD->getASTContext().getPointerType(exprArg.qualType);
            op = "op.UO_ADDROF";
            break;
        }

        case UO_Deref: {
            auto ptr_type = cast<PointerType>(exprArg.qualType.getTypePtr());
            qualType = ptr_type->getPointeeType();
            op = "op.UO_DEREF";
            break;
        }

        case UO_Minus: { op = "op.UO_MINUS"; break;}
        case UO_Plus: { op = "op.UO_MINUS"; break;}
    }

    if (compound_receiver) {
        varExpr = genTmpVariable(qualType, locStr);
        ss << "instr.AssignI(" << varExpr.expr << ", ";
    }

    ss << "expr.UnaryE(" << op << ", " << exprArg.expr;
    ss << ", " << locStr << ")";

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

    std::string locStr = getLocationString(unOp);

    switch(unOp->getOpcode()) {
        case UO_PreInc: {
            ss << "instr.AssignI(" << exprArg.expr << ", ";
            ss << "expr.BinaryE(" << exprArg.expr << ", op.BO_ADD, expr.LitE(1)";
            ss << ", " << locStr << "))";
            exprArg.addSlangStmt(ss.str());

            if (exprArg.nonTmpVar && stu.isDirtyVar(exprArg.varId)) {
                adjustDirtyVar(exprArg, locStr);
            }
            stu.setDirtyVar(exprArg.varId, emptySlangExpr);
            break;
        }

        case UO_PostInc: {
            ss << "instr.AssignI(" << exprArg.expr << ", ";
            ss << "expr.BinaryE(" << exprArg.expr << ", op.BO_ADD, expr.LitE(1)))";

            if (exprArg.nonTmpVar) {
                stu.setDirtyVar(exprArg.varId, emptySlangExpr);
                adjustDirtyVar(exprArg, locStr);
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

SlangExpr SlangGenChecker::convertVarDecl(const VarDecl *varDecl,
        std::string& locStr) const {
    std::stringstream ss;
    SlangExpr slangExpr{};

    ss << "expr.VarE(\"" << stu.convertVarExpr((uint64_t)varDecl) << "\"";
    ss << ", " << locStr << ")";
    slangExpr.expr = ss.str();
    slangExpr.compound = false;
    slangExpr.qualType = varDecl->getType();
    slangExpr.nonTmpVar = true;
    slangExpr.varId = (uint64_t)varDecl;

    return slangExpr;
}

SlangExpr SlangGenChecker::convertDeclRefExpr(const DeclRefExpr *dre) const {
    std::stringstream ss;

    std::string locStr = getLocationString(dre);

    const ValueDecl *valueDecl = dre->getDecl();
    if (isa<VarDecl>(valueDecl)) {
        auto varDecl = cast<VarDecl>(valueDecl);
        SlangExpr slangExpr = convertVarDecl(varDecl, locStr);
        slangExpr.locId = getLocationId(dre);
        return slangExpr;
    } else if (isa<EnumConstantDecl>(valueDecl)) {
        auto ecd = cast<EnumConstantDecl>(valueDecl);
        return convertEnumConst(ecd, getLocationId(dre));
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

// Return vector of Stmts in clang's traversal order.
StmtVector SlangGenChecker::getCaseExprElements(const CaseStmt *caseStmt) const {
    const Expr *condition = cast<Expr>(*(caseStmt->child_begin()));
    StmtVector stmts;
    getCaseExprElements(stmts, condition);
    return stmts;
} // getCaseExprElements()

// Store elements in stmts, to make a new basic block for CaseStmt
void SlangGenChecker::getCaseExprElements(StmtVector& stmts, const Stmt *stmt) const {
    switch (stmt->getStmtClass()) {
        case Stmt::BinaryOperatorClass: {
            const BinaryOperator *binOp = cast<BinaryOperator>(stmt);
            getCaseExprElements(stmts, binOp->getLHS());
            getCaseExprElements(stmts, binOp->getRHS());
            break;
        }

        case Stmt::UnaryOperatorClass: {
            const UnaryOperator *unOp = cast<UnaryOperator>(stmt);
            getCaseExprElements(stmts, unOp->getSubExpr());
            break;
        }

        case Stmt::ImplicitCastExprClass: {
            const ImplicitCastExpr *impCast = cast<ImplicitCastExpr>(stmt);
            getCaseExprElements(stmts, impCast->getSubExpr());
            return;
        }

        case Stmt::ParenExprClass: {
            const ParenExpr *parenExpr = cast<ParenExpr>(stmt);
            getCaseExprElements(stmts, parenExpr->getSubExpr());
            return;
        }

        default:
            stmts.push_back(stmt);
            SLANG_DEBUG("Added CaseExprElement: " << stmt->getStmtClassName())
    }
} // getCaseExprElements()

// returns an empty SlangExpr if var is not dirty
SlangExpr SlangGenChecker::getTmpVarForDirtyVar(uint64_t varId,
        QualType qualType,
        bool& newTmp, std::string& locStr) const {
    SlangExpr slangExpr;
    newTmp = false;

    if (!stu.isDirtyVar(varId)) {
        return slangExpr;
    }

    slangExpr = stu.getTmpVarForDirtyVar(varId);
    if (slangExpr.expr.size() == 0) {
        newTmp = true;
        // allocate tmp var on demand
        slangExpr = genTmpVariable(qualType, locStr);
        stu.setDirtyVar(varId, slangExpr);
    }

    return slangExpr;
}

SlangExpr SlangGenChecker::genTmpVariable(std::string slangTypeStr,
        std::string& locStr) const {
    std::stringstream ss;
    SlangExpr slangExpr{};

    // STEP 1: Populate a SlangVar object with unique name.
    SlangVar slangVar{};
    slangVar.id = stu.nextTmpId();
    ss << "t." << slangVar.id;
    slangVar.setLocalVarName(ss.str(), stu.getCurrFuncName());
    slangVar.typeStr = slangTypeStr;

    // STEP 2: Add to the var map.
    // FIXME: The var's 'id' here should be small enough to not interfere with uint64_t addresses.
    stu.addVar(slangVar.id, slangVar);

    // STEP 3: generate var expression.
    ss.str(""); // empty the stream
    ss << "expr.VarE(\"" << slangVar.name << "\"";
    ss << ", " << locStr << ")";

    slangExpr.expr = ss.str();
    slangExpr.compound = false;
    // slangExpr.qualType = qt;
    slangExpr.nonTmpVar = false;

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

std::string SlangGenChecker::getLocationString(const Stmt *stmt) const {
    std::stringstream ss;
    uint32_t line = 0;
    uint32_t col = 0;

    ss << "Loc(";
    line = FD->getASTContext().getSourceManager()
            .getExpansionLineNumber(stmt->getLocStart());
    ss << line << ",";
    col  = FD->getASTContext().getSourceManager()
            .getExpansionColumnNumber(stmt->getLocStart());
    ss << col << ")";

    return ss.str();
}

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
bool SlangGenChecker::isTopLevel(const Stmt *stmt) const {
    const auto &parents = FD->getASTContext().getParents(*stmt);
    if (!parents.empty()) {
        const Stmt *stmt1 = parents[0].get<Stmt>();
        if (stmt1) {
            // llvm::errs() << "Parent: " << stmt1->getStmtClassName() << "\n";
            switch (stmt1->getStmtClass()) {
                default: {
                    return false;
                    break;
                }

                case Stmt::CaseStmtClass:
                case Stmt::DefaultStmtClass:
                case Stmt::CompoundStmtClass: {
                    return true; // top level
                }
                case Stmt::WhileStmtClass: {
                    auto body = (cast<WhileStmt>(stmt1))->getBody();
                    return ((uint64_t)body == (uint64_t)stmt);
                }
                case Stmt::IfStmtClass: {
                    auto then_ = (cast<IfStmt>(stmt1))->getThen();
                    auto else_ = (cast<IfStmt>(stmt1))->getElse();
                    return ((uint64_t)then_ == (uint64_t)stmt || (uint64_t)else_ == (uint64_t)stmt);
                }
            }
        } else {
            // llvm::errs() << "Parent: Cannot print.\n";
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

