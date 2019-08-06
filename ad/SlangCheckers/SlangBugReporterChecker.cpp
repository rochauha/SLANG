//===----------------------------------------------------------------------===//
//  MIT License.
//  Copyright (c) 2019 The SLANG Authors.
//
//  Author: Anshuman Dhuliya (dhuliya@cse.iitb.ac.in)
//  Author: Ronak Chauhan (r.chauhan@somaiya.edu)
//
// AD If SlangBugReporterChecker class name is added or changed, then also edit,
// AD ../../../include/clang/StaticAnalyzer/Checkers/Checkers.td
//
//===----------------------------------------------------------------------===//
//

// #include "ClangSACheckers.h"
#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/AST/Decl.h" //AD
#include "clang/AST/Expr.h" //AD
#include "clang/AST/Stmt.h" //AD
#include "clang/Analysis/Analyses/Dominators.h"
#include "clang/Analysis/Analyses/LiveVariables.h"
#include "clang/Analysis/CallGraph.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugType.h"
#include "clang/StaticAnalyzer/Core/BugReporter/BugReporter.h" //AD
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

// Each bug has the following format (example)
// ----------------------
// START
// NAME Dead Store
// CATEGORY Dead Variable
//
// LINE 10
// COLUMN 3
// MSG x is not used ahead.
//
//
// LINE 10
// COLUMN 7
// MSG y is not used ahead.
//
// END
// ----------------------

namespace {

class BugMessage {
    uint32_t line;
    uint32_t col;
    std::string messageString;
    Stmt *stmt;

  public:
    BugMessage() {
        line = col = 0;
        messageString = "";
        stmt = nullptr;
    }

    BugMessage(uint32_t line, uint32_t col, std::string messageString) {
        this->line = line;
        this->col = col;
        this->messageString = messageString;
        stmt = nullptr;
    }

    uint64_t genEncodedId() const {
        uint64_t encoded = 0;
        encoded |= line;
        encoded <<= 32;
        encoded |= col;
        return encoded;
    }

    Stmt *getStmt() const { return stmt; }

    void setStmt(Stmt *stmt) { this->stmt = stmt; }

    std::string getMessageString() const { return messageString; }

    bool isEmpty() const { return line == 0 && col == 0 && messageString.length() == 0; }

    void dump() const {
        llvm::errs() << "LINE" << " " << line << "\n";
        llvm::errs() << "COLUMN" << " " << col << "\n";
        llvm::errs() << "MSG" << " " << messageString << "\n";
        llvm::errs() << "STMT :\n";
        if (stmt) {
            stmt->dump();
        } else {
            llvm::errs() << "STMT is nullptr\n";
        }
    }
};

class Bug {
  public:
    std::string bugName;
    std::string bugCategory;
    std::vector<BugMessage> messages;

    Bug() {
        bugName = "";
        bugCategory = "";
    }

    Bug(std::string bugName, std::string bugCategory, std::vector<BugMessage> messages) {
        this->bugName = bugName;
        this->bugCategory = bugCategory;
        this->messages = messages;
    }

    // less than operator based on encoded id of first message
    bool operator<(const Bug &rhs) const {
        return this->messages[0].genEncodedId() < rhs.messages[0].genEncodedId();
    }

    bool isEmpty() const { return bugName == "" && bugCategory == ""; }

    void dump() const {
        llvm::errs() << "START" << "\n";
        llvm::errs() << "NAME" << " " << bugName << "\n";
        llvm::errs() << "CATEGORY" << " " << bugCategory << "\n";
        for (int i = 0; i < (int)messages.size(); ++i) {
            messages[i].dump();
        }
        llvm::errs() << "END" << "\n";
    }
};

class BugRepo {
  public:
    // list of bugs for a particular point in source
    std::vector<Bug> bugVector;
    // stored in vector so that it can be sorted later
    std::string fileName;

    void loadBugReports(std::string bugFileName) {
        // read and store the bug reports for the given bugFileName
        std::ifstream inputTextFile;
        std::string line;
        Bug b;

        inputTextFile.open(bugFileName);
        if (inputTextFile.is_open()) {
            llvm::errs() << "SLANG: loaded_file " << bugFileName << "\n";
            while (true) {
                Bug b = parseSingleBug(inputTextFile);
                if (b.isEmpty()) {
                    break;
                }
                addBug(b);
            }
            inputTextFile.close();
        } else {
            llvm::errs() << "SLANG: ERROR: Cannot load from file '" << fileName << "'\n";
        }

        llvm::errs() << "SLANG: Total bugs loaded: " << bugVector.size() << "\n";
    }

    // trim spaces
    std::string trim(const std::string &str, const std::string &whitespace = " \t") {
        const auto strBegin = str.find_first_not_of(whitespace);
        if (strBegin == std::string::npos)
            return ""; // no content

        const auto strEnd = str.find_last_not_of(whitespace);
        const auto strRange = strEnd - strBegin + 1;

        return str.substr(strBegin, strRange);
    }

    // remove the first word in line
    std::string &removeTag(std::string &line) {
        line.erase(0, line.find(" ") + 1);
        return line;
    }

    std::string getSingleNonBlankLine(std::ifstream &inputTextFile) {
        std::string line;
        while (true) {
            std::getline(inputTextFile, line);
            if (!inputTextFile) { //.bad() || !inputTextFile.eof()) {
                return "";
            }
            line = trim(line);
            line = removeTag(line);
            if (line.size() > 0)
                break;
        }
        return line;
    }

    bool isBugHeader(std::string line) { return line == "START"; }

    bool isBugEnd(std::string &line) { return line == "END" || line == ""; }

    BugMessage parseSingleBugMessage(std::ifstream &inputTextFile) {
        uint32_t line;
        uint32_t col;
        std::string message;

        std::string lineStr = getSingleNonBlankLine(inputTextFile);
        if (isBugEnd(lineStr))
            return BugMessage();

        std::string colStr = getSingleNonBlankLine(inputTextFile);
        message = getSingleNonBlankLine(inputTextFile);

        line = std::stoi(lineStr, nullptr);
        col = std::stoi(colStr, nullptr);

        return BugMessage(line, col, message);
    }

    Bug parseSingleBug(std::ifstream &inputTextFile) {
        std::string headerStr = getSingleNonBlankLine(inputTextFile);
        if (!isBugHeader(headerStr)) {
            return Bug();
        }
        std::string bugName = getSingleNonBlankLine(inputTextFile);
        std::string bugCategory = getSingleNonBlankLine(inputTextFile);

        std::vector<BugMessage> bugMessageVector;
        while (true) {
            BugMessage bugMsg = parseSingleBugMessage(inputTextFile);
            if (bugMsg.isEmpty()) {
                break;
            }
            bugMessageVector.push_back(bugMsg);
        }
        return Bug(bugName, bugCategory, bugMessageVector);
    }

    void addBug(Bug b) { bugVector.push_back(b); }
};
} // namespace

namespace {
class SlangBugReporterChecker : public Checker<check::ASTCodeBody> {
  public:
    static Decl *D;
    static BugReporter *BR;
    static AnalysisDeclContext *AC;
    static CheckerBase *Checker;
    static BugRepo bugRepo;

    void checkASTCodeBody(const Decl *D, AnalysisManager &mgr, BugReporter &BR) const;

    // handling_routines
    void handleCfg(const CFG *cfg) const;
    void handleBBStmts(const CFGBlock *bb) const;
    uint64_t getStmtLocId(const Stmt *stmt) const;
    void matchStmtToBug(const Stmt *stmt) const;
    void reportBugs() const;
    void generateSingleBugReport(Bug &b) const;
}; // class SlangBugReporterChecker
} // anonymous namespace

Decl *SlangBugReporterChecker::D = nullptr;
BugReporter *SlangBugReporterChecker::BR = nullptr;
AnalysisDeclContext *SlangBugReporterChecker::AC = nullptr;
BugRepo SlangBugReporterChecker::bugRepo = BugRepo();

// mainstart, Main Entry Point. Invokes top level Function and Cfg handlers.
// Invoked once for each source translation unit function.
void SlangBugReporterChecker::checkASTCodeBody(const Decl *D, AnalysisManager &mgr,
                                               BugReporter &BR) const {
    SlangBugReporterChecker::D = const_cast<Decl *>(D); // the world is ending
    SlangBugReporterChecker::BR = &BR;
    AC = mgr.getAnalysisDeclContext(D);

    bugRepo.fileName = D->getASTContext().getSourceManager().getFilename(D->getBeginLoc()).str();
    bugRepo.loadBugReports(bugRepo.fileName + ".spanreport");

    if (const CFG *cfg = mgr.getCFG(D)) {
        handleCfg(cfg);
    } else {
        llvm::errs() << "SLANG: ERROR: No CFG for function.\n";
    }
    reportBugs();
} // checkASTCodeBody()

// BOUND START: handling_routines

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
    if (terminator) {
        matchStmtToBug(terminator);
    }
} // handleBBStmts()

// matches bugs to real statement elements
void SlangBugReporterChecker::matchStmtToBug(const Stmt *stmt) const {
    uint64_t locId = getStmtLocId(stmt);
    for (int i = 0; i < (int)bugRepo.bugVector.size(); ++i) {
        Bug currentBug = bugRepo.bugVector[i];
        for (int j = 0; j < (int)currentBug.messages.size(); ++j) {
            if (locId == currentBug.messages[j].genEncodedId()) {
                currentBug.messages[j].setStmt(const_cast<Stmt *>(stmt));
            }
        }
        bugRepo.bugVector[i] = currentBug;
    }
}

uint64_t SlangBugReporterChecker::getStmtLocId(const Stmt *stmt) const {
    uint64_t locId = 0;
    uint32_t line =
        SlangBugReporterChecker::D->getASTContext().getSourceManager().getExpansionLineNumber(
            stmt->getBeginLoc());
    uint32_t col =
        SlangBugReporterChecker::D->getASTContext().getSourceManager().getExpansionColumnNumber(
            stmt->getBeginLoc());

    locId |= line;
    locId <<= 32;
    locId |= col;

    return locId;
}

void SlangBugReporterChecker::reportBugs() const {
    
    // sort bugs based on location
    std::sort(bugRepo.bugVector.begin(), bugRepo.bugVector.end());
    
    // report all the bugs collected
    for (int i = 0; i < (int)bugRepo.bugVector.size(); ++i) {
        Bug currentBug = bugRepo.bugVector[i];
        generateSingleBugReport(currentBug);
    }
}

void SlangBugReporterChecker::generateSingleBugReport(Bug &bug) const {
    llvm::errs() << "\nSLANG: Generating report for:\n";
    bug.dump();

    BugType *bt = new BugType(this->getCheckName(), llvm::StringRef(bug.bugName),
                              llvm::StringRef(bug.bugCategory));

    BR->Register(bt);

    std::string description = bug.messages[0].getMessageString();

    if (bug.messages[0].getStmt()) {
        // BugReport starts at location of first message
        PathDiagnosticLocation startLoc =
            PathDiagnosticLocation::createBegin(bug.messages[0].getStmt(), BR->getSourceManager(), AC);
        auto R = llvm::make_unique<BugReport>(*bt, llvm::StringRef(description), startLoc);

        for (size_t i = 1; i < bug.messages.size(); ++i) {
            Stmt *currentStmt = bug.messages[i].getStmt();
            if (currentStmt) {
                std::string currentMessage = bug.messages[i].getMessageString();

                PathDiagnosticLocation currentLoc =
                    PathDiagnosticLocation::createBegin(currentStmt, BR->getSourceManager(), AC);

                R->addNote(llvm::StringRef(currentMessage), currentLoc);
            } else {
                llvm::errs() << "No Stmt found\n";
                bug.messages[i].dump();
            }
        }
        BR->emitReport(std::move(R));
        llvm::errs() << "SLANG : Report Created\n";
    }
}

// Register the Checker
void ento::registerSlangBugReporterChecker(CheckerManager &mgr) {
    mgr.registerChecker<SlangBugReporterChecker>();
}
