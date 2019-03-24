//===----------------------------------------------------------------------===//
//  MIT License.
//  Copyright (c) 2019 The SLANG Authors.
//
//  Author: Ronak Chauhan (r.chauhan@somaiya.edu)
//  Author: Anshuman Dhuliya (dhuliya@cse.iitb.ac.in)
//
//===----------------------------------------------------------------------===//
// SlangExpr stores intermediate slang expressions.
//===----------------------------------------------------------------------===//

#ifndef SLANG_EXPR_H
#define SLANG_EXPR_H

#include <string>
#include <vector>
#include "clang/AST/Type.h"

using namespace clang;

namespace slang {
// SlangExpr class to store converted expression info.
class SlangExpr {
  public:
    std::string expr;
    bool compound;
    QualType qualType;
    std::vector<std::string> slangStmts;

    bool nonTmpVar;
    uint64_t varId;

    uint64_t locId; // (line_32 << 32) | col_32

    SlangExpr();
    SlangExpr(std::string e, bool compnd, QualType qt);
    std::string toString();
    void addSlangStmtBack(std::string slangStmt);
    void addSlangStmtsBack(std::vector<std::string> &slangStmts);
    void addSlangStmtsFront(std::vector<std::string> &slangStmts);
    void addSlangStmtFront(std::string slangStmt);
    bool isNonTmpVar();
};
} // namespace slang

#endif // SLANG_EXPR_H
