//===----------------------------------------------------------------------===//
//  MIT License.
//  Copyright (c) 2019 The SLANG Authors.
//
//  Author: Anshuman Dhuliya (dhuliya@cse.iitb.ac.in)
//
//AD If SlangBugReporterChecker class name is added or changed, then also edit,
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
#include "clang/StaticAnalyzer/Core/BugReporter/BugReporter.h"  //AD
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
#include <algorithm>                  //AD

#include "SlangUtil.h"

using namespace clang;
using namespace ento;

// #define LOG_ME(X) if (Utility::debug_mode) Utility::log((X), __FUNCTION__, __LINE__)

// Each bug has to be reported using following necessary six lines
// ----------------------
// LINE 32
// COLUMN 24
// BUG_NAME Dead Store
// BUG_CATEGORY Dead Store
// BUG_MSG Value assigned is not used
// ----------------------
// Note that UNIQUE_ID should not be same for two bugs.

namespace {
    class Bug {
    public:
        uint32_t col;
        uint32_t line;
        Stmt *stmt;
        std::string bugName;
        std::string bugCategory;
        std::string bugMsg;

        Bug() {
            col = line = 0;
            stmt = nullptr;
            bugName = "";
            bugCategory = "";
            bugMsg = "";
        }

        void printLocation() {
            llvm::errs() << "Loc(" << line << ":" << col << ")\n";
        }

        uint64_t getEncodedId() {
            return Bug::genEncodedId(line, col);
        }

        static uint64_t genEncodedId(uint32_t line, uint32_t col) {
            uint64_t encoded = 0;
            encoded |= line;
            encoded <<= 32;
            encoded |= col;
            return encoded;
        }

        std::string getLocStr() {
            std::stringstream ss;
            ss << "(" << line << ", " << col << ")";
            return ss.str();
        }
    };

    class BugRepo {
    public:
        // list of bugs for a particular point in source
        std::unordered_map<uint64_t, std::vector<Bug>> bugVectorMap;
        // stored in vector so that it can be sorted later
        std::vector<uint64_t> bugIds;
        std::string fileName;

        static uint32_t counter;

        void loadBugReports(std::string bugFileName) {
            // read and store the bug reports for the given bugFileName
            std::ifstream inputTxtFile;
            std::string line;
            Bug b;

            inputTxtFile.open(bugFileName);
            if (inputTxtFile.is_open()) {
                llvm::errs() << "SLANG: loaded_file " << bugFileName << "\n";
                while (true) {
                    b = loadSingleBugReport(inputTxtFile);
                    if (b.getEncodedId() == 0) {
                        break; // error in loading bug report
                    }
                    llvm::errs() << "SLANG: loading_bug: " << b.getEncodedId() << "\n";
                    addBug(b);
                }
                inputTxtFile.close();
            } else {
                llvm::errs() << "SLANG: ERROR: Cannot load from file '" << fileName << "'\n";
            }

            llvm::errs() << "SLANG: Total bugs loaded: " << bugIds.size() << "\n";
            // sort the bugs w.r.t line and col num.
            std::sort(bugIds.begin(), bugIds.end());
        }

        Bug loadSingleBugReport(std::ifstream& inputTextFile) {
            Bug b;
            std::string line;

            // get line
            line = getSingleNonBlankLine(inputTextFile);
            if (line.size() == 0) {
                b.line = b.col = 0;
                return b;
            }
            std::stringstream(line) >> b.line;

            // get col
            line = getSingleNonBlankLine(inputTextFile);
            if (line.size() == 0) {
                b.line = b.col = 0;
                return b;
            }
            std::stringstream(line) >> b.col;

            // get bugName
            line = getSingleNonBlankLine(inputTextFile);
            if (line.size() == 0) {
                b.line = b.col = 0;
                return b;
            }
            b.bugName = line;

            // get bugCategory
            line = getSingleNonBlankLine(inputTextFile);
            if (line.size() == 0) {
                b.line = b.col = 0;
                return b;
            }
            b.bugCategory = line;

            // get bugMsg
            line = getSingleNonBlankLine(inputTextFile);
            if (line.size() == 0) {
                b.line = b.col = 0;
                return b;
            }
            b.bugMsg = line;

            return b;
        }

        std::string getSingleNonBlankLine(std::ifstream& inputTextFile) {
            std::string line;

            while(1) {
                std::getline(inputTextFile, line);
                if (!inputTextFile) { //.bad() || !inputTextFile.eof()) {
                    return "";
                }
                line = trim(line);
                line = removeTag(line);
                if (line.size() > 0) break;
            }

            //llvm::errs() << "SLANG: READ: " << line << "\n";
            return line;
        }

        std::string trim(const std::string& str,
                         const std::string& whitespace = " \t") {
            const auto strBegin = str.find_first_not_of(whitespace);
            if (strBegin == std::string::npos)
                return ""; // no content

            const auto strEnd = str.find_last_not_of(whitespace);
            const auto strRange = strEnd - strBegin + 1;

            return str.substr(strBegin, strRange);
        }

        // remove the first word in line
        std::string& removeTag(std::string& line) {
            line.erase(0, line.find(" ")+1);
            return line;
        }

        bool locIdPresent(uint64_t locId) {
            if (bugVectorMap.find(locId) == bugVectorMap.end()) {
                return false;
            } else {
                return true;
            }
        }

        void addBug(Bug b) {
            bugVectorMap[b.getEncodedId()].push_back(b);
            bugIds.push_back(b.getEncodedId());
        }
    };
}

uint32_t BugRepo::counter = 0;

namespace {
    class SlangBugReporterChecker : public Checker<check::ASTCodeBody> {
    public:
        static Decl *D;
        static BugReporter *BR;
        static AnalysisDeclContext *AC;
        static CheckerBase *Checker;
        static BugRepo bugRepo;

        void checkASTCodeBody(const Decl *D, AnalysisManager &mgr,
                              BugReporter &BR) const;

        // handling_routines
        void handleCfg(const CFG *cfg) const;

        void handleBBStmts(const CFGBlock *bb) const;

        void reportBugs() const;
        void reportBug(Stmt *stmt, std::string bugName
                , std::string bugCategory, std::string bugMsg) const;

        uint64_t getStmtLocId(const Stmt *stmt) const;
        void matchStmtToBug(const Stmt *stmt) const;
    }; // class SlangBugReporterChecker
} // anonymous namespace

Decl* SlangBugReporterChecker::D = nullptr;
BugReporter* SlangBugReporterChecker::BR = nullptr;
AnalysisDeclContext* SlangBugReporterChecker::AC = nullptr;
BugRepo SlangBugReporterChecker::bugRepo = BugRepo();

// mainstart, Main Entry Point. Invokes top level Function and Cfg handlers.
// Invoked once for each source translation unit function.
void SlangBugReporterChecker::checkASTCodeBody(const Decl *D, AnalysisManager &mgr,
                                     BugReporter &BR) const {
    SlangBugReporterChecker::D = const_cast<Decl*>(D); // the world is ending
    SlangBugReporterChecker::BR = &BR;
    AC = mgr.getAnalysisDeclContext(D);

    bugRepo.fileName = D->getASTContext().getSourceManager().getFilename(D->getLocStart()).str();
    bugRepo.loadBugReports(bugRepo.fileName + ".bugs");

    if (const CFG *cfg = mgr.getCFG(D)) {
        handleCfg(cfg);
    } else {
        llvm::errs() << "SLANG: ERROR: No CFG for function.\n";
    }

    llvm::errs() << "SLANG: Reporting Bugs.\n";
    reportBugs();
} // checkASTCodeBody()

//BOUND START: handling_routines

void SlangBugReporterChecker::handleCfg(const CFG *cfg) const {
    for (const CFGBlock *bb : *cfg) {
        handleBBStmts(bb);
    }
} // handleCfg()

void SlangBugReporterChecker::handleBBStmts(const CFGBlock *bb) const {
    for (auto elem : *bb) {
        Optional<CFGStmt> CS = elem.getAs<CFGStmt>();
        const Stmt *stmt = CS->getStmt();
        matchStmtToBug(stmt);
    } // for (auto elem : *bb)

    // get terminator too
    const Stmt *terminator = nullptr;
    terminator = (bb->getTerminator()).getStmt();
    if(terminator) {
        matchStmtToBug(terminator);
    }
} // handleBBStmts()

// matches bugs to real statement elements
void SlangBugReporterChecker::matchStmtToBug(const Stmt *stmt) const {
    uint64_t locId;

    locId = getStmtLocId(stmt);
    if(bugRepo.locIdPresent(locId)) {
        for(Bug &bug: bugRepo.bugVectorMap[locId]) {
            bug.stmt = const_cast<Stmt*>(stmt);
            SLANG_DEBUG("Matched Bug at location: " << bug.getLocStr() << "\nto Stmt/Expr: ")
            SLANG_DEBUG((stmt->dump(), ""))
        }
    }
}

uint64_t SlangBugReporterChecker::getStmtLocId(const Stmt *stmt) const {
    uint64_t locId = 0;
    uint32_t line = SlangBugReporterChecker::D->getASTContext().getSourceManager().getExpansionLineNumber(stmt->getLocStart());
    uint32_t col = SlangBugReporterChecker::D->getASTContext().getSourceManager().getExpansionColumnNumber(stmt->getLocStart());

    locId |= line;
    locId <<= 32;
    locId |= col;

    return locId;
}

void SlangBugReporterChecker::reportBugs() const {
    // report all the bugs collected
    for(uint64_t locId: bugRepo.bugIds) {
        for(Bug &bug: bugRepo.bugVectorMap[locId]) {
            if (bug.stmt != nullptr) {
                reportBug(bug.stmt, bug.bugName, bug.bugCategory, bug.bugMsg);
            } else {
                llvm::errs() << "SLANG: Unmatched location: ";
                bug.printLocation();
            }
        }
    }
}

void SlangBugReporterChecker::reportBug(Stmt *stmt,
        std::string bugName, std::string bugCategory, std::string bugMsg) const {

    PathDiagnosticLocation ExLoc =
            PathDiagnosticLocation::createBegin(stmt, BR->getSourceManager(), AC);
    BR->EmitBasicReport(D, this, bugName.c_str(), bugCategory.c_str()
                         , bugMsg.c_str(), ExLoc, stmt->getSourceRange());
}

// Register the Checker
void ento::registerSlangBugReporterChecker(CheckerManager &mgr) {
    mgr.registerChecker<SlangBugReporterChecker>();
}

