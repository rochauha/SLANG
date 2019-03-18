#!/usr/bin/env python3

# MIT License
# Copyright (c) 2019 Anshuman Dhuliya

"""
All operators used in expressions.
"""

import logging
_log = logging.getLogger(__name__)

from span.util.logger import LS
import span.ir.types as types

OpCodeT = int

################################################
#BOUND START: op_codes
################################################

# BO = BinaryOperator
# UO = UnaryOperator
# OC = OperatorCode
# Naming: <BO|UO>_<opName>_OC

# the order and ascending sequence may be important

# numeric_ops
UO_PLUS_OC: OpCodeT     = 101 # +
UO_MINUS_OC: OpCodeT    = 102 # -

# unary pointer_ops
UO_ADDROF_OC: OpCodeT   = 103 # &
UO_DEREF_OC: OpCodeT    = 104 # *
UO_SIZEOF_OC: OpCodeT   = 105 # sizeof()

# bitwise_ops
UO_BIT_NOT_OC: OpCodeT  = 110 # ~

# logical_ops
UO_NOT_OC: OpCodeT      = 120 # !

# NOTE: all OpCodes less than 200 are unary.

# numeric_ops
BO_ADD_OC: OpCodeT      = 201 # +
BO_SUB_OC: OpCodeT      = 202 # -
BO_MUL_OC: OpCodeT      = 203 # *
BO_DIV_OC: OpCodeT      = 204 # /
BO_MOD_OC: OpCodeT      = 205 # %

# numeric relational_ops
BO_LT_OC: OpCodeT       = 207 # <
BO_LE_OC: OpCodeT       = 208 # <=
BO_EQ_OC: OpCodeT       = 209 # ==
BO_NE_OC: OpCodeT       = 210 # !=
BO_GE_OC: OpCodeT       = 211 # >=
BO_GT_OC: OpCodeT       = 212 # >

# integer bitwise_ops
BO_BIT_AND_OC: OpCodeT  = 300 # &
BO_BIT_OR_OC: OpCodeT   = 301 # |
BO_BIT_XOR_OC: OpCodeT  = 302 # ^

# integer shift_ops
BO_LSHIFT_OC: OpCodeT   = 400 # <<
BO_RSHIFT_OC: OpCodeT   = 401 # >>
BO_RRSHIFT_OC: OpCodeT  = 402 # >>>

# array_index
BO_ARR_OC: OpCodeT      = 500 # [] e.g. arr[3]

################################################
#BOUND END  : op_codes
################################################

class Op(types.AnyT):
  """class type for all operators.

  Attributes:
    opCode: opcode of the operator
  """
  def __init__(self,
               opCode:OpCodeT
  ) -> None:
    self.opCode = opCode

  def isUnaryOp(self):
    return UO_PLUS_OC <= self.opCode < BO_ADD_OC

  def isBinaryOp(self):
    return self.opCode >= BO_ADD_OC

  def isArithmeticOp(self):
    return BO_ADD_OC <= self.opCode <= BO_MOD_OC\
           or UO_PLUS_OC <= self.opCode <= UO_MINUS_OC

  def isRelationalOp(self):
    return BO_LT_OC <= self.opCode <= BO_GT_OC

  def isInequalityOp(self):
    return BO_EQ_OC <= self.opCode <= BO_NE_OC

  def isPointerOp(self):
    return UO_ADDROF_OC <= self.opCode <= UO_DEREF_OC

  def __eq__(self,
             other: 'Op'
  ) -> bool:
    if not isinstance(other, Op):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.opCode == other.opCode:
      if LS: _log.warning("Operator Differs: %s, %s", self, other)
      return False
    return True

  def __str__(self):
    ss = ""
    opCode = self.opCode
    if   opCode == UO_ADDROF_OC: ss = "&"
    elif opCode == UO_DEREF_OC: ss = "*"

    elif opCode == UO_MINUS_OC: ss = "-"
    elif opCode == UO_PLUS_OC: ss = "+"

    elif opCode == UO_BIT_NOT_OC: ss = "~"
    elif opCode == UO_NOT_OC: ss = "!"

    elif opCode == BO_ADD_OC: ss = "+"
    elif opCode == BO_SUB_OC: ss = "-"
    elif opCode == BO_MUL_OC: ss = "*"
    elif opCode == BO_DIV_OC: ss = "/"
    elif opCode == BO_MOD_OC: ss = "%"

    elif opCode == BO_LT_OC: ss = "<"
    elif opCode == BO_LE_OC: ss = "<="
    elif opCode == BO_EQ_OC: ss = "=="
    elif opCode == BO_NE_OC: ss = "!="
    elif opCode == BO_GE_OC: ss = ">="
    elif opCode == BO_GT_OC: ss = ">"

    elif opCode == BO_BIT_AND_OC: ss = "&"
    elif opCode == BO_BIT_OR_OC: ss = "|"
    elif opCode == BO_BIT_XOR_OC: ss = "^"

    elif opCode == BO_LSHIFT_OC: ss = "<<"
    elif opCode == BO_RSHIFT_OC: ss = ">>"
    elif opCode == BO_RRSHIFT_OC: ss = ">>>"
    elif opCode == BO_ARR_OC: ss = "[]"

    return ss

class BinaryOp(Op):
  def __init__(self,
               opCode: OpCodeT
  ) -> None:
    super().__init__(opCode)

class UnaryOp(Op):
  def __init__(self,
               opCode: OpCodeT
  ) -> None:
    super().__init__(opCode)

################################################
#BOUND START: operator_objects
################################################

UO_PLUS: UnaryOp = UnaryOp(UO_PLUS_OC)
UO_MINUS: UnaryOp = UnaryOp(UO_MINUS_OC)
UO_ADDROF: UnaryOp = UnaryOp(UO_ADDROF_OC)
UO_DEREF: UnaryOp = UnaryOp(UO_DEREF_OC)

UO_SIZEOF: UnaryOp = UnaryOp(UO_SIZEOF_OC)

UO_BIT_NOT: UnaryOp = UnaryOp(UO_BIT_NOT_OC)
UO_NOT: UnaryOp = UnaryOp(UO_NOT_OC)

BO_ADD: BinaryOp = BinaryOp(BO_ADD_OC)
BO_SUB: BinaryOp = BinaryOp(BO_SUB_OC)
BO_MUL: BinaryOp = BinaryOp(BO_MUL_OC)
BO_DIV: BinaryOp = BinaryOp(BO_DIV_OC)
BO_MOD: BinaryOp = BinaryOp(BO_MOD_OC)

BO_LT: BinaryOp = BinaryOp(BO_LT_OC)
BO_LE: BinaryOp = BinaryOp(BO_LE_OC)
BO_EQ: BinaryOp = BinaryOp(BO_EQ_OC)
BO_NE: BinaryOp = BinaryOp(BO_NE_OC)
BO_GE: BinaryOp = BinaryOp(BO_GE_OC)
BO_GT: BinaryOp = BinaryOp(BO_GT_OC)

BO_BIT_AND: BinaryOp = BinaryOp(BO_BIT_AND_OC)
BO_BIT_OR: BinaryOp = BinaryOp(BO_BIT_OR_OC)
BO_BIT_XOR: BinaryOp = BinaryOp(BO_BIT_XOR_OC)

BO_LSHIFT: BinaryOp = BinaryOp(BO_LSHIFT_OC)
BO_RSHIFT: BinaryOp = BinaryOp(BO_RSHIFT_OC)
BO_RRSHIFT: BinaryOp = BinaryOp(BO_RRSHIFT_OC)

BO_INDEX: UnaryOp = BinaryOp(BO_ARR_OC)

################################################
#BOUND END  : operator_objects
################################################
