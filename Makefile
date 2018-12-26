.PHONY: replace format test

replace:
	cp CFG-plugin/MyDebugCheckers.cpp \
~/.itsoflife/local/packages-live/llvm-clang6/llvm/tools/clang/lib/StaticAnalyzer/Checkers/MyDebugCheckers.cpp

format:
	clang-format --style=LLVM -i CFG-plugin/MyDebugCheckers.cpp

test:
	clang -cc1 -analyze -analyzer-checker=debug.MyDumpCFG -std=c99 tests/test.c