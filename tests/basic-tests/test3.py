#!/usr/bin/env python3

# MIT License
# Copyright (c) 2018 Anshuman Dhuliya

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
      cfg= graph.Cfg(
        start_id= 1,
        end_id= 7,
        node_map= {
          1: graph.CfgNode(
            node_id=1,
            insn= instr.AssignI(expr.VarE("v:main:b"), expr.LitE(1)),
            pred_edges= [],
            succ_edges= [
              graph.CfgEdge.make(1, 2, graph.UnCondEdge)
            ]
          ),

          2: graph.CfgNode(
            node_id=2,
            insn = instr.CondI(expr.VarE("v:main:b")),
            pred_edges = [
              graph.CfgEdge.make(1, 2, graph.UnCondEdge)
            ],
            succ_edges = [
              graph.CfgEdge.make(2, 3, graph.TrueEdge),
              graph.CfgEdge.make(2, 6, graph.FalseEdge)
            ]
          ),

          3: graph.CfgNode(
            node_id=3,
            insn = instr.AssignI(expr.VarE("v:main:y"), expr.LitE(0)),
            pred_edges = [
              graph.CfgEdge.make(2, 3, graph.TrueEdge)
            ],
            succ_edges = [
              graph.CfgEdge.make(3, 4, graph.UnCondEdge)
            ]
          ),

          4: graph.CfgNode(
            node_id=4,
            insn = instr.CondI(expr.VarE("v:main:y")),
            pred_edges = [
              graph.CfgEdge.make(3, 4, graph.UnCondEdge)
            ],
            succ_edges = [
              graph.CfgEdge.make(4, 5, graph.TrueEdge),
              graph.CfgEdge.make(4, 7, graph.FalseEdge)
            ]
          ),

          5: graph.CfgNode(
            node_id=5,
            insn = instr.AssignI(expr.VarE("v:main:y"), expr.VarE("v:main:a")),
            pred_edges = [
              graph.CfgEdge.make(4, 5, graph.TrueEdge)
            ],
            succ_edges = [
              graph.CfgEdge.make(5, 7, graph.UnCondEdge)
            ]
          ),

          6: graph.CfgNode(
            node_id=6,
            insn = instr.AssignI(expr.VarE("v:main:y"), expr.VarE("v:main:x")),
            pred_edges = [
              graph.CfgEdge.make(2, 6, graph.FalseEdge)
            ],
            succ_edges = [
              graph.CfgEdge.make(6, 7, graph.UnCondEdge)
            ]
          ),

          7: graph.CfgNode(
            node_id=7,
            insn = instr.UseI(expr.VarE("v:main:y")),
            pred_edges = [
              graph.CfgEdge.make(4, 7, graph.FalseEdge),
              graph.CfgEdge.make(5, 7, graph.UnCondEdge),
              graph.CfgEdge.make(6, 7, graph.UnCondEdge)
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
