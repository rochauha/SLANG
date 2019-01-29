// MIT License.
// Copyright (c) 2019 The SLANG Authors.

// test1.c: A sample test program. (This file)
// test1.py: SPAN IR module for this file.

int g;
void main(int argc, char **argv) {
  int x;
  x = 20;
}

// BB1
// Unhandled DeclStmt
// DeclStmt 0x558d17fbc898
// `-VarDecl 0x558d17fbc838  used x 'int'
// 
// Unhandled IntegerLiteral
// IntegerLiteral 0x558d17fbc8d8 'int' 20
// 
// Unhandled DeclRefExpr
// DeclRefExpr 0x558d17fbc8b0 'int' lvalue Var 0x558d17fbc838 'x' 'int'
// 
// Unhandled BinaryOperator
// BinaryOperator 0x558d17fbc8f8 'int' '='
// |-DeclRefExpr 0x558d17fbc8b0 'int' lvalue Var 0x558d17fbc838 'x' 'int'
// `-IntegerLiteral 0x558d17fbc8d8 'int' 20

