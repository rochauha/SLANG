#!/usr/bin/env bash

# Copies the following files (as of now)
# SlangCheckers/MyScratchpadChecker.cpp  #AD
# SlangCheckers/SlangGenChecker.cpp  #AD
# SlangCheckers/SlangBugReporterChecker.cpp  #AD
# SlangCheckers/MyOwnChecker.cpp  #AD
# SlangCheckers/MyTraverseAST.cpp  #AD
# SlangCheckers/SlangTranslationUnit.cpp #AD
# SlangCheckers/SlangExpr.cpp #AD
# SlangCheckers/SlangUtil.cpp #AD

cp -R SlangCheckers /home/codeman/.itsoflife/local/packages-live/llvm-clang6/llvm/tools/clang/lib/StaticAnalyzer/Checkers
