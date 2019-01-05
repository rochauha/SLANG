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
      cfg= graph.Cfg(
        start_id= 1,
        end_id= 8,
        node_map= {
          1: graph.CfgNode(
            node_id=1,
            insn= instr.AssignI(expr.VarE("v:main:a"), expr.LitE(10)),
            pred_edges= [],
            succ_edges= [
              graph.CfgEdge.make(1, 2, graph.UnCondEdge),
            ]
          ),

          2: graph.CfgNode(
            node_id=2,
            insn= instr.AssignI(expr.VarE("v:main:c"), expr.LitE(20)),
            pred_edges= [
              graph.CfgEdge.make(1, 2, graph.UnCondEdge),
            ],
            succ_edges= [
              graph.CfgEdge.make(2, 3, graph.UnCondEdge),
            ]
          ),

          3: graph.CfgNode(
            node_id=3,
            insn= instr.AssignI(expr.VarE("v:main:b"), expr.LitE(2)),
            pred_edges= [
              graph.CfgEdge.make(2, 3, graph.UnCondEdge),
            ],
            succ_edges= [
              graph.CfgEdge.make(3, 4, graph.UnCondEdge),
            ]
          ),

          4: graph.CfgNode(
            node_id=4,
            insn = instr.CondI(expr.VarE("v:main:b")),
            pred_edges = [
              graph.CfgEdge.make(3, 4, graph.UnCondEdge)
            ],
            succ_edges = [
              graph.CfgEdge.make(4, 5, graph.TrueEdge),
              graph.CfgEdge.make(4, 6, graph.FalseEdge)
            ]
          ),

          5: graph.CfgNode(
            node_id=5,
            insn = instr.AssignI(expr.VarE("v:main:u"),
                                 expr.UnaryE(op.AddrOf, expr.VarE("v:main:a"))),
            pred_edges = [
              graph.CfgEdge.make(4, 5, graph.TrueEdge)
            ],
            succ_edges = [
              graph.CfgEdge.make(5, 7, graph.UnCondEdge)
            ]
          ),

          6: graph.CfgNode(
            node_id=6,
            insn = instr.AssignI(expr.VarE("v:main:u"),
                                 expr.UnaryE(op.AddrOf, expr.VarE("v:main:c"))),
            pred_edges = [
              graph.CfgEdge.make(4, 6, graph.FalseEdge)
            ],
            succ_edges = [
              graph.CfgEdge.make(6, 7, graph.UnCondEdge)
            ]
          ),

          7: graph.CfgNode(
            node_id=7,
            insn = instr.AssignI(expr.VarE("v:main:c"),
                                 expr.UnaryE(op.Deref, expr.VarE("v:main:u"))),
            pred_edges = [
              graph.CfgEdge.make(5, 7, graph.UnCondEdge),
              graph.CfgEdge.make(6, 7, graph.UnCondEdge),
            ],
            succ_edges = [
              graph.CfgEdge.make(7, 8, graph.UnCondEdge)
            ]
          ),

          8: graph.CfgNode(
            node_id=8,
            insn = instr.UseI(expr.VarE("v:main:c")),
            pred_edges = [
              graph.CfgEdge.make(7, 8, graph.UnCondEdge),
            ],
            succ_edges = []
          ),
        }
      )
    ),
} # end all_func dict.

# Always build the universe from a 'program' module.
# Initialize the universe with program in this module.
universe.build(name, description, all_vars, all_func)
