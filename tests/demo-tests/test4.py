#!/usr/bin/env python3

# MIT License.
# Copyright (c) 2019 The SLANG Authors.

from typing import Dict

import span.ir.types as types
import span.ir.op as op
import span.ir.expr as expr
import span.ir.instr as instr

import span.sys.graph as graph
import span.sys.universe as universe

# analysis unit name
name = "span.tests.test4"
description = """
If condition and use of pointers.

1: a = 10;
2: c = 20;
3: b = 2;
4: if b:
5:   u = &a;
.: else:
6:   u = &c;
7: c = *u;
8: use(c)
"""

all_vars: Dict[types.VarNameT, types.ReturnT] = {
  "v:main:a": types.Int,
  "v:main:b": types.Int,
  "v:main:c": types.Int,
  "v:main:u": types.Ptr(to=types.Int),
} # end all_vars dict

all_func: Dict[types.FuncNameT, graph.FuncNode] = {
  "f:main":
    graph.FuncNode(
      name= "f:main",
      params= [],
      returns= types.Int,
      basic_blocks= {
        1: graph.BB([ # 1 is always start/entry BB. (REQUIRED)
          instr.AssignI(expr.VarE("v:main:a"), expr.LitE(10)),
          instr.AssignI(expr.VarE("v:main:c"), expr.LitE(20)),
          instr.AssignI(expr.VarE("v:main:b"), expr.LitE(2)),
          instr.CondI(expr.VarE("v:main:b")),
        ]),
        2: graph.BB([
          instr.AssignI(expr.VarE("v:main:u"), expr.UnaryE(op.AddrOf, expr.VarE("v:main:c"))),
        ]),
        3: graph.BB([
          instr.AssignI(expr.VarE("v:main:u"), expr.UnaryE(op.AddrOf, expr.VarE("v:main:a"))),
        ]),
        -1: graph.BB([ # -1 is end/exit block (REQUIRED, if more than one BB present)
          instr.AssignI(expr.VarE("v:main:c"), expr.UnaryE(op.Deref, expr.VarE("v:main:u"))),
          instr.UseI(expr.VarE("v:main:c")),
        ]),
      },
      bb_edges= [
        graph.BbEdge(1, 2, graph.FalseEdge),
        graph.BbEdge(1, 3, graph.TrueEdge),
        graph.BbEdge(2, -1, graph.UnCondEdge),
        graph.BbEdge(3, -1, graph.UnCondEdge),
      ],
    ),
} # end all_func dict.

# Always build the universe from a 'program' module.
# Initialize the universe with program in this module.
universe.build(name, description, all_vars, all_func)
