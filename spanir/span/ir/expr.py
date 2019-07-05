#!/usr/bin/env python3

# MIT License
# Copyright (c) 2019 Anshuman Dhuliya

"""
All possible expressions used in an instruction.
"""

import logging
_log = logging.getLogger(__name__)
from typing import Optional, List, Set, Any

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
ALLOC_EXPR_EC: ExprCodeT      = 70

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
    self.exprCode = exprCode
    self.loc = loc
    self.type: types.Type = types.Void

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

  def toHooplIr(self) -> str:
    return "TODO"

class BasicET(ExprET):
  """Basic exprs like a var 'x', a value '12.3', a[10], x.y."""
  def __init__(self,
               exprCode: ExprCodeT,
               loc: Optional[types.Loc] = None
  ) -> None:
    super().__init__(exprCode, loc)

  def toHooplIr(self) -> str:
    # a virtual function
    return "TODO"

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
      # escaped = self.val.encode('unicode_escape')
      # return escaped.decode("ascii")
      newVal = repr(self.val)
      newVal = newVal[1:-1]
      newVal = "'" + newVal + "'"
      return f"{newVal}"
    return f"{self.val}"

  def __repr__(self): return self.__str__()

  def toHooplIr(self) -> str:
    if type(self.val) == str:
      return f"Lit \"{self.val}\""
    else:
      return f"Lit {self.val}"

class VarET(UnitET):
  """Entities that have a location."""
  def __init__(self,
               exprCode: ExprCodeT,
               loc: Optional[types.Loc] = None
  ) -> None:
    super().__init__(exprCode, loc)

class VarE(VarET):
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

  def __repr__(self): return self.__str__()

  def toHooplIr(self) -> str:
    if self.type.typeCode == types.PTR_TC:
      return f"PtrVar \"{self.name}\""
    else:
      return f"Var \"{self.name}\""

class PseudoVarE(VarET):
  """Holds a single pseudo variable name.
  Pseudo variables are used to name line
  and type based heap locations etc.

  Note: to avoid circular dependency avoid any operation on
  instructions in this module.
  """
  def __init__(self,
               name: types.VarNameT,
               loc: Optional[types.Loc] = None,
               insns: List[Any] = None, # list of instructions (max size 2)
               sizeExpr: ExprET = None, # the "arg" of malloc, or "arg1 * arg2" of calloc
  ) -> None:
    super().__init__(VAR_EXPR_EC, loc)
    self.name: types.VarNameT = name
    # First insn is always the memory alloc instruction
    # The second is optionally a cast assignment.
    self.insns = insns
    # sizeExpr is either a UnitET, i.e. malloc's arg
    #                 or a BinaryE, i.e. arg1 * arg2 (product of calloc's arg)
    self.sizeExpr = sizeExpr

  def __eq__(self,
             other: 'PseudoVarE'
  ) -> bool:
    if not isinstance(other, PseudoVarE):
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

  def toHooplIr(self) -> str:
    if self.type.typeCode == types.PTR_TC:
      return f"PtrVar \"{self.name}\""
    else:
      return f"Var \"{self.name}\""

class FuncE(VarET):
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

class ArrayE(VarET, BasicET):
  """An array expression."""
  def __init__(self,
               index: UnitET,
               of: 'BasicET',
               loc: Optional[types.Loc] = None
  ) -> None:
    super().__init__(ARR_EXPR_EC, loc)
    self.index = index
    self.of = of
    self.name = self.findName(self.of)

  def findName(self, of: BasicET):
    """Finds and sets the name of the array."""
    if isinstance(of, VarE):
      return of.name
    elif isinstance(of, ArrayE):
      return self.findName(of.of)

  def __eq__(self,
             other: 'ArrayE'
  ) -> bool:
    if not isinstance(other, ArrayE):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.index == other.index:
      if LS: _log.warning("Index Differs: %s, %s", self, other)
      return False
    if not self.of == other.of:
      if LS: _log.warning("Of Differs: %s, %s", self, other)
      return False
    if not self.loc == other.loc:
      if LS: _log.warning("Loc Differs: %s, %s", self, other)
      return False
    return True

  def __hash__(self) -> int:
    return hash(self.of) + hash(self.index)

  def __str__(self): return f"{self.of}[{self.index}]"

  def __repr__(self): return self.__str__()

class MemberE(VarET, BasicET):
  """A member access expression: e.g. x->f or x.f ..."""
  def __init__(self,
               name: types.MemberNameT,
               of: 'BasicET' = None,
               loc: Optional[types.Loc] = None
  ) -> None:
    super().__init__(MEMBER_EXPR_EC, loc)
    self.name = name
    self.of = of
    self.fullName = f"{repr(self.of)}.{self.name}"
    self.isPtrExpr = False # to be set in tunit module

  def __eq__(self,
             other: 'MemberE'
  ) -> bool:
    if not isinstance(other, MemberE):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.name == other.name:
      if LS: _log.warning("Name Differs: %s, %s", self, other)
      return False
    if not self.of == other.of:
      if LS: _log.warning("Parent(.of) Differs: %s, %s", self, other)
      return False
    if not self.loc == other.loc:
      if LS: _log.warning("Loc Differs: %s, %s", self, other)
      return False
    return True

  def __hash__(self) -> int:
    return hash(self.fullName)

  def __str__(self):
    return f"{self.of}.{self.name}"

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

  def toHooplIr(self) -> str:
    opStr = self.opr.toHooplIr()
    arg1Str = self.arg1.toHooplIr()
    arg2Str = self.arg2.toHooplIr()
    return f"Binary {opStr} ({arg1Str}) ({arg2Str})"

class UnaryET(ExprET):
  """A generic unary expression base class."""
  def __init__(self,
               exprCode: ExprCodeT,
               loc: Optional[types.Loc] = None
  ) -> None:
    super().__init__(exprCode, loc)

class UnaryE(UnaryET):
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

  def toHooplIr(self) -> str:
    return f"Unary ({self.arg})"

class AddrOfE(UnaryET):
  """A unary address-of expression."""
  def __init__(self,
               arg: BasicET,
               loc: Optional[types.Loc] = None
  ) -> None:
    super().__init__(ADDROF_EXPR_EC, loc)
    self.arg = arg

  def __eq__(self,
             other: 'AddrOfE'
  ) -> bool:
    if not isinstance(other, AddrOfE):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
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

  def __str__(self): return f"&{self.arg}"

  def __repr__(self): return self.__str__()

  def toHooplIr(self) -> str:
    argStr = self.arg.toHooplIr()
    return f"Unary AddrOf ({argStr})"

class SizeOfE(UnaryET):
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

  def __str__(self): return f"sizeof {self.var}"

  def __repr__(self): return self.__str__()

  def toHooplIr(self) -> str:
    varStr = self.var.toHooplIr()
    return f"Unary SizeOf ({varStr})"

class CastE(UnaryET):
  """A unary type cast expression."""
  def __init__(self,
               arg: BasicET,
               to: op.CastOp,
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

  def __str__(self): return f"({self.to}) {self.arg}"

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
    super().__init__(ALLOC_EXPR_EC, loc)
    self.size = size

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
  """Ternary conditional operator."""
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
