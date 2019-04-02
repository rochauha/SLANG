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

#include "SlangUtil.h"

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

// Generate the SPAN IR from Clang AST.
namespace {
  // the numbering 0,1,2 is important.
  enum EdgeLabel { FalseEdge = 0, TrueEdge = 1, UnCondEdge = 2 };
  enum SlangRecordKind { Struct = 0, Union = 1 };

  // earlier it was named SlangExpr
  class SlangBB {
  public:
    std::string label;
    std::vector<std::string> slangStmts;

    std::string trueLabel;
    std::string falseLabel;
    std::string unCondLabel;

    SlangBB(): slangStmts{} {
      label = "";
      trueLabel = falseLabel = unCondLabel = "";
    }

    SlangBB(std::string e, bool compnd, QualType qt) {
      expr = e;
      compound = compnd;
      qualType = qt;
    }

    // is this object empty?
    bool isEmpty() {
      return slangStmts.empty();
    }

    std::string toString() {
      std::stringstream ss;
      ss << "SlangBB: [" << label << "]\n";
      ss << "Succ: " << trueLabel << ", " << falseLabel;
      ss << ", " << unCondLabel << "\n";

      if(!slangStmts.empty()) {
        for (auto& slangStmt: slangStmts) {
          ss << slangStmt << "\n";
        }
      }

      ss << "\n";
      return ss.str();
    }

    void addSlangStmtBack(std::string slangStmt) { slangStmts.push_back(slangStmt); }

    void addSlangStmtsBack(std::vector<std::string> &slangStmts) {
      for (std::string &slangStmt : slangStmts) {
        this->slangStmts.push_back(slangStmt);
      }
    }

    void addSlangStmtsFront(std::vector<std::string> &slangStmts) {
      std::vector<std::string>::iterator it1;
      for (auto it2 = slangStmts.end() - 1; it2 != slangStmts.begin() - 1; --it2) {
        it1 = this->slangStmts.begin();
        this->slangStmts.insert(it1, (*it2));
      }
    }

    void addSlangStmtFront(std::string slangStmt) {
      std::vector<std::string>::iterator it;
      it = slangStmts.begin();
      slangStmts.insert(it, slangStmt);
    }
  }; // class SlangBB

  // Graph of SlangBB
  class SlangGraph {
    // each graph has to have a unique start and a unique end bb
    // or no bb at all
  public:
    bool inOpen;
    bool outOpen;
    std::string label; // label of the entry bb
    uint32_t totalBlocks;
    SlangBB *entry;
    SlangBB *exit;
    std::vector<SlangBB*> basicBlocks;

    std::string expr;
    std::string locStr;
    bool compound;
    QualType qualType;
    bool nonTmpVar;
    uint64_t varId;

    SlangGraph() {
      inOpen = outOpen = true;
      label = "";
      totalBlocks = 0;
      entry = exit = nullptr;
      expr = "";
      locStr = "";
      compound = false;
      qualType = QualType();
      nonTmpVar = false;
      varId = 0;
    };

    void copyGraph(SlangGraph& slangGraph) {
      inOpen = slangGraph.inOpen;
      outOpen = slangGraph.outOpen;
      label = slangGraph.label;
      totalBlocks = slangGraph.totalBlocks;
      entry = slangGraph.entry;
      exit = slangGraph.exit;
      qualType = slangGraph.qualType;
      nonTmpVar = slangGraph.nonTmpVar;
      varId = slangGraph.varId;
    }

    // is it just an expression
    bool isExpr() {
      if (expr != "") {
        return true;
      }
      return false;
    }

    bool mergeGraph(SlangGraph &slangGraph) {
      bool merged = false;

      if (outOpen) {
        if (!slangGraph.isExpr()) {
          if (exit == nullptr) {
            // i.e. current graph object is empty, just copy
            copyGraph(slangGraph);
          } else {
            exit->unCondLabel = slangGraph.label;
            totalBlocks += slangGraph.totalBlocks;
          }
        }
        merged = true;
      }

      return merged;
    }

    std::string toString() {
      std::stringstream ss;
      ss << "SlangGraph [" << label << "]\n";

      // expression part
      if (isExpr()) {
        ss << "  Expr     : " << expr << "\n";
        ss << "  ExprType : " << qualType.getAsString() << "\n";
        ss << "  Compound : " << (compound ? "true" : "false") << "\n";
        ss << "  NonTmpVar: " << (nonTmpVar ? "true" : "false") << "\n";
        ss << "  VarId    : " << varId << "\n";
      }

      // basic block part
      if (entry != nullptr) {
        ss << "  OutOpen  : " << (outOpen ? "true" : "false") << "\n";
        ss << "  Blocks   : " << totalBlocks << "\n";
        for (auto slangBB : basicBlocks) {
          if (slangBB == entry) {
            ss << "ENTRY BB\n";
          } else if (slangBB == exit) {
            ss << "EXIT BB\n";
          }
          ss << slangBB->toString();
        }
      }

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
      // specially for anonymous field names (needed in member expressions)
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
    int32_t currBbId;
    int32_t nextBbId;
    const CFGBlock *currBb; // the current bb being converted
    const Stmt *lastDeclStmt;

    // stores bbEdges(s); entry bb id is mapped to -1
    std::vector<std::pair<int32_t, std::pair<int32_t, EdgeLabel>>> bbEdges;
    // stmts in bb; entry bb id is mapped to -1, others remain the same
    std::unordered_map<int32_t, std::vector<std::string>> bbStmts;

    SlangFunc() {
      paramNames = std::vector<std::string>{};
      tmpVarCount = 0;
      currBbId = 0;
      nextBbId = 0;
    }
  }; // class SlangFunc

  class SlangRecordField {
  public:
    bool anonymous;
    std::string name;
    std::string typeStr;
    QualType type;

    SlangRecordField() : anonymous{false},
                         name{""}, typeStr{""}, type{QualType()} {}

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
    std::vector<SlangRecordField> fields;
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

    std::vector<SlangRecordField> getFields() const { return fields; }

    std::string toString() {
      std::stringstream ss;
      ss << NBSP6;
      ss << ((recordKind == Struct) ? "types.Struct(\n" : "types.Union(\n");

      ss << NBSP8 << "name = ";
      ss << "\"" << name << "\""
         << ",\n";

      std::string suffix = ",\n";
      ss << NBSP8 << "fields = [\n";
      for (auto field : fields) {
        ss << NBSP10 << field.toString() << suffix;
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
    std::string fileName; // the current translation unit file name
    SlangFunc *currFunc; // the current function being translated

    // to uniquely name anonymous records (see getNextRecordId())
    int32_t recordId;

    // maps a unique variable id to its SlangVar.
    std::unordered_map<uint64_t, SlangVar> varMap;
    // map of var-name to a count:
    // used in case two local variables have same name (blocks)
    std::unordered_map<std::string, int32_t> varNameMap;
    // contains functions
    std::unordered_map<uint64_t, SlangFunc> funcMap;
    // contains structs
    std::unordered_map<uint64_t, SlangRecord> recordMap;

    // stack to help convert ast structure to 3-address code.
    std::vector<const Stmt *> mainStack;
    // tracks variables that become dirty in an expression
    std::unordered_map<uint64_t, SlangBB> dirtyVars;

    // the sequence of graphs to be merged from entry and exits.
    std::vector<SlangGraph> slangGraphs;
    std::unordered_map<std::string, SlangBB> slangBbMap;

    std::vector<std::string> edgeLabels;

    SlangTranslationUnit()
        : currFunc{nullptr}, varMap{}, funcMap{},
        mainStack{}, dirtyVars{}, edgeLabels{3} {
      fileName = "";
      edgeLabels[FalseEdge] = "FalseEdge";
      edgeLabels[TrueEdge] = "TrueEdge";
      edgeLabels[UnCondEdge] = "UnCondEdge";
    }

    void clearMainStack() { mainStack.clear(); }

    // clear the buffer for the next function.
    void clear() {
      varMap.clear();
      dirtyVars.clear();
      clearMainStack();
      slangGraphs.clear();
      slangExprMap.clear();
    } // clear()

    SlangBB *createSlangBb(std::string label = "") {
      SlangBB slangBb;
      if (label == "") {
        slangBb.label = genNextBbIdStr();
      }

      slangBb.label = label;
      slangBbMap[label] = slangBb;
      return &slangBbMap[label];
    }

    void pushBackFuncParams(std::string paramName) {
      SLANG_TRACE("AddingParam: " << paramName << " to func " << currFunc->name)
      currFunc->paramNames.push_back(paramName);
    }

    void setFuncReturnType(std::string &retType) {
      currFunc->retType = retType;
    }

    void setVariadicness(bool variadic) { currFunc->variadic = variadic; }

    std::string getCurrFuncName() {
      return currFunc->name; // not fullName
    }

    void setCurrBb(const CFGBlock *bb) {
      currFunc->currBbId = bb->getBlockID();
      currFunc->currBb = bb;
    }

    int32_t getCurrBbId() { return currFunc->currBbId; }

    void setNextBbId(int32_t nextBbId) { currFunc->nextBbId = nextBbId; }

    int32_t genNextBbId() {
      // zero is reserved for the exit bb
      currFunc->nextBbId += 1;
      return currFunc->nextBbId;
    }

    std::string genNextBbIdStr() {
      std::stringstream ss;
      ss << genNextBbId();
      return ss.str();
    }

    const CFGBlock *getCurrBb() { return currFunc->currBb; }

    SlangVar &getVar(uint64_t varAddr) {
      // FIXME: there is no check
      return varMap[varAddr];
    }

    void setLastDeclStmtTo(const Stmt *declStmt) {
      currFunc->lastDeclStmt = declStmt;
    }

    const Stmt *getLastDeclStmt() const { return currFunc->lastDeclStmt; }

    bool isNewVar(uint64_t varAddr) {
      return varMap.find(varAddr) == varMap.end();
    }

    uint32_t nextTmpId() {
      currFunc->tmpVarCount += 1;
      return currFunc->tmpVarCount;
    }

    /// Add a new basic block with the given bbId
    void addBb(int32_t bbId) {
      std::vector<std::string> emptyVector;
      currFunc->bbStmts[bbId] = emptyVector;
    }

    void setCurrBbId(int32_t bbId) { currFunc->currBbId = bbId; }

    // bb must already be added
    void addBbStmt(std::string stmt) {
      currFunc->bbStmts[currFunc->currBbId].push_back(stmt);
    }

    // bb must already be added
    void addBbStmts(std::vector<std::string> &slangStmts) {
      for (std::string slangStmt : slangStmts) {
        currFunc->bbStmts[currFunc->currBbId].push_back(slangStmt);
      }
    }

    // bb must already be added
    void addBbStmt(int32_t bbId, std::string slangStmt) {
      currFunc->bbStmts[bbId].push_back(slangStmt);
    }

    // bb must already be added
    void addBbStmts(int32_t bbId, std::vector<std::string> &slangStmts) {
      for (std::string slangStmt : slangStmts) {
        currFunc->bbStmts[bbId].push_back(slangStmt);
      }
    }

    void addBbEdge(
        std::pair<int32_t, std::pair<int32_t, EdgeLabel>> bbEdge) {
      currFunc->bbEdges.push_back(bbEdge);
    }

    void addVar(uint64_t varId, SlangVar &slangVar) {
      varMap[varId] = slangVar;
    }

    bool isRecordPresent(uint64_t recordAddr) {
      return !(recordMap.find(recordAddr) == recordMap.end());
    }

    void addRecord(uint64_t recordAddr, SlangRecord slangRecord) {
      recordMap[recordAddr] = slangRecord;
    }

    SlangRecord& getRecord(uint64_t recordAddr) { return recordMap[recordAddr]; }

    int32_t getNextRecordId() {
      recordId += 1;
      return recordId;
    }

    std::string getNextRecordIdStr() {
      std::stringstream ss;
      ss << getNextRecordId();
      return ss.str();
    }

    // BOUND START: dirtyVars_logic

    void setDirtyVar(uint64_t varId, SlangBB slangExpr) {
      // Clear the value for varId to an empty SlangBB.
      // This forces the creation of a new tmp var,
      // whenever getTmpVarForDirtyVar() is called.
      dirtyVars[varId] = slangExpr;
    }

    // If this function is called dirtyVar dict should already have the entry.
    SlangBB getTmpVarForDirtyVar(uint64_t varId) {
      return dirtyVars[varId];
    }

    bool isDirtyVar(uint64_t varId) {
      return !(dirtyVars.find(varId) == dirtyVars.end());
    }

    void clearDirtyVars() { dirtyVars.clear(); }

    // BOUND END  : dirtyVars_logic

    // BOUND START: dump_routines (to SPAN Strings)

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

    std::string convertBbEdges(SlangFunc &slangFunc) {
      std::stringstream ss;

      for (auto p : slangFunc.bbEdges) {
        ss << NBSP10 << "(" << std::to_string(p.first);
        ss << ", " << std::to_string(p.second.first) << ", ";
        ss << "types." << edgeLabels[p.second.second] << "),\n";
      }

      return ss.str();
    } // convertBbEdges()

    // BOUND END  : dump_routines (to SPAN Strings)

    // BOUND START: helper_functions

    // used only for debugging purposes
    void printMainStack() const {
      std::stringstream ss;
      ss << "MAIN_STACK: [";
      for (const Stmt *stmt : mainStack) {
        ss << stmt->getStmtClassName() << ", ";
      }
      ss << "]\n";
      SLANG_DEBUG(ss.str());
    } // printMainStack()

    void pushToMainStack(const Stmt *stmt) {
      mainStack.push_back(stmt);
    } // pushToMainStack()

    const Stmt *popFromMainStack() {
      if (mainStack.size()) {
        auto stmt = mainStack[mainStack.size() - 1];
        mainStack.pop_back();
        return stmt;
      }
      return nullptr;
    } // popFromMainStack()

    bool isMainStackEmpty() const {
      return mainStack.empty();
    } // isMainStackEmpty()

    // BOUND END  : helper_functions

    // BOUND START: dumping_routines

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
      ss << "# import span.ir.expr as expr\n";
      ss << "# import span.ir.instr as instr\n";
      ss << "# import span.ir.obj as obj\n";
      ss << "# import span.ir.tunit as irTUnit\n";
      ss << "# from span.ir.types import Loc\n";
      ss << "\n";
      ss << "# An instance of span.ir.tunit.TUnit class.\n";
      ss << "irTUnit.TUnit(\n";
      ss << NBSP2 << "name = \"" << fileName << "\",\n";
      ss << NBSP2 << "description = \"Auto-Translated from Clang AST.\",\n";
    } // dumpHeader()

    void dumpFooter(std::stringstream &ss) {
      ss << ") # irTUnit.TUnit() ends\n";
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
      ss << NBSP2 << "allObjs = {\n";
      dumpRecords(ss);
      dumpFunctions(ss);
      ss << NBSP2 << "}, # end allObjs dict\n";
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
        ss << NBSP6 << "obj.Func(\n";

        // fields
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

        // field: basicBlocks
        ss << "\n";
        ss << NBSP8 << "# Note: -1 is always start/entry BB. (REQUIRED)\n";
        ss << NBSP8 << "# Note: 0 is always end/exit BB (REQUIRED)\n";
        ss << NBSP8 << "basicBlocks = {\n";
        for (auto bb : slangFunc.second.bbStmts) {
          ss << NBSP10 << bb.first << ": [\n";
          if (bb.second.size()) {
            for (auto &stmt : bb.second) {
              ss << NBSP12 << stmt << ",\n";
            }
          } else {
            ss << NBSP12 << "instr.NopI()"
               << ",\n";
          }
          ss << NBSP10 << "],\n";
          ss << "\n";
        }
        ss << NBSP8 << "}, # basicBlocks end.\n";

        // fields: bbEdges
        ss << "\n";
        ss << NBSP8 << "bbEdges= {\n";
        ss << convertBbEdges(slangFunc.second);
        ss << NBSP8 << "}, # bbEdges end\n";

        // close this function object
        ss << NBSP6 << "), # " << slangFunc.second.fullName << "() end. \n\n";
      }
    } // dumpFunctions()
  }; // class SlangTranslationUnit

  class SlangGenAstChecker :
      public Checker<check::ASTCodeBody, check::EndOfTranslationUnit> {

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
          stu.fileName = D->getASTContext().getSourceManager().getFilename(D->getLocStart()).str();
      }

      FD = dyn_cast<FunctionDecl>(D);
      handleFunction(funcDecl);
      stu.currFunc = &stu.funcMap[(uint64_t)funcDecl];
    } // checkASTCodeBody()

    // invoked when the whole translation unit has been processed
    void checkEndOfTranslationUnit(const TranslationUnitDecl *TU,
        AnalysisManager &Mgr, BugReporter &BR) const {
      stu.dumpSlangIr();
      SLANG_EVENT("Translation Unit Ended.\n")
      SLANG_EVENT("BOUND END  : SLANG_Generated_Output.\n")
    } // checkEndOfTranslationUnit()

    // BOUND END  : top_level_routines

    // BOUND START: handling_routines

    void handleFunction(const FunctionDecl *funcDecl) const {
      handleFuncNameAndType(funcDecl);

      const Stmt *body = funcDecl->getBody();
      if (body) {
        handleFunctionBody(body);
      } else {
        SLANG_ERROR("No body for function: " << funcDecl->getNameAsString())
      }
    }

    // records the function details
    void handleFuncNameAndType(const FunctionDecl *funcDecl) const {
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

    void handleFunctionBody(const Stmt *body) const {
      SlangGraph slangGraph;

      slangGraph = convertStmt(false, body);

      correctGraph(slangGraph);
    } // handleFunctionBody()

    // record the variable name and type
    void handleVariable(const ValueDecl *valueDecl, std::string funcName) const {
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

    void handleDeclStmt(const DeclStmt *declStmt) const {
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

    // BOUND END  : handling_routines

    // records the control flow graph
    void correctGraph(SlangGraph& slangGraph) {
      // TODO
    }

    // BOUND START: conversion_routines

    // stmtconversion
    SlangGraph convertStmt(bool compound_receiver,
        const Stmt *stmt,
        std::unique_ptr<SlangBB> slangExpr) const {
      SlangGraph slangGraph;

      if (!stmt) {
        return std::move(std::unique_ptr<SlangBB>(
            new SlangBB(NULL_STMT, false, QualType())));
      }

      switch (stmt->getStmtClass()) {
        case Stmt::LabelStmtClass:
          return convertLabel(cast<LabelStmt>(stmt));

        case Stmt::BinaryOperatorClass:
          return convertBinaryOperator(compound_receiver,
              cast<BinaryOperator>(stmt));

        case Stmt::CompoundStmtClass:
          return convertCompoundStmt(compound_receiver,
              cast<CompoundStmt>(stmt));

        case Stmt::DeclStmtClass:
          handleDeclStmt(cast<DeclStmt>(stmt));

        case Stmt::DeclRefExprClass:
          return convertDeclRefExpr(cast<DeclRefExpr>(stmt));

        default:
          SLANG_ERROR("Unhandled_Stmt: " << stmt->getStmtClassName())
      }

      return slangGraph;
    } // convertStmt()

    SlangGraph convertVarDecl(const VarDecl *varDecl, std::string &locStr) const {
      std::stringstream ss;
      SlangGraph slangGraph;

      ss << "expr.VarE(\"" << stu.convertVarExpr((uint64_t)varDecl) << "\"";
      ss << ", " << locStr << ")";
      slangGraph.expr = ss.str();
      slangGraph.compound = false;
      slangGraph.qualType = varDecl->getType();
      slangGraph.nonTmpVar = true;
      slangGraph.varId = (uint64_t)varDecl;

      return slangGraph;
    } // convertVarDecl()

    SlangGraph convertDeclRefExpr(const DeclRefExpr *dre) const {
      SlangGraph slangGraph;
      std::stringstream ss;

      std::string locStr = getLocationString(dre);

      const ValueDecl *valueDecl = dre->getDecl();
      if (isa<VarDecl>(valueDecl)) {
        auto varDecl = cast<VarDecl>(valueDecl);
        slangGraph = convertVarDecl(varDecl, locStr);
        slangGraph.locStr = getLocationString(dre);
        return slangGraph;

      } else if (isa<EnumConstantDecl>(valueDecl)) {
        auto ecd = cast<EnumConstantDecl>(valueDecl);
        return convertEnumConst(ecd, locStr);

      } else if (isa<FunctionDecl>(valueDecl)) {
        auto funcDecl = cast<FunctionDecl>(valueDecl);
        std::string funcName = funcDecl->getNameInfo().getAsString();
        ss << "expr.FuncE(\"" << stu.convertFuncName(funcName) << "\"";
        ss << ", " << locStr << ")";
        slangGraph.expr = ss.str();
        slangGraph.qualType = funcDecl->getType();
        slangGraph.locStr = locStr;
        return slangGraph;

      } else {
        SLANG_ERROR("Not_a_VarDecl.")
        slangGraph.expr = "ERROR:convertDeclRefExpr";
        return slangGraph;
      }
    } // convertDeclRefExpr()

    SlangGraph convertBinaryOperator(bool compound_receiver,
        const BinaryOperator *binOp) {
      SlangGraph slangGraph;

      if (binOp->isAssignmentOp()) {
        return convertAssignmentOp(compound_receiver, binOp);
      } else if (binOp->isCompoundAssignmentOp()) {
        return slangGraph;
        // return convertCompoundAssignmentOp(compound_receiver, binOp);
      } else if (binOp->isLogicalOp()) {
        return slangGraph;
        // return convertLogicalOp(compound_receiver, binOp);
      }

      switch(binOp->getOpcode()) {
        case BO_Add:
          break;
        default:
          break;
      }

      return slangGraph;
    } // convertBinaryOperator()

    SlangGraph convertAssignmentOp(bool compound_receiver,
        const BinaryOperator *binOp) const {
      SlangGraph lhsGraph, rhsGraph;

      auto it = binOp->child_begin();
      const Stmt *lhs = *it;
      const Stmt *rhs = *(++it);

      lhsGraph = convertStmt(false, lhs);
      rhsGraph = convertStmt(lhsGraph.compound, rhs);

      bool merged = lhsGraph.mergeGraph(rhsGraph);

      return lhsGraph;
    } // convertAssignmentOp()

    SlangGraph convertCompoundStmt(bool compound_receiver,
        const CompoundStmt *compoundStmt) const {
      SlangGraph slangGraph;
      SlangGraph currSlangGraph;
      bool merged;

      for (auto it = compoundStmt->body_begin();
            it != compoundStmt->body_end();
            ++it) {
        currSlangGraph = convertStmt(*it);
        merged = slangGraph.mergeGraph(currSlangGraph);

        if(!merged) {
          stu.slangGraphs.push_back(slangGraph);
          slangGraph = currSlangGraph;
        }
      }
    } // convertCompoundStmt()

    SlangGraph convertLabel(const LabelStmt *labelStmt) {
      SlangGraph slangGraph;

      slangGraph.label = labelStmt->getName();
      slangGraph.entry = slangGraph.exit = stu.createSlangBb(slangGraph.label);
      slangGraph.totalBlocks = 1;

      return slangGraph;
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

    std::string convertClangBuiltinType(QualType qt) const {
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

    std::string convertClangRecordType(const RecordDecl *recordDecl) const {
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

    std::string getLocationString(const Stmt *stmt) const {
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

    std::string getLocationString(const RecordDecl *recordDecl) const {
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

    // Remove qualifiers and typedefs
    QualType getCleanedQualType(QualType qt) const {
      if (qt.isNull()) return qt;
      qt = qt.getCanonicalType();
      qt.removeLocalConst();
      qt.removeLocalRestrict();
      qt.removeLocalVolatile();
      return qt;
    }


    // BOUND END  : helper_routines

  };
} // anonymous namespace

// static_members initialized
SlangTranslationUnit SlangGenAstChecker::stu = SlangTranslationUnit();
const FunctionDecl *SlangGenAstChecker::FD = nullptr;

