
# START: A_SPAN_translation_unit.

# eval() the contents of this file.
# Keep the following imports in effect when calling eval.

# import span.ir.types as types
# import span.ir.expr as expr
# import span.ir.instr as instr
# import span.ir.obj as obj
# import span.ir.tunit as irTUnit

# An instance of span.ir.tunit.TUnit class.
irTUnit.TUnit(
  name = "switch1.c",
  description = "Auto-Translated from Clang AST.",
  allVars = {
    "v:main:t.3": types.Int,
    "v:main:t.2": types.Int,
    "v:main:argc": types.Int,
    "v:main:t.1": types.Int,
    "v:main:argv": types.Ptr(to=types.Ptr(to=types.Char)),
  }, # end allVars dict

  allObjs = {
    "f:main":
      obj.Func(
        name = "f:main",
        paramsNames = ["v:main:argc", "v:main:argv"]
        variadic = False,
        returnType = types.Int,

        # Note: -1 is always start/entry BB. (REQUIRED)
        # Note: 0 is always end/exit BB (REQUIRED)
        basicBlocks = {
          5: [
            instr.ReturnI(expr.LitE(10, Loc(7,14)), Loc(7,7)),
          ],

          4: [
            instr.NopI(),
          ],

          1: [
            instr.ReturnI(expr.LitE(0, Loc(10,10)), Loc(10,3)),
          ],

          8: [
            instr.AssignI(expr.VarE("v:main:t.3", Loc(6,5)), expr.BinaryE(expr.LitE(10, Loc(3,10)), op.BO_EQ, expr.LitE(3, Loc(6,10)), ), ),
            instr.CondI(expr.VarE("v:main:t.3", Loc(6,5)), ),
          ],

          -1: [
            instr.NopI(),
          ],

          0: [
            instr.NopI(),
          ],

          7: [
            instr.AssignI(expr.VarE("v:main:t.2", Loc(5,5)), expr.BinaryE(expr.LitE(10, Loc(3,10)), op.BO_EQ, expr.LitE(2, Loc(5,10)), ), ),
            instr.CondI(expr.VarE("v:main:t.2", Loc(5,5)), ),
          ],

          2: [
            instr.AssignI(expr.VarE("v:main:t.1", Loc(4,5)), expr.BinaryE(expr.LitE(10, Loc(3,10)), op.BO_EQ, expr.LitE(1, Loc(4,10)), ), ),
            instr.CondI(expr.VarE("v:main:t.1", Loc(4,5)), ),
          ],

          3: [
            instr.NopI(),
          ],

        }, # basicBlocks end.

        bbEdges= {
          (1, 0, types.UnCondEdge),
          (2, 0, types.TrueEdge),
          (7, 0, types.TrueEdge),
          (2, 7, types.FalseEdge),
          (8, 0, types.TrueEdge),
          (7, 8, types.FalseEdge),
          (8, 1, types.FalseEdge),
          (3, 4, types.UnCondEdge),
          (4, 5, types.UnCondEdge),
          (5, 0, types.UnCondEdge),
          (-1, 2, types.UnCondEdge),
        }, # bbEdges end
      ), # f:main() end. 

  }, # end allObjs dict
) # irTUnit.TUnit() ends

# END  : A_SPAN_translation_unit.
