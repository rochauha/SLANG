#!/usr/bin/env python3

# MIT License
# Copyright (c) 2019 Anshuman Dhuliya

"""
Defines a translation unit.
Following important things are available here,
  1. Actions to pre-processes IR before analysis can be done on the same,
     a. transforms the IR (Note that all transformations in the system
        can be found here, or invoked here: see preProcess())
     b. infers types of expressions and instructions
     c. caches information into data structures for easy access
  2. Provides API to fetch useful information from a translation unit.
"""

import logging
_log = logging.getLogger(__name__)
from typing import Dict, Set, Tuple, Optional, List
import io
import re

from span.util.util import LS, AS
import span.util.messages as msg

import span.ir.types as types
import span.ir.op as op
import span.ir.expr as expr
import span.ir.instr as instr
import span.ir.obj as obj
import span.ir.graph as graph

ANY_TMPVAR_REGEX = re.compile(r".*t\.\d+\w+$")
NORMAL_TMPVAR_REGEX = re.compile(r".*t\.\d+t$")
COND_TMPVAR_REGEX = re.compile(r".*t\.\d+if$")
LOGICAL_TMPVAR_REGEX = re.compile(r".*t\.\d+L$")

# Both these variables are inter-related
# One assigns a name the other detects the name.
NAKED_PSEUDO_VAR_NAME = "p.{count}"
PSEUDO_VAR_REGEX = re.compile(r".*p\.\d+$")

"""
The null pointer is considered a null object
of type Void. This object is used in place of
zero or NULL/nullptr assignment to a pointer.
"""
NULL_OBJ_NAME = "g:0/Null"
NULL_OBJ_TYPE = types.Void

# FIXME: how to match size_t (assuming types.UInt64 for now)
memAllocFunctions = {
  # void* malloc (size_t size); // real declaration
  "f:malloc": types.FuncSig(returnType=types.Ptr(to=types.Void),
                            variadic=False,
                            paramTypes=[types.UInt64]),

  # void* calloc (size_t num, size_t size); // real declaration
  "f:calloc": types.FuncSig(returnType=types.Ptr(to=types.Void),
                            variadic=False,
                            paramTypes=[types.UInt64, types.UInt64]),

  # NOTE: realloc and free are written here for reference only.
  # NOTE: There is no need for them to be in this dictionary.
  # void* realloc (void* ptr, size_t size); // real declaration
  # "f:realloc": types.FuncSig(returnType=types.Ptr(to=types.Void),
  #                          variadic=False,
  #                          paramTypes=[types.Ptr(to=types.Void), types.UInt64]),

  # void free (void* ptr); // real declaration
}

def isTmpVar(vName: types.VarNameT) -> bool:
  """Is it a tmp var (of any form)"""
  if ANY_TMPVAR_REGEX.fullmatch(vName):
    return True
  return False

def isNormalTmpVar(vName: types.VarNameT) -> bool:
  """Is it a normal tmp var"""
  if NORMAL_TMPVAR_REGEX.fullmatch(vName):
    return True
  return False

def isCondTmpVar(vName: types.VarNameT) -> bool:
  """Is a tmp var used in if statements."""
  if COND_TMPVAR_REGEX.fullmatch(vName):
    return True
  return False

def isLogicalTmpVar(vName: types.VarNameT) -> bool:
  """Is a tmp var used to break logical expressions: &&, ||"""
  if LOGICAL_TMPVAR_REGEX.fullmatch(vName):
    return True
  return False

def isPseudoVar(vName: types.VarNameT) -> bool:
  """Is it a pseudo var? (used to hide malloc/calloc)"""
  if PSEUDO_VAR_REGEX.fullmatch(vName):
    return True
  return False

def getNullObjName() -> str:
  """Returns the standard name to be used for a null object."""
  return NULL_OBJ_NAME

def isNullObjName(name: str) -> bool:
  """Is the given name that of the null object?"""
  return NULL_OBJ_NAME == name

class TranslationUnit:
  """A Translation Unit.
  It holds the completely converted C file (from Clang AST)
  SPAN IR undergoes many iteration of changes here (see preProcess()).
  """
  def __init__(self,
               name: str,
               description: str,
               allVars: Dict[obj.VarNameT, types.Type],
               allObjs: Dict[obj.ObjNamesT, obj.ObjT]
  ) -> None:
    # analysis unit name and description
    self.name = name
    self.description = description

    # whole of TU is contained in these two dictionaries
    self.allVars = allVars
    self.allObjs = allObjs

    self.initialized: bool = False

    # Set of all global vars in this translation unit.
    self._globalVars: Set[types.VarNameT] = set()
    # Set of all global vars categorized by types
    self._typeGlobalVarMap: Dict[types.Type, Set[types.VarNameT]] = set()
    # The local variables in each function.
    self._localVars: Dict[types.FuncNameT, Set[types.VarNameT]] = {}
    # The local pseudo variables in each function
    self._localPseudoVars: Dict[types.FuncNameT, Set[types.VarNameT]] = {}
    # All the pseudo variables in the translation unit
    self._allPseudoVars: Set[types.VarNameT] = None # IMPORTANT 'None' init
    # map (func, givenType) to vars of givenType accessible in the func (local+global)
    self._typeVarMap: Dict[Tuple[types.FuncNameT, types.Type], Set[types.VarNameT]] = {}
    # Set of all pseudo vars in this translation unit.
    # Note: pseudo vars hide memory allocation under a variable name.
    self._pseudoVars: Set[types.VarNameT] = set()

    # function signature (funcsig) to function object mapping
    self._funcSigToFuncObjMap: Dict[types.FuncSig, List[obj.Func]] = {}
    # maps tmps assigned only once to the assigned expression
    self._funcTmpExprMap: Dict[types.VarNameT, expr.ExprET] = {}

    # stores the increasing counter for pseudo variables in the function
    # pseudo variables replace malloc/calloc calls as addressOf(pseudoVar)
    self._funcPseudoCountMap: Dict[types.FuncNameT, int] = {}

    self.preProcess()

  # mainentry point for processing translation unit
  def preProcess(self):
    """Transforms and tunes the translation unit before it can be used for analysis.
    ALL changes to SPAN IR before analysis are initiated from here.
    The relative positions of the transformations may be critical.
    """
    self.initialized = False

    self.logUsefulInfo()
    if LS: _log.info("PreProcessing_TUnit: START.")

    # STEP 1: Complete the record types that are recursive.
    self.fillTheRecordTypes() # IMPORTANT (MUST)

    # STEP 2: Generate CFG/BB from the linear instructions given.
    self.genBasicBlocks() # IMPORTANT (MUST)

    # STEP 3: Add the reference of this TUnit object to the objects
    #         in the translation unit (so they know who contains them).
    self.addThisTUnitRefToObjs() # IMPORTANT (MUST)

    # STEP 4: Infer and fill types of various
    #         entities (function params, expr, instr)
    self.fillFuncParamTypes() # IMPORTANT (MUST)
    self.inferAllInstrTypes() # IMPORTANT (MUST)

    # STEP 5: Transform/tune memory allocation instructions.
    self.replaceMemAllocations()
    self.replaceZeroWithNullPtr() # FIXME: should be used?

    # STEP 6: Perform various other optimizations
    self.optimizeO3() # OPTIMIZE (MUST)

    # STEP 7: Finally create CFG/Node from CFG/BB
    #         SPAN works on CFG/Node representation only.
    self.genCfgs() # (MUST)

    self.initialized = True
    if LS: _log.info("PreProcessing_TUnit: END/DONE.")

  def replaceZeroWithNullPtr(self):
    """Replace statements assigning Zero to pointers,
    with a special NULL_OBJ."""
    # Add the special null object.
    self.allVars[NULL_OBJ_NAME] = NULL_OBJ_TYPE

    for objName, irObj in self.allObjs.items():
      if not isinstance(irObj, obj.Func): continue
      func: obj.Func = irObj # if here, its a function object
      if not func.basicBlocks: continue # if no body, then don't process

      for bb in func.basicBlocks.values():
        for i in range(len(bb) - 1):
          insn = bb[i]
          if isinstance(insn, instr.AssignI) and\
              insn.type.typeCode == types.PTR_TC:
            rhs = insn.rhs
            if isinstance(rhs, expr.CastE):
              arg = rhs.arg
              if isinstance(arg, expr.LitE):
                if arg.type.isNumeric() and arg.val == 0:
                  rhs = expr.AddrOfE(expr.VarE(NULL_OBJ_NAME, rhs.loc), rhs.loc)
                  insn.rhs = rhs
            if isinstance(rhs, expr.LitE):
              if rhs.type.isNumeric() and rhs.val == 0:
                rhs = expr.AddrOfE(expr.VarE(NULL_OBJ_NAME, rhs.loc), rhs.loc)
                insn.rhs = rhs

  def addThisTUnitRefToObjs(self): # IMPORTANT (MUST)
    """sets func.tUnit to this TUnit here,
    It cannot be done in obj.Func since,
      1. due to lack of info in the obj module
      2. to avoid circular dependency btw obj and this module"""
    for objName, irObj in self.allObjs.items():
      if isinstance(irObj, obj.Func):
        func: obj.Func = irObj
        # Point func.tUnit to TUnit object it belongs to i.e. this
        func.tUnit: 'TranslationUnit' = self

  def genCfgs(self) -> None:
    """Fills obj.Func's self.cfg field to contain a proper CFG graph.
    Its done only for functions with body.
    Assumption: It assumes BB map has been already constructed.
    """
    for objName, irObj in self.allObjs.items():
      if isinstance(irObj, obj.Func):
        func: obj.Func = irObj
        if func.basicBlocks: # should have a body
          func.cfg = graph.Cfg(func.name, func.basicBlocks, func.bbEdges)

  def genBasicBlocks(self) -> None:
    """Generates basic blocks if function objects are initialized by
    instruction sequence only."""
    for objName, irObj in self.allObjs.items():
      if isinstance(irObj, obj.Func):
        func = irObj
        if not func.basicBlocks and func.instrSeq:
          # i.e. basic blocks don't exist and the function has a instr seq body
          func.basicBlocks, func.bbEdges = obj.Func.genBasicBlocks(func.instrSeq)

  def fillFuncParamTypes(self):
    """If function's param type list is empty, fill it."""
    for objName, irObj in self.allObjs.items():
      if isinstance(irObj, obj.Func):
        if not irObj.paramTypes:
          irObj.paramTypes = []
          for paramName in irObj.paramNames:
            irObj.paramTypes.append(self.inferBasicType(paramName))
        irObj.sig.paramTypes = irObj.paramTypes

  def inferAllInstrTypes(self):
    """Fills type field of the instruction (and expressions in it)."""
    for objName, irObj in self.allObjs.items():
      if isinstance(irObj, obj.Func):
        for _, instrs in irObj.basicBlocks.items():
          for insn in instrs:
            self.inferInstrType(insn)
      self.extractTmpVarAssignExprs(irObj)

  ################################################
  #BOUND START: Type_Inference
  ################################################

  def inferBasicType(self, val) -> types.Type:
    """Returns the type for the given value."""

    if type(val) == int: return types.Int
    if type(val) == float: return types.Float

    if type(val) == str:
      if val in self.allVars: return self.allVars[val]
      if val in self.allObjs:
        # must be a function
        func: obj.Func = self.allObjs[val]
        return func.sig.returnType

    msg = f"UnknownValueToInferTypeOf: {val}"
    if LS: _log.error(msg)
    raise ValueError(msg)

  def inferExprType(self, e: expr.ExprET) -> types.Type:
    """Infer expr type, store the type info in the object and return the type."""
    eType = types.Void
    exprCode = e.exprCode
    lExpr = expr # for speed

    if exprCode == lExpr.VAR_EXPR_EC:
      e : expr.VarE = e
      eType = self.inferBasicType(e.name)

    elif exprCode == lExpr.LIT_EXPR_EC:
      e : expr.LitE = e
      eType = self.inferBasicType(e.val)

    elif exprCode == lExpr.CAST_EXPR_EC:
      e : expr.CastE = e
      self.inferExprType(e.arg)
      eType = e.to.type # type its casted to

    elif exprCode == lExpr.UNARY_EXPR_EC:
      e : expr.UnaryE = e
      opCode = e.opr.opCode
      argType = self.inferExprType(e.arg)
      if opCode == op.UO_DEREF_OC:
        assert isinstance(e.arg, lExpr.VarE), f"Operand should be a var: {e}."
        eType: types.Ptr = argType
        eType = eType.to  # the type it points to
      elif opCode == op.UO_LNOT_OC: # logical not
        eType = types.Int32
      else:
        eType = argType # for all other unary ops

    elif exprCode == lExpr.BINARY_EXPR_EC:
      e : expr.BinaryE = e
      opCode = e.opr.opCode
      if op.BO_NUM_START_OC <= opCode <= op.BO_NUM_END_OC:
        itype1 = self.inferExprType(e.arg1)
        itype2 = self.inferExprType(e.arg2)
        # FIXME: conversion rules
        if itype1.bitSize() >= itype2.bitSize():
          if types.FLOAT16_TC <= itype2.typeCode <= types.FLOAT128_TC:
            eType = itype2
          else:
            eType = itype1
        else:
          eType = itype1

      elif op.BO_REL_START_OC <= opCode <= op.BO_REL_END_OC:
        eType = types.Int32

    elif exprCode == lExpr.ARR_EXPR_EC:
      e : expr.ArrayE = e
      subEType = self.inferExprType(e.of)
      if isinstance(subEType, types.Ptr):
        eType = subEType
      elif isinstance(subEType, types.ArrayT):
        eType = subEType.of

    elif exprCode == lExpr.MEMBER_EXPR_EC:
      e : expr.MemberE = e
      fieldName = e.name
      of = e.of
      ofType = self.inferExprType(of)
      if isinstance(ofType, types.Ptr):
        ofType = ofType.to
        e.isPtrExpr = True
      eType = ofType.getFieldType(fieldName)

    elif exprCode == lExpr.SELECT_EXPR_EC:
      e : expr.SelectE = e
      self.inferExprType(e.cond)
      self.inferExprType(e.arg1)
      eType2 = self.inferExprType(e.arg2)
      eType = eType2 # type of 1 and 2 should be the same.

    elif exprCode == lExpr.ALLOC_EXPR_EC:
      eType = types.Ptr(to=types.Void)

    elif exprCode == lExpr.ADDROF_EXPR_EC:
      e : expr.AddrOfE = e
      eType = types.Ptr(to=self.inferExprType(e.arg))

    elif exprCode == lExpr.CALL_EXPR_EC:
      e : expr.CallE = e
      eType = self.inferExprType(e.callee)

    elif exprCode == lExpr.FUNC_EXPR_EC:
      e : expr.FuncE = e
      func: obj.Func = self.allObjs[e.name]
      eType = func.sig.returnType

    else:
      if LS: _log.error("Unknown_Expr_For_TypeInference: %s.", e)

    e.type = eType
    return eType

  def inferInstrType(self,
                     insn: instr.InstrIT,
                     # default argument for speed, since assignment is the most common
                     assignInstrCode: instr.InstrCodeT = instr.ASSIGN_INSTR_IC,
  ) -> types.Type:
    """Infer instruction type from the type of the expressions."""
    iType = types.Void
    instrCode = insn.instrCode
    lInstr = instr # for speed

    if instrCode == assignInstrCode:
      insn: instr.AssignI = insn
      t1 = self.inferExprType(insn.lhs)
      t2 = self.inferExprType(insn.rhs)
      iType = t1
      if AS and t1 != t2:
        _log.error(f"Lhs and Rhs types differ: {insn}, lhstype = {t1}, rhstype = {t2}.")

    elif instrCode == lInstr.USE_INSTR_IC:
      insn: instr.UseI = insn
      for var in insn.vars:
        _ = self.inferExprType(var)

    elif instrCode == lInstr.LIVE_INSTR_IC:
      insn: instr.LiveI = insn
      for var in insn.vars:
        self.inferExprType(var)

    elif instrCode == lInstr.COND_READ_INSTR_IC:
      insn: instr.CondReadI = insn
      iType = self.inferExprType(insn.lhs)
      for ex in insn.rhs:
        self.inferExprType(ex)

    elif instrCode == lInstr.UNDEF_VAL_INSTR_IC:
      insn: instr.UnDefValI = insn
      iType = self.inferExprType(insn.lhs)

    elif instrCode == lInstr.COND_INSTR_IC:
      insn: instr.CondI = insn
      _ = self.inferExprType(insn.arg)

    elif instrCode == lInstr.RETURN_INSTR_IC:
      insn: instr.ReturnI = insn
      if insn.arg is not None:
        iType = self.inferExprType(insn.arg)

    elif instrCode == lInstr.CALL_INSTR_IC:
      insn: instr.CallI = insn
      iType = self.inferExprType(insn.arg)

    elif instrCode == lInstr.NOP_INSTR_IC:
      return iType # i.e. types.Void

    else:
      if LS: _log.error("Unknown_Instr_For_TypeInference: %s.", insn)

    insn.type = iType
    return iType

  ################################################
  #BOUND END  : Type_Inference
  ################################################

  def logUsefulInfo(self) -> None:
    """Logs useful information of this translation unit."""
    if LS: _log.info("\nINITIALIZING_TRANSLATION_UNIT: Name: %s, Vars#: %s, Objs#: %s.\n",
                     self.name, len(self.allVars), len(self.allObjs))
    if LS: _log.info("TU_Description: %s", self.description)

    with io.StringIO() as sio:
      sio.write(f"VarDict (Total: {len(self.allVars)}), {{var name: "
                f"var type}}:\n")
      for varName in sorted(self.allVars):
        sio.write(f"  {varName!r}: {self.allVars[varName]}.\n")
      if LS: _log.info("%s", sio.getvalue())

    with io.StringIO() as sio:
      sio.write(f"ObjDict (Total: {len(self.allObjs)}):\n")
      for objName in sorted(self.allObjs):
        if objName.startswith("f:"):
          func: obj.Func = self.allObjs[objName]
          sio.write(f"  {objName!r}: returns: {func.sig.returnType}, params: "
                    f"{func.paramNames}.\n")
        else:
          sio.write(f"{self.allObjs[objName]}\n")
      if LS: _log.info("%s", sio.getvalue())

  def fillTheRecordTypes(self,):
    """Completes the record types.
    E.g. if only types.Struct("s:node") is present, it replaces it
    with the reference to the complete definition of the Struct.
    """
    for objName in self.allObjs.keys():
      irObj = self.allObjs[objName]
      if isinstance(irObj, (types.Struct, types.Union)):
        newFields = []
        for field in irObj.fields:
          newType = self.findAndFillRecordType(field[1])
          newFields.append((field[0], newType))
        irObj.fields = newFields

    for varName in self.allVars.keys():
      varType = self.allVars[varName]
      completedVarType = self.findAndFillRecordType(varType)
      self.allVars[varName] = completedVarType

  def findAndFillRecordType(self, varType: types.Type):
    """Recursively finds the record type and replaces them with
    the reference to the complete definition in self.allObjs."""
    if isinstance(varType, (types.Struct, types.Union)):
      return self.allObjs[varType.name]

    elif isinstance(varType, types.Ptr):
      ptrTo = self.findAndFillRecordType(varType.to)
      return types.Ptr(to=ptrTo, indir=varType.indir)

    elif isinstance(varType, types.ArrayT):
      arrayOf = self.findAndFillRecordType(varType.of)
      if isinstance(varType, types.ConstSizeArray):
        return types.ConstSizeArray(of=arrayOf, size=varType.size)
      elif isinstance(varType, types.VarArray):
        return types.VarArray(of=arrayOf)
      elif isinstance(varType, types.IncompleteArray):
        return types.IncompleteArray(of=arrayOf)

    return varType # by default return the same type

  def optimizeO3(self) -> None:
    """Optimizes SPAN IR"""
    for name, func in self.allObjs.items():
      if isinstance(func, obj.Func):
        # if here, its a function
        self.reduceAllConstExprs(func) # (MUST)
        self.removeConstIfStmts(func) # (MUST)
        self.removeNopInsns(func) # (OPTIONAL)
        self.removeNopBbs(func) # (OPTIONAL)
        self.removeUnreachableBbsFromFunc(func) # (OPTIONAL)

  def reduceAllConstExprs(self, func: obj.Func) -> None:
    """Reduces/solves all binary/unary constant expressions."""
    assignInstrCode = instr.ASSIGN_INSTR_IC
    for bbId, bb in func.basicBlocks.items():
      for index in range(len(bb)):
        if bb[index].instrCode == assignInstrCode:
          insn: instr.AssignI = bb[index]
          rhs = self.reduceConstExpr(insn.rhs)
          if rhs is not insn.rhs:
            insn.rhs = rhs
            self.inferInstrType(insn)

  def reduceConstExpr(self, e: expr.ExprET) -> expr.ExprET:
    """Converts: 5 + 6, 6 > 7, -5, +6, !7, ~9, ... to a single literal."""
    newExpr = e

    if isinstance(e, expr.BinaryE):
      arg1 = e.arg1
      arg2 = e.arg2

      if arg1.exprCode == expr.LIT_EXPR_EC and\
          arg2.exprCode == expr.LIT_EXPR_EC:
        if e.opr.opCode == op.BO_ADD_OC:
          newExpr = expr.LitE(arg1.val + arg2.val, loc=arg1.loc)
        elif e.opr.opCode == op.BO_SUB_OC:
          newExpr = expr.LitE(arg1.val - arg2.val, loc=arg1.loc)
        elif e.opr.opCode == op.BO_MUL_OC:
          newExpr = expr.LitE(arg1.val * arg2.val, loc=arg1.loc)
        elif e.opr.opCode == op.BO_DIV_OC:
          newExpr = expr.LitE(arg1.val / arg2.val, loc=arg1.loc)
        elif e.opr.opCode == op.BO_MOD_OC:
          newExpr = expr.LitE(arg1.val % arg2.val, loc=arg1.loc)

        elif e.opr.opCode == op.BO_LT_OC:
          newExpr = expr.LitE(int(arg1.val < arg2.val), loc=arg1.loc)
        elif e.opr.opCode == op.BO_LE_OC:
          newExpr = expr.LitE(int(arg1.val <= arg2.val), loc=arg1.loc)
        elif e.opr.opCode == op.BO_EQ_OC:
          newExpr = expr.LitE(int(arg1.val == arg2.val), loc=arg1.loc)
        elif e.opr.opCode == op.BO_NE_OC:
          newExpr = expr.LitE(int(arg1.val != arg2.val), loc=arg1.loc)
        elif e.opr.opCode == op.BO_GE_OC:
          newExpr = expr.LitE(int(arg1.val >= arg2.val), loc=arg1.loc)
        elif e.opr.opCode == op.BO_GT_OC:
          newExpr = expr.LitE(int(arg1.val > arg2.val), loc=arg1.loc)

    elif isinstance(e, expr.UnaryE):
      arg = e.arg

      if arg.exprCode == expr.LIT_EXPR_EC:
        if e.opr.opCode == op.UO_PLUS_OC:
          newExpr = e.arg
        elif e.opr.opCode == op.UO_MINUS_OC:
          newExpr = expr.LitE(e.arg.val * -1, loc=e.arg.loc)
        elif e.opr.opCode == op.UO_LNOT_OC:
          newExpr = expr.LitE(int(not (bool(e.arg.val))), loc=e.arg.loc)
        elif e.opr.opCode == op.UO_BIT_NOT_OC:
          newExpr = expr.LitE(~e.arg.val, loc=e.arg.loc)

    return newExpr

  def removeNopInsns(self, func: obj.Func) -> None:
    """Removes NopI() from bbs with more than one instruction."""
    bbIds = func.basicBlocks.keys()

    for bbId in bbIds:
      bb = func.basicBlocks[bbId]
      newBb = []
      for insn in bb:
        if not isinstance(insn, instr.NopI):
          newBb.append(insn)

      if len(newBb) == 0:
        newBb.append(instr.NopI()) # let one NopI be (such BBs are removed later)
      func.basicBlocks[bbId] = newBb

  def removeUnreachableBbsFromFunc(self, func: obj.Func) -> None:
    """Removes BBs that are not reachable from StartBB."""
    allBbIds = func.basicBlocks.keys()

    # collect all dest bbIds
    destBbIds = {-1} # start bbId is always reachable
    for bbEdge in func.bbEdges:
      destBbIds.add(bbEdge[1])
    unreachableBbIds = allBbIds - destBbIds

    # remove all edges going out of reachable bbs
    takenEdges = []
    for bbEdge in func.bbEdges:
      if bbEdge[0] in unreachableBbIds: continue
      takenEdges.append(bbEdge)
    func.bbEdges = takenEdges

    # remove unreachableBbIds one by one
    for bbId in unreachableBbIds:
      del func.basicBlocks[bbId]

    if unreachableBbIds:
      # go recursive, since there could be new unreachable bb ids
      return self.removeUnreachableBbsFromFunc(func)

  def removeConstIfStmts(self, func: obj.Func) -> None:
    """Changes if stmt on a const value to a Nop().
    It may lead to some unreachable BBs."""

    for bbId, bbInsns in func.basicBlocks.items():
      if not bbInsns: continue # if bb is blank
      if isinstance(bbInsns[-1], instr.CondI):
        if isinstance(bbInsns[-1].arg, expr.LitE):
          val: types.NumericT = bbInsns[-1].arg.val
          redundantEdgeLabel = types.TrueEdge if val == 0 else types.FalseEdge

          # replace instr.CondI with instr.NopI
          bbInsns[-1] = instr.NopI()

          # remove the redundant edge, and make the other edge unconditional
          retainedEdges = []
          for bbEdge in func.bbEdges:
            if bbEdge[0] == bbId: # edge source is this bb
              if bbEdge[2] == redundantEdgeLabel: continue
              bbEdge = (bbId, bbEdge[1], types.UnCondEdge)
            retainedEdges.append(bbEdge)
          func.bbEdges = retainedEdges

  def removeNopBbs(self, func: obj.Func) -> None:
    """Remove BBs that only have instr.NopI(). Except START and END."""

    bbIds = func.basicBlocks.keys()
    for bbId in bbIds:
      if bbId in [-1, 0]: continue # leave START and END BBs as it is.

      onlyNop = True
      for insn in func.basicBlocks[bbId]:
        if isinstance(insn, instr.NopI): continue
        onlyNop = False

      if onlyNop:
        # then remove this bb and related edges
        retainedEdges = []
        predEdges = []
        succEdges = []
        for bbEdge in func.bbEdges:
          if bbEdge[0] == bbId:
            succEdges.append(bbEdge) # ONLY ONE EDGE
          elif bbEdge[1] == bbId:
            predEdges.append(bbEdge)
          else:
            retainedEdges.append(bbEdge)

        if AS: assert len(succEdges) == 1, msg.SHOULD_BE_ONLY_ONE_EDGE

        for predEdge in predEdges:
          newEdge = (predEdge[0], succEdges[0][1], predEdge[2])
          retainedEdges.append(newEdge)
        func.bbEdges = retainedEdges

  def replaceMemAllocations(self) -> None:
    """Replace calloc(), malloc() with pseudo variables.
    Should be called when types for expressions have been inferred.
    """
    for objName, irObj in self.allObjs.items():
      if not isinstance(irObj, obj.Func): continue
      func: obj.Func = irObj # if here, its a function object
      if not func.basicBlocks: continue # if no body, then don't process

      for bb in func.basicBlocks.values():
        for i in range(len(bb) - 1):
          insn = bb[i]
          # SPAN IR separates a call and its cast into two statements.
          if isinstance(insn, instr.AssignI) and isinstance(insn.rhs, expr.CallE):
            if self.isMemoryAllocationCall(insn.rhs):
              memAllocInsn: instr.AssignI = insn
              if isTmpVar(memAllocInsn.lhs.name): # stored in a void* temporary
                # then next insn must be a cast and store to a non tmp variable
                castInstr = bb[i+1]
                newInstr = self.conditionallyAddPseudoVar(func.name, castInstr, memAllocInsn)
                if newInstr is not None: # hence pseudo var has been added
                  bb[i] = instr.NopI() # i.e. remove current instruction
                  bb[i+1] = newInstr
              else:
                newInstr = self.conditionallyAddPseudoVar(func.name, memAllocInsn)
                if newInstr:
                  bb[i] = newInstr

      self.removeNopInsns(func)

  def conditionallyAddPseudoVar(self,
                                funcName: types.FuncNameT,
                                insn: instr.AssignI,
                                prevInsn: instr.AssignI = None,
  ) -> Optional[instr.InstrIT]:
    """Modifies rhs to address of a pseudo var with the correct type.
    Only two instruction forms should be in insn:
      <ptr_var> = (<type>*) <tmp_var>; // cast insn
      <ptr_var> = <malloc/calloc>(...); // memory alloc insn
    """
    lhs: expr.VarE = insn.lhs
    # if isTmpVar(lhs.name): return None

    rhs = insn.rhs
    if isinstance(rhs, expr.CastE):
      if not isTmpVar(rhs.arg.name): return None
      # if here, assume that the tmp var is assigned a heap location

      lhsType: types.Ptr = lhs.type
      pVar = self.genPseudoVar(funcName, rhs.loc,
                               lhsType.getPointeeType(), insn, prevInsn)
      newInsn = instr.AssignI(lhs, expr.AddrOfE(pVar, loc=rhs.loc))
      self.inferInstrType(newInsn)
      return newInsn

    elif isinstance(rhs, expr.CallE):
      # assume it is malloc/calloc (it should be)
      lhsType: types.Ptr = lhs.type
      pVar = self.genPseudoVar(funcName, rhs.loc,
                               lhsType.getPointeeType(), insn, prevInsn)
      newInsn = instr.AssignI(lhs, expr.AddrOfE(pVar, loc=rhs.loc))
      self.inferInstrType(newInsn)
      return newInsn

    return None

  def genPseudoVar(self,
                   funcName: types.FuncNameT,
                   loc: types.Loc,
                   varType: types.Type,
                   insn: instr.AssignI,
                   prevInsn: instr.AssignI = None,
  ) -> expr.PseudoVarE:
    if funcName not in self._funcPseudoCountMap:
      self._funcPseudoCountMap[funcName] = 1
    currCount = self._funcPseudoCountMap[funcName]
    self._funcPseudoCountMap[funcName] = currCount + 1

    nakedPvName = NAKED_PSEUDO_VAR_NAME.format(count=currCount)
    pureFuncName = funcName.split(":")[1]
    pvName = f"v:{pureFuncName}:{nakedPvName}"

    self.allVars[pvName] = varType # add to tunit variable map
    self._pseudoVars.add(pvName)
    if prevInsn is None: # insn can never be None
      sizeExpr = self.getMemAllocSizeExpr(insn)
      insns = [insn]
    else:
      sizeExpr = self.getMemAllocSizeExpr(prevInsn)
      insns = [prevInsn, insn]

    pVarE = expr.PseudoVarE(pvName, loc=loc, insns=insns, sizeExpr=sizeExpr)
    pVarE.type = varType

    return pVarE

  def getMemAllocSizeExpr(self, insn: instr.AssignI) -> expr.ExprET:
    """Returns the expression deciding the size of memory allocated."""
    lhs: expr.CallE = insn.lhs
    calleeName = lhs.callee.name
    sizeExpr = None

    if calleeName == "f:malloc":
      sizeExpr = lhs.args[0] # the one and only argument is the size expr
    elif calleeName == "f:calloc":
      sizeExpr = expr.BinaryE(lhs.args[0], op.BO_MUL, lhs.args[1], loc=lhs.args[0].loc)
      self.inferExprType(sizeExpr)

    return sizeExpr

  def isMemoryAllocationCall(self,
                             callExpr: expr.CallE,
  ) -> bool:
    memAllocCall = False
    calleeName = callExpr.callee.name
    if calleeName in memAllocFunctions:
      # memAllocCall = True
      func: obj.Func = self.allObjs[calleeName]
      if func.sig == memAllocFunctions["f:malloc"] or \
          func.sig == memAllocFunctions["f:calloc"]:
        memAllocCall = True

    return memAllocCall

  def getTmpVarExpr(self,
                    vName: types.VarNameT,
  ) -> Optional[expr.ExprET]:
    """Returns the expression the given tmp var is assigned.
    It only tracks some tmp vars, e.g. ones like t.3, t.1if, t.2if ...
    The idea is to map the tmp vars that are assigned only once.
    """
    if vName in self._funcTmpExprMap:
      return self._funcTmpExprMap[vName]
    return None # None if tmp var is not tracked

  def extractTmpVarAssignExprs(self, func: obj.Func) -> None:
    """Extract temporary variables and the unique expressions they hold the value of.
    It caches the result in a global map."""

    tmpExprMap = self._funcTmpExprMap
    if func.instrSeq:
      for insn in func.instrSeq:
        if insn.instrCode == instr.ASSIGN_INSTR_IC:
          insn: instr.AssignI = insn
          lhs: expr.VarE = insn.lhs
          if lhs.exprCode == expr.VAR_EXPR_EC:
            name = lhs.name
            if NORMAL_TMPVAR_REGEX.match(name) or COND_TMPVAR_REGEX.match(name):
              tmpExprMap[name] = insn.rhs

    elif func.basicBlocks:
      for _, bb in func.basicBlocks.items():
        for insn in bb:
          if insn.instrCode == instr.ASSIGN_INSTR_IC:
            insn: instr.AssignI = insn
            lhs: expr.VarE = insn.lhs
            if lhs.exprCode == expr.VAR_EXPR_EC:
              name = lhs.name
              if NORMAL_TMPVAR_REGEX.match(name) or COND_TMPVAR_REGEX.match(name):
                tmpExprMap[name] = insn.rhs

    # tmpExprMap updates self._funcTmpExprMap automatically

  def getLocalVars(self,
                   funcName: types.FuncNameT,
                   givenType: types.Type = None
  ) -> Set[types.VarNameT]:
    """Returns set of variable names local to a function."""
    if funcName in self._localVars:
      return self._localVars[funcName]

    vNameSet = set()
    for vName in self.allVars:
      count = vName.count(":")
      if count == 2:
        sp = vName.split(":")
        if funcName[2:] == sp[1]:
          if givenType is not None: # when type is given
            if self.allVars[vName] == givenType:
              vNameSet.add(vName)
            else:
              vNameSet |= self.genComplexNames(vName, givenType)
          else:
            vNameSet.add(vName)
    self._localVars[funcName] = vNameSet # cache the result
    return vNameSet

  def genComplexNames(self,
                      varName: types.VarNameT,
                      givenType: types.Type,
  ) -> Set[types.VarNameT]:
    """Returns names of complex types
    such as structures and arrays that contain
    the objects of the givenType.
    For example:
      If the givenType is INT, and a variable x in foo() is
      a record type with an INT fields f, g, h,
      a set {"v:foo:x.f", "v:foo:x.g", "v:foo:x.h"} is returned.

      Moreover, if an array (of whatever dimension) holds
      elements of givenType, its name is returned.
    """
    varSet = set()
    originalType = self.allVars[varName]
    if isinstance(originalType, types.ArrayT):
      if originalType.elementType() == givenType:
        varSet.add(varName)
    elif isinstance(originalType, types.RecordT):
      varSet = originalType.getFieldsOfType(givenType, varName)

    return varSet

  def getLocalPseudoVars(self,
                         funcName: types.FuncNameT,
                         givenType: types.Type = None
  ) -> Set[types.VarNameT]:
    """Returns set of pseudo variable names local to a function."""
    if funcName in self._localPseudoVars:
      return self._localPseudoVars[funcName]

    # use getLocalVars() to do most work
    localVars: Set[types.VarNameT] = self.getLocalVars(funcName, givenType)

    vNameSet = set()
    for vName in localVars:
      if PSEUDO_VAR_REGEX.fullmatch(vName):
        vNameSet.add(vName)

    self._localPseudoVars[funcName] = vNameSet # cache the result
    return vNameSet

  def getAllPseudoVars(self) -> Set[types.VarNameT]:
    """Returns set of all pseudo var names in the translation unit."""
    if self._allPseudoVars is not None:
      return self._allPseudoVars

    vNameSet = set()
    for vName in self.allVars.keys():
      if PSEUDO_VAR_REGEX.fullmatch(vName):
        vNameSet.add(vName)

    self._allPseudoVars = vNameSet
    return vNameSet

  def getGlobalVars(self,
                    givenType: types.Type = None
  ) -> Set[types.VarNameT]:
    """Returns set of global variable names."""
    vNameSet = set()
    if self._globalVars is None:
      for vName in self.allVars:
        if vName.count(":") == 1: # hence a global
          vNameSet.add(vName)
      self._globalVars = vNameSet # cache the result

    if givenType is None:
      return self._globalVars
    else:
      if givenType in self._typeGlobalVarMap:
        return self._typeGlobalVarMap[givenType]
      else:
        vNameSet = set() # IMPORTANT: create new object
        for vName in self._globalVars:
          if self.allVars[vName] == givenType:
            vNameSet.add(vName)
          else:
            vNameSet |= self.genComplexNames(vName, givenType)
        self._typeGlobalVarMap[givenType] = vNameSet  # cache result

      return vNameSet

  def getEnvVars(self,
                 funcName: types.FuncNameT,
                 givenType: types.Type = None
  ) -> Set[types.VarNameT]:
    """Returns set of variables accessible in a given function (of the given type).
    Without givenType it returns all the variables accessible."""
    tup = (funcName, givenType)
    if tup in self._typeVarMap: return self._typeVarMap[tup]

    envVars = self.getGlobalVars(givenType) | self.getLocalVars(funcName, givenType)
    self._typeVarMap[tup] = envVars # cache the result

    return envVars

  def __eq__(self,
             other: 'TranslationUnit'
  ) -> bool:
    """This method is elaborate to assist testing."""
    isEqual = True
    if not isinstance(other, TranslationUnit):
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

    if not isEqual:
      if LS: _log.warning("TUnits differ: (TUnit: '%s')", self.name)

    return isEqual

  def getFunctionsOfGivenSignature(self,
      givenSignature: types.FuncSig
  ) -> List[obj.Func]:
    """Returns functions with the given signature."""
    if givenSignature in self._funcSigToFuncObjMap:
      return self._funcSigToFuncObjMap[givenSignature]

    funcList: List[obj.Func] = []
    for irObj in self.allObjs:
      if isinstance(irObj, obj.Func):
        if irObj.sig == givenSignature:
          funcList.append(irObj)

    self._funcSigToFuncObjMap[givenSignature] = funcList
    return funcList

