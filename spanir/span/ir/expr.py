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
BASIC_EXPR_EC: ExprCodeT      = 22
ADDROF_EXPR_EC: ExprCodeT     = 23
SIZEOF_EXPR_EC: ExprCodeT     = 24
BINARY_EXPR_EC: ExprCodeT     = 30
ARR_EXPR_EC: ExprCodeT        = 31

CALL_EXPR_EC: ExprCodeT       = 40
PTRCALL_EXPR_EC: ExprCodeT    = 41
MEMBER_EXPR_EC: ExprCodeT     = 45
PHI_EXPR_EC: ExprCodeT        = 50
SELECT_EXPR_EC: ExprCodeT     = 60

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

  def __eq__(self,
             other: 'ExprET'
  ) -> bool:
    return self.exprCode == other.exprCode and self.type == other.type

  def __hash__(self) -> int:
    return hash(self.exprCode) + hash(self.type)

class BasicET(ExprET):
  """Basic exprs like a var 'x', a value '12.3', a[10], x.y."""
  def __init__(self,
               exprCode: ExprCodeT,
               loc: Optional[types.Loc] = None
  ) -> None:
    super().__init__(exprCode, loc)

class UnitET(BasicET):
  """Unit exprs like a var 'x', a value '12.3'."""
  def __init__(self,
               exprCode: ExprCodeT,
               loc: Optional[types.Loc] = None
  ) -> None:
    super().__init__(exprCode, loc)

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

  def __hash__(self) -> int:
    return hash(self.val) + hash(self.exprCode)

  def __str__(self):
    if isinstance(self.val, str):
      #escaped = self.val.encode('unicode_escape')
      newVal = repr(self.val)
      newVal = newVal[1:-1]
      newVal = "'" + newVal + "'"
      return f"{newVal}"
    return f"{self.val}"

  def __repr__(self): return self.__str__()

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

  def __hash__(self):
    return hash(self.name) + hash(self.exprCode)

  def __str__(self):
    name = self.name.split(":")[-1]
    return f"{name}"

class FuncE(VarE):
  """Holds a single function name."""
  def __init__(self,
               name: types.FuncNameT,
               loc: Optional[types.Loc] = None
  ) -> None:
    super().__init__(FUNC_EXPR_EC, loc)
    self.name: types.FuncNameT = name

  def __eq__(self,
             other: 'FuncE'
  ) -> bool:
    if not isinstance(other, FuncE):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.name == other.name:
      if LS: _log.warning("Function Differs: %s, %s", repr(self), repr(other))
      return False
    if not self.loc == other.loc:
      if LS: _log.warning("Loc Differs: %s, %s", self, other)
      return False
    return True

  def __hash__(self):
    return hash(self.name) + hash(self.exprCode)

  def __str__(self):
    name = self.name.split(":")[-1]
    return f"{name}"
  def __repr__(self): return self.name

class ArrayE(BasicET):
  """An array expression.
  TODO: allow one subscript only.
  """
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

  def __hash__(self) -> int:
    return hash(self.var) + hash(self.indexSeq)

  def __str__(self):
    indexStr = ""
    with io.StringIO() as sio:
      for index in self.indexSeq:
        sio.write(f"[{index}]")
      indexStr = sio.getvalue()

    return f"{self.var}{indexStr}"

  def __repr__(self): return self.__str__()

class MemberE(BasicET):
  """A member access expression: e.g. x->f.c or x.f.c ...
  TODO: max one member access, x.y or u->y.
  """
  def __init__(self,
               var: VarE,
               fields: List[types.FieldNameT],
               loc: Optional[types.Loc] = None
  ) -> None:
    super().__init__(MEMBER_EXPR_EC, loc)
    self.var = var
    self.fields = fields

  def __eq__(self,
             other: 'MemberE'
  ) -> bool:
    if not isinstance(other, MemberE):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.var == other.var:
      if LS: _log.warning("Args Differ: %s, %s", self, other)
      return False
    if not self.fields == other.fields:
      if LS: _log.warning("Fields Differ: %s, %s", self, other)
      return False
    if not self.loc == other.loc:
      if LS: _log.warning("Loc Differs: %s, %s", self, other)
      return False
    return True

  def __hash__(self) -> int:
    return hash(self.var) + hash(self.fields)

  def __str__(self):
    members = ".".join(self.fields)
    return f"{self.var}.{members}"

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

  def __hash__(self) -> int:
    return hash(self.arg1) + hash(self.arg2) + hash(self.opr)

  def __str__(self): return f"{self.arg1} {self.opr} {self.arg2}"

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

  def __hash__(self) -> int:
    return hash(self.arg) + hash(self.opr)

  def __str__(self): return f"{self.opr}{self.arg}"

  def __repr__(self): return self.__str__()

class AddrOfE(UnaryE):
  """A unary arithmetic expression."""
  def __init__(self,
               arg: BasicET,
               loc: Optional[types.Loc] = None
  ) -> None:
    super().__init__(ADDROF_EXPR_EC, loc)
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

  def __hash__(self) -> int:
    return hash(self.arg) + hash(self.exprCode)

  def __str__(self): return f"{self.opr}{self.arg}"

  def __repr__(self): return self.__str__()

class SizeOfE(UnaryE):
  """Calculates size of the argument type in bytes at runtime."""
  def __init__(self,
               var: VarE, # var of types.VarArray type only
               loc: Optional[types.Loc] = None,
  ) -> None:
    super().__init__(SIZEOF_EXPR_EC, loc)
    self.var = var

  def __eq__(self,
             other: 'SizeOfE'
  ) -> bool:
    if not isinstance(other, SizeOfE):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.var == other.var:
      if LS: _log.warning("Arg Differs: %s, %s", self, other)
      return False
    if not self.loc == other.loc:
      if LS: _log.warning("Loc Differs: %s, %s", self, other)
      return False
    return True

  def __hash__(self) -> int:
    return hash(self.var) + hash(self.exprCode)

  def __str__(self): return f"{self.opr}{self.var}"

  def __repr__(self): return self.__str__()

class CastE(UnaryE):
  """A unary type cast expression."""
  def __init__(self,
               arg: UnitET,
               to: types.Type,
               loc: Optional[types.Loc] = None
  ) -> None:
    super().__init__(CAST_EXPR_EC, loc)
    self.arg = arg
    self.to = to

  def __eq__(self,
             other: 'CastE'
  ) -> bool:
    if not isinstance(other, CastE):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.to == other.to:
      if LS: _log.warning("CastType Differs: %s, %s", self, other)
      return False
    if not self.arg == other.arg:
      if LS: _log.warning("Arg Differs: %s, %s", self, other)
      return False
    if not self.loc == other.loc:
      if LS: _log.warning("Loc Differs: %s, %s", self, other)
      return False
    return True

  def __hash__(self) -> int:
    return hash(self.to) + hash(self.arg) + hash(self.exprCode)

  def __str__(self): return f"({self.to}){self.arg}"

  def __repr__(self): return self.__str__()

class AllocE(ExprET):
  """A stack allocator instruction.

  It allocates size * sizeof(self.type) on the stack,
  and returns the pointer to it.
  """
  def __init__(self,
               size: UnitET,
               loc: types.Loc,
  ) -> None:
    self.size = size
    self.loc = loc

  def __eq__(self,
             other: 'AllocE'
  ) -> bool:
    if not isinstance(other, AllocE):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.size == other.size:
      if LS: _log.warning("AllocaSize Differs: %s, %s", self, other)
      return False
    if not self.loc == other.loc:
      if LS: _log.warning("Loc Differs: %s, %s", self, other)
      return False
    return True

  def __hash__(self) -> int:
    return hash(self.size) + hash(self.exprCode)

  def __str__(self):
    return f"alloca {self.size}"

  def __repr__(self): return self.__str__()

class CallE(ExprET):
  """A function call expression.
  If callee is a types.VarE then its a function pointer.
  """
  def __init__(self,
               callee: VarE, # i.e. VarE or FuncE
               args: Optional[List[UnitET]] = None,
               loc: Optional[types.Loc] = None,
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

  def __hash__(self) -> int:
    return hash(self.callee) + hash(self.exprCode)

  def __str__(self):
    if self.args:
      args = [str(arg) for arg in self.args]
      expr = ",".join(args)
    else:
      expr = ""
    return f"{self.callee}({expr})"

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

  def __hash__(self) -> int:
    return hash(self.args) + hash(self.exprCode)

  def __str__(self): return f"phi({self.args})"

  def __repr__(self): return self.__str__()

class SelectE(ExprET):
  """Ternary operator."""
  def __init__(self,
               cond: UnitET, # use as a boolean value
               arg1: UnitET,
               arg2: UnitET,
               loc: Optional[types.Loc] = None,
  ) -> None:
    super().__init__(SELECT_EXPR_EC, loc)
    self.cond = cond
    self.arg1 = arg1
    self.arg2 = arg2

  def __eq__(self,
             other: 'SelectE'
  ) -> bool:
    if not isinstance(other, SelectE):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.cond == other.cond:
      if LS: _log.warning("CondVar Differs: %s, %s", self, other)
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

  def __hash__(self) -> int:
    return hash(self.cond) + hash(self.arg1) + hash(self.arg2)

  def __str__(self): return f"{self.cond} ? {self.arg1} : {self.arg2}"

  def __repr__(self): return self.__str__()
