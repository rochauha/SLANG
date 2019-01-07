#!/usr/bin/env python3

# MIT License
# Copyright (c) 2018 Anshuman Dhuliya

from typing import Dict

import span.ir.types as types
import span.ir.op as op
import span.ir.expr as expr
import span.ir.instr as instr

import span.sys.graph as graph
import span.sys.universe as universe

# analysis unit name
name = "span.tests.test6"
description = """
Loop with pointers and expressions,
Different format specification for span.tests.test5 code.

 1: a = 11;
 2: u = &a;
 3: input(cond); // special instruction
 4: while cond: // `cond` value is undeterministic.
 5:   tmp = *u; // point-of-interest
 6:   b = tmp % 2;
 7:   if b:
 8:     b = 15;
 9:     u = &b;
..:   else:
10:     b = 16;
11: noop();
"""

all_vars: Dict[types.VarNameT, types.ReturnT] = {
  "v:main:u": types.Ptr(to=types.Int),
  "v:main:a": types.Int,
  "v:main:b": types.Int,
  "v:main:tmp": types.Int,
  "v:main:cond": types.Int,
} # end all_vars dict

all_func: Dict[types.FuncNameT, graph.FuncNode] = {
  "f:main":
    graph.FuncNode(
      name= "f:main",
      params= [],
      returns= types.Int,
      basic_blocks= {
        1: graph.BB([ # 1 is always start/entry BB. (REQUIRED)
          instr.AssignI(expr.VarE("v:main:a"), expr.LitE(11)),
          instr.AssignI(expr.VarE("v:main:u"), expr.UnaryE(op.AddrOf, expr.VarE("v:main:a"))),
          instr.InputI(expr.VarE("v:main:cond")),
        ]),
        2: graph.BB([
          instr.CondI(expr.VarE("v:main:cond")),
        ]),
        3: graph.BB([
          instr.AssignI(expr.VarE("v:main:tmp"), expr.UnaryE(op.Deref, expr.VarE("v:main:u"))),
          instr.AssignI(expr.VarE("v:main:b"), expr.BinaryE(expr.VarE("v:main:tmp"), op.Modulo, expr.LitE(2))),
          instr.CondI(expr.VarE("v:main:b")),
        ]),
        4: graph.BB([
          instr.AssignI(expr.VarE("v:main:b"), expr.LitE(15)),
          instr.AssignI(expr.VarE("v:main:u"), expr.UnaryE(op.AddrOf, expr.VarE("v:main:b"))),
        ]),
        5: graph.BB([
          instr.AssignI(expr.VarE("v:main:b"), expr.LitE(16))
        ]),
        -1: graph.BB(), # -1 is end/exit block (REQUIRED, if more than one BB present)
      },
      bb_edges= [
        graph.BbEdge(1, 2, graph.UnCondEdge),
        graph.BbEdge(2, -1, graph.FalseEdge),
        graph.BbEdge(2, 3, graph.TrueEdge),
        graph.BbEdge(3, 4, graph.TrueEdge),
        graph.BbEdge(3, 5, graph.FalseEdge),
        graph.BbEdge(4, 2, graph.UnCondEdge),
        graph.BbEdge(5, 2, graph.UnCondEdge),
      ],
    ),
} # end all_func dict.

# Always build the universe from a 'program' module.
# Initialize the universe with program in this module.
universe.build(name, description, all_vars, all_func)
