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
    enum EdgeKind {FalseEdge, TrueEdge, UnCondEdge} EK;

    class TraversedInfoBuffer {
    public:
        int id;
        int tmp_var_counter;

        std::string func_name;
        std::string func_ret_t;
        std::string func_params;

        // stack to help convert ast structure to 3-address code.
        std::vector<Stmt*> stmt_stack;
        // maps a unique variable id to its string representation.
        std::unordered_map<uint32_t , std::string> var_map;
        // maps bb_edge
        std::vector<std::pair<int, std::pair<int , EdgeKind>>> bb_edges;
        // map to help remap bb ids, to mark start as 1 and end as -1.
        std::unordered_map<unsigned, int32_t> remap_bb_ids;
        std::unordered_map<uint32_t , std::vector<std::string>> bb_stmts;

        TraversedInfoBuffer();
        void clear();

        // conversion to SPAN Strings
        std::string convertClangType(QualType qt);
        std::string convertFuncName(std::string func_name);
        std::string convertLocalVarName(std::string var_name);
        std::string convertGlobalVarName(std::string var_name);

        // SPAN IR Printing routines
        TraversedInfoBuffer& printSpanIr();
        // void printHeader();
        // void printVariables();
        // void printFunctions();
        // void printFooter();
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

std::string TraversedInfoBuffer::convertFuncName(std::string func_name) {
    std::string fname = "";
    fname += "f:";
    fname += func_name;
    return fname;
}

std::string TraversedInfoBuffer::convertGlobalVarName(std::string var_name) {
    std::string vname = "";
    vname += "v:";
    vname += var_name;
    return vname;
}

std::string TraversedInfoBuffer::convertLocalVarName(std::string var_name) {
    // assumes func_name is set
    std::string vname = "";
    vname += "v:";
    vname += func_name;
    vname += ":";
    vname += var_name;
    return vname;
}

// Converts Clang Types into SPAN parsable type strings:
// Examples,
// for `int` it returns `types.Int`.
// for `int*` it returns `types.Ptr(to=types.Int)`.
std::string TraversedInfoBuffer::convertClangType(QualType qt) {
   std::string span_t = "";
   const Type *type = qt.getTypePtr();
   if (type->isBuiltinType()) {
       if(type->isIntegerType()) {
           if(type->isCharType()) {
               span_t += "types.Char";
           } else {
               span_t += "types.Int";
           }
       } else if(type->isFloatingType()) {
           span_t += "types.Float";
       } else {
           span_t += "UnknownBuiltinType.";
       }
   } else if(type->isPointerType()) {
       span_t += "types.Ptr(to=";
       QualType pqt = type->getPointeeType();
       span_t += convertClangType(pqt);
       span_t += ")";
   } else {
       span_t += "UnknownType.";
   }

   return span_t;
} // convertClangType()

// clear the buffer for the next function.
void TraversedInfoBuffer::clear() {
    func_name = "";
    func_ret_t = "";
    func_params = "";

    stmt_stack.clear();
    var_map.clear();
    bb_edges.clear();
    bb_stmts.clear();
    remap_bb_ids.clear();
}

TraversedInfoBuffer& TraversedInfoBuffer::printSpanIr() {
    llvm::errs() << "Printing the TraversedInfoBuffer as SPAN IR:\n";
    llvm::errs() << "FuncName: " << func_name << "\n";
    llvm::errs() << convertBbEdges();
    return *this;
}

std::string TraversedInfoBuffer::convertBbEdges() {
    std::string span_bb_edges = "";
    for (auto p: bb_edges) {
        span_bb_edges += "graph.BbEdge(";
        span_bb_edges += std::to_string(p.first);
        span_bb_edges += ", ";
        span_bb_edges += std::to_string(p.second.first);
        span_bb_edges += ", ";
        switch(p.second.second) {
            case FalseEdge: {
                span_bb_edges += "graph.FalseEdge";
                break;
            }
            case TrueEdge: {
                span_bb_edges += "graph.TrueEdge";
                break;
            }
            case UnCondEdge: {
                span_bb_edges += "graph.UnCondEdge";
                break;
            }
        }
        span_bb_edges += "),\n";
    }
    return span_bb_edges;
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

        void handleDeclStmt(const Stmt *S, const CFGBlock *bb,
                            std::unordered_map<const Expr *, int> &visited_nodes) const;

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
        // print and clear the buffer after each function.
        tib.printSpanIr().clear();
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
    std::string Proto;
    llvm::errs() << "Params  : ";
    if (func_decl->doesThisDeclarationHaveABody()) { //& !func_decl->hasPrototype())
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
            //Proto += T.getAsString();
            Proto += tib.convertClangType(T);

            // Parameter name
            Proto += " \"";
            Proto += tib.convertLocalVarName(varDecl->getNameAsString());
            Proto += "\"";
        }
    }
    llvm::errs() << Proto << "\n";

    // STEP 1.3: Print function return type.
    const QualType returnQType = func_decl->getReturnType();
    llvm::errs() << "ReturnT : " << returnQType.getAsString() << "\n";
} // handleFunctionDef()

void SlangGenChecker::handleBBInfo(const CFGBlock *bb, const CFG *cfg) const {
    int32_t counter = 2;

    unsigned bb_id = bb->getBlockID();
    llvm::errs() << "BB" << bb_id << "\n";
    unsigned succ_id;

    if (bb == &cfg->getEntry()) {
        if (tib.remap_bb_ids.find(bb_id) != tib.remap_bb_ids.end()) {
            tib.remap_bb_ids[bb_id] = 1;
        }
    } else if (bb == &cfg->getExit()) {
        if (tib.remap_bb_ids.find(bb_id) != tib.remap_bb_ids.end()) {
            tib.remap_bb_ids[bb_id] = -1;
        }
    } else {
        if (tib.remap_bb_ids.find(bb_id) != tib.remap_bb_ids.end()) {
            tib.remap_bb_ids[bb_id] = counter;
            counter += 1;
        }
    }

    // access and record successor blocks
    const Stmt *terminator = (bb->getTerminator()).getStmt();
    if (terminator && isa<IfStmt>(terminator)) {
        bool true_edge = true;

        for (CFGBlock::const_succ_iterator I = bb->succ_begin();
             I != bb->succ_end(); ++I) {
            if (!true_edge) {
                llvm::errs() << "SPAN: ERROR: If has more than two successors.\n";
            }

            CFGBlock *succ = *I;
            succ_id = succ->getBlockID();
            if (tib.remap_bb_ids.find(succ_id) != tib.remap_bb_ids.end()) {
                tib.remap_bb_ids[succ_id] = counter;
                counter += 1;
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
                    llvm::errs() << "(Unreachable)";
                    continue;
                }

                succ_id = succ->getBlockID();
                if (tib.remap_bb_ids.find(succ_id) != tib.remap_bb_ids.end()) {
                    tib.remap_bb_ids[succ_id] = counter;
                    counter += 1;
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
                //stmt->dump();
                //llvm::errs() << "\n";
                break;
            }
            case Stmt::IntegerLiteralClass: {
                const IntegerLiteral *int_lit = cast<IntegerLiteral>(stmt);
                handleIntegerLiteral(int_lit);
                break;
            }
            // case Stmt::DeclStmtClass: {
            //     handleDeclStmt(stmt, bb, visited_nodes);
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

void SlangGenChecker::handleDeclStmt(const Stmt *S, const CFGBlock *bb,
                                 std::unordered_map<const Expr *, int>& visited_nodes) const {
    unsigned bb_id = bb->getBlockID();

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
        var_decl->dump();
        const NamedDecl *named_decl = cast<NamedDecl>(decl);
        const Expr *value = var_decl->getInit();

        if (value) {
            llvm::errs() << named_decl->getNameAsString() << " = ";
            handleBinaryOperator(value, visited_nodes, bb_id);
            llvm::errs() << "\n";
        }
    }
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
