#!/usr/bin/env python3

# MIT License
# Copyright (c) 2019 Anshuman Dhuliya

"""
All possible expressions used in an instruction.
"""

import logging
_log = logging.getLogger(__name__)
from typing import Optional, List

from span.util.logger import LS
import span.ir.types as types
from span.ir.types import SourceLocationT
import span.ir.op as op
import span.util.util as util

ExprId = int
ExprCodeT = int

################################################
#BOUND START: expr_codes
################################################

# the order and ascending sequence is important
VAR_EXPR_EC: ExprCodeT       = 11
LIT_EXPR_EC: ExprCodeT       = 12

UNARY_EXPR_EC: ExprCodeT     = 20
BINARY_EXPR_EC: ExprCodeT    = 30

CALL_EXPR_EC: ExprCodeT      = 40

################################################
#BOUND END  : expr_codes
################################################

class ExprET(types.AnyT):
  """Base class for all expressions."""
  def __init__(self,
               exprCode: ExprCodeT,
               loc: SourceLocationT = 0
  ) -> None:
    if self.__class__.__name__.endswith("T"): super().__init__()
    self.type = types.Void
    self.exprCode = exprCode
    self.loc = loc

  def isUnaryExpr(self):
    return self.exprCode == UNARY_EXPR_EC

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
               loc: SourceLocationT = 0
  ) -> None:
    super().__init__(exprCode, loc)

class VarE(UnitET):
  """A single numeric literal. Bools are also numeric."""
  def __init__(self,
               name: types.VarNameT,
               loc: SourceLocationT = 0
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

  def __str__(self):
    name = self.name.split(":")[-1]
    return f"{name}"

  def __repr__(self): return self.name

class LitE(UnitET):
  """A single numeric literal. Bools are also numeric."""
  def __init__(self,
               val,
               loc: SourceLocationT = 0
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

  def __str__(self): return f"{self.val}"

  def __repr__(self): return self.__str__()

class BinaryE(ExprET):
  """A binary expression."""
  def __init__(self,
               arg1: UnitET,
               opr: op.BinaryOp,
               arg2: UnitET,
               loc: SourceLocationT = 0
  ) -> None:
    super().__init__(BINARY_EXPR_EC, loc)
    self.arg1 = arg1
    self.op = opr
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

  def __str__(self): return f"{self.arg1} {self.op} {self.arg2}"

  def __repr__(self): return self.__str__()

class UnaryE(ExprET):
  """A unary arithmetic expression."""
  def __init__(self,
               opr: op.UnaryOp,
               arg: UnitET,
               loc: SourceLocationT = 0
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

  def __str__(self): return f"{self.op}{self.arg}"

  def __repr__(self): return self.__str__()

class CallE(ExprET):
  """A function call expression."""
  def __init__(self,
               funcName: types.FuncNameT,
               args: Optional[List[UnitET]],
               loc: SourceLocationT = 0
  ) -> None:
    super().__init__(CALL_EXPR_EC, loc)
    self.funcName = funcName
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
    if not self.funcName == other.funcName:
      if LS: _log.warning("FuncName Differs: %s, %s", self, other)
      return False
    if not self.args == other.args:
      if LS: _log.warning("Args Differ: %s, %s", self, other)
      return False
    if not self.loc == other.loc:
      if LS: _log.warning("Loc Differs: %s, %s", self, other)
      return False
    return True

  def __str__(self): return f"CallE[{self.funcName}({self.args})]"

  def __repr__(self): return self.__str__()

