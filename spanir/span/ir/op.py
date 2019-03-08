#!/usr/bin/env python3

# MIT License
# Copyright (c) 2019 Anshuman Dhuliya

"""
All operators used in expressions.
"""

import logging
_log = logging.getLogger(__name__)

import span.ir.types as types

OpCodeT = int

################################################
#BOUND START: op_codes
################################################

# the order and ascending sequence is important
UO_PLUS_OC:OpCodeT     = 101
UO_MINUS_OC:OpCodeT    = 102
UO_ADDROF_OC:OpCodeT   = 103
UO_DEREF_OC:OpCodeT    = 104

BO_ADD_OC:OpCodeT      = 201
BO_SUB_OC:OpCodeT      = 202
BO_MUL_OC:OpCodeT      = 203
BO_DIV_OC:OpCodeT      = 204
BO_MOD_OC:OpCodeT      = 205
BO_LT_OC:OpCodeT       = 207
BO_LE_OC:OpCodeT       = 208
BO_EQ_OC:OpCodeT       = 209
BO_NE_OC:OpCodeT       = 210
BO_GE_OC:OpCodeT       = 211
BO_GT_OC:OpCodeT       = 212

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
    return self.opCode == other.opCode

  def __str__(self):
    ss = ""
    opCode = self.opCode
    if   opCode == UO_ADDROF_OC: ss = "&"
    elif opCode == UO_DEREF_OC: ss = "*"
    elif opCode == UO_MINUS_OC: ss = "-"
    elif opCode == UO_PLUS_OC: ss = "+"
    elif opCode == BO_ADD_OC: ss = "+"
    elif opCode == BO_MUL_OC: ss = "*"
    elif opCode == BO_MOD_OC: ss = "%"

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

UO_PLUS = UnaryOp(UO_PLUS_OC)
UO_MINUS = UnaryOp(UO_MINUS_OC)
UO_ADDROF = UnaryOp(UO_ADDROF_OC)
UO_DEREF = UnaryOp(UO_DEREF_OC)

BO_ADD = BinaryOp(BO_ADD_OC)
BO_SUB = BinaryOp(BO_SUB_OC)
BO_MUL = BinaryOp(BO_MUL_OC)
BO_DIV = BinaryOp(BO_DIV_OC)
BO_MOD = BinaryOp(BO_MOD_OC)
BO_LT = BinaryOp(BO_LT_OC)
BO_LE = BinaryOp(BO_LE_OC)
BO_EQ = BinaryOp(BO_EQ_OC)
BO_NE = BinaryOp(BO_NE_OC)
BO_GE = BinaryOp(BO_GE_OC)
BO_GT = BinaryOp(BO_GT_OC)

################################################
#BOUND END  : operator_objects
################################################
