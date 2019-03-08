#!/usr/bin/env python3

# MIT License
# Copyright (c) 2019 Anshuman Dhuliya

"""Contains the information on one analysis/translation unit.

This module reinitializes itself for each analysis unit.
Every analysis unit should have the following call at its end,

  buildTUnit(name, description, all_vars, all_func)

"""

import logging
_log = logging.getLogger(__name__)
from typing import Dict, Set, Tuple
import io

from span.util.logger import LS
import span.util.messages as msg

import span.ir.types as types
import span.ir.op as op
import span.ir.expr as expr
import span.ir.instr as instr
import span.ir.obj as obj

import span.api.lattice as lattice

class TUnit:
  """A Translation Unit."""
  def __init__(self,
               name: str,
               description: str,
               allVars: Dict[obj.VarNameT, types.Type],
               allObjs: Dict[obj.ObjNamesT, obj.ObjT]
  ) -> None:
    self.name = name
    self.description = description
    self.allVars = allVars
    self.allObjs = allObjs
    
  def __eq__(self,
             other: 'TUnit'
  ) -> bool:
    isEqual = True
    if not isinstance(other, TUnit):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      isEqual = False
    if not self.name == other.name:
      if LS: _log.warning("TUnitName Differs: '%s', '%s'", self.name, other.name)
      isEqual = False

    selfAllVarNames = self.allVars.keys()
    otherAllVarNames = other.allVars.keys()
    matchVariables = True
    if not len(selfAllVarNames) == len(otherAllVarNames):
      if LS: _log.warning("NumOfVars Differ: (TUnit: '%s')", self.name)
      isEqual = False
      matchVariables = False
    if not selfAllVarNames == otherAllVarNames:
      if LS: _log.warning("VarNamesDiffer: (TUnit: '%s')", self.name)
      isEqual = False
      matchVariables = False
    if matchVariables:
      for varName in selfAllVarNames:
        selfVarType = self.allVars[varName]
        otherVarType = other.allVars[varName]
        if not selfVarType == otherVarType:
          if LS: _log.warning("VarType Differs: (Var: '%s')", varName)
          isEqual = False

    selfAllObjNames = self.allObjs.keys()
    otherAllObjNames = other.allObjs.keys()
    matchObjects = True
    if not len(selfAllObjNames) == len(otherAllObjNames):
      if LS: _log.warning("NumOfObjs Differ: (TUnit: '%s')", self.name)
      isEqual = False
      matchObjects = False
    if not selfAllObjNames == otherAllObjNames:
      if LS: _log.warning("ObjNamesDiffer: (TUnit: '%s')", self.name)
      isEqual = False
      matchObjects = False
    if matchObjects:
      for objName in selfAllObjNames:
        selfObj = self.allObjs[objName]
        otherObj = other.allObjs[objName]
        if not isinstance(otherObj, selfObj.__class__):
          if LS: _log.warning("ObjType Differs: (Obj: '%s')", objName)
          isEqual = False
        if not selfObj == otherObj:
          if LS: _log.warning("Obj Differs: (Obj: '%s', Type: '%s')",
                              objName, selfObj.__class__)
          isEqual = False

    return isEqual

# analysis unit name and description
initialized = False

#BOUND START: given_input; see build()
unitName: str = None
unitDescription: str = None

unitVarMap: Dict[types.VarNameT, types.Type] = None
unitObjMap: Dict[types.FuncNameT, obj.Func] = None
#BOUND END  : given_input; see build()

_typeVarMap: Dict[Tuple[types.FuncNameT, types.Type], Set[types.VarNameT]] = None
_globalVars: Set[types.VarNameT] = None
_localVars: Dict[types.FuncNameT, Set[types.VarNameT]] = None

def buildTUnit(currTUnit: TUnit) -> None:
  """Intializes the universe with the given analysis unit."""
  global unitVarMap, unitObjMap
  global unitName, unitDescription
  global initialized
  logUsefulInfo(currTUnit)

  unitName = currTUnit.name
  unitDescription = currTUnit.description
  unitVarMap = currTUnit.allVars
  unitObjMap = currTUnit.allObjs

  initialized = False
  if LS: _log.info("Constructing_TUnit: START.")
  for objName, irObj in unitObjMap.items():
    irObj: obj.Func = irObj
    if objName.startswith("f:"):
      for _, instrs in irObj.basicBlocks.items():
        for insn in instrs:
          inferInstrType(insn)
      if LS: _log.debug("Processed_Function %s.", objName)

  if LS: _log.info("Constructing_TUnit: END.")
  initialized = True

def logUsefulInfo(currTUnit: TUnit) -> None:
  """Logs the information from a translation unit."""
  if LS: _log.info("\nNEW TRANSLATION_UNIT: Name: %s, Vars: %s, Objs: %s.\n",
                   currTUnit.name, len(currTUnit.allVars), len(currTUnit.allObjs))
  if LS: _log.info("Description: %s", currTUnit.description)

  with io.StringIO() as sio:
    sio.write(f"VarDict (Total: {len(currTUnit.allVars)}), {{var name: "
              f"var type}}:\n")
    for varName in sorted(currTUnit.allVars):
      sio.write(f"  {varName!r}: {currTUnit.allVars[varName]}.\n")
    if LS: _log.info("%s", sio.getvalue())

  with io.StringIO() as sio:
    sio.write(f"ObjDict (Total: {len(currTUnit.allObjs)}):\n")
    for objName in sorted(currTUnit.allObjs):
      if objName.startswith("f:"):
        func: obj.Func = currTUnit.allObjs[objName]
        sio.write(f"  {objName!r}: returns: {func.sig.returnType}, params: "
                f"{func.paramNames}.\n")
      else:
        sio.write(f"{currTUnit.allObjs[objName]}\n")
    if LS: _log.info("%s", sio.getvalue())

def getEnvVars(funcName: types.FuncNameT
) -> Set[types.VarNameT]:
  """Returns set of variables visible/accessible in a given function."""
  assert initialized, "TUnit_not_initialized."

  return getGlobalVars() | getLocalVars(funcName)

def getLocalVars(funcName: types.FuncNameT
) -> Set[types.VarNameT]:
  """Returns set of variable names local to a function."""
  assert initialized, "TUnit_not_initialized."
  global _localVars
  if _localVars is None: _localVars = dict()
  if funcName in _localVars:
    return _localVars[funcName]

  vnameSet = set()
  for vname in unitVarMap:
    count = vname.count(":")
    if count == 2:
      sp = vname.split(":")
      if funcName[2:] == sp[1]: vnameSet.add(vname)
  _localVars[funcName] = vnameSet
  return vnameSet

def getGlobalVars() -> Set[types.VarNameT]:
  """Returns set of global variable names."""
  assert initialized, "TUnit_not_initialized."
  global _globalVars

  if _globalVars is not None: return _globalVars

  vname_set = set()
  for vname in unitVarMap:
    if vname.count(":") == 1:
      vname_set.add(vname)
  _globalVars = vname_set
  return vname_set

def getVarsOfType(fname: types.FuncNameT,
                  tp: types.Type,
) -> Set[types.VarNameT]:
  """Returns set of vars visible in a function of the given kind."""
  global _typeVarMap, unitVarMap
  tup = (fname, tp)
  if _typeVarMap is None: _typeVarMap = dict()
  if tup in _typeVarMap: return _typeVarMap[tup]

  scope_vars = getEnvVars(fname)

  varset = set()
  for vname in scope_vars:
    currtp = inferBasicType(vname)
    if currtp == tp: varset.add(vname)
  _typeVarMap[tup] = varset
  return varset

################################################
#BOUND START: Type_Inference
################################################

class NumL(lattice.LatticeLT):
  """Lattice for numeric types."""
  """TODO: include all int and float types."""
  _the_top: 'NumL' = None
  _the_bot: 'NumL' = None

  def __init__(self,
               numType: types.Type
  ) -> None:
    top = bot = False
    if numType.isInteger(): top = True
    if numType.isFloat(): bot = True
    super().__init__(top, bot)
    self.numType = numType

  def meet(self, other: 'NumL') -> Tuple['NumL', lattice.ChangeL]:
    if self.bot: return self, lattice.NoChange
    if other.bot: return other, lattice.Changed
    return self, lattice.NoChange

  @classmethod
  def get_top(cls):
    if cls._the_top is None: cls._the_top = NumL(types.Int)
    return cls._the_top

  @classmethod
  def get_bot(cls):
    if cls._the_bot is None: cls._the_bot = NumL(types.Float)
    return cls._the_bot

  @classmethod
  def make(cls, expr_type: types.Type):
    if expr_type == types.Int: return cls.get_top()
    if expr_type == types.Float: return cls.get_bot()

    msg = "Invalid type given."
    if LS: _log.error(msg)
    raise ValueError(msg)

def inferBasicType(val):
  """Returns the type for the given value."""
  global unitVarMap

  if type(val) == int: return types.Int
  if type(val) == float: return types.Float

  if type(val) == str:
    if val in unitVarMap: return unitVarMap[val]
    if val in unitObjMap: return unitObjMap[val].sig.returnType

  msg = f"UnknownValue: {val}"
  if LS: _log.error(msg)
  raise ValueError(msg)

def inferExprType(e: expr.ExprET):
  """Infer expr type, store the type info in the object and return the type."""
  eType = types.Void
  exprCode = e.exprCode
  if exprCode == expr.VAR_EXPR_EC:
    eType = inferBasicType(e.name)
  elif exprCode == expr.LIT_EXPR_EC:
    eType = inferBasicType(e.val)
  elif exprCode == expr.UNARY_EXPR_EC:
    opCode = e.op.opCode
    if opCode == op.UO_ADDROF_OC:
      assert isinstance(e.arg, expr.VarE), f"Operand should be a var: {e}."
      eType = inferExprType(e.arg)
      if eType.typeCode == types.PTR_TC:
        eType = types.Ptr(eType.to, eType.indir+1)
      else:
        eType = types.Ptr(eType, 1)
    elif opCode == op.UO_DEREF_OC:
      assert isinstance(e.arg, expr.VarE), f"Operand should be a var: {e}."
      eType = inferExprType(e.arg)
      eType = eType.to  # the type it points to
  elif isinstance(e, expr.BinaryE) and e.op.isArithmeticOp():
    itype1 = inferExprType(e.arg1)
    itype2 = inferExprType(e.arg2)
    if itype1.isNumeric() or itype2.isNumeric():
      eType = NumL(itype1).meet(NumL(itype2))[0].numType
    elif itype1 == itype2:
      eType = itype1
    else:
      assert False, msg.CONTROL_HERE_ERROR
  else:
    if LS: _log.error("Unknown_Expr_For_TypeInference: %s.", e)

  e.type = eType
  return eType

def inferInstrType(insn: instr.InstrIT):
  """Infer instruction type automatically."""
  iType = types.Void
  if isinstance(insn, instr.AssignI):
    t1 = inferExprType(insn.lhs)
    t2 = inferExprType(insn.rhs)
    if t1.isNumeric() or t2.isNumeric():
      iType = NumL(t1).meet(NumL(t2))[0].numType
    elif t1 == t2:
      iType = t1
    else:
      assert False, f"Lhs and Rhs types differ: {insn}."
  elif isinstance(insn, instr.UseI):
    for var in insn.vars:
      _ = inferExprType(var)
    iType = types.Void
  elif isinstance(insn, instr.CondReadI):
    iType = inferExprType(insn.lhs)
    for ex in insn.rhs:
      inferExprType(ex)
  elif isinstance(insn, instr.UnDefValI):
    iType = inferExprType(insn.lhs)
  elif isinstance(insn, instr.CondI):
    _ = inferExprType(insn.arg)
    iType = types.Void
  elif isinstance(insn, instr.ReturnI):
    iType = inferExprType(insn.arg)
  else:
    if LS: _log.error("Unknown_Instr_For_TypeInference: %s.", insn)

  insn.type = iType
  return iType

################################################
#BOUND END  : Type_Inference
################################################

