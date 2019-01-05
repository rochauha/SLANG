SLANG
=======
A bridge between SPAN and Clang.

Author: Anshuman Dhuliya (dhuliya@cse.iitb.ac.in)

Summary
--------
The SLANG project interfaces SPAN (Synergistic Program Analyzer) with Clang. Specifically it does the following,

1. Converts Clang's AST to CFG based SPAN IR.
2. Processes SPAN resuts to generate Clang checkers reports.

###What is the supported Clang version?

Currently the system is tested to work on Clang 6.0.1 only. We have plans to shift to Clang 7.0.1 in the near future.

###How to use?

We require that clang/llvm has been built from source and `MY_LLVM_DIR` points to the directory housing the `build` as well as the `llvm` source directory.

Now to use `CFG-plugin/MyDebugChecker.cpp` do the following,

    $ cp CFG-plugin/MyDebugChecker.cpp $MY_LLVM_DIR/llvm/tools/clang/lib/StaticAnalyzer/Checkers/

or you can also create a symbolic link with the same name in the Clang's source, to point to the `cpp` file in this repo (recommended).

Modify `$MY_LLVM_DIR/llvm/tools/clang/lib/StaticAnalyzer/Checkers/CMakeLists.txt` to add the name of the new source file so that `cmake` can pick it up. Add the file name just below  the line that reads `DebugCheckers.cpp`, maintaining the indentation. The file would then read something like this,

    $ cd $MY_LLVM_DIR/llvm/tools/clang/lib/StaticAnalyzer/Checkers/
    $ cat CMakeLists.txt
    ...
      DebugCheckers.cpp
      MyDebugCheckers.cpp
    ...

Modify `$MY_LLVM_DIR/llvm/tools/clang/include/clang/StaticAnalyzer/Checkers/Checkers.td` and add the three line `MyDebugChecker` entry just below the `CFGViewer` entry, as shown below,

    $ cd $MY_LLVM_DIR/llvm/tools/clang/include/clang/StaticAnalyzer/Checkers/
    $ cat Checkers.td
    ...
    def CFGViewer : Checker<"ViewCFG">,
      HelpText<"View Control-Flow Graphs using GraphViz">,
      DescFile<"DebugCheckers.cpp">;
    
    def MyCFGDumper : Checker<"MyDumpCFG">,
      HelpText<"Checker to convert Clang AST to SPAN IR and dump it.">,
      DescFile<"MyDebugCheckers.cpp">;
    ...


Now go to the `$MY_LLVM_DIR/build` directory and build the system using `make` or `ninja` (which ever you have used to build clang/llvm system).

Once done you can use the checker as any other checker in the system. The invocation name of the checker is `debug.MyDumpCFG`.

