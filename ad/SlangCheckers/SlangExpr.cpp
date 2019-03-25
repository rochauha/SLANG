//===----------------------------------------------------------------------===//
//  MIT License.
//  Copyright (c) 2019 The SPAN Authors.
//
//  Author: Anshuman Dhuliya (dhuliya@cse.iitb.ac.in)
//
//===----------------------------------------------------------------------===//
// SlangExpr stores intermediate slang expressions.
//===----------------------------------------------------------------------===//

#include "SlangExpr.h"
#include <string>
#include <vector>
#include <sstream>
#include "clang/AST/Type.h"

using namespace clang;

slang::SlangExpr::SlangExpr() {
    expr = "";
    compound = true;

    nonTmpVar = false;
    varId = 0;
}

slang::SlangExpr::SlangExpr(std::string e, bool compnd, QualType qt) {
    expr = e;
    compound = compnd;
    qualType = qt;
}

std::string slang::SlangExpr::toString() {
    std::stringstream ss;
    ss << "SlangExpr(";
    ss << expr << ", " << compound << ", ";
    ss << qualType.getAsString();
    ss << ")\n";
    return ss.str();
}

void slang::SlangExpr::addSlangStmtBack(std::string slangStmt) { slangStmts.push_back(slangStmt); }

void slang::SlangExpr::addSlangStmtsBack(std::vector<std::string> &slangStmts) {
    for (std::string &slangStmt : slangStmts) {
        this->slangStmts.push_back(slangStmt);
    }
}

void slang::SlangExpr::addSlangStmtsFront(std::vector<std::string> &slangStmts) {
    std::vector<std::string>::iterator it1;
    for (auto it2 = slangStmts.end() - 1; it2 != slangStmts.begin() - 1; --it2) {
        it1 = this->slangStmts.begin();
        this->slangStmts.insert(it1, (*it2));
    }
}

void slang::SlangExpr::addSlangStmtFront(std::string slangStmt) {
    std::vector<std::string>::iterator it;
    it = slangStmts.begin();
    slangStmts.insert(it, slangStmt);
}

bool slang::SlangExpr::isNonTmpVar() { return nonTmpVar; }
