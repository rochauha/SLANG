//===----------------------------------------------------------------------===//
//  MIT License.
//  Copyright (c) 2019 The SLANG Authors.
//
//  Author: Ronak Chauhan (r.chauhan@somaiya.edu)
//  Author: Anshuman Dhuliya (dhuliya@cse.iitb.ac.in)
//
//AD If SlangGenChecker class name is added or changed, then also edit,
//AD ../../../include/clang/StaticAnalyzer/Checkers/Checkers.td
//
//===----------------------------------------------------------------------===//
//

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
#include <vector>                     //AD
#include <utility>                    //AD
#include <unordered_map>              //AD
#include <fstream>                    //AD
#include <sstream>                    //AD

using namespace clang;
using namespace ento;

//===----------------------------------------------------------------------===//
// FIXME: Utility Class
// Some useful static utility functions in a class.
//===----------------------------------------------------------------------===//
namespace {
    class Utility {
    public:
        static void readFile1();
    };
}

void Utility::readFile1() {
    //Read a specific local file.
    std::ifstream inputTxtFile;
    std::string line;
    std::string fileName("/home/codeman/.itsoflife/local/tmp/checker-input.txt");
    inputTxtFile.open(fileName);
    if (inputTxtFile.is_open()) {
      while(std::getline(inputTxtFile, line)) {
          llvm::errs() << line << "\n";
      }
      inputTxtFile.close();
    } else {
        llvm::errs() << "SLANG: ERROR: Cannot open file '" << fileName << "'\n";
    }
}

//===----------------------------------------------------------------------===//
// FIXME: Test TraversedInfoBuffer functionality.
// Information is stored while traversing the function's CFG. Specifically,
// 1. The information of the variables used.
// 2. The CFG and the basic block structure.
// 3. The three address code representation.
//===----------------------------------------------------------------------===//
namespace {
    enum EdgeLabel {FalseEdge, TrueEdge, UnCondEdge};

    class VarInfo {
    public:
        uint64_t id;
        // variable name: e.g. a variable 'x' in main function, is "v:main:x".
        std::string var_name;
        std::string type_str;

        std::string convertToString() {
            std::stringstream ss;
            ss << "\"" << var_name << "\": " << type_str << ",";
            return ss.str();
        }
    };

    class TraversedInfoBuffer {
    public:
        int id;
        int tmp_var_counter;

        std::string func_name;
        std::string func_ret_t;
        std::string func_params;

        int32_t bb_counter; // give bb their unique (new) id.

        // stack to help convert ast structure to 3-address code.
        std::vector<Stmt*> stmt_stack;
        // maps a unique variable id to its string representation.
        std::unordered_map<uint64_t , VarInfo> var_map;
        // maps bb_edge
        std::vector<std::pair<int, std::pair<int , EdgeLabel>>> bb_edges;
        // map to help remap bb ids, to mark start as 1 and end as -1.
        std::unordered_map<unsigned, int32_t> remap_bb_ids;
        std::unordered_map<uint32_t , std::vector<std::string>> bb_stmts;

        TraversedInfoBuffer();
        void clear();

        int32_t nextBbId();
        // conversion to SPAN Strings
        std::string convertClangType(QualType qt);
        std::string convertFuncName(std::string func_name);
        std::string convertLocalVarName(std::string var_name);
        std::string convertGlobalVarName(std::string var_name);

        // SPAN IR Printing routines
        TraversedInfoBuffer& dumpSpanIr();
        // void dumpHeader();
        // void dumpVariables();
        // void dumpFunctions();
        // void dumpFooter();
        std::string convertBbEdges();

        // For Program state purpose.
        // Overload the == operator
        bool operator==(const TraversedInfoBuffer &tib) const;
        // LLVMs equivalent of a hash function
        void Profile(llvm::FoldingSetNodeID &ID) const;
    };
}

TraversedInfoBuffer::TraversedInfoBuffer(): id{1}, tmp_var_counter{}, stmt_stack{}, var_map{},
        bb_edges{}, bb_stmts{}, remap_bb_ids{} {
}

int32_t TraversedInfoBuffer::nextBbId() {
    bb_counter += 1;
    return bb_counter;
}

std::string TraversedInfoBuffer::convertFuncName(std::string func_name) {
    std::stringstream ss;
    ss << "f:" << func_name;
    return ss.str();
}

std::string TraversedInfoBuffer::convertGlobalVarName(std::string var_name) {
    std::stringstream ss;
    ss << "v:" << var_name;
    return ss.str();
}

std::string TraversedInfoBuffer::convertLocalVarName(std::string var_name) {
    // assumes func_name is set
    std::stringstream ss;
    ss << "v:" << func_name << ":" << var_name;
    return ss.str();
}

// Converts Clang Types into SPAN parsable type strings:
// Examples,
// for `int` it returns `types.Int`.
// for `int*` it returns `types.Ptr(to=types.Int)`.
std::string TraversedInfoBuffer::convertClangType(QualType qt) {
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

// clear the buffer for the next function.
void TraversedInfoBuffer::clear() {
    func_name = "";
    func_ret_t = "";
    func_params = "";

    bb_counter = 1;
    stmt_stack.clear();
    var_map.clear();
    bb_edges.clear();
    bb_stmts.clear();
    remap_bb_ids.clear();
}

TraversedInfoBuffer& TraversedInfoBuffer::dumpSpanIr() {
    llvm::errs() << "Printing the TraversedInfoBuffer as SPAN IR:\n";
    llvm::errs() << "FuncName: " << func_name << "\n";
    llvm::errs() << convertBbEdges();
    return *this;
}

std::string TraversedInfoBuffer::convertBbEdges() {
    std::stringstream ss;
    ss << "Remapped BB ids:\n";
    for (auto i :remap_bb_ids) {
        ss << "  BB" << i.first << " as BB" << i.second << ".\n";
    }

    for (auto p: bb_edges) {
        ss << "graph.BbEdge(" << std::to_string(p.first);
        ss << ", " << std::to_string(p.second.first) << ", ";
        switch(p.second.second) {
            case FalseEdge: {
                ss << "graph.FalseEdge";
                break;
            }
            case TrueEdge: {
                ss << "graph.TrueEdge";
                break;
            }
            case UnCondEdge: {
                ss << "graph.UnCondEdge";
                break;
            }
        }
        ss << "),\n";
    }
    return ss.str();
}

// For Program state's purpose: not in use currently.
// Overload the == operator
bool TraversedInfoBuffer::operator==(const TraversedInfoBuffer &tib) const { return id == tib.id; }
// LLVMs equivalent of a hash function
void TraversedInfoBuffer::Profile(llvm::FoldingSetNodeID &ID) const { ID.AddInteger(1); }


//===----------------------------------------------------------------------===//
// SlangGenChecker
//===----------------------------------------------------------------------===//

namespace {
    class SlangGenChecker : public Checker<check::ASTCodeBody> {
        static TraversedInfoBuffer tib;

    public:
        void checkASTCodeBody(const Decl *D, AnalysisManager &mgr,
                              BugReporter &BR) const;

        void handleFunctionDef(const FunctionDecl *D) const;

        void handleCfg(const CFG *cfg) const;

        void handleBBInfo(const CFGBlock *bb, const CFG *cfg) const;

        void handleBBStmts(const CFGBlock *bb) const;

        void handleVariable(const ValueDecl *val_decl) const;

        void handleDeclStmt(const DeclStmt *declStmt) const;

        void handleIntegerLiteral(const IntegerLiteral *IL) const;

        void handleDeclRefExpr(const DeclRefExpr *DRE) const;

        void handleBinaryOperator(const Expr *ES,
                                  std::unordered_map<const Expr *, int> &visited,
                                  unsigned int block_id) const;

        void handleTerminator(const Stmt *terminator,
                              std::unordered_map<const Expr *, int> &visited,
                              unsigned int block_id) const;

    }; // class SlangGenChecker
} // anonymous namespace

TraversedInfoBuffer SlangGenChecker::tib = TraversedInfoBuffer();

// Main Entry Point. Invokes top level Function and Cfg handlers.
// Invoked once for each source translation unit function.
void SlangGenChecker::checkASTCodeBody(const Decl *D, AnalysisManager &mgr,
                                   BugReporter &BR) const {
    Utility::readFile1();
    llvm::errs() << "\nBOUND START: SLANG_Generated_Output.\n";

    const FunctionDecl *func_decl = dyn_cast<FunctionDecl>(D);
    handleFunctionDef(func_decl);

    if (const CFG *cfg = mgr.getCFG(D)) {
        handleCfg(cfg);
        tib.dumpSpanIr();
        tib.clear(); // clear the buffer for next function.
    } else {
        llvm::errs() << "SLANG: ERROR: No CFG for function.\n";
    }

    llvm::errs() << "\nBOUND END  : SLANG_Generated_Output.\n";
} // checkASTCodeBody()

void SlangGenChecker::handleCfg(const CFG *cfg) const {
    for (const CFGBlock *bb : *cfg) {
        handleBBInfo(bb, cfg);
        handleBBStmts(bb);
    }
} // handleCfg()

// Gets the function name, paramaters and return type.
void SlangGenChecker::handleFunctionDef(const FunctionDecl *func_decl) const {
    // STEP 1.1: Get function name.
    tib.func_name = func_decl->getNameInfo().getAsString();

    // STEP 1.2: Get function parameters.
    if (func_decl->doesThisDeclarationHaveABody()) { //& !func_decl->hasPrototype())
        for (unsigned i = 0, e = func_decl->getNumParams(); i != e; ++i) {
            const ParmVarDecl *paramVarDecl = func_decl->getParamDecl(i);
            handleVariable(paramVarDecl);
        }
    }

    // STEP 1.3: Get function return type.
    const QualType returnQType = func_decl->getReturnType();
    tib.func_ret_t = tib.convertClangType(returnQType);
} // handleFunctionDef()

void SlangGenChecker::handleBBInfo(const CFGBlock *bb, const CFG *cfg) const {
    unsigned bb_id = bb->getBlockID();
    llvm::errs() << "BB" << bb_id << "\n";
    unsigned succ_id;

    if (bb == &cfg->getEntry()) {
        tib.remap_bb_ids[bb_id] = 1;
        llvm::errs() << "ENTRY BB\n";
    } else if (bb == &cfg->getExit()) {
        tib.remap_bb_ids[bb_id] = -1;
        llvm::errs() << "EXIT BB\n";
    } else {
        if (tib.remap_bb_ids.find(bb_id) == tib.remap_bb_ids.end()) {
            tib.remap_bb_ids[bb_id] = tib.nextBbId();
        }
    }

    // access and record successor blocks
    const Stmt *terminator = (bb->getTerminator()).getStmt();
    if (terminator && isa<IfStmt>(terminator)) {
        bool true_edge = true;
        if (bb->succ_size() > 2) {
            llvm::errs() << "SPAN: ERROR: 'If' has more than two successors.\n";
        }

        for (CFGBlock::const_succ_iterator I = bb->succ_begin();
             I != bb->succ_end(); ++I) {

            CFGBlock *succ = *I;
            succ_id = succ->getBlockID();
            if (tib.remap_bb_ids.find(succ_id) == tib.remap_bb_ids.end()) {
                tib.remap_bb_ids[succ_id] = tib.nextBbId();
            }
            if (true_edge) {
                tib.bb_edges.push_back(std::make_pair(tib.remap_bb_ids[bb_id],
                        std::make_pair(tib.remap_bb_ids[succ_id], TrueEdge)));
                true_edge = false;
            } else {
                tib.bb_edges.push_back(std::make_pair(tib.remap_bb_ids[bb_id],
                        std::make_pair(tib.remap_bb_ids[succ_id], FalseEdge)));
            }
        }
    } else {
        if (!bb->succ_empty()) {
            // num. of succ: bb->succ_size()
            for (CFGBlock::const_succ_iterator I = bb->succ_begin();
                    I != bb->succ_end(); ++I) {
                CFGBlock *succ = *I;
                if (!succ) {
                    // unreachable block ??
                    succ = I->getPossiblyUnreachableBlock();
                    llvm::errs() << "(Unreachable BB)";
                    continue;
                }

                succ_id = succ->getBlockID();
                if (tib.remap_bb_ids.find(succ_id) == tib.remap_bb_ids.end()) {
                    tib.remap_bb_ids[succ_id] = tib.nextBbId();
                }
                tib.bb_edges.push_back(std::make_pair(tib.remap_bb_ids[bb_id],
                         std::make_pair(tib.remap_bb_ids[succ_id], UnCondEdge)));
            }
        }
    }
} // handleBBInfo()

void SlangGenChecker::handleBBStmts(const CFGBlock *bb) const {
    std::unordered_map<const Expr *, int> visited_nodes;

    unsigned bb_id = bb->getBlockID();

    for (auto elem : *bb) {
        // ref: https://clang.llvm.org/doxygen/CFG_8h_source.html#l00056
        // ref for printing block:
        // https://clang.llvm.org/doxygen/CFG_8cpp_source.html#l05234

        Optional<CFGStmt> CS = elem.getAs<CFGStmt>();
        const Stmt *stmt = CS->getStmt();
        Stmt::StmtClass stmt_cls = stmt->getStmtClass();

        // the main statement selection conditions.
        // Ronak and Manav: Create a separate function,
        // to handle each kind of statement/expression.
        switch (stmt_cls) {
            default: {
                llvm::errs() << "SLANG: ERROR: Unhandled Stmt Class: " <<
                             stmt->getStmtClassName() << ".\n";
                stmt->dump();
                llvm::errs() << "\n";
                break;
            }
            case Stmt::DeclRefExprClass: {
                const DeclRefExpr *dre = cast<DeclRefExpr>(stmt);
                const ValueDecl *val_decl = dre->getDecl();
                handleVariable(val_decl);
                break;
            }
            case Stmt::DeclStmtClass: {
                const DeclStmt *declStmt = cast<DeclStmt>(stmt);
                handleDeclStmt(declStmt);
                break;
            }
            // case Stmt::IntegerLiteralClass: {
            //     const IntegerLiteral *int_lit = cast<IntegerLiteral>(stmt);
            //     handleIntegerLiteral(int_lit);
            //     break;
            // }
            // case Stmt::BinaryOperatorClass : {
            //     handleBinaryOperator(ES, visited_nodes, bb_id);
            //     llvm::errs() << "\n";
            //     break;
            // }
        }
        // if (stmt_class == "DeclStmt") {
        // } else if (stmt_class == "BinaryOperator") {
        //     handleBinaryOperator(ES, visited_nodes, bb_id);
        //     llvm::errs() << "\n";
        // } else {
        //     // llvm::errs() << "found " << stmt_class << "\n";
        // }
        // llvm::errs() << "Partial AST info \n";
        // S->dump(); // Dumps partial AST
        // llvm::errs() << "\n";
    } // for (auto elem : *bb)

    // get terminator
    const Stmt *terminator = (bb->getTerminator()).getStmt();
    handleTerminator(terminator, visited_nodes, bb_id);

    llvm::errs() << "\n\n";
} // handleBBStmts()

void SlangGenChecker::handleVariable(const ValueDecl *val_decl) const {
    uint64_t var_id = (uint64_t) val_decl;
    if (tib.var_map.find(var_id) == tib.var_map.end()) {
        // seeing the variable for the first time.
        VarInfo varInfo{};
        varInfo.id = var_id;
        const VarDecl *varDecl = dyn_cast<VarDecl>(val_decl);
        if (varDecl) {
            if (varDecl->hasLocalStorage()) {
                varInfo.var_name = tib.convertLocalVarName(val_decl->getNameAsString());
            } else if(varDecl->hasGlobalStorage()) {
                varInfo.var_name = tib.convertGlobalVarName(val_decl->getNameAsString());
            } else {
                llvm::errs() << "SLANG: ERROR: Unknown variable storage.\n";
            }
        } else {
            llvm::errs() << "SLANG: ERROR: ValueDecl not a VarDecl!\n";
        }
        varInfo.type_str = tib.convertClangType(val_decl->getType());
        tib.var_map[var_id] = varInfo;
        llvm::errs() << "NEW_VAR: " << varInfo.convertToString() << "\n";
    } else {
        llvm::errs() << "SEEN_VAR: " << tib.var_map[var_id].convertToString() << "\n";
    }
}

void SlangGenChecker::handleDeclStmt(const DeclStmt *declStmt) const {
    // assumes there is only single decl inside (the likely case).
    const VarDecl *varDecl = cast<VarDecl>(declStmt->getSingleDecl());
    handleVariable(varDecl);
}

void SlangGenChecker::handleIntegerLiteral(const IntegerLiteral *IL) const {
    bool is_signed = IL->getType()->isSignedIntegerType();
    llvm::errs() << "expr.Lit(" << IL->getValue().toString(10, is_signed) << ")\n";
}

void SlangGenChecker::handleDeclRefExpr(const DeclRefExpr *DRE) const {
    const ValueDecl *ident = DRE->getDecl();
    llvm::errs() << ident->getName() << " ";
}

// Before accessing an expression, the CFG accesses its sub expressions in a
// bottom-up fashion. This can be seen when we dump the CFG using clang. To have
// similar access manually, we keep track of already traversed sub-expressions
// in the 'visited' hash table.
void SlangGenChecker::handleBinaryOperator(
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
            } else {
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
    } else if (isa<DeclRefExpr>(ES)) {
        const DeclRefExpr *decl_ref_expr = cast<DeclRefExpr>(ES);
        handleDeclRefExpr(decl_ref_expr);
    } else if (isa<IntegerLiteral>(ES)) {
        const IntegerLiteral *int_literal = cast<IntegerLiteral>(ES);
        handleIntegerLiteral(int_literal);
    } else if (isa<ImplicitCastExpr>(ES)) {
        auto ES2 = ES->IgnoreParenImpCasts();
        handleBinaryOperator(ES2, visited, block_id);
    }
}

void SlangGenChecker::handleTerminator(
        const Stmt *terminator, std::unordered_map<const Expr *, int> &visited,
        unsigned int block_id) const {

    if (terminator && isa<IfStmt>(terminator)) {
        const Expr *condition = (cast<IfStmt>(terminator))->getCond();
        llvm::errs() << "if ";
        handleBinaryOperator(condition, visited, block_id);
        llvm::errs() << "\n";
    }
}

// Register the Checker
void ento::registerSlangGenChecker(CheckerManager &mgr) {
    mgr.registerChecker<SlangGenChecker>();
}
