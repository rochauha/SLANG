#!/usr/bin/env python3

# MIT License.
# Copyright (c) 2019 The SLANG Authors.

from typing import Dict

import span.ir.types as types
import span.ir.expr as expr
import span.ir.instr as instr

import span.sys.graph as graph
import span.sys.universe as universe

# analysis unit name
name = "span.tests.test3"
description = """
Nested if conditions, with only numeric unit assignments.

1. b = 1;
2. if b:
3.   y = 0;
4.   if y:
5.     y = a;
   else:
6.   y = x;
7. use(y);
"""

all_vars: Dict[types.VarNameT, types.ReturnT] = {
  "v:main:x": types.Int,
  "v:main:y": types.Int,
  "v:main:a": types.Int,
  "v:main:b": types.Int,
} # end all_vars dict

all_func: Dict[types.FuncNameT, graph.FuncNode] = {
  "f:main":
    graph.FuncNode(
      name= "f:main",
      params= [],
      returns= types.Int,
      basic_blocks= {
        1: graph.BB([ # 1 is always start/entry BB. (REQUIRED)
          instr.AssignI(expr.VarE("v:main:b"), expr.LitE(1)),
          instr.CondI(expr.VarE("v:main:b")),
        ]),
        2: graph.BB([
          instr.AssignI(expr.VarE("v:main:y"), expr.VarE("v:main:x")),
        ]),
        3: graph.BB([
          instr.AssignI(expr.VarE("v:main:y"), expr.LitE(0)),
          instr.CondI(expr.VarE("v:main:y")),
        ]),
        4: graph.BB([
          instr.AssignI(expr.VarE("v:main:y"), expr.VarE("v:main:a")),
        ]),
        -1: graph.BB([ # -1 is end/exit block (REQUIRED, if more than one BB present)
          instr.UseI(expr.VarE("v:main:y")),
        ]),
      },
      bb_edges= [
        graph.BbEdge(1, 2, graph.FalseEdge),
        graph.BbEdge(1, 3, graph.TrueEdge),
        graph.BbEdge(3, 4, graph.TrueEdge),
        graph.BbEdge(3, -1, graph.FalseEdge),
        graph.BbEdge(2, -1, graph.UnCondEdge),
        graph.BbEdge(4, -1, graph.UnCondEdge),
      ],
    ),
} # end all_func dict.

# Always build the universe from a 'program' module.
# Initialize the universe with program in this module.
universe.build(name, description, all_vars, all_func)
