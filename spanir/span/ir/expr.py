#!/usr/bin/env python3

# MIT License
# Copyright (c) 2019 Anshuman Dhuliya

"""
All possible expressions used in an instruction.
"""

import logging
_log = logging.getLogger(__name__)
from typing import Optional, List, Set

from span.util.logger import LS
import span.ir.types as types
import span.ir.op as op
import span.util.util as util

import io

ExprId = int
ExprCodeT = int

################################################
#BOUND START: expr_codes
################################################

# the order and ascending sequence is important
VAR_EXPR_EC: ExprCodeT        = 11
LIT_EXPR_EC: ExprCodeT        = 12
FUNC_EXPR_EC: ExprCodeT       = 13

UNARY_EXPR_EC: ExprCodeT      = 20
CAST_EXPR_EC: ExprCodeT       = 21
BINARY_EXPR_EC: ExprCodeT     = 30
ARR_EXPR_EC: ExprCodeT        = 31

CALL_EXPR_EC: ExprCodeT       = 40
MEMBER_EXPR_EC: ExprCodeT     = 45
PHI_EXPR_EC: ExprCodeT        = 50

################################################
#BOUND END  : expr_codes
################################################

class ExprET(types.AnyT):
  """Base class for all expressions."""
  def __init__(self,
               exprCode: ExprCodeT,
               loc: Optional[types.Loc] = None
  ) -> None:
    if self.__class__.__name__.endswith("T"): super().__init__()
    self.type: types.Type = types.Void
    self.exprCode = exprCode
    self.loc = loc

  def isUnaryExpr(self):
    return self.exprCode == UNARY_EXPR_EC or self.exprCode == CAST_EXPR_EC

  def isBinaryExpr(self):
    return self.exprCode == BINARY_EXPR_EC

  def isUnitExpr(self):
    return self.exprCode == VAR_EXPR_EC or self.exprCode == LIT_EXPR_EC

  def isCallExpr(self):
    return self.exprCode == CALL_EXPR_EC

class UnitET(ExprET):
  """Unit exprs like a var 'x', a value '12.3'."""
  def __init__(self,
               exprCode: ExprCodeT,
               loc: Optional[types.Loc] = None
  ) -> None:
    super().__init__(exprCode, loc)

class VarE(UnitET):
  """Holds a single variable name."""
  def __init__(self,
               name: types.VarNameT,
               loc: Optional[types.Loc] = None
  ) -> None:
    super().__init__(VAR_EXPR_EC, loc)
    self.name: types.VarNameT = name

  def __eq__(self,
             other: 'VarE'
  ) -> bool:
    if not isinstance(other, VarE):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.name == other.name:
      if LS: _log.warning("VarName Differs: %s, %s", repr(self), repr(other))
      return False
    if not self.loc == other.loc:
      if LS: _log.warning("Loc Differs: %s, %s", self, other)
      return False
    return True

  def __hash__(self): return 10

  def __str__(self):
    name = self.name.split(":")[-1]
    return f"{name}"

  def __repr__(self): return self.name

class FuncE(VarE):
  """A function name that is called. Used in CallE class."""
  def __init__(self,
               name: types.FuncNameT,
               loc: Optional[types.Loc] = None
  ) -> None:
    super().__init__(FUNC_EXPR_EC, loc)
    self.name: types.VarNameT = name

  def __eq__(self,
             other: 'FuncE'
  ) -> bool:
    if not isinstance(other, FuncE):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.name == other.name:
      if LS: _log.warning("FuncName Differs: %s, %s", repr(self), repr(other))
      return False
    if not self.loc == other.loc:
      if LS: _log.warning("Loc Differs: %s, %s", self, other)
      return False
    return True

  def __hash__(self): return 10

  def __str__(self):
    name = self.name.split(":")[-1]
    return f"{name}"

  def __repr__(self): return self.name

class LitE(UnitET):
  """A single numeric literal. Bools are also numeric."""
  def __init__(self,
               val: types.LitT,
               loc: Optional[types.Loc] = None
  ) -> None:
    super().__init__(LIT_EXPR_EC, loc)
    self.val = val

  def __eq__(self,
             other: 'LitE'
  ) -> bool:
    if not isinstance(other, LitE):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.val == other.val:
      if LS: _log.warning("LitValue Differs: %s, %s", self, other)
      return False
    if not self.loc == other.loc:
      if LS: _log.warning("Loc Differs: %s, %s", self, other)
      return False
    return True

  def __str__(self):
    if isinstance(self.val, str):
      #escaped = self.val.encode('unicode_escape')
      newVal = repr(self.val)
      newVal = newVal[1:-1]
      newVal = "'" + newVal + "'"
      return f"{newVal}"
    return f"{self.val}"

  def __repr__(self): return self.__str__()

class BinaryE(ExprET):
  """A binary expression."""
  def __init__(self,
               arg1: UnitET,
               opr: op.BinaryOp,
               arg2: UnitET,
               loc: Optional[types.Loc] = None
  ) -> None:
    super().__init__(BINARY_EXPR_EC, loc)
    self.arg1 = arg1
    self.opr = opr
    self.arg2 = arg2

  def __eq__(self,
             other: 'BinaryE'
  ) -> bool:
    if not isinstance(other, BinaryE):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.opr == other.opr:
      if LS: _log.warning("Operator Differs: %s, %s", self, other)
      return False
    if not self.arg1 == other.arg1:
      if LS: _log.warning("Arg1 Differs: %s, %s", self, other)
      return False
    if not self.arg2 == other.arg2:
      if LS: _log.warning("Arg2 Differs: %s, %s", self, other)
      return False
    if not self.loc == other.loc:
      if LS: _log.warning("Loc Differs: %s, %s", self, other)
      return False
    return True

  def __str__(self): return f"{self.arg1} {self.opr} {self.arg2}"

  def __repr__(self): return self.__str__()

class ArrayE(UnitET):
  """A binary array expression."""
  def __init__(self,
               var: VarE,
               indexSeq: List[UnitET],
               loc: Optional[types.Loc] = None
  ) -> None:
    super().__init__(ARR_EXPR_EC, loc)
    self.var = var
    self.indexSeq = indexSeq

  def __eq__(self,
             other: 'BinaryE'
  ) -> bool:
    if not isinstance(other, BinaryE):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.var == other.var:
      if LS: _log.warning("ArrayName Differs: %s, %s", self, other)
      return False
    if not self.indexSeq == other.indexSeq:
      if LS: _log.warning("IndexSeq Differs: %s, %s", self, other)
      return False
    if not self.loc == other.loc:
      if LS: _log.warning("Loc Differs: %s, %s", self, other)
      return False
    return True

  def __str__(self):
    indexStr = ""
    with io.StringIO() as sio:
      for index in self.indexSeq:
        sio.write(f"[{index}]")
      indexStr = sio.getvalue()

    return f"{self.var}{indexStr}"

  def __repr__(self): return self.__str__()

class UnaryE(ExprET):
  """A unary arithmetic expression."""
  def __init__(self,
               opr: op.UnaryOp,
               arg: UnitET,
               loc: Optional[types.Loc] = None
  ) -> None:
    super().__init__(UNARY_EXPR_EC, loc)
    self.opr = opr
    self.arg = arg

  def __eq__(self,
             other: 'UnaryE'
  ) -> bool:
    if not isinstance(other, UnaryE):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.opr == other.opr:
      if LS: _log.warning("Operator Differs: %s, %s", self, other)
      return False
    if not self.arg == other.arg:
      if LS: _log.warning("Arg Differs: %s, %s", self, other)
      return False
    if not self.loc == other.loc:
      if LS: _log.warning("Loc Differs: %s, %s", self, other)
      return False
    return True

  def __str__(self): return f"{self.opr}{self.arg}"

  def __repr__(self): return self.__str__()

class CastE(UnaryE):
  """A unary type cast expression."""
  def __init__(self,
               cast: types.Type,
               arg: UnitET,
               loc: Optional[types.Loc] = None
  ) -> None:
    super().__init__(CAST_EXPR_EC, loc)
    self.cast = cast
    self.arg = arg

  def __eq__(self,
             other: 'CastE'
  ) -> bool:
    if not isinstance(other, CastE):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.cast == other.cast:
      if LS: _log.warning("CastType Differs: %s, %s", self, other)
      return False
    if not self.arg == other.arg:
      if LS: _log.warning("Arg Differs: %s, %s", self, other)
      return False
    if not self.loc == other.loc:
      if LS: _log.warning("Loc Differs: %s, %s", self, other)
      return False
    return True

  def __str__(self): return f"{self.cast}{self.arg}"

  def __repr__(self): return self.__str__()

class AllocE(ExprET):
  """A stack allocator instruction.

  It allocates size * sizeof(self.type) on the stack,
  and returns the pointer to it.
  """
  def __init__(self,
               sizeExpr: UnitET,
               dataType: types.Type,
               loc: types.Loc,
  ) -> None:
    self.sizeExpr = sizeExpr
    self.type = dataType
    self.loc = loc

  def __eq__(self,
             other: 'AllocE'
  ) -> bool:
    if not isinstance(other, AllocE):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.sizeExpr == other.sizeExpr:
      if LS: _log.warning("AllocaSize Differs: %s, %s", self, other)
      return False
    if not self.type == other.type:
      if LS: _log.warning("AllocaType Differs: %s, %s", self, other)
      return False
    if not self.loc == other.loc:
      if LS: _log.warning("Loc Differs: %s, %s", self, other)
      return False
    return True

  def __str__(self):
    return f"[{self.sizeExpr} x {self.type}]"

  def __repr__(self): return self.__str__()

class CallE(ExprET):
  """A function call expression."""
  def __init__(self,
               callee: VarE,  # i.e. VarE or FuncE
               args: Optional[List[UnitET]] = None,
               loc: Optional[types.Loc] = None
  ) -> None:
    super().__init__(CALL_EXPR_EC, loc)
    self.callee = callee
    if not args:
      self.args = None
    else:
      self.args = args

  def __eq__(self,
             other: 'CallE'
  ) -> bool:
    if not isinstance(other, CallE):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.callee == other.callee:
      if LS: _log.warning("FuncName Differs: %s, %s", self, other)
      return False
    if not self.args == other.args:
      if LS: _log.warning("Args Differ: %s, %s", self, other)
      return False
    if not self.loc == other.loc:
      if LS: _log.warning("Loc Differs: %s, %s", self, other)
      return False
    return True

  def __str__(self):
    if self.args:
      args = [str(arg) for arg in self.args]
      expr = ",".join(args)
    else:
      expr = ""
    return f"{self.callee}({expr})"

  def __repr__(self): return self.__str__()

class MemberE(ExprET):
  """A member access expression: e.g. x->f.c or x.f.c ..."""
  def __init__(self,
               args: List[types.VarNameT],
               loc: Optional[types.Loc] = None
  ) -> None:
    super().__init__(MEMBER_EXPR_EC, loc)
    self.args = args

  def __eq__(self,
             other: 'MemberE'
  ) -> bool:
    if not isinstance(other, MemberE):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.args == other.args:
      if LS: _log.warning("Args Differ: %s, %s", self, other)
      return False
    if not self.loc == other.loc:
      if LS: _log.warning("Loc Differs: %s, %s", self, other)
      return False
    return True

  def __str__(self):
    args = [arg.split(":")[-1] for arg in self.args]
    expr = args.join(".")
    return expr

  def __repr__(self): return self.__str__()

class PhiE(ExprET):
  """A phi expression. For a possible future SSA form."""
  def __init__(self,
               args: Set[VarE],
               loc: Optional[types.Loc] = None
  ) -> None:
    super().__init__(PHI_EXPR_EC, loc)
    if not args:
      self.args = None
    else:
      self.args = args

  def __eq__(self,
             other: 'PhiE'
  ) -> bool:
    if not isinstance(other, PhiE):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.args == other.args:
      if LS: _log.warning("Args Differ: %s, %s", self, other)
      return False
    if not self.loc == other.loc:
      if LS: _log.warning("Loc Differs: %s, %s", self, other)
      return False
    return True

  def __str__(self): return f"phi({self.args})"

  def __repr__(self): return self.__str__()

