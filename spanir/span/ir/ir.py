#!/usr/bin/env python3

# MIT License
# Copyright (c) 2019 Anshuman Dhuliya

"""
This is a convenience module that brings the scattered API
of the SPAN IR at one place.

The API here is useful in writing an analysis/diagnosis.

Note about circular dependency:
  * No other module in span.ir package should import this module.
  * This module uses all other modules in span.ir package.
"""

import logging
_log = logging.getLogger(__name__)
from typing import Dict, Set, Tuple, Optional, List

import span.ir.types as types
import span.ir.op as op
import span.ir.expr as expr
import span.ir.instr as instr
import span.ir.obj as obj
import span.ir.tunit as tunit
import span.ir.graph as graph

"""
This import allows these function to be called
using this module's scope like: `ir.isTmpVar("v:main:t.1t")`
"""
from span.ir.tunit import\
  isTmpVar,\
  isCondTmpVar,\
  isLogicalTmpVar,\
  isNormalTmpVar,\
  getNullObjName,\
  isNullObjName,\
  isPseudoVar,\
  TranslationUnit

def inferBasicType(func: obj.Func, val) -> types.Type:
  """Returns the type for the given value."""
  tUnit: tunit.TranslationUnit = func.tUnit
  return tUnit.inferBasicType(val)

def inferExprType(func: obj.Func, e: expr.ExprET) -> types.Type:
  """Infer expr type, store the type info in the object and return the type."""
  tUnit: tunit.TranslationUnit = func.tUnit
  return tUnit.inferExprType(e)

def inferInstrType(func: obj.Func, insn: instr.InstrIT) -> types.Type:
  """Infer instruction type from the type of the expressions."""
  tUnit: tunit.TranslationUnit = func.tUnit
  return tUnit.inferInstrType(insn)

def getLocalVars(func: obj.Func,
                 varType: types.Type = None
) -> Set[types.VarNameT]:
  """Returns set of variable names local to a function."""
  tUnit: tunit.TranslationUnit = func.tUnit
  return tUnit.getLocalVars(func.name, varType)

def getLocalPseudoVars(func: obj.Func,
                       varType: types.Type = None
) -> Set[types.VarNameT]:
  """Returns set of variable names local to a function."""
  tUnit: tunit.TranslationUnit = func.tUnit
  return tUnit.getLocalPseudoVars(func.name, varType)

def getGlobalVars(func: obj.Func,
                  varType: types.Type = None
) -> Set[types.VarNameT]:
  """Returns set of global variable names."""
  tUnit: tunit.TranslationUnit = func.tUnit
  return tUnit.getGlobalVars(varType)

def getAllPseudoVars(tUnit: tunit.TranslationUnit) -> Set[types.VarNameT]:
  """Returns the set of global variable names."""
  return tUnit.getAllPseudoVars()

def getEnvVars(func: obj.Func,
               varType: types.Type = None
) -> Set[types.VarNameT]:
  """Returns set of variables accessible
  in a given function (of the given type).
  Without varType it returns all the variables accessible."""
  tUnit: tunit.TranslationUnit = func.tUnit
  return tUnit.getEnvVars(func.name, varType)

def getAllEnvVars(func: obj.Func) -> Set[types.VarNameT]:
  """Returns set of all variables accessible in a function."""
  tUnit: tunit.TranslationUnit = func.tUnit
  return tUnit.getEnvVars(func.name, None)

def getTmpVarExpr(func: obj.Func,
                  vName: types.VarNameT,
) -> Optional[expr.ExprET]:
  """Returns the expression the given tmp var is assigned.
  It only tracks some tmp vars, e.g. ones like t.3, t.1if, t.2if ...
  The idea is to map the tmp vars that are assigned only once.
  (Tmp vars t.1L, t.5L, ... may be assigned more than once.)
  """
  tUnit: tunit.TranslationUnit = func.tUnit
  return tUnit.getTmpVarExpr(vName)

def getFunctionsOfGivenSignature(
    tUnit: tunit.TranslationUnit,
    givenSignature: types.FuncSig
) -> List[obj.Func]:
  """Returns functions of the signature given."""
  return tUnit.getFunctionsOfGivenSignature(givenSignature)


