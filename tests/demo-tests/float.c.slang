
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
  name = "float.c",
  description = "Auto-Translated from Clang AST.",
  allVars = {
    "v:main:f":types.Float,
    "v:main:i":types.Int,
    "v:main:argc":types.Int,
    "v:main:argv":types.Ptr(to=types.Ptr(to=types.Char)),
  }, # end allVars dict

  allObjs = {
    "f:main":
      obj.Func(
        name = "f:main",
        paramsNames = [v:main:argc, v:main:argv]
        variadic = False,
        returnType = types.Int,

        # Note: -1 is always start/entry BB. (REQUIRED)
        # Note: 0 is always end/exit BB (REQUIRED)
        basicBlocks = {
          0: [
            instr.NopI(),
          ],

          -1: [
            instr.NopI(),
          ],

          1: [
            instr.CallI(expr.CallE("f:main", [expr.VarE("v:main:argc"), expr.VarE("v:main:argv")])),
            instr.AssignI(expr.VarE("v:main:f"), expr.LitE(2.345600)),
          ],

        }, # basicBlocks end.

        bbEdges= {
          (1, 0, types.UnCondEdge),
          (-1, 1, types.UnCondEdge),
        }, # bbEdges end
      ), # f:main() end. 

  }, # end allObjs dict
) # irTUnit.TUnit() ends

# END  : A_SPAN_translation_unit.
