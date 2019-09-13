//===----------------------------------------------------------------------===//
//  MIT License.
//  Copyright (c) 2019 The SLANG Authors.
//
//  Author: Ronak Chauhan (r.chauhan@somaiya.edu)
//  Author: Anshuman Dhuliya [AD] (dhuliya@cse.iitb.ac.in)
//
//  If SlangGenAstChecker class name is added or changed, then also edit,
//  ../../../../include/clang/StaticAnalyzer/Checkers/Checkers.td
//  if this checker is named `SlangGenAst` (in Checkers.td) then it can be used as follows,
//
//      clang --analyze -Xanalyzer -analyzer-checker=debug.slanggen test.c |& tee mylog
//
//  which generates the file `test.c.spanir`.
//===----------------------------------------------------------------------===//

//#include "ClangSACheckers.h"
#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
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

#include "SlangUtil.h"

using namespace slang;
using namespace clang;
using namespace ento;

typedef std::vector<const Stmt *> StmtVector;

// non-breaking space
#define NBSP1 " "
#define NBSP2 NBSP1 NBSP1
#define NBSP4 NBSP2 NBSP2
#define NBSP6 NBSP2 NBSP4
#define NBSP8 NBSP4 NBSP4
#define NBSP10 NBSP4 NBSP6
#define NBSP12 NBSP6 NBSP6

#define VAR_NAME_PREFIX "v:"
#define FUNC_NAME_PREFIX "f:"

#define DONT_PRINT "DONT_PRINT"
#define NULL_STMT "NULL_STMT"

#define LABEL_PREFIX "instr.LabelI(\""
#define LABEL_SUFFIX "\")"

// Generate the SPAN IR from Clang AST.
namespace {
// the numbering 0,1,2 is important.
enum EdgeLabel { FalseEdge = 0, TrueEdge = 1, UnCondEdge = 2 };
enum SlangRecordKind { Struct = 0, Union = 1 };

class SlangExpr {
public:
  std::string expr;
  bool compound;
  std::string locStr;
  QualType qualType;
  bool nonTmpVar;
  uint64_t varId;

  SlangExpr() {
    expr = "";
    compound = false;
    locStr = "";
    qualType = QualType();
    nonTmpVar = true;
    varId = 0;
  };

  std::string toString() {
    std::stringstream ss;
    ss << "SlangExpr:\n";
    ss << "  Expr     : " << expr << "\n";
    ss << "  ExprType : " << qualType.getAsString() << "\n";
    ss << "  NonTmpVar: " << (nonTmpVar ? "true" : "false") << "\n";
    ss << "  VarId    : " << varId << "\n";

    return ss.str();
  }
};

class SlangVar {
public:
  uint64_t id;
  // variable name: e.g. a variable 'x' in main function, is "v:main:x".
  std::string name;
  std::string typeStr;

  SlangVar() {}

  SlangVar(uint64_t id, std::string name) {
    // specially for anonymous member names (needed in member expressions)
    this->id = id;
    this->name = name;
    this->typeStr = DONT_PRINT;
  }

  std::string convertToString() {
    std::stringstream ss;
    ss << "\"" << name << "\": " << typeStr << ",";
    return ss.str();
  }

  void setLocalVarName(std::string varName, std::string funcName) {
    name = VAR_NAME_PREFIX;
    name += funcName + ":" + varName;
  }

  void setGlobalVarName(std::string varName) {
    name = VAR_NAME_PREFIX;
    name += varName;
  }
}; // class SlangVar

// holds info of a single function
class SlangFunc {
public:
  std::string name;     // e.g. 'main'
  std::string fullName; // e.g. 'f:main'
  std::string retType;
  std::vector<std::string> paramNames;
  bool variadic;

  uint32_t tmpVarCount;
  const Stmt *lastDeclStmt;

  std::vector<std::string> spanStmts;

  SlangFunc() {
    variadic = false;
    paramNames = std::vector<std::string>{};
    tmpVarCount = 0;
  }
}; // class SlangFunc

class SlangRecord;

class SlangRecordField {
public:
  bool anonymous;
  std::string name;
  std::string typeStr;
  SlangRecord *slangRecord;
  QualType type;

  SlangRecordField() : anonymous{false}, name{""}, typeStr{""}, type{QualType()} {}

  std::string getName() const { return name; }

  std::string toString() {
    std::stringstream ss;
    ss << "("
       << "\"" << name << "\"";
    ss << ", " << typeStr << ")";
    return ss.str();
  }

  void clear() {
    anonymous = false;
    name = "";
    typeStr = "";
    type = QualType();
  }
}; // class SlangRecordField

// holds a struct or a union record
class SlangRecord {
public:
  SlangRecordKind recordKind; // Struct, or Union
  bool anonymous;
  std::string name;
  std::vector<SlangRecordField> members;
  std::string locStr;
  int32_t nextAnonymousFieldId;

  SlangRecord() {
    recordKind = Struct; // Struct, or Union
    anonymous = false;
    name = "";
    nextAnonymousFieldId = 0;
  }

  std::string getNextAnonymousFieldIdStr() {
    std::stringstream ss;
    nextAnonymousFieldId += 1;
    ss << nextAnonymousFieldId;
    return ss.str();
  }

  std::vector<SlangRecordField> getFields() const { return members; }

  std::string genMemberExpr(std::vector<uint32_t> indexVector) {
    std::stringstream ss;

    std::vector<std::string> members;
    SlangRecord *currentRecord = this;
    llvm::errs() << "\n------------------------\n" << currentRecord->members.size() << "\n";
    llvm::errs() << "\n------------------------\n" << indexVector.size() << "\n";
    llvm::errs() << "\n------------------------\n" << indexVector[0] << indexVector[1] << "\n";
    llvm::errs().flush();
    for (auto it = indexVector.begin(); it != indexVector.end(); ++it) {
      members.push_back(currentRecord->members[*it].name);
      if (currentRecord->members[*it].slangRecord != nullptr) {
        // means its a member of type record
        currentRecord = currentRecord->members[*it].slangRecord;
      }
    }

    std::string prefix = "";
    for (auto it = members.end()-1; it != members.begin()-1; --it) {
      ss << prefix << "expr.MemberE(\"" << *it << "\"";
      if (prefix == "") {
        prefix = ", ";
      }
    }

    return ss.str();
  }

  std::string toString() {
    std::stringstream ss;
    ss << NBSP6;
    ss << ((recordKind == Struct) ? "types.Struct(\n" : "types.Union(\n");

    ss << NBSP8 << "name = ";
    ss << "\"" << name << "\""
       << ",\n";

    std::string suffix = ",\n";
    ss << NBSP8 << "members = [\n";
    for (auto member : members) {
      ss << NBSP10 << member.toString() << suffix;
    }
    ss << NBSP8 << "],\n";

    ss << NBSP8 << "loc = " << locStr << ",\n";
    ss << NBSP6 << ")"; // close types.*(...

    return ss.str();
  }

  std::string toShortString() {
    std::stringstream ss;

    if (recordKind == Struct) {
      ss << "types.Struct";
    } else {
      ss << "types.Union";
    }
    ss << "(\"" << name << "\")";

    return ss.str();
  }
}; // class SlangRecord

// holds details of the entire translation unit
class SlangTranslationUnit {
public:
  uint64_t uniqueId;
  std::string fileName; // the current translation unit file name
  SlangFunc *currFunc;  // the current function being translated

  uint32_t labelCount;

  // to uniquely name anonymous records (see getNextRecordId())
  int32_t recordId;

  // maps a unique variable id to its SlangVar.
  std::unordered_map<uint64_t, SlangVar> varMap;
  // map of var-name to a count:
  // used in case two local variables have same name (blocks)
  std::unordered_map<std::string, uint64_t> varCountMap;
  // contains functions
  std::unordered_map<uint64_t, SlangFunc> funcMap;
  // contains structs
  std::unordered_map<uint64_t, SlangRecord> recordMap;

  // tracks variables that become dirty in an expression
  std::unordered_map<uint64_t, SlangExpr> dirtyVars;

  // vector of start and exit label of constructs which can contain break and continue stmts.
  std::vector<std::pair<std::string, std::string>> entryExitLabels;

  void pushLabels(std::string entry, std::string exit) {
    auto labelPair = std::make_pair(entry, exit);
    entryExitLabels.push_back(labelPair);
  }

  void popLabel() {
    entryExitLabels.pop_back();
  }

  std::pair<std::string, std::string>& peekLabel() {
    return entryExitLabels[entryExitLabels.size()-1];
  }

  std::string peekEntryLabel() {
    return entryExitLabels[entryExitLabels.size()-1].first;
  }

  std::string peekExitLabel() {
    return entryExitLabels[entryExitLabels.size()-1].second;
  }

  SlangTranslationUnit()
      : uniqueId{0}, fileName{}, currFunc{nullptr}, recordId{0}, varMap{}, varCountMap{}, funcMap{}, dirtyVars{} {
  }

  // clear the buffer for the next function.
  void clear() {
    varMap.clear();
    dirtyVars.clear();
    varCountMap.clear();
  } // clear()

  uint32_t genNextLabelCount() {
    labelCount += 1;
    return labelCount;
  }

  std::string genNextLabelCountStr() {
    std::stringstream ss;
    ss << genNextLabelCount();
    return ss.str();
  }

  void addStmt(std::string spanStmt) { currFunc->spanStmts.push_back(spanStmt); }

  void pushBackFuncParams(std::string paramName) {
    SLANG_TRACE("AddingParam: " << paramName << " to func " << currFunc->name)
    currFunc->paramNames.push_back(paramName);
  }

  void setFuncReturnType(std::string &retType) { currFunc->retType = retType; }

  void setVariadicness(bool variadic) { currFunc->variadic = variadic; }

  std::string getCurrFuncName() {
    return currFunc->name; // not fullName
  }

  SlangVar &getVar(uint64_t varAddr) {
    // FIXME: there is no check
    return varMap[varAddr];
  }

  void setLastDeclStmtTo(const Stmt *declStmt) { currFunc->lastDeclStmt = declStmt; }

  const Stmt *getLastDeclStmt() const { return currFunc->lastDeclStmt; }

  bool isNewVar(uint64_t varAddr) { return varMap.find(varAddr) == varMap.end(); }

  uint32_t nextTmpId() {
    currFunc->tmpVarCount += 1;
    return currFunc->tmpVarCount;
  }

  uint64_t nextUniqueId() {
    uniqueId += 1;
    return uniqueId;
  }

  void addVar(uint64_t varId, SlangVar &slangVar) { varMap[varId] = slangVar; }

  bool isRecordPresent(uint64_t recordAddr) {
    return !(recordMap.find(recordAddr) == recordMap.end());
  }

  void addRecord(uint64_t recordAddr, SlangRecord slangRecord) {
    recordMap[recordAddr] = slangRecord;
  }

  SlangRecord &getRecord(uint64_t recordAddr) { return recordMap[recordAddr]; }

  int32_t getNextRecordId() {
    recordId += 1;
    return recordId;
  }

  std::string getNextRecordIdStr() {
    std::stringstream ss;
    ss << getNextRecordId();
    return ss.str();
  }

  std::string convertFuncName(std::string funcName) {
    std::stringstream ss;
    ss << FUNC_NAME_PREFIX << funcName;
    return ss.str();
  }

  std::string convertVarExpr(uint64_t varAddr) {
    // if here, var should already be in varMap
    std::stringstream ss;

    auto slangVar = varMap[varAddr];
    ss << slangVar.name;

    return ss.str();
  }

  // BOUND START: dump_routines (to SPAN Strings)

  // dump entire span ir module for the translation unit.
  void dumpSlangIr() {
    std::stringstream ss;

    dumpHeader(ss);
    dumpVariables(ss);
    dumpObjs(ss);
    dumpFooter(ss);

    // TODO: print the content to a file.
    std::string fileName = this->fileName + ".spanir";
    Util::writeToFile(fileName, ss.str());
    llvm::errs() << ss.str();
  } // dumpSlangIr()

  void dumpHeader(std::stringstream &ss) {
    ss << "\n";
    ss << "# START: A_SPAN_translation_unit.\n";
    ss << "\n";
    ss << "# eval() the contents of this file.\n";
    ss << "# Keep the following imports in effect when calling eval.\n";
    ss << "\n";
    ss << "# import span.ir.types as types\n";
    ss << "# import span.ir.op as op\n";
    ss << "# import span.ir.expr as expr\n";
    ss << "# import span.ir.instr as instr\n";
    ss << "# import span.ir.constructs as constructs\n";
    ss << "# import span.ir.tunit as tunit\n";
    ss << "# from span.ir.types import Loc\n";
    ss << "\n";
    ss << "# An instance of span.ir.tunit.TranslationUnit class.\n";
    ss << "tunit.TranslationUnit(\n";
    ss << NBSP2 << "name = \"" << fileName << "\",\n";
    ss << NBSP2 << "description = \"Auto-Translated from Clang AST.\",\n";
  } // dumpHeader()

  void dumpFooter(std::stringstream &ss) {
    ss << ") # tunit.TranslationUnit() ends\n";
    ss << "\n# END  : A_SPAN_translation_unit.\n";
  } // dumpFooter()

  void dumpVariables(std::stringstream &ss) {
    ss << "\n";
    ss << NBSP2 << "allVars = {\n";
    for (const auto &var : varMap) {
      if (var.second.typeStr == DONT_PRINT)
        continue;
      ss << NBSP4;
      ss << "\"" << var.second.name << "\": " << var.second.typeStr << ",\n";
    }
    ss << NBSP2 << "}, # end allVars dict\n\n";
  } // dumpVariables()

  void dumpObjs(std::stringstream &ss) {
    ss << NBSP2 << "allConstructs = {\n";
    dumpRecords(ss);
    dumpFunctions(ss);
    ss << NBSP2 << "}, # end allConstructs dict\n";
  }

  void dumpRecords(std::stringstream &ss) {
    for (auto slangRecord : recordMap) {
      ss << NBSP4;
      ss << "\"" << slangRecord.second.name << "\":\n";
      ss << slangRecord.second.toString();
      ss << ",\n\n";
    }
    ss << "\n";
  }

  void dumpFunctions(std::stringstream &ss) {
    std::string prefix;
    for (auto slangFunc : funcMap) {
      ss << NBSP4; // indent
      ss << "\"" << slangFunc.second.fullName << "\":\n";
      ss << NBSP6 << "constructs.Func(\n";

      // members
      ss << NBSP8 << "name = "
         << "\"" << slangFunc.second.fullName << "\",\n";
      ss << NBSP8 << "paramNames = [";
      prefix = "";
      for (std::string &paramName : slangFunc.second.paramNames) {
        ss << prefix << "\"" << paramName << "\"";
        if (prefix.size() == 0) {
          prefix = ", ";
        }
      }
      ss << "],\n";
      ss << NBSP8 << "variadic = " << (slangFunc.second.variadic ? "True" : "False") << ",\n";

      ss << NBSP8 << "returnType = " << slangFunc.second.retType << ",\n";

      // member: basicBlocks
      ss << "\n";
      ss << NBSP8 << "# Note: -1 is always start/entry BB. (REQUIRED)\n";
      ss << NBSP8 << "# Note: 0 is always end/exit BB (REQUIRED)\n";
      ss << NBSP8 << "instrSeq = [\n";
      for (auto insn : slangFunc.second.spanStmts) {
        ss << NBSP12 << insn << ",\n";
      }
      ss << NBSP8 << "], # instrSeq end.\n";

      // close this function object
      ss << NBSP6 << "), # " << slangFunc.second.fullName << "() end. \n\n";
    }
  } // dumpFunctions()

  // BOUND END  : dump_routines (to SPAN Strings)

}; // class SlangTranslationUnit

class SlangGenAstChecker : public Checker<check::ASTCodeBody, check::EndOfTranslationUnit> {

  // static_members initialized
  static SlangTranslationUnit stu;
  static const FunctionDecl *FD; // funcDecl

public:
  // BOUND START: top_level_routines

  // mainentry, main entry point. Invokes top level Function and Cfg handlers.
  // It is invoked once for each source translation unit function.
  void checkASTCodeBody(const Decl *D, AnalysisManager &mgr, BugReporter &BR) const {
    SLANG_EVENT("BOUND START: SLANG_Generated_Output.\n")

    // SLANG_DEBUG("slang_add_nums: " << slang_add_nums(1,2) << "only\n"; // lib testing
    if (stu.fileName.size() == 0) {
      stu.fileName = D->getASTContext().getSourceManager().getFilename(D->getBeginLoc()).str();
    }

    FD = dyn_cast<FunctionDecl>(D);
    if (FD) {
      FD = FD->getCanonicalDecl();
      FD = handleFuncNameAndType(FD, true);
      stu.currFunc = &stu.funcMap[(uint64_t) FD];
      SLANG_DEBUG("Current Function: " << stu.currFunc->name << " " << (uint64_t)FD->getCanonicalDecl())
      handleFunctionBody(FD);
    } else {
      SLANG_ERROR("Decl is not a Function")
    }
  } // checkASTCodeBody()

  // invoked when the whole translation unit has been processed
  void checkEndOfTranslationUnit(const TranslationUnitDecl *TU, AnalysisManager &Mgr,
                                 BugReporter &BR) const {
    stu.dumpSlangIr();
    SLANG_EVENT("Translation Unit Ended.\n")
    SLANG_EVENT("BOUND END  : SLANG_Generated_Output.\n")
  } // checkEndOfTranslationUnit()

  // BOUND END  : top_level_routines

  // BOUND START: handling_routines

  void handleFunctionBody(const FunctionDecl *funcDecl) const {
    const Stmt *body = funcDecl->getBody();
    if (body) {
      convertStmt(body);
    } else {
      SLANG_ERROR("No body for function: " << funcDecl->getNameAsString())
    }
  }

  // records the function details
  const FunctionDecl* handleFuncNameAndType(const FunctionDecl *funcDecl,
      bool force=false) const {
    const FunctionDecl *realFuncDecl = funcDecl;

    if (funcDecl->isDefined()) {
      funcDecl = funcDecl->getDefinition();
      realFuncDecl = funcDecl;
      // funcDecl = funcDecl->getCanonicalDecl();
    }

    if (stu.funcMap.find((uint64_t)funcDecl) == stu.funcMap.end() || force) {
      // if here, function not already present. Add its details.

      SlangFunc slangFunc{};
      slangFunc.name = funcDecl->getNameInfo().getAsString();
      slangFunc.fullName = stu.convertFuncName(slangFunc.name);
      SLANG_DEBUG("AddingFunction: " << slangFunc.name << " " << (uint64_t)funcDecl\
      << " " << funcDecl->isDefined() << " " << (uint64_t)funcDecl->getCanonicalDecl())


      // STEP 1.2: Get function parameters.
      // if (funcDecl->doesThisDeclarationHaveABody()) { //& !funcDecl->hasPrototype())
      for (unsigned i = 0, e = funcDecl->getNumParams(); i != e; ++i) {
        const ParmVarDecl *paramVarDecl = funcDecl->getParamDecl(i);
        handleValueDecl(paramVarDecl, slangFunc.name); // adds the var too
        slangFunc.paramNames.push_back(stu.getVar((uint64_t)paramVarDecl).name);
      }
      slangFunc.variadic = funcDecl->isVariadic();

      // STEP 1.3: Get function return type.
      slangFunc.retType = convertClangType(funcDecl->getReturnType());

      // STEP 2: Copy the function to the map.
      stu.funcMap[(uint64_t)funcDecl] = slangFunc;
    }

    return realFuncDecl;
  } // handleFunction()

  void handleFuncDecl(const FunctionDecl *funcDecl) {

  } // handleFuncDecl()

  // record the variable name and type
  void handleValueDecl(const ValueDecl *valueDecl, std::string funcName) const {
    uint64_t varAddr = (uint64_t)valueDecl;
    const VarDecl *varDecl = dyn_cast<VarDecl>(valueDecl);

    std::string varName;
    if (varDecl) {
      if (stu.isNewVar(varAddr)) {
        // seeing the variable for the first time.
        SlangVar slangVar{};
        slangVar.id = varAddr;

        varName = valueDecl->getNameAsString();

        slangVar.typeStr = convertClangType(valueDecl->getType());
        SLANG_DEBUG("NEW_VAR: " << slangVar.convertToString())

        if (varName == "") {
          // used only to name anonymous function parameters
          varName = Util::getNextUniqueIdStr() + "param";
        }

        if (varDecl->hasLocalStorage()) {
          slangVar.setLocalVarName(varName, funcName);
          if (stu.varCountMap.find(slangVar.name) != stu.varCountMap.end()) {
            stu.varCountMap[slangVar.name]++;
            uint64_t newVarId = stu.varCountMap[slangVar.name];
            slangVar.setLocalVarName(std::to_string(newVarId) + "D" + varName, funcName);
          } else {
            stu.varCountMap[slangVar.name] = 1;
          }
        } else if (varDecl->hasGlobalStorage()) {
          slangVar.setGlobalVarName(varName);
        } else if (varDecl->hasExternalStorage()) {
          SLANG_ERROR("External Storage Not Handled.")
        } else {
          SLANG_ERROR("Unknown variable storage.")
        }

        stu.addVar(slangVar.id, slangVar);

        if (valueDecl->getType()->isArrayType()) {
          auto arrayType = valueDecl->getType()->getAsArrayTypeUnsafe();
          if (isa<VariableArrayType>(arrayType)) {
            SlangExpr varExpr = convertVariable(varDecl, getLocationString(valueDecl));
            SlangExpr sizeExpr = convertVarArrayVariable(valueDecl->getType(),
                                                         arrayType->getElementType());

            SlangExpr allocExpr;
            std::stringstream ss;
            ss << "expr.AllocE(" << sizeExpr.expr;
            ss << ", " << getLocationString(valueDecl) << ")";
            allocExpr.expr = ss.str();
            allocExpr.qualType = FD->getASTContext().VoidPtrTy;
            allocExpr.locStr = getLocationString(valueDecl);
            allocExpr.compound = true;

            SlangExpr tmpVoidPtr = convertToTmp(allocExpr);

            SlangExpr castExpr;
            ss.str("");
            ss << "expr.CastE(" << tmpVoidPtr.expr;
            ss << ", op.CastOp(" << convertClangType(valueDecl->getType()) << ")";
            ss << ", " << getLocationString(valueDecl) << ")";
            castExpr.expr = ss.str();
            castExpr.qualType = valueDecl->getType();
            castExpr.compound = true;
            castExpr.locStr = getLocationString(valueDecl);

            addAssignInstr(varExpr, castExpr, getLocationString(valueDecl));
          }
        }

        // check if it has a initialization body
        if (varDecl->hasInit()) {
          // yes it has, so initialize it
          if (varDecl->getInit()->getStmtClass() == Stmt::InitListExprClass) {
            // TODO: uncomment when initializer list is fully supported
            // std::vector<uint32_t> indexVector;
            // convertInitListExpr(slangVar, cast<InitListExpr>(varDecl->getInit()),
            //    varDecl, indexVector);

          } else {
            if (varDecl->hasLocalStorage()) {
              SlangExpr slangExpr = convertStmt(varDecl->getInit());
              std::string locStr = getLocationString(valueDecl);
              std::stringstream ss;
              ss << "instr.AssignI(";
              ss << "expr.VarE(\"" << slangVar.name << "\"";
              ss << ", " << locStr << ")"; // close expr.VarE(...
              ss << ", " << slangExpr.expr;
              ss << ", " << locStr << ")"; // close instr.AssignI(...
              stu.addStmt(ss.str());
            }
          }
        }
      }

    } else if(valueDecl->getAsFunction()) {
      handleFuncNameAndType(valueDecl->getAsFunction());

    } else {
      SLANG_ERROR("ValueDecl not a VarDecl or FunctionDecl!")
      valueDecl->dump(); //delit
    }
  } // handleValueDecl()

  void handleDeclStmt(const DeclStmt *declStmt) const {
    stu.setLastDeclStmtTo(declStmt);
    SLANG_DEBUG("Set last DeclStmt to DeclStmt at " << (uint64_t)(declStmt));

    std::stringstream ss;
    std::string locStr = getLocationString(declStmt);

    for (auto it = declStmt->decl_begin(); it != declStmt->decl_end(); ++it) {
      if (isa<VarDecl>(*it)) {
        handleValueDecl(cast<ValueDecl>(*it), stu.currFunc->name);
      }
    }
  } // handleDeclStmt()

  // BOUND END  : handling_routines

  // BOUND START: conversion_routines

  // stmtconversion
  SlangExpr convertStmt(const Stmt *stmt) const {
    SlangExpr slangExpr;

    if (!stmt) { return slangExpr; }

    SLANG_DEBUG("ConvertingStmt : " << stmt->getStmtClassName() << "\n")
    stmt->dump();

    switch (stmt->getStmtClass()) {
    case Stmt::BreakStmtClass:
      return convertBreakStmt(cast<BreakStmt>(stmt));

    case Stmt::ContinueStmtClass:
      return convertContinueStmt(cast<ContinueStmt>(stmt));

    case Stmt::LabelStmtClass:
      return convertLabel(cast<LabelStmt>(stmt));

    case Stmt::ConditionalOperatorClass:
      return convertConditionalOp(cast<ConditionalOperator>(stmt));

    case Stmt::IfStmtClass:
      return convertIfStmt(cast<IfStmt>(stmt));

    case Stmt::WhileStmtClass:
      return convertWhileStmt(cast<WhileStmt>(stmt));

    case Stmt::DoStmtClass:
      return convertDoStmt(cast<DoStmt>(stmt));

    case Stmt::ForStmtClass:
      return convertForStmt(cast<ForStmt>(stmt));

    case Stmt::UnaryOperatorClass:
      return convertUnaryOperator(cast<UnaryOperator>(stmt));

    case Stmt::CompoundAssignOperatorClass:
    case Stmt::BinaryOperatorClass:
      return convertBinaryOperator(cast<BinaryOperator>(stmt));

    case Stmt::ParenExprClass:
      return convertParenExpr(cast<ParenExpr>(stmt));

    case Stmt::CompoundStmtClass:
      return convertCompoundStmt(cast<CompoundStmt>(stmt));

    case Stmt::DeclStmtClass:
      handleDeclStmt(cast<DeclStmt>(stmt)); break;

    case Stmt::DeclRefExprClass:
      return convertDeclRefExpr(cast<DeclRefExpr>(stmt));

    case Stmt::ConstantExprClass:
      return convertConstantExpr(cast<ConstantExpr>(stmt));

    case Stmt::IntegerLiteralClass:
      return convertIntegerLiteral(cast<IntegerLiteral>(stmt));

    case Stmt::CharacterLiteralClass:
      return convertCharacterLiteral(cast<CharacterLiteral>(stmt));

    case Stmt::FloatingLiteralClass:
      return convertFloatingLiteral(cast<FloatingLiteral>(stmt));

    case Stmt::StringLiteralClass:
      return convertStringLiteral(cast<StringLiteral>(stmt));

    case Stmt::ImplicitCastExprClass:
      return convertImplicitCastExpr(cast<ImplicitCastExpr>(stmt));

    case Stmt::ReturnStmtClass:
      return convertReturnStmt(cast<ReturnStmt>(stmt));

    case Stmt::SwitchStmtClass:
      return convertSwitchStmt(cast<SwitchStmt>(stmt));

    case Stmt::GotoStmtClass:
      return convertGotoStmt(cast<GotoStmt>(stmt));

    case Stmt::CStyleCastExprClass:
      return convertCStyleCastExpr(cast<CStyleCastExpr>(stmt));

    case Stmt::MemberExprClass:
      return convertMemberExpr(cast<MemberExpr>(stmt));

    case Stmt::ArraySubscriptExprClass:
      return convertArraySubscriptExpr(cast<ArraySubscriptExpr>(stmt));

    case Stmt::UnaryExprOrTypeTraitExprClass:
      return convertUnaryExprOrTypeTraitExpr(cast<UnaryExprOrTypeTraitExpr>(stmt));    

    case Stmt::CallExprClass:
      return convertCallExpr(cast<CallExpr>(stmt));

    case Stmt::CaseStmtClass:
      // we manually handle case stmt when we handle switch stmt
      break;

    case Stmt::NullStmtClass: // just a ";"
      stu.addStmt("instr.NopI(" + getLocationString(stmt) + ")");
      break;

    default:
      SLANG_ERROR("Unhandled_Stmt: " << stmt->getStmtClassName())
      stmt->dump();
      break;
    }

    slangExpr.expr = "Unknown";
    return slangExpr;
  } // convertStmt()

  SlangExpr convertVarArrayVariable(QualType valueType, QualType elementType) const {
    const Type *elemTypePtr = elementType.getTypePtr();
    const VariableArrayType *varArrayType =
        cast<VariableArrayType>(valueType.getTypePtr()->getAsArrayTypeUnsafe());

    if (elemTypePtr->isArrayType()) {
      // it will definitely be a VarArray Type (since this func is called)
      SlangExpr tmpSubArraySize = convertVarArrayVariable(elementType,
          elemTypePtr->getAsArrayTypeUnsafe()->getElementType());

      SlangExpr thisVarArrSizeExpr = convertToTmp(
          convertStmt(varArrayType->getSizeExpr()));

      SlangExpr sizeOfThisVarArrExpr = convertToTmp(createBinaryExpr(thisVarArrSizeExpr,
          "op.BO_MUL", tmpSubArraySize, thisVarArrSizeExpr.locStr));

      SlangExpr tmpThisArraySize = convertToTmp(sizeOfThisVarArrExpr);
      return tmpThisArraySize;

    } else {
      // a non-array element type
      TypeInfo typeInfo = FD->getASTContext().getTypeInfo(elementType);
      uint64_t size = typeInfo.Width / 8;

      SlangExpr thisVarArrSizeExpr = convertToTmp(
          convertStmt(varArrayType->getSizeExpr()));

      SlangExpr sizeOfInnerNonVarArrType;
      std::stringstream ss;
      ss << "expr.LitE(" << size;
      ss << ", " << thisVarArrSizeExpr.locStr << ")";
      sizeOfInnerNonVarArrType.expr = ss.str();
      sizeOfInnerNonVarArrType.qualType = FD->getASTContext().UnsignedIntTy;
      sizeOfInnerNonVarArrType.locStr = thisVarArrSizeExpr.locStr;

      SlangExpr sizeOfThisVarArrExpr = convertToTmp(
          createBinaryExpr(thisVarArrSizeExpr,
              "op.BO_MUL", sizeOfInnerNonVarArrType, thisVarArrSizeExpr.locStr));

      SlangExpr tmpThisArraySize = convertToTmp(sizeOfThisVarArrExpr);
      return tmpThisArraySize;
    }
  } // convertVarArrayVariable()

  SlangExpr convertInitListExpr(SlangVar& slangVar, const InitListExpr *initListExpr,
      const VarDecl *varDecl, std::vector<uint32_t>& indexVector) const {
    uint32_t index = 0;
    for (auto it = initListExpr->begin(); it != initListExpr->end(); ++it) {
      const Stmt *stmt = *it;
      if (stmt->getStmtClass() == Stmt::InitListExprClass) {
          // && isCompoundTypeAt(varDecl, indexVector)) {
        indexVector.push_back(index);
        convertInitListExpr(slangVar, cast<InitListExpr>(stmt), varDecl, indexVector);
        indexVector.pop_back();
      } else {
        SlangExpr rhs = convertToTmp(convertStmt(stmt));

        indexVector.push_back(index);
        SlangExpr lhs = genInitLhsExpr(slangVar, varDecl, indexVector);
        indexVector.pop_back();

        addAssignInstr(lhs, rhs, getLocationString(stmt));
      }
      index += 1;
    }

    return SlangExpr{};
  } // convertInitListExpr()

  // checks if the
  bool isCompoundTypeAt(const VarDecl *varDecl,
      std::vector<int>& indexVector) const {
    // TODO
    return true;
  }

  // used to generate lhs (lvalue) for initializer lists like
  // int arr[][2] = {{1, 2}, {3, 4}, {5, 6}}; // for each element
  // it also works for the struct types
  SlangExpr genInitLhsExpr(SlangVar& slangVar,
      const VarDecl *varDecl, std::vector<uint32_t>& indexVector) const {
    SlangExpr slangExpr;
    std::stringstream ss;

    std::string prefix = "";
    if (varDecl->getType()->isArrayType()) {
      for (auto it = indexVector.end()-1; it != indexVector.begin()-1; --it) {
        ss << prefix << "expr.ArrayE(" << *it;
        if (prefix == "") {
          prefix = ", ";
        }
      }

      ss << ", expr.VarE(\"" << slangVar.name << "\"";
      ss << ", " << getLocationString(varDecl) << ")";

      for (auto it = indexVector.begin(); it != indexVector.end(); ++it) {
        ss << ", " << getLocationString(varDecl) << ")";
      }

      slangExpr.expr = ss.str();
      slangExpr.compound = true;
      slangExpr.qualType = varDecl->getType();
      slangExpr.locStr = getLocationString(varDecl);
    } else {
      // must be a record type
      auto type = varDecl->getType();
      const RecordDecl *recordDecl;

      if (type->isStructureType()) {
        recordDecl = type->getAsStructureType()->getDecl();
      } else {
        // must be a union then
        recordDecl = type->getAsUnionType()->getDecl();
      }

      std::string memberListStr =
          stu.getRecord((uint64_t)recordDecl).genMemberExpr(indexVector);

      ss << memberListStr;
      ss << ", expr.VarE(\"" << slangVar.name << "\"";
      ss << ", " << getLocationString(varDecl) << ")";

      for (auto it = indexVector.begin(); it != indexVector.end(); ++it) {
        ss << ", " << getLocationString(varDecl) << ")";
      }

      slangExpr.expr = ss.str();
      slangExpr.compound = true;
      slangExpr.qualType = varDecl->getType();
      slangExpr.locStr = getLocationString(varDecl);
    }

    return slangExpr;
  } // genInitLhsExpr()

  // guaranteed to be a comma operator
  SlangExpr convertBinaryCommaOp(const BinaryOperator *binOp) const {
    auto it = binOp->child_begin();
    const Stmt *leftOprnd = *it;
    ++it;
    const Stmt *rightOprnd = *it;

    convertStmt(leftOprnd);

    SlangExpr rightExpr = convertToTmp(convertStmt(rightOprnd));

    return rightExpr;
  } // convertBinaryCommaOp()

  SlangExpr convertCallExpr(const CallExpr *callExpr) const {
    SlangExpr slangExpr;

    auto it = callExpr->child_begin();

    const Stmt *callee = *it;
    SlangExpr calleeExpr = convertToTmp(convertStmt(callee));

    std::vector<const Stmt*> args;
    ++it; // skip the callee expression
    for (; it != callExpr->child_end(); ++it) {
      args.push_back(*it);
    }

    std::stringstream ss;
    ss << "expr.CallE(" << calleeExpr.expr;
    if (args.size()) {
      std::string prefix = "";
      ss << ", [";
      for (auto argIter = args.begin(); argIter != args.end(); ++argIter) {
        SlangExpr tmpExpr = convertToTmp(convertStmt(*argIter));
        ss << prefix << tmpExpr.expr;
        if (prefix == "") {
          prefix = ", ";
        }
      }
      ss << "]";
    } else {
      ss << ", " << "None";
    }

    ss << ", " << getLocationString(callExpr) <<  ")"; // close expr.CallE(...

    slangExpr.expr = ss.str();
    slangExpr.qualType = callExpr->getType();
    slangExpr.locStr = getLocationString(callExpr);
    slangExpr.compound = true;
    ss.str("");

    if (isTopLevel(callExpr)) {
      ss << "instr.CallI(" << slangExpr.expr << ", " << slangExpr.locStr << ")";
      stu.addStmt(ss.str());
      return SlangExpr{}; // return empty expression
    }

    return slangExpr;
  }

  SlangExpr convertArraySubscriptExpr(const ArraySubscriptExpr *arrayExpr) const {
    SlangExpr slangExpr;
    std::stringstream ss;

    auto it = arrayExpr->child_begin();
    const Stmt *object = *it;
    ++it;
    const Stmt *index = *it;

    SlangExpr parentExpr = convertStmt(object);
    SlangExpr indexExpr = convertToTmp(convertStmt(index));
    SlangExpr tmpExpr;

    tmpExpr = parentExpr;
    if (parentExpr.compound && parentExpr.qualType.getTypePtr()->isArrayType()) {
      ss << "expr.CastE(" << parentExpr.expr;
      ss << ", op.CastOp(";
      ss << convertClangType(FD->getASTContext().getPointerType(arrayExpr->getType()));
      ss << ")";
      ss << ", " << getLocationString(arrayExpr) << ")";
      tmpExpr.expr = ss.str();
      tmpExpr.qualType = FD->getASTContext().getPointerType(arrayExpr->getType());
      tmpExpr.compound = true;
      tmpExpr.locStr = getLocationString(arrayExpr);

      tmpExpr = convertToTmp(tmpExpr);

    } else if (parentExpr.compound) {
      tmpExpr = convertToTmp(parentExpr);
    }

    // if (parentExpr.compound &&
    //     !((object->getStmtClass() == Stmt::ImplicitCastExprClass) &&
    //     (cast<ImplicitCastExpr>(object)->getCastKind() == CK_ArrayToPointerDecay))) {
    //   parentExpr = convertToTmp(parentExpr);
    // }

    ss.str("");
    ss << "expr.ArrayE(" << indexExpr.expr;
    ss << ", " << tmpExpr.expr;
    ss << ", " << getLocationString(arrayExpr) << ")";

    slangExpr.expr = ss.str();
    slangExpr.qualType = arrayExpr->getType();
    slangExpr.locStr = getLocationString(arrayExpr);
    slangExpr.compound = true;

    return slangExpr;
  } // convertArraySubscript()

  SlangExpr convertMemberExpr(const MemberExpr *memberExpr) const {
    auto it = memberExpr->child_begin();
    const Stmt *child = *it;
    SlangExpr parentExpr = convertStmt(child);
    SlangExpr parentTmpExpr;
    SlangExpr memSlangExpr;
    std::stringstream ss;

    // store parent to a temporary
    parentTmpExpr = parentExpr;
    if (parentExpr.compound) {
      if (parentExpr.qualType.getTypePtr()->isPointerType()) {
        parentTmpExpr = convertToTmp(parentExpr);
      } else {
        SlangExpr addrOfExpr;
        ss << "expr.AddrOfE(" << parentExpr.expr;
        ss << ", " << getLocationString(memberExpr) << ")";

        addrOfExpr.expr = ss.str();
        addrOfExpr.qualType = FD->getASTContext().getPointerType(parentExpr.qualType);
        addrOfExpr.locStr = getLocationString(memberExpr);
        addrOfExpr.compound = true;

        parentTmpExpr = convertToTmp(addrOfExpr);
      }
    }

    std::string memberName;
    memberName = memberExpr->getMemberNameInfo().getAsString();
    if (memberName == "") {
      memberName = stu.getVar((uint64_t)(memberExpr->getMemberDecl())).name;
    }

    ss.str("");
    ss << "expr.MemberE(\"" << memberName << "\"";
    ss << ", " << parentTmpExpr.expr;
    ss << ", " << getLocationString(memberExpr) << ")";

    memSlangExpr.expr = ss.str();
    memSlangExpr.qualType = memberExpr->getType();
    memSlangExpr.locStr = getLocationString(memberExpr);
    memSlangExpr.compound = true;

    return memSlangExpr;
  } // convertMemberExpr()

  SlangExpr convertCStyleCastExpr(const CStyleCastExpr *cCast) const {
    SlangExpr castExpr;
    auto it = cCast->child_begin();
    SlangExpr exprArg = convertToTmp(convertStmt(*it));
    std::string castTypeStr = convertClangType(cCast->getType());

    std::stringstream ss;
    ss << "expr.CastE(" << exprArg.expr;
    ss << ", op.CastOp(" << castTypeStr << ")";
    ss << ", " << getLocationString(cCast) << ")";

    castExpr.expr = ss.str();
    castExpr.compound = true;
    castExpr.qualType = cCast->getType();
    castExpr.locStr = getLocationString(cCast);

    return castExpr;
  } // convertCStyleCastExpr()

  SlangExpr convertGotoStmt(const GotoStmt *gotoStmt) const {
    std::string label = gotoStmt->getLabel()->getNameAsString();
    addGotoInstr(label);
    return SlangExpr{};
  } // convertGotoStmt()

  SlangExpr convertBreakStmt(const BreakStmt *breakStmt) const {
    addGotoInstr(stu.peekExitLabel());
    return SlangExpr{};
  }

  SlangExpr convertContinueStmt(const ContinueStmt *continueStmt) const {
    addGotoInstr(stu.peekEntryLabel());
    return SlangExpr{};
  }

  SlangExpr convertSwitchStmt(const SwitchStmt *switchStmt) const {
    std::string id = stu.genNextLabelCountStr();
    std::string switchStartLabel = id + "SwitchStart";
    std::string switchExitLabel = id + "SwitchExit";
    std::string caseCondLabel = id + "CaseCond" + "-";
    std::string caseBodyLabel = id + "CaseBody" + "-";
    std::string defaultLabel = id + "Default";
    bool defaultLabelAdded = false;

    stu.pushLabels(switchStartLabel, switchExitLabel);

    addLabelInstr(switchStartLabel);

    std::vector<const Stmt*> caseStmtsWithDefault;
    // std::vector<const Stmt*> defaultStmt;

    const Expr *condExpr = switchStmt->getCond();
    SlangExpr switchCond = convertToTmp(convertStmt(condExpr));

    // Get all case statements inside switch.
    if (switchStmt->getBody()) {
      switchStmt->getBody()->dump(); // delit
      getCaseStmts(caseStmtsWithDefault, switchStmt->getBody());
      // getDefaultStmt(defaultStmt, switchStmt->getBody());

    } else {
      for (auto it = switchStmt->child_begin(); it != switchStmt->child_end(); ++it) {
        if (isa<CaseStmt>(*it)) {
          getCaseStmts(caseStmtsWithDefault, (*it));
          // getDefaultStmt(defaultStmt, (*it));
        }
      }
    }

    std::stringstream ss;
    std::string label;
    std::string nextLabel;
    size_t totalStmts = caseStmtsWithDefault.size();
    for (size_t index=0; index < caseStmtsWithDefault.size(); ++index) {
      // for (const Stmt *stmt: caseStmtsWithDefault) {
      const Stmt *stmt = caseStmtsWithDefault[index];

      if (isa<CaseStmt>(stmt)) {
        const CaseStmt *caseStmt = cast<CaseStmt>(stmt);
        // find where to jump to if the case condtion is false
        std::string falseLabel;
        falseLabel = defaultLabel;

        if (index != totalStmts - 1) {
          // try jumping to the next case's cond
          for (size_t i=index+1; i < totalStmts; ++i) {
            if (isa<CaseStmt>(caseStmtsWithDefault[i])) {
              ss << caseCondLabel << i;
              falseLabel = ss.str();
              ss.str("");
              break;
            }
          }
        }

        // armed with the falseLabel add the condition
        ss << caseCondLabel << index;
        std::string condLabel = ss.str();
        ss.str("");

        const Stmt *cond = *(caseStmt->child_begin());
        // llvm::errs() << "CASE-CASE-CASE\n"; cond->dump();
        SlangExpr caseCond = convertToTmp(convertStmt(cond));

        // generate body label
        ss << caseBodyLabel << index;
        std::string bodyLabel = ss.str();
        ss.str("");

        addLabelInstr(condLabel); // condition label
        // add the actual condition
        SlangExpr eqExpr = convertToIfTmp(createBinaryExpr(switchCond,
            "op.BO_EQ", caseCond, getLocationString(caseStmt)));
        addCondInstr(eqExpr.expr, bodyLabel, falseLabel, getLocationString(caseStmt));

        // case body
        addLabelInstr(bodyLabel);
        for (auto it = caseStmt->child_begin();
             it != caseStmt->child_end();
             ++it) {
          convertStmt(*it);
        }

        // if it has break, then jump to exit
        // Note: a break as child stmt is covered recursively
        if (caseOrDefaultStmtHasSiblingBreak(caseStmt)) {
          addGotoInstr(switchExitLabel);
        } else {
          // try jumping to the next case's body if present :)
          if (index != totalStmts-1) {
            if (isa<CaseStmt>(caseStmtsWithDefault[index + 1])) {
              ss << caseBodyLabel << index + 1;
              addGotoInstr(ss.str());
              ss.str("");
            } else {
              // must be default then, hence fall through to it
            }
          }
        }

      } else if (isa<DefaultStmt>(stmt)) {
        // add the default case
        addLabelInstr(defaultLabel);
        defaultLabelAdded = true;
        for (auto it = stmt->child_begin(); it != stmt->child_end();
             ++it) {
          convertStmt(*it);
        }

        // if it has break, then jump to exit
        // Note: a break as child stmt is covered recursively
        if (caseOrDefaultStmtHasSiblingBreak(stmt)) {
          addGotoInstr(switchExitLabel);
        } else {
          // try jumping to the next case's body
          if (index != totalStmts-1) {
            // must be a case stmt, since this is a default stmt :)
            ss << caseBodyLabel << index+1;
            addGotoInstr(ss.str());
            ss.str("");
          }
        }
      }

    }

    if (!defaultLabelAdded) {
      addLabelInstr(defaultLabel); // needed
    }
    addLabelInstr(switchExitLabel);

    stu.popLabel();
    return SlangExpr{};
  } // convertSwitchStmt()

  // many times BreakStmt is a sibling of CaseStmt/DefaultStmt
  // this function detects that
  bool caseOrDefaultStmtHasSiblingBreak(const Stmt *stmt) const {
    const auto &parents = FD->getASTContext().getParents(*stmt);

    const Stmt *parentStmt = parents[0].get<Stmt>();
    bool lastStmtWasTheGivenCaseOrDefaultStmt = false;
    bool hasBreak = false;

    for (auto it = parentStmt->child_begin();
          it != parentStmt->child_end();
          ++it) {
      if (! *it) { continue; }

      if (isa<BreakStmt>(*it)) {
        if (lastStmtWasTheGivenCaseOrDefaultStmt) {
          hasBreak = true;
        }
        break;
      }

      if (lastStmtWasTheGivenCaseOrDefaultStmt) {
        lastStmtWasTheGivenCaseOrDefaultStmt = false;
      }
      if ((*it) == stmt) {
        lastStmtWasTheGivenCaseOrDefaultStmt = true;
      }
    }

    return hasBreak;
  } // caseOrDefaultStmtHasSiblingBreak()

  // Returns true if the type is not complete enough to give away a constant size
  bool isIncompleteType(const Type *type) const {
      bool retVal = false;

      if (type->isIncompleteArrayType() || type->isVariableArrayType()) {
          retVal = true;
      }
      return retVal;
  }

  // get all case statements recursively (case stmts can be hierarchical)
  void getCaseStmts(std::vector<const Stmt*>& caseStmtsWithDefault, const Stmt *stmt) const {
    if (! stmt) return;

    if (isa<CaseStmt>(stmt)) {
      auto caseStmt = cast<CaseStmt>(stmt);
      caseStmtsWithDefault.push_back(stmt);
      for (auto it = caseStmt->child_begin(); it != caseStmt->child_end(); ++it) {
        if ((*it) && isa<CaseStmt>(*it)) {
          getCaseStmts(caseStmtsWithDefault, (*it));
        }
      }

    } else if (isa<CompoundStmt>(stmt)) {
      const CompoundStmt *compoundStmt = cast<CompoundStmt>(stmt);
      for (auto it = compoundStmt->body_begin(); it != compoundStmt->body_end(); ++it) {
        getCaseStmts(caseStmtsWithDefault, (*it));
      }
    } else if (isa<SwitchStmt>(stmt)) {
      // do nothing, as it will be handled separately

    } else if (isa<DefaultStmt>(stmt)) {
      auto defaultStmt = cast<DefaultStmt>(stmt);
      caseStmtsWithDefault.push_back(stmt);
      for (auto it = defaultStmt->child_begin(); it != defaultStmt->child_end(); ++it) {
        if ((*it) && isa<CaseStmt>(*it)) {
          getCaseStmts(caseStmtsWithDefault, (*it));
        }
      }

    } else {
      if (stmt->child_begin() != stmt->child_end()) {
        for (auto it = stmt->child_begin(); it != stmt->child_end(); ++it) {
          getCaseStmts(caseStmtsWithDefault, (*it));
        }
      }
    }
  }

  // get the default stmt if present
  void getDefaultStmt(std::vector<const Stmt*>& defaultStmt, const Stmt *stmt) const {
    if (! stmt) return;

    if (isa<DefaultStmt>(stmt)) {
      defaultStmt.push_back(stmt);

    } else if (isa<CaseStmt>(stmt)) {
      auto caseStmt = cast<CaseStmt>(stmt);
      for (auto it = caseStmt->child_begin(); it != caseStmt->child_end(); ++it) {
        if ((*it) && isa<CaseStmt>(*it)) {
          getDefaultStmt(defaultStmt, (*it));
        }
      }

    } else if (isa<CompoundStmt>(stmt)) {
      const CompoundStmt *compoundStmt = cast<CompoundStmt>(stmt);
      for (auto it = compoundStmt->body_begin(); it != compoundStmt->body_end(); ++it) {
        getDefaultStmt(defaultStmt, (*it));
      }
    } else if (isa<SwitchStmt>(stmt)) {
      // do nothing, as it will be handled separately
    } else {
      if (stmt->child_begin() != stmt->child_end()) {
        for (auto it = stmt->child_begin(); it != stmt->child_end(); ++it) {
          getDefaultStmt(defaultStmt, (*it));
        }
      }
    }
  }


  SlangExpr convertReturnStmt(const ReturnStmt *returnStmt) const {
    const Expr *retVal = returnStmt->getRetValue();

    SlangExpr retExpr = convertToTmp(convertStmt(retVal));

    std::stringstream ss;
    ss << "instr.ReturnI(" << retExpr.expr << ")";
    stu.addStmt(ss.str());

    return SlangExpr{};
  }

  SlangExpr convertConditionalOp(const ConditionalOperator *condOp) const {
    const Expr *condition = condOp->getCond();

    SlangExpr cond = convertToTmp(convertStmt(condition));
    SlangExpr trueExpr = convertToTmp(convertStmt(condOp->getTrueExpr()));
    SlangExpr falseExpr = convertToTmp(convertStmt(condOp->getFalseExpr()));

    SlangExpr slangExpr;
    std::stringstream ss;
    ss << "expr.SelectE(" << cond.expr;
    ss << ", " << trueExpr.expr;
    ss << ", " << falseExpr.expr;
    ss << ", " << getLocationString(condition) << ")";
    slangExpr.expr = ss.str();
    slangExpr.compound = true;
    slangExpr.qualType = condition->getType();

    return slangExpr;
  } // convertConditionalOp()

  SlangExpr convertIfStmt(const IfStmt *ifStmt) const {
    std::string id = stu.genNextLabelCountStr();
    std::string ifTrueLabel = id + "IfTrue";
    std::string ifFalseLabel = id + "IfFalse";
    std::string ifExitLabel = id + "IfExit";

    const Stmt *condition = ifStmt->getCond();
    SlangExpr conditionExpr = convertStmt(condition);
    conditionExpr = convertToIfTmp(conditionExpr);

    addCondInstr(conditionExpr.expr,
        ifTrueLabel, ifFalseLabel, getLocationString(condition));

    addLabelInstr(ifTrueLabel);

    const Stmt *body = ifStmt->getThen();
    if (body) { convertStmt(body); }

    addGotoInstr(ifExitLabel);
    addLabelInstr(ifFalseLabel);

    const Stmt *elseBody = ifStmt->getElse();
    if (elseBody) {
      convertStmt(elseBody);
    }

    addLabelInstr(ifExitLabel);

    return SlangExpr{}; // return empty expression
  } // convertIfStmt()

  SlangExpr convertWhileStmt(const WhileStmt *whileStmt) const {
    std::string id = stu.genNextLabelCountStr();
    std::string whileCondLabel = id + "WhileCond";
    std::string whileBodyLabel = id + "WhileBody";
    std::string whileExitLabel = id + "WhileExit";

    stu.pushLabels(whileCondLabel, whileExitLabel);

    addLabelInstr(whileCondLabel);

    const Stmt *condition = whileStmt->getCond();
    SlangExpr conditionExpr = convertStmt(condition);
    conditionExpr = convertToIfTmp(conditionExpr);

    addCondInstr(conditionExpr.expr,
        whileBodyLabel, whileExitLabel, getLocationString(condition));

    addLabelInstr(whileBodyLabel);

    const Stmt *body = whileStmt->getBody();
    if (body) { convertStmt(body); }

    // unconditional jump to startConditionLabel
    addGotoInstr(whileCondLabel);

    addLabelInstr(whileExitLabel);

    stu.popLabel();
    return SlangExpr{}; // return empty expression
  } // convertWhileStmt()

  SlangExpr convertDoStmt(const DoStmt *doStmt) const {
    std::string id = stu.genNextLabelCountStr();
    std::string doEntry = "DoEntry" + id;
    std::string doCond = "DoCond" + id;
    std::string doExit = "DoExit" + id;

    stu.pushLabels(doCond, doExit);

    // do body
    addLabelInstr(doEntry);
    const Stmt *body = doStmt->getBody();
    if (body) { convertStmt(body); }

    // while condition
    addLabelInstr(doCond);
    const Stmt *condition = doStmt->getCond();
    SlangExpr conditionExpr = convertToIfTmp(convertStmt(condition));
    addCondInstr(conditionExpr.expr,
        doEntry, doExit, getLocationString(condition));

    addLabelInstr(doExit);

    stu.popLabel();
    return SlangExpr{}; // return empty expression
  } // convertDoStmt()

  SlangExpr convertForStmt(const ForStmt *forStmt) const {
    std::string id = stu.genNextLabelCountStr();
    std::string forCondLabel = id + "ForCond";
    std::string forBodyLabel = id + "ForBody";
    std::string forExitLabel = id + "ForExit";

    stu.pushLabels(forCondLabel, forExitLabel);

    // for init
    const Stmt *init = forStmt->getInit();
    if (init) { convertStmt(init); }

    // for condition
    const Stmt *condition = forStmt->getCond();

    addLabelInstr(forCondLabel);

    if (condition) {
      SlangExpr conditionExpr = convertToIfTmp(convertStmt(condition));

      addCondInstr(conditionExpr.expr,
          forBodyLabel, forExitLabel, getLocationString(condition));
    } else {
      addCondInstr("expr.LitE(1)",
                   forBodyLabel, forExitLabel, getLocationString(forStmt));
    }

    // for body
    addLabelInstr(forBodyLabel);

    const Stmt *body = forStmt->getBody();
    if (body) { convertStmt(body); }

    const Stmt *inc = forStmt->getInc();
    if (inc) { convertStmt(inc); }

    addGotoInstr(forCondLabel); // jump to for cond
    addLabelInstr(forExitLabel); // for exit

    stu.popLabel();
    return SlangExpr{}; // return empty expression
  } // convertForStmt()

  SlangExpr convertImplicitCastExpr(const ImplicitCastExpr *iCast) const {
    // only one child is expected
    auto it = iCast->child_begin();
    auto ck = iCast->getCastKind();

    switch(ck) {
      case CastKind::CK_FloatingToIntegral:
      case CastKind::CK_IntegralToFloating:
      //case CastKind::CK_FunctionToPointerDecay:
      case CastKind::CK_ArrayToPointerDecay: {
        SlangExpr castExpr;
        SlangExpr exprArg = convertToTmp(convertStmt(*it));
        std::string castTypeStr = convertClangType(iCast->getType());

        std::stringstream ss;
        ss << "expr.CastE(" << exprArg.expr;
        ss << ", op.CastOp(" << castTypeStr << ")";
        ss << ", " << getLocationString(iCast) << ")";

        castExpr.expr = ss.str();
        castExpr.compound = true;
        castExpr.qualType = iCast->getType();
        castExpr.locStr = getLocationString(iCast);
        return castExpr;
      }

      default:
        return convertStmt(*it);
    }
  }

  SlangExpr convertCharacterLiteral(const CharacterLiteral *cl) const {
    std::stringstream ss;
    ss << "expr.LitE(" << cl->getValue();
    ss << ", " << getLocationString(cl) << ")";

    SlangExpr slangExpr;
    slangExpr.expr = ss.str();
    slangExpr.locStr = getLocationString(cl);
    slangExpr.qualType = cl->getType();

    return slangExpr;
  } // convertCharacterLiteral()

  SlangExpr convertConstantExpr(const ConstantExpr *constExpr) const {
    // a ConstantExpr contains a literal expression
    return convertStmt(constExpr->getSubExpr());
  } // convertConstantExpr()

  SlangExpr convertIntegerLiteral(const IntegerLiteral *il) const {
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

    SlangExpr slangExpr;
    slangExpr.expr = ss.str();
    slangExpr.qualType = il->getType();
    slangExpr.locStr = getLocationString(il);

    return slangExpr;
  } // convertIntegerLiteral()

  SlangExpr convertFloatingLiteral(const FloatingLiteral *fl) const {
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

    SlangExpr slangExpr;
    slangExpr.expr = ss.str();
    slangExpr.qualType = fl->getType();
    slangExpr.locStr = getLocationString(fl);

    return slangExpr;
  } // convertFloatingLiteral()

  SlangExpr convertStringLiteral(const StringLiteral *sl) const {
    SlangExpr slangExpr;
    std::stringstream ss;

    std::string locStr = getLocationString(sl);

    llvm::errs() << "STRING_LITERAL:"; //delit
    sl->dump(); //delit
    // with extra text at the end since """" could occur
    // making the string invalid in python
    ss << "expr.LitE(\"\"\"" << sl->getBytes().str() << "XXX\"\"\"";
    ss << ", " << locStr << ")";
    slangExpr.expr = ss.str();
    slangExpr.locStr = locStr;

    return slangExpr;
  } // convertStringLiteral()

  SlangExpr convertVariable(const VarDecl *varDecl,
      std::string locStr = "Loc(3,3)") const {
    std::stringstream ss;
    SlangExpr slangExpr;

    ss << "expr.VarE(\"" << stu.convertVarExpr((uint64_t)varDecl) << "\"";
    ss << ", " << locStr << ")";
    slangExpr.expr = ss.str();
    slangExpr.qualType = varDecl->getType();
    slangExpr.varId = (uint64_t)varDecl;
    slangExpr.locStr = getLocationString(varDecl);

    return slangExpr;
  } // convertVariable()

  SlangExpr convertEnumConst(const EnumConstantDecl *ecd, std::string &locStr) const {
    SlangExpr slangExpr;

    std::stringstream ss;
    ss << "expr.LitE(" << (ecd->getInitVal()).toString(10);
    ss << ", " << locStr << ")";

    slangExpr.expr = ss.str();
    slangExpr.locStr = locStr;
    slangExpr.qualType = ecd->getType();

    return slangExpr;
  }

  SlangExpr convertDeclRefExpr(const DeclRefExpr *dre) const {
    SlangExpr slangExpr;
    std::stringstream ss;

    std::string locStr = getLocationString(dre);

    const ValueDecl *valueDecl = dre->getDecl();
    handleValueDecl(valueDecl, stu.currFunc->name);
    if (isa<VarDecl>(valueDecl)) {
      auto varDecl = cast<VarDecl>(valueDecl);
      slangExpr = convertVariable(varDecl, getLocationString(dre));
      slangExpr.locStr = getLocationString(dre);
      return slangExpr;

    } else if (isa<EnumConstantDecl>(valueDecl)) {
      auto ecd = cast<EnumConstantDecl>(valueDecl);
      return convertEnumConst(ecd, locStr);

    } else if (isa<FunctionDecl>(valueDecl)) {
      auto funcDecl = cast<FunctionDecl>(valueDecl);
      std::string funcName = funcDecl->getNameInfo().getAsString();
      ss << "expr.FuncE(\"" << stu.convertFuncName(funcName) << "\"";
      ss << ", " << locStr << ")";
      slangExpr.expr = ss.str();
      slangExpr.qualType = funcDecl->getType();
      slangExpr.locStr = locStr;
      return slangExpr;

    } else {
      SLANG_ERROR("Not_a_VarDecl.")
      slangExpr.expr = "ERROR:convertDeclRefExpr";
      return slangExpr;
    }
  } // convertDeclRefExpr()

  // a || b , a && b
  SlangExpr convertLogicalOp(const BinaryOperator *binOp) const {
    std::string nextCheck;
    std::string tmpReAssign;
    std::string exitLabel;

    std::string op;
    std::string id = stu.genNextLabelCountStr();
    switch(binOp->getOpcode()) {
      case BO_LOr:
        op = "||";
        nextCheck = id + "NextCheckLor";
        tmpReAssign = id + "TmpAssignLor";
        exitLabel = id + "ExitLor";
        break;
      case BO_LAnd:
        op = "&&";
        nextCheck = id + "NextCheckLand";
        tmpReAssign = id + "TmpAssignLand";
        exitLabel = id + "ExitLand";
        break;
      default: SLANG_ERROR("ERROR:unknownLogicalOp"); break;
    }

    auto it = binOp->child_begin();
    const Stmt *leftOprStmt = *it;
    ++it;
    const Stmt *rightOprStmt = *it;

    SlangExpr trueValue;
    SlangExpr falseValue;
    trueValue.expr = "expr.LitE(1, " + getLocationString(binOp) + ")";
    falseValue.expr = "expr.LitE(0, " + getLocationString(binOp) + ")";
    trueValue.locStr = falseValue.locStr = getLocationString(binOp);

    // assign tmp = 1
    SlangExpr tmpVar = genTmpVariable("L", "types.Int32", getLocationString(binOp));
    addAssignInstr(tmpVar, trueValue, getLocationString(binOp));

    // check first part a ||, a &&
    SlangExpr leftOprExpr = convertToIfTmp(convertStmt(leftOprStmt));
    if (op == "||") {
      addCondInstr(leftOprExpr.expr, exitLabel, nextCheck, leftOprExpr.locStr);
    } else {
      addCondInstr(leftOprExpr.expr, nextCheck, tmpReAssign, leftOprExpr.locStr);
    }

    // check second part || b, && b
    addLabelInstr(nextCheck);
    SlangExpr rightOprExpr = convertToIfTmp(convertStmt(rightOprStmt));
    addCondInstr(rightOprExpr.expr, exitLabel, tmpReAssign, leftOprExpr.locStr);

    // assign tmp = 0
    addLabelInstr(tmpReAssign);
    addAssignInstr(tmpVar, falseValue, getLocationString(binOp));

    // exit label
    addLabelInstr(exitLabel);

    return tmpVar;
  } // convertLogicalOp()

  SlangExpr convertUnaryIncDecOp(const UnaryOperator *unOp) const {
    auto it = unOp->child_begin();
    SlangExpr exprArg = convertStmt(*it);

    std::string op;
    switch(unOp->getOpcode()) {
      case UO_PreInc:
      case UO_PostInc: op = "op.BO_ADD"; break;
      case UO_PostDec:
      case UO_PreDec: op = "op.BO_SUB"; break;
      default:  break;
    }

    SlangExpr litOne;
    litOne.expr = "expr.LitE(1, " + getLocationString(unOp) + ")";
    litOne.locStr = getLocationString(unOp);

    SlangExpr incDecExpr = createBinaryExpr(exprArg, op,
        litOne, getLocationString(unOp));

    switch(unOp->getOpcode()) {
      case UO_PreInc:
      case UO_PreDec: {
        addAssignInstr(exprArg, incDecExpr, getLocationString(unOp));
        return convertToTmp(exprArg, true);
      }

      case UO_PostInc:
      case UO_PostDec: {
        SlangExpr tmpExpr = convertToTmp(exprArg, true);
        addAssignInstr(exprArg, incDecExpr, getLocationString(unOp));
        return tmpExpr;
      }

      default:
        SLANG_ERROR("ERROR:unknownIncDecOps" <<
            unOp->getOpcodeStr(unOp->getOpcode()));
        break;
    }
    return exprArg;
  }

  SlangExpr convertUnaryOperator(const UnaryOperator *unOp) const {
    switch(unOp->getOpcode()) {
      case UO_PreInc:
      case UO_PostInc:
      case UO_PreDec:
      case UO_PostDec:
        return convertUnaryIncDecOp(unOp);
      default:  break;
    }

    SlangExpr exprArg;
    auto it = unOp->child_begin();

    if (unOp->getOpcode() == UO_AddrOf) {
      exprArg = convertStmt(*it); // special case: e.g. &arr[7][5], ...
    } else {
      exprArg = convertToTmp(convertStmt(*it));
    }

    std::string op;
    switch (unOp->getOpcode()) {
      default:
        SLANG_DEBUG("convertUnaryOp: " << unOp->getOpcodeStr(unOp->getOpcode()))
        break;
      case UO_AddrOf: op = "op.UO_ADDROF"; break;
      case UO_Deref: op = "op.UO_DEREF"; break;
      case UO_Minus: op = "op.UO_MINUS"; break;
      case UO_Plus: op = "op.UO_MINUS"; break;
      case UO_LNot: op = "op.UO_LNOT"; break;
      case UO_Not: op = "op.UO_BIT_NOT"; break;
    }

    return createUnaryExpr(op, exprArg, getLocationString(unOp), unOp->getType());
  } // convertUnaryOperator()

  SlangExpr convertUnaryExprOrTypeTraitExpr(const UnaryExprOrTypeTraitExpr *stmt) const {
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

            const Stmt *firstChild = *iterator;
            innerExpr = convertStmt(firstChild);
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
            slangExpr.qualType = stmt->getType();
            TypeInfo typeInfo = FD->getASTContext().getTypeInfo(
                stmt->getArgumentType());
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

  SlangExpr convertBinaryOperator(const BinaryOperator *binOp) const {
    SlangExpr slangExpr;

    if (binOp->isCompoundAssignmentOp()) {
      return convertCompoundAssignmentOp(binOp);
    } else if (binOp->isAssignmentOp()) {
      return convertAssignmentOp(binOp);
    } else if (binOp->isLogicalOp()) {
      return convertLogicalOp(binOp);
    }

    std::string op;
    switch (binOp->getOpcode()) {
    // NOTE : && and || are handled in convertConditionalOp()

    case BO_Add: op = "op.BO_ADD"; break;
    case BO_Sub: op = "op.BO_SUB"; break;
    case BO_Mul: op = "op.BO_MUL"; break;
    case BO_Div: op = "op.BO_DIV"; break;
    case BO_Rem: op = "op.BO_MOD"; break;

    case BO_LT: op = "op.BO_LT"; break;
    case BO_LE: op = "op.BO_LE"; break;
    case BO_EQ: op = "op.BO_EQ"; break;
    case BO_NE: op = "op.BO_NE"; break;
    case BO_GE: op = "op.BO_GE"; break;
    case BO_GT: op = "op.BO_GT"; break;

    case BO_Or: op = "op.BO_BIT_OR"; break;
    case BO_And: op = "op.BO_BIT_AND"; break;
    case BO_Xor: op = "op.BO_BIT_XOR"; break;

    case BO_Shl: op = "op.BO_LSHIFT"; break;
    case BO_Shr: op = "op.BO_RSHIFT"; break;

    case BO_Comma: return convertBinaryCommaOp(binOp);

    default: op = "ERROR:binOp"; break;
    }

    auto it = binOp->child_begin();
    const Stmt *leftOprStmt = *it;
    ++it;
    const Stmt *rightOprStmt = *it;

    SlangExpr leftOprExpr = convertStmt(leftOprStmt);
    SlangExpr rightOprExpr = convertStmt(rightOprStmt);

    slangExpr = createBinaryExpr(leftOprExpr,
        op, rightOprExpr, getLocationString(binOp));

    return slangExpr;
  } // convertBinaryOperator()

  // stores the given expression into a tmp variable
  SlangExpr convertToTmp(SlangExpr slangExpr, bool force = false) const {
    if (slangExpr.compound || force == true) {
      SlangExpr tmpExpr;
      if (slangExpr.qualType.isNull()) {
        tmpExpr = genTmpVariable("t", "types.Int32", slangExpr.locStr);
      } else {
        tmpExpr = genTmpVariable("t", slangExpr.qualType, slangExpr.locStr);
      }
      std::stringstream ss;

      ss << "instr.AssignI(" << tmpExpr.expr << ", " << slangExpr.expr;
      ss << ", " << slangExpr.locStr << ")"; // close instr.AssignI(...
      stu.addStmt(ss.str());

      return tmpExpr;
    } else {
      return slangExpr;
    }
  } // convertToTmp()

  // special tmp variable for if "t.1if", "t.2if" etc...
  SlangExpr convertToIfTmp(SlangExpr slangExpr, bool force = false) const {
    if (slangExpr.compound || force == true) {
      SlangExpr tmpExpr;
      if (slangExpr.qualType.isNull()) {
        tmpExpr = genTmpVariable("if", "types.Int32", slangExpr.locStr);
      } else {
        tmpExpr = genTmpVariable("if", slangExpr.qualType, slangExpr.locStr);
      }
      std::stringstream ss;

      ss << "instr.AssignI(" << tmpExpr.expr << ", " << slangExpr.expr;
      ss << ", " << slangExpr.locStr << ")"; // close instr.AssignI(...
      stu.addStmt(ss.str());

      return tmpExpr;
    } else {
      return slangExpr;
    }
  } // convertToIfTmp()

  SlangExpr convertCompoundAssignmentOp(const BinaryOperator *binOp) const {
    SlangExpr slangExpr;

    auto it = binOp->child_begin();
    const Stmt *lhs = *it;
    const Stmt *rhs = *(++it);

    SlangExpr rhsExpr = convertStmt(rhs);
    SlangExpr lhsExpr = convertStmt(lhs);

    if (lhsExpr.compound && rhsExpr.compound) {
      rhsExpr = convertToTmp(rhsExpr);
    }

    std::string op;
    switch(binOp->getOpcode()) {
      case BO_ShlAssign: op = "op.BO_LSHIFT"; break;
      case BO_ShrAssign: op = "op.BO_RSHIFT"; break;

      case BO_OrAssign: op = "op.BO_BIT_OR"; break;
      case BO_AndAssign: op = "op.BO_BIT_AND"; break;
      case BO_XorAssign: op = "op.BO_BIT_XOR"; break;

      case BO_AddAssign: op = "op.BO_ADD"; break;
      case BO_SubAssign: op = "op.BO_SUB"; break;
      case BO_MulAssign: op = "op.BO_MUL"; break;
      case BO_DivAssign: op = "op.BO_DIV"; break;
      case BO_RemAssign: op = "op.BO_MOD"; break;

      default: op = "ERROR:compoundAssignOp"; break;
    }

    SlangExpr newRhsExpr;
    if (lhsExpr.compound) {
      newRhsExpr = convertToTmp(createBinaryExpr(
          lhsExpr, op, rhsExpr, getLocationString(binOp)));
    } else {
      newRhsExpr = createBinaryExpr(
          lhsExpr, op, rhsExpr, getLocationString(binOp));
    }

    addAssignInstr(lhsExpr, newRhsExpr, getLocationString(binOp));

    return slangExpr;
  } // convertCompoundAssignmentOp()

  SlangExpr convertAssignmentOp(const BinaryOperator *binOp) const {
    SlangExpr lhsExpr, rhsExpr;

    auto it = binOp->child_begin();
    const Stmt *lhs = *it;
    const Stmt *rhs = *(++it);

    rhsExpr = convertStmt(rhs);
    lhsExpr = convertStmt(lhs);

    if (lhsExpr.compound && rhsExpr.compound) {
      rhsExpr = convertToTmp(rhsExpr);
    }

    addAssignInstr(lhsExpr, rhsExpr, getLocationString(binOp));

    return lhsExpr;
  } // convertAssignmentOp()

  SlangExpr convertCompoundStmt(const CompoundStmt *compoundStmt) const {
    SlangExpr slangExpr;

    for (auto it = compoundStmt->body_begin(); it != compoundStmt->body_end(); ++it) {
      // don't care about the return value
      convertStmt(*it);
    }

    return slangExpr;
  } // convertCompoundStmt()

  SlangExpr convertParenExpr(const ParenExpr *parenExpr) const {
    auto it = parenExpr->child_begin(); // should have only one child
    return convertStmt(*it);
  } // convertParenExpr()

  SlangExpr convertLabel(const LabelStmt *labelStmt) const {
    SlangExpr slangExpr;
    std::stringstream ss;

    std::string locStr = getLocationString(labelStmt);

    ss << "instr.LabelI(\"" << labelStmt->getName() << "\"";
    ss << ", " << locStr << ")"; // close instr.LabelI(...
    stu.addStmt(ss.str());

    for (auto it = labelStmt->child_begin(); it != labelStmt->child_end(); ++it) {
      convertStmt(*it);
    }

    return slangExpr;
  } // convertLabel()

  // BOUND START: type_conversion_routines

  // converts clang type to span ir types
  std::string convertClangType(QualType qt) const {
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
      SlangRecord *getBackSlangRecord;
      if (type->isStructureType()) {
        return convertClangRecordType(qt.getTypePtr()->getAsStructureType()->getDecl(),
            getBackSlangRecord);
      } else if (type->isUnionType()) {
        return convertClangRecordType(qt.getTypePtr()->getAsUnionType()->getDecl(),
            getBackSlangRecord);
      } else {
        ss << "ERROR:RecordType";
      }

    } else if (type->isArrayType()) {
      return convertClangArrayType(qt);

    } else if (type->isFunctionProtoType()) {
      return convertFunctionProtoType(qt);

    } else {
      ss << "UnknownType.";
    }

    return ss.str();
  } // convertClangType()

  std::string convertClangBuiltinType(QualType qt) const {
    std::stringstream ss;

    const Type *type = qt.getTypePtr();

    if (type->isSignedIntegerType()) {
      if (type->isCharType()) {
        ss << "types.Int8";
      } else if (type->isChar16Type()) {
        ss << "types.Int16";
      } else if (type->isIntegerType()) {
        TypeInfo typeInfo = FD->getASTContext().getTypeInfo(qt);
        size_t size = typeInfo.Width;
        ss << "types.Int" << size;
      } else {
        ss << "UnknownSignedIntType.";
      }

    } else if (type->isUnsignedIntegerType()) {
      if (type->isCharType()) {
        ss << "types.UInt8";
      } else if (type->isChar16Type()) {
        ss << "types.UInt16";
      } else if (type->isIntegerType()) {
        TypeInfo typeInfo = FD->getASTContext().getTypeInfo(qt);
        size_t size = typeInfo.Width;
        ss << "types.UInt" << size;
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

  std::string convertClangRecordType(const RecordDecl *recordDecl,
      SlangRecord *&returnSlangRecord) const {
    // a hack1 for anonymous decls (it works!) see test 000193.c and its AST!!
    static const RecordDecl *lastAnonymousRecordDecl = nullptr;

    if (recordDecl == nullptr) {
      // default to the last anonymous record decl
      return convertClangRecordType(lastAnonymousRecordDecl, returnSlangRecord);
    }

    if (stu.isRecordPresent((uint64_t)recordDecl)) {
      returnSlangRecord = &stu.getRecord((uint64_t)recordDecl); // return pointer back
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
    returnSlangRecord = &newSlangRecord; // IMPORTANT

    SlangRecordField slangRecordField;

    SlangRecord *getBackSlangRecord;
    for (auto it = recordDecl->decls_begin(); it != recordDecl->decls_end(); ++it) {
      (*it)->dump();
      if (isa<RecordDecl>(*it)) {
        convertClangRecordType(cast<RecordDecl>(*it), getBackSlangRecord);
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
          auto slangVar = SlangVar((uint64_t) fieldDecl, slangRecordField.name);
          stu.addVar((uint64_t) fieldDecl, slangVar);
          slangRecordField.typeStr = convertClangRecordType(nullptr,
              slangRecordField.slangRecord);

        } else if (fieldDecl->getType()->isRecordType()) {
          auto type = fieldDecl->getType();
          if (type->isStructureType()) {
            slangRecordField.typeStr =
                convertClangRecordType(type->getAsStructureType()->getDecl(),
                   slangRecordField.slangRecord);
          } else if (type->isUnionType()) {
            slangRecordField.typeStr =
                convertClangRecordType(type->getAsUnionType()->getDecl(),
                    slangRecordField.slangRecord);
          }
        } else {
          slangRecordField.typeStr = convertClangType(slangRecordField.type);
        }

        newSlangRecord.members.push_back(slangRecordField);
      }
    }

    // store for later use (part-of-hack1))
    lastAnonymousRecordDecl = recordDecl;

    // no need to add newSlangRecord, its a reference to its entry in the stu.recordMap
    return newSlangRecord.toShortString();
  } // convertClangRecordType()

  std::string convertClangArrayType(QualType qt) const {
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

  std::string convertFunctionProtoType(QualType qt) const {
    std::stringstream ss;

    const Type *funcType = qt.getTypePtr();

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

    } else {
      ss << "UnknownFunctionProtoType";
    }

    return ss.str();
  } // convertFunctionPointerType()

  std::string convertFunctionPointerType(QualType qt) const {
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
  // BOUND END  : conversion_routines

  // BOUND START: helper_routines

  SlangExpr genTmpVariable(std::string suffix, std::string typeStr,
      std::string locStr) const {
    std::stringstream ss;
    SlangExpr slangExpr{};

    // STEP 1: Populate a SlangVar object with unique name.
    SlangVar slangVar{};
    slangVar.id = stu.nextUniqueId();
    uint64_t tmpNumbering = stu.nextTmpId();
    ss << "" << tmpNumbering << suffix;
    slangVar.setLocalVarName(ss.str(), stu.getCurrFuncName());
    slangVar.typeStr = typeStr;

    // STEP 2: Add to the var map.
    // FIXME: The var's 'id' here should be small enough to not interfere with uint64_t addresses.
    stu.addVar(slangVar.id, slangVar);

    // STEP 3: generate var expression.
    ss.str(""); // empty the stream
    ss << "expr.VarE(\"" << slangVar.name << "\"";
    ss << ", " << locStr << ")";

    slangExpr.expr = ss.str();
    slangExpr.locStr = locStr;
    // slangExpr.qualType = qt;
    slangExpr.nonTmpVar = false;

    return slangExpr;
  } // genTmpVariable()

  SlangExpr genTmpVariable(std::string suffix,
      QualType qt, std::string locStr, bool ifTmp = false) const {
    std::stringstream ss;
    SlangExpr slangExpr{};

    // STEP 1: Populate a SlangVar object with unique name.
    SlangVar slangVar{};
    slangVar.id = stu.nextUniqueId();
    uint64_t tmpNumbering = stu.nextTmpId();
    ss << "" << tmpNumbering << suffix;
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
    slangExpr.locStr = locStr;
    slangExpr.qualType = qt;
    slangExpr.nonTmpVar = false;

    return slangExpr;
  } // genTmpVariable()

  std::string getLocationString(const Stmt *stmt) const {
    std::stringstream ss;
    uint32_t line = 0;
    uint32_t col = 0;

    ss << "Loc(";
    line = FD->getASTContext().getSourceManager().getExpansionLineNumber(stmt->getBeginLoc());
    ss << line << ",";
    col = FD->getASTContext().getSourceManager().getExpansionColumnNumber(stmt->getBeginLoc());
    ss << col << ")";

    return ss.str();
  }

  std::string getLocationString(const RecordDecl *recordDecl) const {
    std::stringstream ss;
    uint32_t line = 0;
    uint32_t col = 0;

    ss << "Loc(";
    line = FD->getASTContext().getSourceManager().getExpansionLineNumber(recordDecl->getBeginLoc());
    ss << line << ",";
    col =
        FD->getASTContext().getSourceManager().getExpansionColumnNumber(recordDecl->getBeginLoc());
    ss << col << ")";

    return ss.str();
  }

  std::string getLocationString(const ValueDecl *valueDecl) const {
    std::stringstream ss;
    uint32_t line = 0;
    uint32_t col = 0;

    ss << "Loc(";
    line = FD->getASTContext().getSourceManager().getExpansionLineNumber(valueDecl->getBeginLoc());
    ss << line << ",";
    col = FD->getASTContext().getSourceManager().getExpansionColumnNumber(valueDecl->getBeginLoc());
    ss << col << ")";

    return ss.str();
  }

  // Remove qualifiers and typedefs
  QualType getCleanedQualType(QualType qt) const {
    if (qt.isNull())
      return qt;
    qt = qt.getCanonicalType();
    qt.removeLocalConst();
    qt.removeLocalRestrict();
    qt.removeLocalVolatile();
    return qt;
  }

  void addGotoInstr(std::string label) const {
    std::stringstream ss;
    ss << "instr.GotoI(\"" << label << "\")";
    stu.addStmt(ss.str());
  }

  void addLabelInstr(std::string label) const {
    std::stringstream ss;
    ss << "instr.LabelI(\"" << label << "\")";
    stu.addStmt(ss.str());
  }

  void addCondInstr(std::string expr,
      std::string trueLabel, std::string falseLabel, std::string locStr) const {
    std::stringstream ss;
    ss << "instr.CondI(" << expr;
    ss << ", \"" << trueLabel << "\"";
    ss << ", \"" << falseLabel << "\"";
    ss << ", " << locStr << ")";
    stu.addStmt(ss.str());
  }

  void addAssignInstr(SlangExpr& lhs, SlangExpr rhs, std::string locStr) const {
    std::stringstream ss;
    if (lhs.compound && rhs.compound) {
      rhs = convertToTmp(rhs);
    }
    ss << "instr.AssignI(" << lhs.expr;
    ss << ", " << rhs.expr << ", " << locStr << ")";
    stu.addStmt(ss.str());
  }

  // Note: unlike createBinaryExpr, createUnaryExpr doesn't convert its expr to tmp expr.
  SlangExpr createUnaryExpr(std::string op,
      SlangExpr expr, std::string locStr, QualType qt) const {
    SlangExpr unaryExpr;

    std::stringstream ss;

    if (op == "op.UO_ADDROF") {
      ss << "expr.AddrOfE(";
      ss << expr.expr;
      ss << ", " << locStr << ")";
    } else {
      ss << "expr.UnaryE(" << op;
      ss << ", " << expr.expr;
      ss << ", " << locStr << ")";
    }

    unaryExpr.expr = ss.str();
    unaryExpr.qualType = qt;
    unaryExpr.compound = true;
    unaryExpr.locStr = locStr;

    return unaryExpr;
  } // createUnaryExpr()

  SlangExpr createBinaryExpr(SlangExpr lhsExpr,
      std::string op, SlangExpr rhsExpr, std::string locStr) const {
    SlangExpr binaryExpr;

    lhsExpr = convertToTmp(lhsExpr);
    rhsExpr = convertToTmp(rhsExpr);

    std::stringstream ss;
    ss << "expr.BinaryE(" << lhsExpr.expr;
    ss << ", " << op;
    ss << ", " << rhsExpr.expr;
    ss << ", " << locStr << ")";

    binaryExpr.expr = ss.str();
    binaryExpr.qualType = lhsExpr.qualType;
    binaryExpr.compound = true;
    binaryExpr.locStr = locStr;

    return binaryExpr;
  } // createBinaryExpr()

  // If an element is top level, return true.
  // e.g. in statement "x = y = z = 10;" the first "=" from left is top level.
  bool isTopLevel(const Stmt *stmt) const {
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

  SlangExpr addAndReturnSizeOfInstrExpr(SlangExpr tmpElementVarArr) const {
    std::stringstream ss;

    SlangExpr tmpExpr = convertToTmp(tmpElementVarArr);

    SlangExpr sizeOfExpr;
    ss << "expr.SizeOfE(" << tmpExpr.expr;
    ss << ", " << tmpElementVarArr.locStr << ")";
    sizeOfExpr.expr = ss.str();
    sizeOfExpr.qualType = FD->getASTContext().UnsignedIntTy;
    sizeOfExpr.compound = true;
    sizeOfExpr.locStr = tmpElementVarArr.locStr;

    SlangExpr slangExpr = convertToTmp(sizeOfExpr);

    return slangExpr;
  }

  // BOUND END  : helper_routines
};
} // anonymous namespace

// static_members initialized
SlangTranslationUnit SlangGenAstChecker::stu = SlangTranslationUnit();
const FunctionDecl *SlangGenAstChecker::FD = nullptr;

// Register the Checker
void ento::registerSlangGenAstChecker(CheckerManager &mgr) {
  mgr.registerChecker<SlangGenAstChecker>();
}
