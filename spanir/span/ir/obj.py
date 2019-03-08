#!/usr/bin/env python3

# MIT License
# Copyright (c) 2019 Anshuman Dhuliya

"""All value types available in the IR."""

import logging
_log = logging.getLogger(__name__)
from typing import List, Dict, Tuple, Optional

from span.util.logger import LS
from span.ir.types import StructNameT, UnionNameT, FieldNameT, FuncNameT, VarNameT,\
  EdgeLabelT, BasicBlockId, Void,\
  Type, FuncSig, StructSig, UnionSig
from span.ir.instr import InstrIT

# object names: var name, func name, struct name, union name.
ObjNamesT = str

class ObjT:
  """Objects in IR"""
  def __init__(self): pass

class Var (ObjT):
  """A variable."""
  def __init__(self,
               name: VarNameT,
               type: Type,
  ) -> None:
    self.name = name
    self.type = type

  def __eq__(self,
             other: 'Var'
  ) -> bool:
    if not isinstance(other, Var):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.name == other.name:
      if LS: _log.warning("VarName Differs: '%s', '%s'", self.name, other.name)
      return False
    if not self.type == other.type:
      if LS: _log.warning("VarType Differs: '%s', '%s'", self.name, other.name)
      return False
    return True

class Func (ObjT):
  """A function.

  A function with instructions divided into basic blocks.
  """
  def __init__(self,
               name: FuncNameT,
               paramNames: Optional[List[VarNameT]] = None,
               returnType: Type = Void,
               paramTypes: Optional[List[Type]] = None,
               basicBlocks: Optional[Dict[BasicBlockId, List[InstrIT]]] = None,
               bbEdges: Optional[List[Tuple[BasicBlockId, BasicBlockId, EdgeLabelT]]] =
               None,
               loc: int = 0, # location
  ) -> None:
    self.name = name
    self.paramNames = paramNames
    self.sig = FuncSig(returnType, paramTypes)
    self.basicBlocks = basicBlocks if basicBlocks else dict()
    self.bbEdges = bbEdges if bbEdges else []
    self.loc = loc

  def __eq__(self,
             other: 'Func'
  ) -> bool:
    if not isinstance(other, Func):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.name == other.name:
      if LS: _log.warning("FuncName Differs: '%s', '%s'", self.name, other.name)
      return False
    if not self.paramNames == other.paramNames:
      if LS: _log.warning("ParamNames Differ: (Func: '%s') %s, %s",
                          self.name, self.paramNames, other.paramNames)
      return False
    if not self.sig == other.sig:
      if LS: _log.warning("FuncSig Differs: (Func: '%s')", self.name)
      return False

    if not self.bbEdges == other.bbEdges:
      if LS: _log.warning("CfgStructure Differs (func: '%s'):\n\n%s,\n\n,%s",
                          self.name, self.bbEdges, other.bbEdges)
      return False
    selfBbIds = self.basicBlocks.keys()
    otherBbIds = other.basicBlocks.keys()
    if not len(selfBbIds) == len(otherBbIds):
      if LS: _log.warning("NumOf_BBs differ: (Func: '%s') %s, %s",
                          self.name, selfBbIds, otherBbIds)
      return False
    if not selfBbIds == otherBbIds:
      if LS: _log.warning("BbNumbering Differ: (Func: '%s') %s, %s",
                          self.name, self, other)
      return False
    for bbId in self.basicBlocks.keys():
      selfBb = self.basicBlocks[bbId]
      otherBb = other.basicBlocks[bbId]
      if not selfBb == otherBb:
        if LS: _log.warning("BB Differs: (Func: '%s') %s, %s",
                            self.name, selfBb, otherBb)
        return False
      return True

    if not self.loc == other.loc:
      if LS: _log.warning("Loc Differs: (Func: '%s') %s, %s",
                          self.name, self.loc, other.loc)
      return False
    return True

class Struct (ObjT):
  """A structure."""
  def __init__(self,
               name: StructNameT,
               fieldNames: List[FieldNameT],
               fieldTypes: List[Type],
               loc: int = 0  # definition location if available
  ) -> None:
    self.name = name
    self.fieldNames = fieldNames
    self.sig: StructSig = StructSig(fieldTypes)
    self.loc = loc

  def __eq__(self,
             other: 'Struct'
  ) -> bool:
    if not isinstance(other, Struct):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.fieldNames == other.fieldNames:
      if LS: _log.warning("FieldNames Differ: %s, %s", self, other)
      return False
    if not self.fieldTypes == other.fieldTypes:
      if LS: _log.warning("FieldTypes Differ: %s, %s", self, other)
      return False
    if not self.loc == other.loc:
      if LS: _log.warning("Loc Differs: (Struct: '%s') %s, %s",
                          self.name, self.loc, other.loc)
      return False
    return True

class Union (ObjT):
  """A union."""
  def __init__(self,
               name: UnionNameT,
               fieldNames: List[FieldNameT],
               fieldTypes: List[Type],
               loc: int = 0
  ) -> None:
    self.name = name
    self.fieldNames = fieldNames
    self.sig: UnionSig = UnionSig(fieldTypes)
    self.loc = loc

  def __eq__(self,
             other: 'Union'
  ) -> bool:
    if not isinstance(other, Union):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.fieldNames == other.fieldNames:
      if LS: _log.warning("FieldNames Differ: (Struct: '%s') %s, %s",
                          self.name, self, other)
      return False
    if not self.fieldTypes == other.fieldTypes:
      if LS: _log.warning("FieldTypes Differ: (Struct: '%s') %s, %s",
                          self.name, self, other)
      return False
    if not self.loc == other.loc:
      if LS: _log.warning("Loc Differs: (Struct: '%s') %s, %s",
                          self.name, self.loc, other.loc)
      return False
    return True
