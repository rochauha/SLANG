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
name = "span.tests.test5"
description = """
Loop with pointers and expressions,

 1: a = 11;
 2: u = &a;
 3: input(cond); // special instruction
 4: while cond: // `cond` value is indeterministic.
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
      cfg= graph.Cfg(
        start_id= 1,
        end_id= 11,
        node_map= {
          1: graph.CfgNode(
            node_id=1,
            insn= instr.AssignI(expr.VarE("v:main:a"), expr.LitE(11)),
            pred_edges= [],
            succ_edges= [
              graph.CfgEdge.make(1, 2, graph.UnCondEdge)
            ],
          ),

          2: graph.CfgNode(
            node_id=2,
            insn = instr.AssignI(expr.VarE("v:main:u"),
                                 expr.UnaryE(op.AddrOf, expr.VarE("v:main:a"))),
            pred_edges = [
              graph.CfgEdge.make(1, 2, graph.UnCondEdge)
            ],
            succ_edges = [
              graph.CfgEdge.make(2, 3, graph.UnCondEdge),
            ],
          ),

          3: graph.CfgNode(
            node_id=3,
            insn = instr.InputI(expr.VarE("v:main:cond")),
            pred_edges = [
              graph.CfgEdge.make(2, 3, graph.UnCondEdge)
            ],
            succ_edges = [
              graph.CfgEdge.make(3, 4, graph.UnCondEdge)
            ],
          ),

          4: graph.CfgNode(
            node_id=4,
            insn = instr.CondI(expr.VarE("v:main:cond")),
            pred_edges = [
              graph.CfgEdge.make(3, 4, graph.UnCondEdge),
              graph.CfgEdge.make(9, 4, graph.UnCondEdge),
              graph.CfgEdge.make(10, 4, graph.UnCondEdge),
            ],
            succ_edges = [
              graph.CfgEdge.make(4, 5, graph.TrueEdge),
              graph.CfgEdge.make(4, 11, graph.FalseEdge)
            ],
          ),

          5: graph.CfgNode(
            node_id=5,
            insn = instr.AssignI(expr.VarE("v:main:tmp"),
                                 expr.UnaryE(op.Deref, expr.VarE("v:main:u"))),
            pred_edges = [
              graph.CfgEdge.make(4, 5, graph.TrueEdge),
            ],
            succ_edges = [
              graph.CfgEdge.make(5, 6, graph.UnCondEdge)
            ],
          ),

          6: graph.CfgNode(
            node_id=6,
            insn = instr.AssignI(expr.VarE("v:main:b"),
                                 expr.BinaryE(expr.VarE("v:main:tmp"),
                                              op.Modulo,
                                              expr.LitE(2))),
            pred_edges = [
              graph.CfgEdge.make(5, 6, graph.UnCondEdge)
            ],
            succ_edges = [
              graph.CfgEdge.make(6, 7, graph.UnCondEdge)
            ],
          ),

          7: graph.CfgNode(
            node_id=7,
            insn = instr.CondI(expr.VarE("v:main:b")),
            pred_edges = [
              graph.CfgEdge.make(6, 7, graph.UnCondEdge),
            ],
            succ_edges = [
              graph.CfgEdge.make(7, 8, graph.TrueEdge),
              graph.CfgEdge.make(7, 10, graph.FalseEdge),
            ],
          ),

          8: graph.CfgNode(
            node_id=8,
            insn = instr.AssignI(expr.VarE("v:main:b"), expr.LitE(15)),
            pred_edges = [
              graph.CfgEdge.make(7, 8, graph.TrueEdge),
            ],
            succ_edges = [
              graph.CfgEdge.make(8, 9, graph.UnCondEdge),
            ],
          ),

          9: graph.CfgNode(
            node_id=9,
            insn = instr.AssignI(expr.VarE("v:main:u"),
                                 expr.UnaryE(op.AddrOf, expr.VarE("v:main:b"))),
            pred_edges = [
              graph.CfgEdge.make(8, 9, graph.UnCondEdge),
            ],
            succ_edges = [
              graph.CfgEdge.make(9, 4, graph.UnCondEdge),
            ],
          ),

          10: graph.CfgNode(
            node_id=10,
            insn = instr.AssignI(expr.VarE("v:main:b"), expr.LitE(16)),
            pred_edges = [
              graph.CfgEdge.make(7, 10, graph.FalseEdge),
            ],
            succ_edges = [
              graph.CfgEdge.make(10, 4, graph.UnCondEdge),
            ],
          ),

          11: graph.CfgNode(
            node_id=11,
            insn = instr.NoOpI(),
            pred_edges = [
              graph.CfgEdge.make(4, 11, graph.FalseEdge),
            ],
            succ_edges = [],
          ),
        }
      )
    ),
} # end all_func dict.

# Always build the universe from a 'program' module.
# Initialize the universe with program in this module.
universe.build(name, description, all_vars, all_func)
