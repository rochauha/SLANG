.PHONY: replace format test ast-dump cfg-dump gen_replace simple_replace br_replace br_test

replace:
	cp CFG-plugin/MyDebugCheckers.cpp \
~/.itsoflife/local/packages-live/llvm-clang6/llvm/tools/clang/lib/StaticAnalyzer/Checkers/MyDebugCheckers.cpp

format:
	clang-format --style="{BasedOnStyle: llvm, ColumnLimit: 100, SortIncludes: false}" -i ad/SlangCheckers/SlangGenAstChecker.cpp
# 	clang-format --style="{BasedOnStyle: llvm, IndentWidth: 4, ColumnLimit: 100, SortIncludes: false}" -i ad/SlangCheckers/*.h
# 	clang-format --style="{BasedOnStyle: llvm, IndentWidth: 4, ColumnLimit: 100, SortIncludes: false}" -i rc/*
# 	clang-format --style="{BasedOnStyle: llvm, IndentWidth: 4, ColumnLimit: 100, SortIncludes: false}" -i CFG-plugin/SlangGenChecker.cpp
# 	clang-format --style="{BasedOnStyle: llvm, IndentWidth: 4, ColumnLimit: 100, SortIncludes: false}" -i CFG-plugin/MyDebugCheckers.cpp
test:
	clang -cc1 -analyze -analyzer-checker=debug.MyDumpCFG -std=c99 tests/test.c

cfg-dump:
	clang -cc1 -analyze -analyzer-checker=debug.ViewCFG -std=c99 tests/test.c

ast-dump:
	clang -Xclang -ast-dump -fsyntax-only -std=c99 tests/test.c

gen_test:
	clang -cc1 -analyze -analyzer-checker=debug.SlangGenAst -std=c99 tests/test.c

gen_replace:
	cp ad/SlangCheckers/SlangGenAstChecker.cpp \
~/.itsoflife/local/packages-live/llvm-clang6/llvm/tools/clang/lib/StaticAnalyzer/Checkers/SlangCheckers/SlangGenAstChecker.cpp	

simple_test:
	clang -cc1 -analyze -analyzer-checker=debug.TraverseAST -std=c99 tests/test.c

simple_replace:
	cp ad/MyTraverseAST.cpp \
~/.itsoflife/local/packages-live/llvm-clang6/llvm/tools/clang/lib/StaticAnalyzer/Checkers/MyTraverseAST.cpp

br_test:
	scan-build -V -enable-checker debug.SlangBugReport clang -std=c99 tests/test_spanreport.c	
# 	clang -cc1 -analyze -analyzer-checker=debug.SlangBugReport -std=c99 tests/test.c
br_replace:
	cp ad/SlangCheckers/SlangBugReporterChecker.cpp \
~/.itsoflife/local/packages-live/llvm-clang6/llvm/tools/clang/lib/StaticAnalyzer/Checkers/SlangCheckers/SlangBugReporterChecker.cpp
