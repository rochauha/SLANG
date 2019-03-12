
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
  name = "switch.c",
  description = "Auto-Translated from Clang AST.",
  allVars = {
    "v:main:t.3": types.Int,
    "v:main:t.2": types.Int,
    "v:main:t.1": types.Int,
    "v:printf:__format": types.Ptr(to=types.Char),
    "v:main:argv": types.Ptr(to=types.Ptr(to=types.Char)),
    "v:main:argc": types.Int,
    "v:main:i": types.Int,
    "v:printMe:str": types.Ptr(to=types.Char),
    "v:main:f": types.Float,
  }, # end allVars dict

  allObjs = {
    "f:printMe":
      obj.Func(
        name = "f:printMe",
        paramsNames = ["v:printMe:str"]
        variadic = False,
        returnType = types.Float,

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
            instr.CallI(expr.CallE("f:printf", [expr.VarE("v:printMe:str")])),
            instr.ReturnI(expr.LitE(2.0)),
          ],

        }, # basicBlocks end.

        bbEdges= {
          (1, 0, types.UnCondEdge),
          (-1, 1, types.UnCondEdge),
        }, # bbEdges end
      ), # f:printMe() end. 

    "f:main":
      obj.Func(
        name = "f:main",
        paramsNames = ["v:main:argc", "v:main:argv"]
        variadic = False,
        returnType = types.Int,

        # Note: -1 is always start/entry BB. (REQUIRED)
        # Note: 0 is always end/exit BB (REQUIRED)
        basicBlocks = {
          8: [
            instr.AssignI(expr.VarE("v:main:i"), expr.LitE(20)),
          ],

          7: [
            instr.AssignI(expr.VarE("v:main:i"), expr.BinaryE(expr.VarE("v:main:i"), op.BO_ADD, expr.LitE(21))),
          ],

          6: [
            instr.AssignI(expr.VarE("v:main:i"), expr.BinaryE(expr.VarE("v:main:i"), op.BO_MUL, expr.LitE(3))),
          ],

          5: [
            instr.AssignI(expr.VarE("v:main:i"), expr.VarE("v:main:i")),
          ],

          11: [
            instr.AssignI(expr.VarE("v:main:t.3"), expr.BinaryE(expr.VarE("v:main:i"), op.BO_EQ, expr.LitE(10))),
            instr.CondI(expr.VarE("v:main:t.3")),
          ],

          1: [
            instr.ReturnI(expr.LitE(0)),
          ],

          -1: [
            instr.NopI(),
          ],

          0: [
            instr.NopI(),
          ],

          2: [
            instr.NopI(),
          ],

          3: [
            instr.CallI(expr.CallE("f:printf", [expr.LitE("""Hello default
""")])),
          ],

          10: [
            instr.AssignI(expr.VarE("v:main:t.2"), expr.BinaryE(expr.VarE("v:main:i"), op.BO_EQ, expr.LitE(20))),
            instr.CondI(expr.VarE("v:main:t.2")),
          ],

          4: [
            instr.AssignI(expr.VarE("v:main:i"), expr.LitE(10)),
            instr.CallI(expr.CallE("f:printf", [expr.LitE("""Hello, World %d %d
"""), expr.VarE("v:main:i"), expr.LitE(10)])),
            instr.AssignI(expr.VarE("v:main:f"), expr.CallE("f:printMe", [expr.LitE("""Hello
""")])),
            instr.AssignI(expr.VarE("v:main:f"), expr.LitE(2.345600)),
            instr.AssignI(expr.VarE("v:main:i"), expr.BinaryE(expr.LitE(22), op.BO_ADD, expr.LitE(33))),
            instr.AssignI(expr.VarE("v:main:t.1"), expr.BinaryE(expr.VarE("v:main:i"), op.BO_EQ, expr.LitE(30))),
            instr.CondI(expr.VarE("v:main:t.1")),
          ],

        }, # basicBlocks end.

        bbEdges= {
          (1, 0, types.UnCondEdge),
          (2, 3, types.UnCondEdge),
          (3, 1, types.UnCondEdge),
          (4, 5, types.TrueEdge),
          (10, 6, types.TrueEdge),
          (4, 10, types.FalseEdge),
          (11, 8, types.TrueEdge),
          (10, 11, types.FalseEdge),
          (11, 7, types.FalseEdge),
          (5, 2, types.UnCondEdge),
          (6, 5, types.UnCondEdge),
          (7, 6, types.UnCondEdge),
          (8, 2, types.UnCondEdge),
          (-1, 4, types.UnCondEdge),
        }, # bbEdges end
      ), # f:main() end. 

    "f:printf":
      obj.Func(
        name = "f:printf",
        paramsNames = ["v:printf:__format"]
        variadic = True,
        returnType = types.Int,

        # Note: -1 is always start/entry BB. (REQUIRED)
        # Note: 0 is always end/exit BB (REQUIRED)
        basicBlocks = {
        }, # basicBlocks end.

        bbEdges= {
        }, # bbEdges end
      ), # f:printf() end. 

  }, # end allObjs dict
) # irTUnit.TUnit() ends

# END  : A_SPAN_translation_unit.
