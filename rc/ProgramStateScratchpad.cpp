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
#include <utility>                    // RC
#include <vector>                     //RC

using namespace clang;
using namespace ento;

// Rough struct for playing around with ProgramState
class FunctionSignature {
public:
  typedef std::pair<std::string, std::string> Parameter;
  typedef std::vector<Parameter> ParamVector;

  bool operator==(const FunctionSignature &FS) const { return id == FS.id; }

  void Profile(llvm::FoldingSetNodeID &ID) const { ID.AddInteger(1); }

  FunctionSignature(std::string &func_name, std::string &ret_type,
                    ParamVector &parameters) {
    function_name = func_name;
    return_type = ret_type;

    for (auto it = parameters.begin(); it != parameters.end(); it++) {
      param_list.push_back(*it);
    }
  }

  void log() {
    llvm::errs() << "Function name : " << function_name << "\n";
    llvm::errs() << "Return type : " << return_type << "\n";
    llvm::errs() << "Parameters : ";

    if (param_list.empty()) {
      llvm::errs() << "None"
                   << "\n";
    }

    else {
      llvm::errs() << "\n";
      for (auto it = param_list.begin(); it != param_list.end(); it++) {
        llvm::errs() << "  Parameter name : " << it->first << "\t";
        llvm::errs() << "  Parameter type : " << it->second << "\n\n";
      }
    }
  }

private:
  int id;
  std::string function_name;
  std::string return_type;
  std::vector<Parameter> param_list;
};

REGISTER_LIST_WITH_PROGRAMSTATE(SpanList, FunctionSignature)

//===----------------------------------------------------------------------===//
// MyCFGDumper
//===----------------------------------------------------------------------===//

namespace {
class MyCFGDumper : public Checker<check::ASTCodeBody, check::EndAnalysis> {
public:
  void checkASTCodeBody(const Decl *D, AnalysisManager &mgr,
                        BugReporter &BR) const {
    llvm::errs() << "----------------\n";

    static int count = 1;

    PrintingPolicy Policy(mgr.getLangOpts());
    Policy.TerseOutput = false;
    Policy.PolishForDeclaration = true;
    D->print(llvm::errs(), Policy);

    llvm::errs() << "--------\n";

    const FunctionDecl *func_decl = dyn_cast<FunctionDecl>(D);
    std::string function_name = func_decl->getNameInfo().getAsString();

    typedef std::pair<std::string, std::string> Parameter;
    typedef std::vector<Parameter> ParamVector;

    // first element in pair is param name, second is it's type

    ParamVector params;
    if (func_decl
            ->doesThisDeclarationHaveABody()) { //& !func_decl->hasPrototype())
                                                //{
      for (unsigned i = 0, e = func_decl->getNumParams(); i != e; ++i) {

        const ParmVarDecl *paramVarDecl = func_decl->getParamDecl(i);
        const VarDecl *varDecl = dyn_cast<VarDecl>(paramVarDecl);

        // Parameter type
        QualType T =
            varDecl->getTypeSourceInfo()
                ? varDecl->getTypeSourceInfo()->getType()
                : varDecl->getASTContext().getUnqualifiedObjCPointerType(
                      varDecl->getType());

        params.push_back(
            std::make_pair(varDecl->getNameAsString(), T.getAsString()));
      }
    }

    const QualType returnQType = func_decl->getReturnType();
    std::string return_type = returnQType.getAsString();

    FunctionSignature FS(function_name, return_type, params);

    // TODO: Somehow get access to current state
    ProgramStateRef current_state;
    const FunctionSignature &FSRef = FS;
    FS.log();
    ProgramStateRef new_state = current_state->add<SpanList>(FSRef);

    // STEP 2: Print the CFG.
    // if (CFG *cfg = mgr.getCFG(D)) {
    //   for (auto bb : *cfg) {
    //     unsigned bb_id = bb->getBlockID();
    //     llvm::errs() << "BB" << bb_id << ":\n";
    //     for (auto elem : *bb) {
    //       // ref: https://clang.llvm.org/doxygen/CFG_8h_source.html#l00056
    //       // ref for printing block:
    //       // https://clang.llvm.org/doxygen/CFG_8cpp_source.html#l05234
    //     }
    //   }
    // }
  }
};
} // anonymous namespace

void ento::registerMyCFGDumper(CheckerManager &mgr) {
  mgr.registerChecker<MyCFGDumper>();
}
