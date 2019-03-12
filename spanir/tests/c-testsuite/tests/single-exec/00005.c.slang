
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
  name = "00005.c",
  description = "Auto-Translated from Clang AST.",
  allVars = {
    "v:main:t.4": types.Int,
    "v:main:pp": types.Ptr(to=types.Ptr(to=types.Int)),
    "v:main:t.3": types.Int,
    "v:main:x": types.Int,
    "v:main:t.1": types.Ptr(to=types.Int),
    "v:main:t.2": types.Ptr(to=types.Int),
    "v:main:p": types.Ptr(to=types.Int),
  }, # end allVars dict

  allObjs = {
    "f:main":
      obj.Func(
        name = "f:main",
        paramsNames = []
        variadic = False,
        returnType = types.Int,

        # Note: -1 is always start/entry BB. (REQUIRED)
        # Note: 0 is always end/exit BB (REQUIRED)
        basicBlocks = {
          8: [
            instr.AssignI(expr.VarE("v:main:x", Loc(8,2)), expr.LitE(0)),
            instr.AssignI(expr.VarE("v:main:p", Loc(9,2)), expr.UnaryE(op.UO_ADDROF, expr.VarE("v:main:x", Loc(9,7)), Loc(9,6))),
            instr.AssignI(expr.VarE("v:main:pp", Loc(10,2)), expr.UnaryE(op.UO_ADDROF, expr.VarE("v:main:p", Loc(10,8)), Loc(10,7))),
            instr.AssignI(expr.VarE("v:main:t.4"), expr.UnaryE(op.UO_DEREF, expr.VarE("v:main:p", Loc(12,6)), Loc(12,5))),
            instr.CondI(expr.VarE("v:main:t.4")),
          ],

          7: [
            instr.ReturnI(expr.LitE(1)),
          ],

          6: [
            instr.AssignI(expr.VarE("v:main:t.2"), expr.UnaryE(op.UO_DEREF, expr.VarE("v:main:pp", Loc(14,7)), Loc(14,6))),
            instr.AssignI(expr.VarE("v:main:t.3"), expr.UnaryE(op.UO_DEREF, expr.VarE("v:main:t.2"), Loc(14,5))),
            instr.CondI(expr.VarE("v:main:t.3")),
          ],

          1: [
            instr.ReturnI(expr.LitE(1)),
          ],

          -1: [
            instr.NopI(),
          ],

          0: [
            instr.NopI(),
          ],

          2: [
            instr.ReturnI(expr.LitE(0)),
          ],

          3: [
            instr.CondI(expr.VarE("v:main:x", Loc(19,5))),
          ],

          4: [
            instr.AssignI(expr.VarE("v:main:t.1"), expr.UnaryE(op.UO_DEREF, expr.VarE("v:main:pp", Loc(17,5)), Loc(17,4))),
            instr.AssignI(expr.UnaryE(op.UO_DEREF, expr.VarE("v:main:t.1"), Loc(17,3)), expr.LitE(1)),
          ],

          5: [
            instr.ReturnI(expr.LitE(1)),
          ],

        }, # basicBlocks end.

        bbEdges= {
          (1, 0, types.UnCondEdge),
          (2, 0, types.UnCondEdge),
          (3, 2, types.TrueEdge),
          (3, 1, types.FalseEdge),
          (4, 3, types.UnCondEdge),
          (5, 0, types.UnCondEdge),
          (6, 5, types.TrueEdge),
          (6, 4, types.FalseEdge),
          (7, 0, types.UnCondEdge),
          (8, 7, types.TrueEdge),
          (8, 6, types.FalseEdge),
          (-1, 8, types.UnCondEdge),
        }, # bbEdges end
      ), # f:main() end. 

  }, # end allObjs dict
) # irTUnit.TUnit() ends

# END  : A_SPAN_translation_unit.
