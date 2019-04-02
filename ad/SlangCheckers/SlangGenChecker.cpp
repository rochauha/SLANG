//===----------------------------------------------------------------------===//
//  MIT License.
//  Copyright (c) 2019 The SLANG Authors.
//
//  Author: Ronak Chauhan (r.chauhan@somaiya.edu)
//  Author: Anshuman Dhuliya [AD] (dhuliya@cse.iitb.ac.in)
//
//  If SlangGenChecker class name is added or changed, then also edit,
//  ../../../../include/clang/StaticAnalyzer/Checkers/Checkers.td
//  if this checker is named `slanggen` (in Checkers.td) then it can be used as follows,
//
//      clang --analyze -Xanalyzer -analyzer-checker=debug.slanggen test.c |& tee mylog
//
//  which generates the file `test.c.spanir`.
//===----------------------------------------------------------------------===//
//

#include "ClangSACheckers.h"
#include "clang/AST/Decl.h" //AD
#include "clang/AST/Expr.h" //AD
#include "clang/AST/Stmt.h" //AD
#include "clang/AST/Type.h" //AD
#include "clang/Analysis/CFG.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/PathSensitive/AnalysisManager.h"
#include "llvm/Support/Process.h"
#include "llvm/Support/raw_ostream.h" //AD
#include <fstream>                    //AD
#include <iomanip>                    //AD for std::fixed
#include <sstream>                    //AD
#include <string>                     //AD
#include <unordered_map>              //AD
#include <utility>                    //AD
#include <vector>                     //AD

#include "SlangExpr.h"
#include "SlangTranslationUnit.h"
#include "SlangUtil.h"

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
    static const FunctionDecl *FD; // funcDecl

  public:
    // mainentry
    void checkASTCodeBody(const Decl *D, AnalysisManager &mgr, BugReporter &BR) const;
    void checkEndOfTranslationUnit(const TranslationUnitDecl *TU, AnalysisManager &Mgr,
                                   BugReporter &BR) const;

    // handling_routines
    void handleFunctionDef(const FunctionDecl *D) const;
    void handleFunction(const FunctionDecl *funcDecl) const;
    void handleCfg(const CFG *cfg) const;
    void handleBbInfo(const CFGBlock *bb, const CFG *cfg) const;
    void handleAstStmts(const Stmt *stmt) const;
    void handleBbStmts(const CFGBlock *bb) const;
    void handleStmt(const Stmt *stmt) const;
    void handleVariable(const ValueDecl *valueDecl, std::string funcName) const;
    void handleDeclStmt(const DeclStmt *declStmt) const;
    void handleInitListExpr(const InitListExpr *initListExpr) const;
    void handleDeclRefExpr(const DeclRefExpr *DRE) const;
    void handleBinaryOperator(const BinaryOperator *binOp) const;
    void handleUnaryOperator(const UnaryOperator *unOp) const;
    void handleCStyleCastExpr(const CStyleCastExpr *cCast) const;
    void handleCallExpr(const CallExpr *callExpr) const;
    void handleReturnStmt(std::string &locStr) const;
    void handleIfStmt(const Stmt *stmt, std::string &locStr) const;
    void handleSwitchStmt(const SwitchStmt *switchStmt) const;

    // conversion_routines
    SlangExpr convertAssignment(bool compound_receiver, std::string &compoundAssignOp,
                                std::string &locStr) const;
    SlangExpr convertIntegerLiteral(const IntegerLiteral *IL) const;
    SlangExpr convertFloatingLiteral(const FloatingLiteral *fl) const;
    SlangExpr convertStringLiteral(const StringLiteral *sl) const;
    // a function, if stmt, *y on lhs, arr[i] on lhs
    // are examples of a compound_receiver.
    SlangExpr convertExpr(bool compound_receiver) const;
    SlangExpr convertInitListExpr(const InitListExpr *initListExpr) const;
    SlangExpr convertMemberExpr(const MemberExpr *memberExpr, bool compound_receiver) const;
    SlangExpr convertDeclRefExpr(const DeclRefExpr *dre) const;
    SlangExpr convertVarDecl(const VarDecl *varDecl, std::string &locStr) const;
    SlangExpr convertUnaryOp(const UnaryOperator *unOp, bool compound_receiver) const;
    SlangExpr convertArraySubscript(const ArraySubscriptExpr *arrayExpr,
                                    bool compound_receiver) const;
    SlangExpr convertUnaryIncDec(const UnaryOperator *unOp, bool compound_receiver) const;
    SlangExpr convertBinaryOp(const BinaryOperator *binOp, bool compound_receiver) const;
    SlangExpr convertBinaryLogicalOp(const BinaryOperator *binOp, bool compound_receiver) const;
    SlangExpr convertEnumConst(const EnumConstantDecl *ecd, std::string &locStr) const;
    SlangExpr convertCallExpr(const CallExpr *callExpr, bool compound_receiver) const;
    SlangExpr convertUnaryExprOrTypeTraitExpr(const UnaryExprOrTypeTraitExpr *stmt,
                                              bool compound_receiver) const;
    SlangExpr convertCStyleCastExpr(const CStyleCastExpr *cCast, bool compound_receiver) const;
    SlangExpr convertConditionalOp(const ConditionalOperator *condOp, bool compound_receiver) const;

    // type_conversion_routines
    std::string convertClangType(QualType qt) const;
    std::string convertClangBuiltinType(QualType qt) const;
    std::string convertClangRecordType(const RecordDecl *recordDecl) const;
    std::string convertClangArrayType(QualType qt) const;
    std::string convertFunctionPointerType(QualType qt) const;

    SlangExpr convertAstExpr(const Stmt *stmt, bool compound_receiver) const;

    // helper_functions
    SlangExpr genTmpVariable(QualType qt, std::string &locStr) const;
    SlangExpr genTmpVariable(std::string slangTypeStr, std::string &locStr) const;
    SlangExpr getTmpVarForDirtyVar(uint64_t varId, QualType qualType, bool &newTmp,
                                   std::string &locStr) const;
    bool isTopLevel(const Stmt *stmt) const;
    uint64_t getLocationId(const Stmt *stmt) const;
    std::string getLocationString(const Stmt *stmt) const;
    std::string getLocationString(const RecordDecl *recordDecl) const;
    void getCaseExprElements(StmtVector &stmts, const Stmt *stmt) const;
    void getCaseExpr(std::vector<StmtVector> &stmtVecVec, std::vector<std::string> &locStrs,
                     const Stmt *stmt) const;
    std::string getCompoundAssignOpString(const BinaryOperator *binOp) const;
    void adjustDirtyVar(SlangExpr &slangExpr, std::string &locStr) const;
    bool isIncompleteType(const Type *type) const;
    void genStmtVectorFromAST(const Stmt *stmt, StmtVector &stmtVec) const;
    QualType getCleanedQualType(QualType qt) const;

}; // class SlangGenChecker
} // anonymous namespace

SlangTranslationUnit SlangGenChecker::stu = SlangTranslationUnit();
const FunctionDecl *SlangGenChecker::FD = nullptr;

// mainentry, main entry point. Invokes top level Function and Cfg handlers.
// It is invoked once for each source translation unit function.
void SlangGenChecker::checkASTCodeBody(const Decl *D, AnalysisManager &mgr, BugReporter &BR) const {
    SLANG_EVENT("BOUND START: SLANG_Generated_Output.\n")

    // SLANG_DEBUG("slang_add_nums: " << slang_add_nums(1,2) << "only\n"; // lib testing
    if (stu.fileName.size() == 0) {
        stu.fileName = D->getASTContext().getSourceManager().getFilename(D->getLocStart()).str();
    }

    const FunctionDecl *funcDecl = dyn_cast<FunctionDecl>(D);
    handleFunctionDef(funcDecl);

    if (const CFG *cfg = mgr.getCFG(D)) {
        handleCfg(cfg);
    } else {
        SLANG_ERROR("No CFG for function.")
    }
} // checkASTCodeBody()

void SlangGenChecker::checkEndOfTranslationUnit(const TranslationUnitDecl *TU, AnalysisManager &Mgr,
                                                BugReporter &BR) const {
    stu.dumpSlangIr();
    SLANG_EVENT("Translation Unit Ended.\n")
    SLANG_EVENT("BOUND END  : SLANG_Generated_Output.\n")
} // checkEndOfTranslationUnit()

// BOUND START: handling_routines

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
            slangFunc.paramNames.push_back(stu.getVar((uint64_t)paramVarDecl).name);
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
        stu.clearMainStack(); // IMPORTANT (necessary when conditional operator)
        handleBbStmts(bb);
    }
} // handleCfg()

void SlangGenChecker::handleBbInfo(const CFGBlock *bb, const CFG *cfg) const {
    int32_t succId, bbId;

    stu.setCurrBb(bb);

    unsigned entryBbId = cfg->getEntry().getBlockID();
    if ((bbId = bb->getBlockID()) == (int32_t)entryBbId) {
        bbId = -1; // entry block is ided -1.
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
    const Stmt *terminator = (bb->getTerminator()).getStmt();
    if (terminator
             && (isa<IfStmt>(terminator)
                 || isa<WhileStmt>(terminator)
                 || isa<ConditionalOperator>(terminator)
                 || isa<ForStmt>(terminator) || isa<DoStmt>(terminator)
                 || (isa<BinaryOperator>(terminator)
                     && cast<BinaryOperator>(terminator)->isLogicalOp()
                    ))) {
        // if here, then only conditional edges are present
        bool trueEdge = true;
        if (bb->succ_size() > 2) {
            SLANG_ERROR("BB (with no switch) has more than two successors.")
        }

        for (CFGBlock::const_succ_iterator I = bb->succ_begin(); I != bb->succ_end(); ++I) {

            CFGBlock *succ = *I;
            if (succ) {
                succId = succ->getBlockID();
                if (succId == (int32_t)entryBbId) {
                    succId = -1;
                }
            } else {
                succId = 0;
            }

            if (isa<ConditionalOperator>(terminator)
                || (isa<BinaryOperator>(terminator)
                    && cast<BinaryOperator>(terminator)->isLogicalOp()
                    )) {
                // in case of a conditional terminator just record the first edge as uncoditional and done.
                stu.addBbEdge(std::make_pair(bbId, std::make_pair(succId, UnCondEdge)));
                break; // important
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
            for (CFGBlock::const_succ_iterator I = bb->succ_begin(); I != bb->succ_end(); ++I) {
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
    const Stmt *terminator = (bb->getTerminator()).getStmt();
    if (terminator) {
        if (! isa<ConditionalOperator>(terminator)) {
            handleStmt(terminator);
        }
    }

} // handleBbStmts()

void SlangGenChecker::handleAstStmts(const Stmt *stmt) const {
    auto firstChild = stmt->child_begin();
    auto lastChild = stmt->child_end();
    const Stmt *st;

    if (firstChild != lastChild) {
        // there are children
        if (isa<BinaryOperator>(stmt) && cast<BinaryOperator>(stmt)->isAssignmentOp()) {
            // iterate children in from right-to-left order
            auto lhs = *firstChild;
            auto rhs = *(++firstChild);
            handleAstStmts(rhs);
            handleAstStmts(lhs);

        } else {
            // usual order: left-to-right
            for (auto iter = firstChild; iter != lastChild; ++iter) {
                st = *iter;
                if (st) {
                    handleAstStmts(st);
                }
            }
        }
    }
    handleStmt(stmt);
} // handleAstStmts()

void SlangGenChecker::handleStmt(const Stmt *stmt) const {
    Stmt::StmtClass stmtClass = stmt->getStmtClass();

    stu.printMainStack();
    std::string locStr = getLocationString(stmt);
    SLANG_DEBUG("Processing: " << stmt->getStmtClassName())

    // to handle each kind of statement/expression.
    switch (stmtClass) {
    default:
        // push to stack by default.
        stu.pushToMainStack(stmt);
        SLANG_DEBUG("SLANG: DEFAULT: Pushed to stack: " << stmt->getStmtClassName())
        stmt->dump();
        break;

    case Stmt::UnaryOperatorClass:
        SLANG_DEBUG("here handleStmt")
        handleUnaryOperator(cast<UnaryOperator>(stmt)); break;

    case Stmt::CStyleCastExprClass:
        handleCStyleCastExpr(cast<CStyleCastExpr>(stmt)); break;

    case Stmt::DeclRefExprClass:
        handleDeclRefExpr(cast<DeclRefExpr>(stmt)); break;

    case Stmt::DeclStmtClass:
        handleDeclStmt(cast<DeclStmt>(stmt));
        stu.printMainStack();
        break;

    case Stmt::InitListExprClass:
        handleInitListExpr(cast<InitListExpr>(stmt)); break;

    case Stmt::CompoundAssignOperatorClass:
    case Stmt::BinaryOperatorClass:
        handleBinaryOperator(cast<BinaryOperator>(stmt));
        break;

    case Stmt::ReturnStmtClass:
        handleReturnStmt(locStr); break;

    case Stmt::DoStmtClass:    // same as Stmt::IfStmtClass
    case Stmt::WhileStmtClass: // same as Stmt::IfStmtClass
    case Stmt::ForStmtClass:   // same as Stmt::IfStmtClass
    case Stmt::IfStmtClass:
        handleIfStmt(stmt, locStr); break;

    case Stmt::SwitchStmtClass:
        handleSwitchStmt(cast<SwitchStmt>(stmt)); break;

    case Stmt::CallExprClass:
        handleCallExpr(cast<CallExpr>(stmt)); break;

    case Stmt::ParenExprClass:
    case Stmt::BreakStmtClass:
    case Stmt::ContinueStmtClass:
    case Stmt::ImplicitCastExprClass:
        break; // do nothing
    }
    stu.printMainStack();
} // handleStmt()

// record the variable name and type
void SlangGenChecker::handleVariable(const ValueDecl *valueDecl, std::string funcName) const {
    uint64_t varAddr = (uint64_t)valueDecl;
    std::string varName;
    if (stu.isNewVar(varAddr)) {
        // seeing the variable for the first time.
        SlangVar slangVar{};
        slangVar.id = varAddr;
        const VarDecl *varDecl = dyn_cast<VarDecl>(valueDecl);
        if (varDecl) {
            varName = valueDecl->getNameAsString();
            if (varName == "") {
                varName = "p." + Util::getNextUniqueIdStr();
            }
            if (varDecl->hasLocalStorage()) {
                slangVar.setLocalVarName(varName, funcName);
            } else if (varDecl->hasGlobalStorage()) {
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
    stu.setLastDeclStmtTo(declStmt);
    SLANG_DEBUG("Set last DeclStmt to DeclStmt at " << (uint64_t)(declStmt));

    std::stringstream ss;
    std::string locStr = getLocationString(declStmt);

    const VarDecl *varDecl = cast<VarDecl>(declStmt->getSingleDecl());
    handleVariable(varDecl, stu.getCurrFuncName());

    if (!stu.isMainStackEmpty()) {
        // there is smth on the stack, hence on the rhs.
        SlangExpr slangExpr{};
        auto exprLhs = convertVarDecl(varDecl, locStr);
        exprLhs.locId = getLocationId(declStmt);
        auto exprRhs = convertExpr(exprLhs.compound);

        // order_correction for DeclStmt
        slangExpr.addSlangStmtsBack(exprRhs.slangStmts);
        slangExpr.addSlangStmtsBack(exprLhs.slangStmts);

        // slangExpr.qualType = exprLhs.qualType;
        ss << "instr.AssignI(" << exprLhs.expr << ", " << exprRhs.expr;
        ss << ", " << locStr << ")"; // close instr.AssignI(...
        slangExpr.addSlangStmtBack(ss.str());

        stu.addBbStmts(slangExpr.slangStmts);
    }
} // handleDeclStmt()

void SlangGenChecker::handleIfStmt(const Stmt *stmt, std::string &locStr) const {
    std::stringstream ss;

    SlangExpr exprArg = convertAstExpr(stmt, true);

    ss << "instr.CondI(";
    if (exprArg.expr == "NullStmt") {
        // only for for(;;) stmt
        ss << "expr.LitE(1)";
    } else {
        ss << exprArg.expr;
    }
    ss << ", " << locStr << ")";

    // order_correction for if stmt
    exprArg.addSlangStmtBack(ss.str());
    stu.addBbStmts(exprArg.slangStmts);
} // handleIfStmt()

void SlangGenChecker::handleReturnStmt(std::string &locStr) const {
    std::stringstream ss;

    if (!stu.isMainStackEmpty()) {
        // return has an argument
        auto exprArg = convertExpr(true);
        ss << "instr.ReturnI(" << exprArg.expr;
        ss << ", " << locStr << ")";

        // order_correction for return stmt
        exprArg.addSlangStmtBack(ss.str());
        stu.addBbStmts(exprArg.slangStmts);
    } else {
        ss << "instr.ReturnI(";
        ss << locStr << ")";
        stu.addBbStmt(ss.str()); // okay
    }
} // handleReturnStmt()

void SlangGenChecker::handleInitListExpr(const InitListExpr *initListExpr) const {
    stu.pushToMainStack(initListExpr);
} // handleInitListExpr();

void SlangGenChecker::handleDeclRefExpr(const DeclRefExpr *declRefExpr) const {
    const ValueDecl *valueDecl = declRefExpr->getDecl();
    stu.pushToMainStack(declRefExpr);
    if (isa<FunctionDecl>(valueDecl)) {
        handleFunction(cast<FunctionDecl>(valueDecl));
    } else if (isa<VarDecl>(valueDecl)) {
        handleVariable(valueDecl, stu.getCurrFuncName());
    } else {
        SLANG_DEBUG("handleDeclRefExpr: unhandled " << declRefExpr->getStmtClassName())
    }
} // handleDeclRefExpr()

void SlangGenChecker::handleBinaryOperator(const BinaryOperator *binOp) const {
    if ((binOp->isAssignmentOp() || binOp->isCompoundAssignmentOp()) && isTopLevel(binOp)) {
        std::string locStr = getLocationString(binOp);
        std::string compoundAssignOp = "";
        if (binOp->isCompoundAssignmentOp()) {
            compoundAssignOp = getCompoundAssignOpString(binOp);
        }
        SlangExpr slangExpr = convertAssignment(false, compoundAssignOp, locStr);
        stu.addBbStmts(slangExpr.slangStmts);
    // } else if (binOp->isLogicalOp()) {
    //     // for logical ops: && and ||, do the same as done with a if stmt
    //     std::string locStr = getLocationString(binOp);
    //     handleIfStmt(locStr);
    } else {
        stu.pushToMainStack(binOp);
    }
} // handleBinaryOperator()

void SlangGenChecker::handleUnaryOperator(const UnaryOperator *unOp) const {
    stu.pushToMainStack(unOp);
    if (isTopLevel(unOp)) {
        switch (unOp->getOpcode()) {
        // special handling for increment and decrement ops
        case UO_PreInc:
        case UO_PreDec:
        case UO_PostInc:
        case UO_PostDec: {
            SlangExpr slangExpr = convertExpr(false); // top level is never compound
            stu.addBbStmts(slangExpr.slangStmts);
        }

        default: { break; }
        }
    }
} // handleUnaryOperator()

void SlangGenChecker::handleCStyleCastExpr(const CStyleCastExpr *cCast) const {
    if (isTopLevel(cCast)) {
        SlangExpr slangExpr = convertCStyleCastExpr(cCast, true);
        stu.addBbStmts(slangExpr.slangStmts);
    } else {
        stu.pushToMainStack(cCast);
    }
} // handleCStyleCastExpr()

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

    switchStmt->dump(); // delit

    switchCondVar = convertExpr(true);
    stu.addBbStmts(switchCondVar.slangStmts);

    // Get all successor ids
    std::vector<int32_t> succIds;
    for (CFGBlock::const_succ_iterator I = stu.getCurrBb()->succ_begin();
         I != stu.getCurrBb()->succ_end(); ++I) {
        CFGBlock *succ = *I;
        if (succ) {
            succIds.push_back(succ->getBlockID());
        } else {
            succIds.push_back(0); // succ is nullptr sometimes (weird)
        }
    }

    if (succIds.size() == 1) {
        // only default case present
        stu.addBbEdge(std::make_pair(stu.getCurrBbId(), std::make_pair(succIds[0], UnCondEdge)));
        return;
    }

    // Get full expressions used in all case stmts in Clang-style order.
    std::vector<StmtVector> stmtVecVec;
    std::vector<std::string> locStrs;

    // Get all case statements inside switch.
    if (switchStmt->getBody()) {
        switchStmt->getBody()->dump(); // delit
        getCaseExpr(stmtVecVec, locStrs, switchStmt->getBody());
    } else {
        for (auto it = switchStmt->child_begin(); it != switchStmt->child_end(); ++it) {
            if (isa<CaseStmt>(*it)) {
                getCaseExpr(stmtVecVec, locStrs, (*it));
            }
        }
    }

    // start creating and adding if-condition blocks for each case stmt
    int32_t ifBbId;
    int32_t oldIfBbId = 0; // init with zero is necessary
    std::stringstream ss;
    uint32_t index = 0;
    std::string locStr;
    // reverse iterating to correct the order
    for (auto stmtVecPtr = stmtVecVec.end() - 1; stmtVecPtr != stmtVecVec.begin() - 1;
         --stmtVecPtr, ++index) {
        // for (auto stmtVecPtr = stmtVecVec.begin();
        //         stmtVecPtr != stmtVecVec.end();
        //         ++stmtVecPtr, ++index) {
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
        ss << switchCondVar.expr << ", op.BO_EQ, " << caseCondVar.expr;
        ss << ", " << locStr << ")"; // close expr.BinaryE(
        ss << ", " << locStr << ")"; // close instr.AssignI(...
        newIfCondVar.addSlangStmtBack(ss.str());

        ss.str("");
        ss << "instr.CondI(" << newIfCondVar.expr;
        ss << ", " << locStr << ")";

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
        stu.addBbEdge(std::make_pair(ifBbId, std::make_pair(succIds[index], TrueEdge)));

        if (oldIfBbId != 0) {
            // add false edge from previous if-stmt bb to this if-stmt bb
            stu.addBbEdge(std::make_pair(oldIfBbId, std::make_pair(ifBbId, FalseEdge)));
        }

        oldIfBbId = ifBbId;
    } // for

    // for the last ifBbId add the last successor as FalseEdge successor
    int32_t lastSuccBbId = succIds[succIds.size() - 1];
    stu.addBbEdge(std::make_pair(ifBbId, std::make_pair(lastSuccBbId, FalseEdge)));
} // handleSwitchStmt()

// BOUND END  : handling_routines

// BOUND START: conversion_routines to SlangExpr

// convert top of stack to SPAN IR.
// returns converted string, and false if the converted string is only a simple const/var
// expression.
SlangExpr SlangGenChecker::convertExpr(bool compound_receiver) const {
    std::stringstream ss;

    const Stmt *stmt = stu.popFromMainStack();
    if (!stmt)
        return SlangExpr("NullStmt", false, QualType());

    switch (stmt->getStmtClass()) {
    case Stmt::IntegerLiteralClass:
        return convertIntegerLiteral(cast<IntegerLiteral>(stmt));

    case Stmt::FloatingLiteralClass:
        return convertFloatingLiteral(cast<FloatingLiteral>(stmt));

    case Stmt::StringLiteralClass:
        return convertStringLiteral(cast<StringLiteral>(stmt));

    case Stmt::DeclRefExprClass:
        return convertDeclRefExpr(cast<DeclRefExpr>(stmt));

    case Stmt::CompoundAssignOperatorClass:
    case Stmt::BinaryOperatorClass:
        return convertBinaryOp(cast<BinaryOperator>(stmt), compound_receiver);

    case Stmt::UnaryOperatorClass:
        return convertUnaryOp(cast<UnaryOperator>(stmt), compound_receiver);

    case Stmt::ArraySubscriptExprClass:
        return convertArraySubscript(cast<ArraySubscriptExpr>(stmt), compound_receiver);

    case Stmt::InitListExprClass:
        return convertInitListExpr(cast<InitListExpr>(stmt));

    case Stmt::CallExprClass:
        return convertCallExpr(cast<CallExpr>(stmt), compound_receiver);

    case Stmt::UnaryExprOrTypeTraitExprClass:
        return convertUnaryExprOrTypeTraitExpr(cast<UnaryExprOrTypeTraitExpr>(stmt),
                                               compound_receiver);

    case Stmt::ParenExprClass:
        return convertExpr(compound_receiver);

    case Stmt::MemberExprClass:
        return convertMemberExpr(cast<MemberExpr>(stmt), compound_receiver);

    case Stmt::CStyleCastExprClass:
        return convertCStyleCastExpr(cast<CStyleCastExpr>(stmt), compound_receiver);

    case Stmt::ConditionalOperatorClass:
        return convertConditionalOp(cast<ConditionalOperator>(stmt), compound_receiver);

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
    std::string suffix = ""; // helps make int appear float

    std::string locStr = getLocationString(il);

    // check if int is implicitly casted to floating
    const auto &parents = FD->getASTContext().getParents(*il);
    if (!parents.empty()) {
        const Stmt *stmt1 = parents[0].get<Stmt>();
        if (stmt1) {
            switch (stmt1->getStmtClass()) {
            default:
                break;
            case Stmt::ImplicitCastExprClass: {
                const ImplicitCastExpr *ice = cast<ImplicitCastExpr>(stmt1);
                switch (ice->getCastKind()) {
                default:
                    break;
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
    ss << suffix;
    ss << ", " << locStr << ")";
    SLANG_TRACE(ss.str())

    return SlangExpr(ss.str(), false, il->getType());
} // convertIntegerLiteral()

SlangExpr SlangGenChecker::convertFloatingLiteral(const FloatingLiteral *fl) const {
    std::stringstream ss;
    bool toInt = false;

    std::string locStr = getLocationString(fl);

    // check if float is implicitly casted to int
    const auto &parents = FD->getASTContext().getParents(*fl);
    if (!parents.empty()) {
        const Stmt *stmt1 = parents[0].get<Stmt>();
        if (stmt1) {
            switch (stmt1->getStmtClass()) {
                default:
                    break;
                case Stmt::ImplicitCastExprClass: {
                    const ImplicitCastExpr *ice = cast<ImplicitCastExpr>(stmt1);
                    switch (ice->getCastKind()) {
                        default:
                            break;
                        case CastKind::CK_FloatingToIntegral:
                            toInt = true;
                            break;
                    }
                }
            }
        }
    }

    ss << "expr.LitE(";
    if (toInt) {
        ss << (int64_t)fl->getValue().convertToDouble();
    } else {
        ss << std::fixed << fl->getValue().convertToDouble();
    }
    ss << ", " << locStr << ")";
    SLANG_TRACE(ss.str())

    return SlangExpr(ss.str(), false, fl->getType());
}

SlangExpr SlangGenChecker::convertStringLiteral(const StringLiteral *sl) const {
    std::stringstream ss;

    std::string locStr = getLocationString(sl);

    ss << "expr.LitE(\"\"\"" << sl->getBytes().str() << "\"\"\"";
    ss << ", " << locStr << ")";
    SLANG_TRACE(ss.str() << "---- " << sl->getByteLength())

    return SlangExpr(ss.str(), false, sl->getType());
} // convertStringLiteral()

SlangExpr SlangGenChecker::convertMemberExpr(const MemberExpr *memberExpr,
                                             bool compound_receiver) const {
    std::stringstream ss;
    SlangExpr slangExpr;
    std::vector<std::string> memberNames;
    const Stmt *stmt;

    // slangExpr.qualType = FD->getASTContext().getTypeOfExprType(
    //        const_cast<Expr*>(cast<Expr>(memberExpr)));
    slangExpr.qualType = memberExpr->getType();
    stmt = memberExpr;
    std::string memberName;
    do {
        memberName = cast<MemberExpr>(stmt)->getMemberNameInfo().getAsString();
        if (memberName == "") {
            memberName = stu.getVar((uint64_t)(cast<MemberExpr>(stmt)->getMemberDecl())).name;
        }
        memberNames.push_back(memberName);
        stmt = stu.popFromMainStack();
    } while (isa<MemberExpr>(stmt));
    std::string locStr = getLocationString(stmt);

    // the last stmt can be converted by a call to convertExpr
    stu.pushToMainStack(stmt);
    SlangExpr mainVarExpr = convertExpr(true);

    if (compound_receiver) {
        slangExpr = genTmpVariable(slangExpr.qualType, locStr);
        ss << "instr.AssignI(" << slangExpr.expr << ", ";
    }

    ss << "expr.MemberE(" << mainVarExpr.expr << ", ";
    std::string prefix = "";
    ss << "[";
    std::reverse(memberNames.begin(), memberNames.end());
    for (auto memberName : memberNames) {
        ss << prefix << "\"" << memberName << "\"";
        if (prefix == "")
            prefix = ", ";
    }
    ss << "]";
    ss << ", " << locStr << ")"; // close expr.MemberE(...

    slangExpr.addSlangStmtsBack(mainVarExpr.slangStmts);
    if (compound_receiver) {
        ss << ", " << locStr << ")"; // close instr.AssignI(...
        slangExpr.addSlangStmtBack(ss.str());
    } else {
        slangExpr.expr = ss.str();
    }

    return slangExpr;
} // convertMemberExpr()

SlangExpr SlangGenChecker::convertCallExpr(const CallExpr *callExpr, bool compound_receiver) const {
    std::stringstream ss;
    std::vector<SlangExpr> args;
    SlangExpr slangExpr;
    std::string calleeName;

    std::string locStr = getLocationString(callExpr);

    slangExpr.compound = true;

    uint32_t numOfArgs = callExpr->getNumArgs();
    slangExpr.qualType = callExpr->getType();

    // convert each argument
    for (uint32_t i = 0; i < numOfArgs; ++i) {
        args.push_back(convertExpr(true));
    }

    // convert callee name/expr argument
    SlangExpr calleeExpr = convertExpr(true);

    slangExpr.addSlangStmtsBack(calleeExpr.slangStmts);
    ss.str("");
    std::string prefix = "";
    for (auto argIter = args.end() - 1; argIter != args.begin() - 1; --argIter) {
        slangExpr.addSlangStmtsBack(argIter->slangStmts);
        ss << prefix << argIter->expr;
        if (prefix.size() == 0) {
            prefix = ", ";
        }
    }
    std::string argString = ss.str();

    ss.str("");
    ss << "expr.CallE(" << calleeExpr.expr;
    ss << ", [" << argString << "]";
    ss << ", " << locStr << ")"; // end expr.CallE
    slangExpr.expr = ss.str();

    if (compound_receiver) {
        ss.str("");
        std::string locStr = getLocationString(callExpr);
        SlangExpr tmpVar = genTmpVariable(slangExpr.qualType, locStr);
        ss << "instr.AssignI(" << tmpVar.expr << ", ";
        ss << slangExpr.expr;
        ss << ", " << locStr << ")";
        tmpVar.addSlangStmtsBack(slangExpr.slangStmts);
        tmpVar.addSlangStmtBack(ss.str());
        return tmpVar;
    }

    return slangExpr;
} // convertCallExpr()

SlangExpr SlangGenChecker::convertAssignment(bool compound_receiver, std::string &compoundAssignOp,
                                             std::string &locStr) const {
    std::stringstream ss;
    SlangExpr slangExpr{};
    SlangExpr lhsRvalueTmp;
    SlangExpr newRhsExpr;
    SlangExpr exprLhs, exprRhs;

    // AD FIXME: separate out compound assignment logic into another function.
    if (!compoundAssignOp.empty()) {
        // compound assignments: +=, -=, %=, &=, ...
        exprRhs = convertExpr(true); // unconditionally true
        exprLhs = convertExpr(false);
    } else {
        exprLhs = convertExpr(false); // unconditionally false
        exprRhs = convertExpr(exprLhs.compound);
    }
    newRhsExpr.expr = exprRhs.expr;

    if (!compoundAssignOp.empty()) {
        newRhsExpr.expr = exprLhs.expr;
        if (exprLhs.compound) {
            // e.g.: if stmt is: *x += 1
            // gen stmt: t.1 = *x;
            lhsRvalueTmp = genTmpVariable(exprLhs.qualType, locStr);
            ss << "instr.AssignI(" << lhsRvalueTmp.expr << ", " << exprLhs.expr;
            ss << ", " << locStr << ")";
            newRhsExpr.expr = lhsRvalueTmp.expr;
            newRhsExpr.addSlangStmtBack(ss.str());
        }

        // for: x += 1, gen (x + 1)
        // for: *x += 1, gen (t.1 + 1) -- we get t.1 from above
        ss.str("");
        ss << "expr.BinaryE(" << newRhsExpr.expr;
        ss << ", " << compoundAssignOp << ", " << exprRhs.expr;
        ss << ", " << locStr << ")";
        newRhsExpr.expr = ss.str();

        if (exprLhs.compound) {
            lhsRvalueTmp = genTmpVariable(exprLhs.qualType, locStr);
            ss.str("");
            ss << "instr.AssignI(" << lhsRvalueTmp.expr;
            ss << ", " << newRhsExpr.expr;
            ss << ", " << locStr << ")";
            newRhsExpr.expr = lhsRvalueTmp.expr;
            newRhsExpr.addSlangStmtBack(ss.str());
        }
    } // if

    ss.str("");
    ss << "instr.AssignI(" << exprLhs.expr << ", " << newRhsExpr.expr;
    ss << ", " << locStr << ")";
    SLANG_DEBUG(ss.str())

    if (compound_receiver && exprLhs.compound) {
        // i.e. lhs is compound, and receiver is compound,
        // store lhs in temporary
        slangExpr = genTmpVariable(exprLhs.qualType, locStr);

        // order_correction for assignment
        slangExpr.addSlangStmtsBack(exprRhs.slangStmts);
        slangExpr.addSlangStmtsBack(newRhsExpr.slangStmts);
        slangExpr.addSlangStmtsBack(exprLhs.slangStmts);

        slangExpr.addSlangStmtBack(ss.str());

        ss.str(""); // empty the stream
        ss << "instr.AssignI(" << slangExpr.expr << ", " << exprLhs.expr;
        ss << ", " << locStr << ")";
        slangExpr.addSlangStmtBack(ss.str());
    } else {
        // order_correction for assignment
        slangExpr.addSlangStmtsBack(exprRhs.slangStmts);
        slangExpr.addSlangStmtsBack(newRhsExpr.slangStmts);
        slangExpr.addSlangStmtsBack(exprLhs.slangStmts);

        slangExpr.addSlangStmtBack(ss.str());

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
} // convertAssignment()

void SlangGenChecker::adjustDirtyVar(SlangExpr &slangExpr, std::string &locStr) const {
    std::stringstream ss;
    bool newTmp;
    if (slangExpr.isNonTmpVar() && stu.isDirtyVar(slangExpr.varId)) {
        SlangExpr sp = getTmpVarForDirtyVar(slangExpr.varId, slangExpr.qualType, newTmp, locStr);
        if (newTmp) {
            // only add if a new temporary was generated
            ss << "instr.AssignI(" << sp.expr << ", " << slangExpr.expr;
            ss << ", " << locStr << ")";
            slangExpr.addSlangStmtBack(ss.str());
        }
        slangExpr.expr = sp.expr;
        slangExpr.nonTmpVar = false;
    }
}

SlangExpr SlangGenChecker::convertEnumConst(const EnumConstantDecl *ecd,
                                            std::string &locStr) const {
    std::stringstream ss;
    ss << "expr.LitE(" << (ecd->getInitVal()).toString(10);
    ss << ", " << locStr << ")";
    return SlangExpr(ss.str(), false, QualType());
}

SlangExpr SlangGenChecker::convertBinaryOp(const BinaryOperator *binOp,
                                           bool compound_receiver) const {
    std::stringstream ss;
    std::string op;
    SlangExpr varExpr{};

    std::string locStr = getLocationString(binOp);

    // convert logical ops separately, since they are converted to
    // `select` expressions.
    switch (binOp->getOpcode()) {
        case BO_LAnd:
        case BO_LOr:
            return convertBinaryLogicalOp(binOp, compound_receiver);
        default:
            break;
    }

    if (binOp->isAssignmentOp() || binOp->isCompoundAssignmentOp()) {
        std::string compoundAssignOp = "";
        if (binOp->isCompoundAssignmentOp()) {
            compoundAssignOp = getCompoundAssignOpString(binOp);
        }
        return convertAssignment(compound_receiver, compoundAssignOp, locStr);
    }

    SlangExpr exprR = convertExpr(true);
    SlangExpr exprL = convertExpr(true);
    adjustDirtyVar(exprL, locStr);

    if (compound_receiver) {
        varExpr = genTmpVariable(exprL.qualType, locStr);
        ss << "instr.AssignI(" << varExpr.expr << ", ";
    }

    // order_correction binary operator
    varExpr.addSlangStmtsBack(exprL.slangStmts);
    varExpr.addSlangStmtsBack(exprR.slangStmts);

    varExpr.qualType = exprL.qualType;

    switch (binOp->getOpcode()) {
    default: {
        SLANG_DEBUG("convertBinaryOp: " << binOp->getOpcodeStr())
        return SlangExpr("ERROR:convertBinaryOp", false, QualType());
    }

    case BO_Add: {
        op = "op.BO_ADD";
        break;
    }
    case BO_Sub: {
        op = "op.BO_SUB";
        break;
    }
    case BO_Mul: {
        op = "op.BO_MUL";
        break;
    }
    case BO_Div: {
        op = "op.BO_DIV";
        break;
    }
    case BO_Rem: {
        op = "op.BO_MOD";
        break;
    }

    case BO_LT: {
        op = "op.BO_LT";
        break;
    }
    case BO_LE: {
        op = "op.BO_LE";
        break;
    }
    case BO_EQ: {
        op = "op.BO_EQ";
        break;
    }
    case BO_NE: {
        op = "op.BO_NE";
        break;
    }
    case BO_GE: {
        op = "op.BO_GE";
        break;
    }
    case BO_GT: {
        op = "op.BO_GT";
        break;
    }

    case BO_Or: {
        op = "op.BO_BIT_OR";
        break;
    }
    case BO_And: {
        op = "op.BO_BIT_AND";
        break;
    }
    case BO_Xor: {
        op = "op.BO_BIT_XOR";
        break;
    }
    }

    ss << "expr.BinaryE(" << exprL.expr << ", " << op << ", " << exprR.expr << ")";

    if (compound_receiver) {
        ss << ", " << locStr << ")"; // close instr.AssignI(...
        varExpr.addSlangStmtBack(ss.str());
    } else {
        varExpr.expr = ss.str();
        varExpr.compound = true; // since a binary expression
    }

    return varExpr;
}

SlangExpr SlangGenChecker::convertArraySubscript(const ArraySubscriptExpr *arrayExpr,
                                                 bool compound_receiver) const {
    std::stringstream ss;
    SlangExpr varExpr;
    QualType qualType;
    std::vector<std::string> indexExprs;
    SlangExpr subScriptExpr;
    SlangExpr tmpSlangExpr;
    const Stmt *stmt;

    std::string locStr = getLocationString(arrayExpr);

    // e.g. if expr is : arr[x][y]
    do {
        // extract subscripts first y, then x...
        tmpSlangExpr = convertExpr(true);
        indexExprs.push_back(tmpSlangExpr.expr);
        subScriptExpr.addSlangStmtsFront(tmpSlangExpr.slangStmts);
        stmt = stu.popFromMainStack();
    } while (isa<ArraySubscriptExpr>(stmt));
    stu.pushToMainStack(stmt); // put the last one back

    SlangExpr arrExpr = convertExpr(true); // extracts the `arr` part

    // get the type (depends on the number of indexes)
    qualType = getCleanedQualType(arrExpr.qualType);
    for (auto _ : indexExprs) {
        auto type = qualType.getTypePtr();
        if (type->isArrayType()) {
            auto arrType = cast<ArrayType>(type);
            qualType = arrType->getElementType();
        } else if (type->isPointerType()) {
            auto ptrType = cast<PointerType>(type);
            qualType = ptrType->getPointeeType();
        }
    }

    varExpr.qualType = qualType;

    if (compound_receiver) {
        varExpr = genTmpVariable(qualType, locStr);
        ss << "instr.AssignI(" << varExpr.expr << ", ";
    }

    varExpr.addSlangStmtsBack(arrExpr.slangStmts);
    varExpr.addSlangStmtsBack(subScriptExpr.slangStmts);

    ss << "expr.ArrayE(" << arrExpr.expr << ", ";
    ss << "[";
    std::string prefix = "";
    std::reverse(indexExprs.begin(), indexExprs.end()); // y, x ---> x, y
    for (auto indexExpr : indexExprs) {
        ss << prefix << indexExpr;
        if (prefix == "") {
            prefix = ", ";
        }
    }
    ss << "]";
    ss << ", " << locStr << ")"; // close expr.ArrayE(...

    if (compound_receiver) {
        ss << ", " << locStr << ")"; // close instr.AssignI(...
        varExpr.addSlangStmtBack(ss.str());
    } else {
        varExpr.expr = ss.str();
        varExpr.compound = true; // since a binary expression
    }

    return varExpr;
}

SlangExpr SlangGenChecker::convertUnaryOp(const UnaryOperator *unOp, bool compound_receiver) const {
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

    default: { break; }
    }

    SlangExpr exprArg;
    if (unOp->getOpcode() == UO_AddrOf) {
        // special case: e.g. &arr[7][5], ...
        exprArg = convertExpr(false);
    } else {
        exprArg = convertExpr(true);
    }

    adjustDirtyVar(exprArg, locStr);
    exprArg.qualType.dump();
    exprArg.qualType = qualType = getCleanedQualType(exprArg.qualType);
    qualType.dump();

    switch (unOp->getOpcode()) {
    default: {
        SLANG_DEBUG("convertUnaryOp: " << unOp->getOpcodeStr(unOp->getOpcode()))
        return SlangExpr("ERROR:convertUnaryOp", false, QualType());
    }

    case UO_AddrOf: {
        qualType = FD->getASTContext().getPointerType(qualType);
        op = "op.UO_ADDROF";
        break;
    }

    case UO_Deref: {
        const Type *type = qualType.getTypePtr();
        if (type->isArrayType()) {
            qualType = type->getAsArrayTypeUnsafe()->getElementType();
        } else if(type->isPointerType()) {
            auto ptrType = cast<PointerType>(qualType.getTypePtr());
            qualType = ptrType->getPointeeType();
        } else {
            SLANG_ERROR("Unhandled_TYPE_UO_Deref");
        }
        op = "op.UO_DEREF";
        break;
    }

    case UO_Minus: {
        op = "op.UO_MINUS";
        break;
    }
    case UO_Plus: {
        op = "op.UO_MINUS";
        break;
    }
    case UO_LNot: {
        op = "op.UO_NOT";
        break;
    }
    }

    if (compound_receiver) {
        varExpr = genTmpVariable(qualType, locStr);
        ss << "instr.AssignI(" << varExpr.expr << ", ";
    }

    ss << "expr.UnaryE(" << op << ", " << exprArg.expr;
    ss << ", " << locStr << ")";

    // order_correction unary operator
    varExpr.addSlangStmtsBack(exprArg.slangStmts);

    if (compound_receiver) {
        ss << ", " << locStr << ")"; // close instr.AssignI(...
        varExpr.addSlangStmtBack(ss.str());
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

    switch (unOp->getOpcode()) {
    case UO_PreInc: {
        ss << "instr.AssignI(" << exprArg.expr << ", ";
        ss << "expr.BinaryE(" << exprArg.expr << ", op.BO_ADD, expr.LitE(1)";
        ss << ", " << locStr << ")"; // close expr.BinaryE(
        ss << ", " << locStr << ")"; // close instr.AssignI(...
        exprArg.addSlangStmtBack(ss.str());

        if (exprArg.nonTmpVar && stu.isDirtyVar(exprArg.varId)) {
            adjustDirtyVar(exprArg, locStr);
        }
        stu.setDirtyVar(exprArg.varId, emptySlangExpr);
        break;
    }

    case UO_PostInc: {
        ss << "instr.AssignI(" << exprArg.expr << ", ";
        ss << "expr.BinaryE(" << exprArg.expr << ", op.BO_ADD, expr.LitE(1";
        ss << ", " << locStr << ")"; // close expr.LitE(
        ss << ", " << locStr << ")"; // close expr.BinaryE(
        ss << ", " << locStr << ")"; // close instr.AssignI(...

        if (exprArg.nonTmpVar) {
            stu.setDirtyVar(exprArg.varId, emptySlangExpr);
            if (!isTopLevel(unOp)) {
                adjustDirtyVar(exprArg, locStr);
            }
        }
        // add increment after storing in temporary
        exprArg.addSlangStmtBack(ss.str());
        break;
    }

    case UO_PreDec: {
        ss << "instr.AssignI(" << exprArg.expr << ", ";
        ss << "expr.BinaryE(" << exprArg.expr << ", op.BO_SUB, expr.LitE(1)";
        ss << ", " << locStr << ")"; // close expr.BinaryE(
        ss << ", " << locStr << ")"; // close instr.AssignI(...
        exprArg.addSlangStmtBack(ss.str());

        if (exprArg.nonTmpVar && stu.isDirtyVar(exprArg.varId)) {
            adjustDirtyVar(exprArg, locStr);
        }
        stu.setDirtyVar(exprArg.varId, emptySlangExpr);
        break;
    }

    case UO_PostDec: {
        ss << "instr.AssignI(" << exprArg.expr << ", ";
        ss << "expr.BinaryE(" << exprArg.expr << ", op.BO_SUB, expr.LitE(1";
        ss << ", " << locStr << ")"; // close expr.LitE(
        ss << ", " << locStr << ")"; // close expr.BinaryE(
        ss << ", " << locStr << ")"; // close instr.AssignI(...

        if (exprArg.nonTmpVar) {
            stu.setDirtyVar(exprArg.varId, emptySlangExpr);
            if (!isTopLevel(unOp)) {
                adjustDirtyVar(exprArg, locStr);
            }
        }
        // add increment after storing in temporary
        exprArg.addSlangStmtBack(ss.str());
        break;
    }

    default: {
        SLANG_ERROR("UnknownOp")
        break;
    }
    }

    return exprArg;
} // convertUnaryIncDec()

SlangExpr SlangGenChecker::convertVarDecl(const VarDecl *varDecl, std::string &locStr) const {
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

SlangExpr SlangGenChecker::convertInitListExpr(const InitListExpr *initListExpr) const {
    QualType qualType = initListExpr->getType();

    std::string locStr;
    const Stmt *lastDeclStmt = stu.getLastDeclStmt();
    SLANG_DEBUG("Fetched last DeclStmt");

    if (lastDeclStmt) {
        // for top level InitListExpr
        SLANG_DEBUG("Last DeclStmt at " << (uint64_t)lastDeclStmt);
        locStr = getLocationString(lastDeclStmt);
        stu.setLastDeclStmtTo(nullptr);
        SLANG_DEBUG("Set last DeclStmt to nullptr");
    } else {
        SLANG_DEBUG("Last DeclStmt is nullptr");
        locStr = getLocationString(initListExpr);
    }

    SlangExpr tmp = genTmpVariable(qualType, locStr); // store it into temp

    const Type *typePtr = qualType.getTypePtr();
    RecordDecl *recordDecl = nullptr;

    if (typePtr->isStructureType())
        recordDecl = typePtr->getAsStructureType()->getDecl();
    else if (typePtr->isUnionType())
        recordDecl = typePtr->getAsUnionType()->getDecl();

    // get info from map
    SlangRecord slangRecord = stu.recordMap[(uint64_t)recordDecl];

    std::vector<SlangRecordField> recordFields = slangRecord.getFields();
    int fieldCount = recordFields.size();

    std::stack<SlangExpr> slangStmtStack;
    for (int i = 0; i < fieldCount; ++i) {
        SlangExpr currentExpr = convertExpr(true);
        slangStmtStack.push(currentExpr);
    }

    std::stringstream ss;
    for (int i = 0; i < fieldCount; ++i) {
        SlangExpr currentStmt = slangStmtStack.top();
        tmp.addSlangStmtsBack(currentStmt.slangStmts);
        ss << "instr.AssignI("
           << "expr.MemberE(" << tmp.expr << ", [\"" << recordFields[i].getName() << "\"], "
           << locStr << "), " << currentStmt.expr << ")"; // close instr.AssignI(...
        slangStmtStack.pop();
        tmp.addSlangStmtBack(ss.str());
        ss.str("");
    }
    return tmp;
} // convertInitListExpr()

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
        return convertEnumConst(ecd, locStr);
    } else if (isa<FunctionDecl>(valueDecl)) {
        auto funcDecl = cast<FunctionDecl>(valueDecl);
        std::string funcName = funcDecl->getNameInfo().getAsString();
        ss << "expr.FuncE(\"" << stu.convertFuncName(funcName) << "\"";
        ss << ", " << locStr << ")";
        return SlangExpr(ss.str(), false, funcDecl->getType());
    } else {
        SLANG_ERROR("Not_a_VarDecl.")
        return SlangExpr("ERROR:convertDeclRefExpr", false, QualType());
    }
}
// BOUND START: type_conversion_routines

// converts clang type to span ir types
std::string SlangGenChecker::convertClangType(QualType qt) const {
    std::stringstream ss;

    if (qt.isNull()) {
        return "types.Int32"; // the default type
    }

    qt = getCleanedQualType(qt);

    const Type *type = qt.getTypePtr();

    if (type->isBuiltinType()) {
        return convertClangBuiltinType(qt);

    } else if (type->isEnumeralType()) {
        ss << "types.Int32";

    } else if (type->isFunctionPointerType()) {
        // should be before ->isPointerType() check below
        return convertFunctionPointerType(qt);

    } else if (type->isPointerType()) {
        ss << "types.Ptr(to=";
        QualType pqt = type->getPointeeType();
        ss << convertClangType(pqt);
        ss << ")";

    } else if (type->isRecordType()) {
        if (type->isStructureType()) {
            return convertClangRecordType(qt.getTypePtr()->getAsStructureType()->getDecl());
        } else if (type->isUnionType()) {
            return convertClangRecordType(qt.getTypePtr()->getAsUnionType()->getDecl());
        } else {
            ss << "ERROR:RecordType";
        }

    } else if (type->isArrayType()) {
        return convertClangArrayType(qt);

    } else {
        ss << "UnknownType.";
    }

    return ss.str();
} // convertClangType()

std::string SlangGenChecker::convertClangBuiltinType(QualType qt) const {
    std::stringstream ss;

    const Type *type = qt.getTypePtr();

    if (type->isSignedIntegerType()) {
        if (type->isCharType()) {
            ss << "types.Int8";
        } else if (type->isChar16Type()) {
            ss << "types.Int16";
        } else if (type->isIntegerType()) {
            ss << "types.Int32";
        } else {
            ss << "UnknownSignedIntType.";
        }

    } else if (type->isUnsignedIntegerType()) {
        if (type->isCharType()) {
            ss << "types.UInt8";
        } else if (type->isChar16Type()) {
            ss << "types.UInt16";
        } else if (type->isIntegerType()) {
            ss << "types.UInt32";
        } else {
            ss << "UnknownUnsignedIntType.";
        }

    } else if (type->isFloatingType()) {
        ss << "types.Float32";
    } else if (type->isRealFloatingType()) {
        ss << "types.Float64"; // FIXME: is realfloat a double?

    } else if (type->isVoidType()) {
        ss << "types.Void";
    } else {
        ss << "UnknownBuiltinType.";
    }

    return ss.str();
} // convertClangBuiltinType()

std::string SlangGenChecker::convertClangRecordType(const RecordDecl *recordDecl) const {
    // a hack1 for anonymous decls (it works!) see test 000193.c and its AST!!
    static const RecordDecl *lastAnonymousRecordDecl = nullptr;

    if (recordDecl == nullptr) {
        // default to the last anonymous record decl
        return convertClangRecordType(lastAnonymousRecordDecl);
    }

    if (stu.isRecordPresent((uint64_t)recordDecl)) {
        return stu.getRecord((uint64_t)recordDecl).toShortString();
    }

    std::string namePrefix;
    SlangRecord slangRecord;

    if (recordDecl->isStruct()) {
        namePrefix = "s:";
        slangRecord.recordKind = Struct;
    } else if (recordDecl->isUnion()) {
        namePrefix = "u:";
        slangRecord.recordKind = Union;
    }

    if (recordDecl->getNameAsString() == "") {
        slangRecord.anonymous = true;
        slangRecord.name = namePrefix + stu.getNextRecordIdStr();
    } else {
        slangRecord.anonymous = false;
        slangRecord.name = namePrefix + recordDecl->getNameAsString();
    }

    slangRecord.locStr = getLocationString(recordDecl);

    stu.addRecord((uint64_t)recordDecl, slangRecord);                  // IMPORTANT
    SlangRecord &newSlangRecord = stu.getRecord((uint64_t)recordDecl); // IMPORTANT

    SlangRecordField slangRecordField;

    for (auto it = recordDecl->decls_begin(); it != recordDecl->decls_end(); ++it) {
        (*it)->dump();
        if (isa<RecordDecl>(*it)) {
            convertClangRecordType(cast<RecordDecl>(*it));
        } else if (isa<FieldDecl>(*it)) {
            const FieldDecl *fieldDecl = cast<FieldDecl>(*it);

            slangRecordField.clear();

            if (fieldDecl->getNameAsString() == "") {
                slangRecordField.name = newSlangRecord.getNextAnonymousFieldIdStr() + "a";
                slangRecordField.anonymous = true;
            } else {
                slangRecordField.name = fieldDecl->getNameAsString();
                slangRecordField.anonymous = false;
            }

            slangRecordField.type = fieldDecl->getType();
            if (slangRecordField.anonymous) {
                auto slangVar = SlangVar((uint64_t)fieldDecl, slangRecordField.name);
                stu.addVar((uint64_t)fieldDecl, slangVar);
                slangRecordField.typeStr = convertClangRecordType(nullptr);
            } else {
                slangRecordField.typeStr = convertClangType(slangRecordField.type);
            }

            newSlangRecord.fields.push_back(slangRecordField);
        }
    }

    // store for later use (part-of-hack1))
    lastAnonymousRecordDecl = recordDecl;

    // no need to add newSlangRecord, its a reference to its entry in the stu.recordMap
    return newSlangRecord.toShortString();
} // convertClangRecordType()

std::string SlangGenChecker::convertClangArrayType(QualType qt) const {
    std::stringstream ss;

    const Type *type = qt.getTypePtr();
    const ArrayType *arrayType = type->getAsArrayTypeUnsafe();

    if (isa<ConstantArrayType>(arrayType)) {
        ss << "types.ConstSizeArray(of=";
        ss << convertClangType(arrayType->getElementType());
        ss << ", ";
        auto constArrType = cast<ConstantArrayType>(arrayType);
        ss << "size=" << constArrType->getSize().toString(10, true);
        ss << ")";

    } else if (isa<VariableArrayType>(arrayType)) {
        ss << "types.VarArray(of=";
        ss << convertClangType(arrayType->getElementType());
        ss << ")";

    } else if (isa<IncompleteArrayType>(arrayType)) {
        ss << "types.IncompleteArray(of=";
        ss << convertClangType(arrayType->getElementType());
        ss << ")";

    } else {
        ss << "UnknownArrayType";
    }

    SLANG_DEBUG(ss.str())
    return ss.str();
} // convertClangArrayType()

std::string SlangGenChecker::convertFunctionPointerType(QualType qt) const {
    std::stringstream ss;

    const Type *type = qt.getTypePtr();

    ss << "types.Ptr(to=";
    const Type *funcType = type->getPointeeType().getTypePtr();
    funcType = funcType->getUnqualifiedDesugaredType();
    if (isa<FunctionProtoType>(funcType)) {
        auto funcProtoType = cast<FunctionProtoType>(funcType);
        ss << "types.FuncSig(returnType=";
        ss << convertClangType(funcProtoType->getReturnType());
        ss << ", "
           << "paramTypes=[";
        std::string prefix = "";
        for (auto qType : funcProtoType->getParamTypes()) {
            ss << prefix << convertClangType(qType);
            if (prefix == "")
                prefix = ", ";
        }
        ss << "]";
        if (funcProtoType->isVariadic()) {
            ss << ", variadic=True";
        }
        ss << ")"; // close types.FuncSig(...
        ss << ")"; // close types.Ptr(...

    } else if (isa<FunctionNoProtoType>(funcType)) {
        ss << "types.FuncSig(returnType=types.Int32)";
        ss << ")"; // close types.Ptr(...

    } else if (isa<FunctionType>(funcType)) {
        ss << "FuncType";

    } else {
        ss << "UnknownFunctionPtrType";
    }

    return ss.str();
} // convertFunctionPointerType()

// BOUND END  : type_conversion_routines

SlangExpr SlangGenChecker::convertUnaryExprOrTypeTraitExpr(const UnaryExprOrTypeTraitExpr *stmt,
                                                           bool compound_receiver) const {
    SlangExpr slangExpr;
    SlangExpr innerExpr;
    std::stringstream ss;
    uint64_t size = 0;

    std::string locStr = getLocationString(stmt);

    UnaryExprOrTypeTrait kind = stmt->getKind();
    switch (kind) {
    // the sizeof operator
    case UETT_SizeOf: {
        auto iterator = stmt->child_begin();
        if (iterator != stmt->child_end()) {
            // then child is an expression
            innerExpr = convertAstExpr(stmt, true);
            slangExpr.addSlangStmtsBack(innerExpr.slangStmts);

            const Stmt *firstChild = *iterator;
            const Expr *expr = cast<Expr>(firstChild);
            // slangExpr.qualType = FD->getASTContext().getTypeOfExprType(const_cast<Expr*>(expr));
            slangExpr.qualType = expr->getType();
            const Type *type = slangExpr.qualType.getTypePtr();
            if (type && !isIncompleteType(type)) {
                TypeInfo typeInfo = FD->getASTContext().getTypeInfo(slangExpr.qualType);
                size = typeInfo.Width / 8;
            } else {
                // FIXME: handle runtime sizeof support too
                SLANG_ERROR("SizeOf_Expr_is_incomplete. Loc:" << locStr)
            }
        } else {
            // child is a type
            slangExpr.qualType = stmt->getArgumentType();
            TypeInfo typeInfo = FD->getASTContext().getTypeInfo(slangExpr.qualType);
            size = typeInfo.Width / 8;
        }

        ss << "expr.LitE(";
        if (size == 0) {
            ss << "ERROR:sizeof()";
        } else {
            ss << size;
        }
        ss << ", " << locStr << ")";
        slangExpr.expr = ss.str();
        break;
    }

    default:
        SLANG_ERROR("UnaryExprOrTypeTrait not handled. Kind: " << kind)
        break;
    }
    return slangExpr;
} // convertUnaryExprOrTypeTraitExpr()

SlangExpr SlangGenChecker::convertAstExpr(const Stmt *stmt, bool compound_receiver) const {
    StmtVector stmtVec;
    bool stmtsPresent = false;

    // don't add the current stmt
    auto firstChild = stmt->child_begin();
    auto lastChild = stmt->child_end();

    if (firstChild != lastChild) {
        // there are children
        for (auto iter = firstChild; iter != lastChild; ++iter) {
            const Stmt *stmt = *iter;
            if (stmt) {
                stmtsPresent = true;
                handleAstStmts(stmt);
            }
        }
    }

    if (stmtsPresent) {
        return convertExpr(compound_receiver);
    } else {
        return SlangExpr("", false, QualType());
    }
} // convertAstExpr()

SlangExpr SlangGenChecker::convertCStyleCastExpr(const CStyleCastExpr *cCast,
                                                 bool compound_receiver) const {
    std::stringstream ss;
    std::string op;
    SlangExpr varExpr{};
    QualType qualType;

    std::string locStr = getLocationString(cCast);

    SlangExpr exprArg = convertExpr(true);

    adjustDirtyVar(exprArg, locStr);
    qualType = cCast->getType();

    if (compound_receiver) {
        varExpr = genTmpVariable(qualType, locStr);
        ss << "instr.AssignI(" << varExpr.expr << ", ";
    }

    ss << "expr.CastE(" << exprArg.expr << ", " << convertClangType(qualType);
    ss << ", " << locStr << ")";

    // order_correction cast expression
    varExpr.addSlangStmtsBack(exprArg.slangStmts);

    if (compound_receiver) {
        ss << ", " << locStr << ")"; // close instr.AssignI(...
        varExpr.addSlangStmtBack(ss.str());
    } else {
        varExpr.expr = ss.str();
        varExpr.compound = true;
        varExpr.qualType = qualType;
    }
    return varExpr;
} // convertCStyleCastExpr()

SlangExpr SlangGenChecker::convertConditionalOp(const ConditionalOperator *condOp,
        bool compound_receiver) const {
    // assumed compound_receiver is true, hence not as parameter
    std::stringstream ss;
    SlangExpr slangExpr;
    std::vector<const Stmt*> stmts;

    std::string locStr = getLocationString(condOp);

    for (auto it = condOp->child_begin(); it != condOp->child_end(); ++it) {
        // there should be only three children
        stmts.push_back(*it);
    }

    if (stmts.size() != 3) {
        SLANG_ERROR("ConditionalOp: There should be three children. Found: " << stmts.size())
    }

    handleAstStmts(stmts[0]);
    SlangExpr condExpr = convertExpr(true);
    handleAstStmts(stmts[1]);
    SlangExpr arg1 = convertExpr(true);
    handleAstStmts(stmts[2]);
    SlangExpr arg2 = convertExpr(true);

    if (compound_receiver) {
        slangExpr = genTmpVariable(condOp->getType(), locStr);
        ss << "instr.AssignI(" << slangExpr.expr;
        ss << ", ";
    }

    slangExpr.addSlangStmtsBack(condExpr.slangStmts);
    slangExpr.addSlangStmtsBack(arg1.slangStmts);
    slangExpr.addSlangStmtsBack(arg2.slangStmts);

    ss << "expr.SelectE(";
    ss << condExpr.expr;
    ss << ", " << arg1.expr;
    ss << ", " << arg2.expr;
    ss << ", " << locStr << ")"; // close expr.SelectE(...

    if (compound_receiver) {
        ss << ", " << locStr << ")"; // close instr.AssignI(...
        slangExpr.addSlangStmtBack(ss.str());
    } else {
        slangExpr.expr = ss.str();
    }

    return slangExpr;
} // convertConditionalOp()

// only to handle && and || operators
// since they are converted to `select` expressions.
SlangExpr SlangGenChecker::convertBinaryLogicalOp(const BinaryOperator *binOp,
        bool compound_receiver) const {
    std::stringstream ss;
    std::stringstream ss1;
    // expr1 && expr2 or expr1 || expr2
    SlangExpr slangExpr;
    SlangExpr expr1;
    SlangExpr expr2;
    SlangExpr expr2Select;
    std::vector<const Stmt*> stmts;

    std::string locStr = getLocationString(binOp);

    for (auto it = binOp->child_begin(); it != binOp->child_end(); ++it) {
        // there should be two children only
        stmts.push_back(*it);
    }

    if (stmts.size() != 2) {
        SLANG_ERROR("BinaryLogicalOp: There should be two children. Found: " << stmts.size())
    }

    handleAstStmts(stmts[0]);
    expr1 = convertExpr(true);
    handleAstStmts(stmts[1]);
    expr2 = convertExpr(true);

    if (compound_receiver) {
        slangExpr = genTmpVariable(binOp->getType(), locStr);
        ss << "instr.AssignI(";
        ss << slangExpr.expr << ", ";
    }

    slangExpr.addSlangStmtsBack(expr1.slangStmts);
    slangExpr.addSlangStmtsBack(expr2.slangStmts);

    ss << "expr.SelectE(";
    ss << expr1.expr;
    if (binOp->getOpcode() == BO_LAnd) {
        ss << ", " << expr2.expr;
        ss << ", expr.Lit(0)";
    } else if (binOp->getOpcode() == BO_LOr){
        ss << ", expr.Lit(1)";
        ss << ", " << expr2.expr;
    } else {
        SLANG_ERROR("Wrong_operator: " << binOp->getStmtClassName())
    }
    ss << ", " << locStr << ")"; // close expr.SelectE(...

    if (compound_receiver) {
        ss << ", " << locStr << ")"; // close instr.AssignI(...
        slangExpr.addSlangStmtBack(ss.str());
    } else {
        slangExpr.expr = ss.str();
        slangExpr.qualType = binOp->getType();
    }

    return slangExpr;
} // convertBinaryLogicalOp()

// BOUND END  : conversion_routines to SlangExpr

// BOUND START: helper_functions

// manually generate a sequence of stmts from a given AST root
void SlangGenChecker::genStmtVectorFromAST(const Stmt *stmt, StmtVector &stmtVec) const {
    auto firstChild = stmt->child_begin();
    auto lastChild = stmt->child_end();

    if (firstChild != lastChild) {
        // there are children
        for (auto iter = firstChild; iter != lastChild; ++iter) {
            const Stmt *stmt = *iter;
            if (stmt) {
                genStmtVectorFromAST(stmt, stmtVec);
            }
        }
    }
    stmtVec.push_back(stmt);
} // genStmtVectorFromAST()

// get all case statements recursively (case stmts can be hierarchical)
void SlangGenChecker::getCaseExpr(std::vector<StmtVector> &stmtVecVec,
                                  std::vector<std::string> &locStrs, const Stmt *stmt) const {
    StmtVector stmts;

    if (! stmt) return;

    if (isa<CaseStmt>(stmt)) {
        auto caseStmt = cast<CaseStmt>(stmt);
        std::string locStr = getLocationString(caseStmt);
        const Expr *condition = cast<Expr>(*(caseStmt->child_begin()));

        getCaseExprElements(stmts, condition);
        stmtVecVec.push_back(stmts);
        locStrs.push_back(locStr);

        for (auto it = caseStmt->child_begin(); it != caseStmt->child_end(); ++it) {
            if ((*it) && isa<CaseStmt>(*it)) {
                getCaseExpr(stmtVecVec, locStrs, (*it));
            }
        }

    } else if (isa<CompoundStmt>(stmt)) {
        const CompoundStmt *compoundStmt = cast<CompoundStmt>(stmt);
        for (auto it = compoundStmt->body_begin(); it != compoundStmt->body_end(); ++it) {
            getCaseExpr(stmtVecVec, locStrs, (*it));
        }
    } else if (isa<SwitchStmt>(stmt)) {
        // do nothing, as it will be handled in a different basic block.
    } else {
        if (stmt->child_begin() != stmt->child_end()) {
            for (auto it = stmt->child_begin(); it != stmt->child_end(); ++it) {
                getCaseExpr(stmtVecVec, locStrs, (*it));
            }
        }
    }
}

// Store elements in stmts, to make a new basic block for CaseStmt
void SlangGenChecker::getCaseExprElements(StmtVector &stmts, const Stmt *stmt) const {
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
SlangExpr SlangGenChecker::getTmpVarForDirtyVar(uint64_t varId, QualType qualType, bool &newTmp,
                                                std::string &locStr) const {
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

SlangExpr SlangGenChecker::genTmpVariable(std::string slangTypeStr, std::string &locStr) const {
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

SlangExpr SlangGenChecker::genTmpVariable(QualType qt, std::string &locStr) const {
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
    ss << "expr.VarE(\"" << slangVar.name << "\"";
    ss << ", " << locStr << ")";

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
    line = FD->getASTContext().getSourceManager().getExpansionLineNumber(stmt->getLocStart());
    ss << line << ",";
    col = FD->getASTContext().getSourceManager().getExpansionColumnNumber(stmt->getLocStart());
    ss << col << ")";

    return ss.str();
}

std::string SlangGenChecker::getLocationString(const RecordDecl *recordDecl) const {
    std::stringstream ss;
    uint32_t line = 0;
    uint32_t col = 0;

    ss << "Loc(";
    line = FD->getASTContext().getSourceManager().getExpansionLineNumber(recordDecl->getLocStart());
    ss << line << ",";
    col =
        FD->getASTContext().getSourceManager().getExpansionColumnNumber(recordDecl->getLocStart());
    ss << col << ")";

    return ss.str();
}

// get and encode the location of a statement element
uint64_t SlangGenChecker::getLocationId(const Stmt *stmt) const {
    uint64_t locId = 0;
    uint32_t line = 0;
    uint32_t col = 0;

    line = FD->getASTContext().getSourceManager().getExpansionLineNumber(stmt->getLocStart());
    col = FD->getASTContext().getSourceManager().getExpansionColumnNumber(stmt->getLocStart());

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
            switch (stmt1->getStmtClass()) {
            default:
                return false;

            case Stmt::DoStmtClass:
            case Stmt::ForStmtClass:
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
            return false;
        }
    } else {
        return true; // top level
    }
} // isTopLevel()

std::string SlangGenChecker::getCompoundAssignOpString(const BinaryOperator *binOp) const {
    std::string op;

    switch (binOp->getOpcode()) {
    case BO_AddAssign: {
        op = "op.BO_ADD";
        break;
    }
    case BO_SubAssign: {
        op = "op.BO_SUB";
        break;
    }
    case BO_MulAssign: {
        op = "op.BO_MUL";
        break;
    }
    case BO_DivAssign: {
        op = "op.BO_DIV";
        break;
    }
    case BO_RemAssign: {
        op = "op.BO_MOD";
        break;
    }

    case BO_AndAssign: {
        op = "op.BO_BIT_AND";
        break;
    }
    case BO_OrAssign: {
        op = "op.BO_BIT_OR";
        break;
    }
    case BO_XorAssign: {
        op = "op.BO_BIT_XOR";
        break;
    }

    case BO_ShlAssign: {
        op = "op.BO_SHL";
        break;
    }
    case BO_ShrAssign: {
        op = "op.BO_SHR";
        break;
    }

    default: {
        op = "ErrorAssignOp";
        break;
    }
    }

    return op;

} // getCompoundAssignOpString()

// Returns true if the type is not complete enough to give away a constant size
bool SlangGenChecker::isIncompleteType(const Type *type) const {
    bool retVal = false;

    if (type->isIncompleteArrayType() || type->isVariableArrayType()) {
        retVal = true;
    }

    return retVal;
}

// Remove qualifiers and typedefs
QualType SlangGenChecker::getCleanedQualType(QualType qt) const {
    if (qt.isNull()) return qt;
    qt = qt.getCanonicalType();
    qt.removeLocalConst();
    qt.removeLocalRestrict();
    qt.removeLocalVolatile();
    return qt;
}

// BOUND END  : helper_functions

// Register the Checker
void ento::registerSlangGenChecker(CheckerManager &mgr) { mgr.registerChecker<SlangGenChecker>(); }
