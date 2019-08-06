#!/usr/bin/env bash

# Copies the following files (as of now)
# SlangCheckers/MyScratchpadChecker.cpp  #AD
# SlangCheckers/SlangGenChecker.cpp  #AD
# SlangCheckers/SlangGenAstChecker.cpp  #AD
# SlangCheckers/SlangBugReporterChecker.cpp  #AD
# SlangCheckers/MyOwnChecker.cpp  #AD
# SlangCheckers/MyTraverseAST.cpp  #AD
# SlangCheckers/SlangTranslationUnit.cpp #AD
# SlangCheckers/SlangExpr.cpp #AD
# SlangCheckers/SlangUtil.cpp #AD

cp -R /home/codeman/itsoflife/mydata/local/packages-live/llvm-clang8.0.1/llvm/tools/clang/lib/StaticAnalyzer/Checkers/SlangCheckers .
