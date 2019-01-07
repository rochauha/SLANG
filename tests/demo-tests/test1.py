#!/usr/bin/env python3

# MIT License
# Copyright (c) 2018 Anshuman Dhuliya

"""
A sample analysis unit.

Every translation unit or program, has to be correctly converted
into an equivalent analysis unit (a python module).
Use this module as a target template.
"""

from typing import Dict

import span.ir.types as types
import span.ir.expr as expr
import span.ir.instr as instr

import span.sys.graph as graph
import span.sys.universe as universe

# analysis unit name
name = "span.tests.test1"
description = """
A simple sequential program.

1. x = 10;
2. y = 20;
3. z = y;
4. g = z; //g is a global var
"""

"""
Dictionary of all the variables in a module.
Even local/automatic variables are included.

The naming of the variables is such that none of the names collide.

Naming convention:
1. Global variables are prefixed with "v:"
2. Automatic variables are prefixed with, "v:" along with
 their function name, separated with a colon.
   
Eg.
"v:x" is a global variable. (no colon in name)
"v:main:x" is a local variable in function main. (has colon)
"""
all_vars: Dict[types.VarNameT, types.ReturnT] = {
  "v:main:argc": types.Int,
  "v:main:argv": types.Ptr(to=types.Ptr(to=types.Char)),
  "v:main:x": types.Int,
  "v:main:y": types.Int,
  "v:main:z": types.Int,
  "v:g": types.Int,
} # end all_vars dict

all_func: Dict[types.FuncNameT, graph.FuncNode] = {
  "f:main":
    graph.FuncNode(
      name= "f:main",
      params= ["v:main:argc", "v:main:argv"],
      returns= types.Int,
      basic_blocks= {
        1: graph.BB([ # 1 is always start/entry BB. (REQUIRED)
          instr.AssignI(expr.VarE("v:main:x"), expr.LitE(10)),
          instr.AssignI(expr.VarE("v:main:y"), expr.LitE(20)),
          instr.AssignI(expr.VarE("v:main:z"), expr.VarE("v:main:y")),
          instr.AssignI(expr.VarE("v:g"), expr.VarE("v:main:z")),
        ]),
        #if only one BB, its START&END both.
        #-1: graph.BB(), # -1 is end/exit block (REQUIRED, if more than one BB present)
      },
      bb_edges= [],
    ),
} # end all_func dict.

# Always build the universe from a 'program' module.
# Initialize the universe with program in this module.
universe.build(name, description, all_vars, all_func)

