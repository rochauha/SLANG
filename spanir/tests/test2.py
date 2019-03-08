irTUnit.TUnit(
  # translation unit name
  name = "span.tests.test1",
  description = """
    A simple sequential program.

    1. x = 10;
    2. y = 20;
    3. z = y;
    4. g = z; //g is a global var
  """,

  allVars = {
    # all global and local variables go here
    "v:main:argc": types.Int,
    "v:main:argv": types.Ptr(types.Char, 2),
    "v:main:x1":    types.Int,
    "v:main:y":    types.Int,
    "v:main:z":    types.Int,  # local variable z in function main
    "v:g":         types.Int,  # global variable g
  }, # end allVars dict

  allObjs = {
    # all global objects func, struct, union (except variables) go here
    "f:main":  # function main
      obj.Func(
        name = "f:main",
        paramNames = ["v:main:argc", "v:main:argv"],
        paramTypes = [types.Int, types.Ptr(types.Char, 2)],
        returnType = types.Int,

        basicBlocks = {
          -1: [ # -1 is always start/entry BB. (REQUIRED)
            instr.AssignI(expr.VarE("v:main:x1", 10), expr.LitE(10, 11)),
            instr.AssignI(expr.VarE("v:main:y", 12), expr.LitE(20, 13)),
            instr.AssignI(expr.VarE("v:main:z", 14), expr.VarE("v:main:y", 15)),
            instr.AssignI(expr.VarE("v:g", 16), expr.VarE("v:main:z", 17)),
          ],

          0: [ # 0 is always end/exit block (REQUIRED)
          ],
        },

        bbEdges = [
          (-1, 0, types.UnCondEdge),
        ],

        loc = 23,
      ),
  } # end allObjs dict
) # end irTUnit.TUnit object
