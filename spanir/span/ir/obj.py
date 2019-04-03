#!/usr/bin/env python3

# MIT License
# Copyright (c) 2019 Anshuman Dhuliya

"""All value types available in the IR."""

import logging
_log = logging.getLogger(__name__)
from typing import List, Dict, Tuple, Optional, Set
import io

from span.util.logger import LS
from span.ir.types import StructNameT, UnionNameT, FieldNameT, FuncNameT, VarNameT,\
  EdgeLabelT, BasicBlockIdT, Void,\
  Type, FuncSig, Loc, LabelNameT
import span.ir.instr as instr
from span.ir.instr import InstrIT, LabelI, GotoI, CondI, NopI, ReturnI
from span.ir.types import BasicBlockIdT, FalseEdge, TrueEdge, UnCondEdge

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
               variadic: bool = False,
               basicBlocks: Optional[Dict[BasicBlockIdT, List[InstrIT]]] = None,
               bbEdges: Optional[List[Tuple[BasicBlockIdT, BasicBlockIdT, EdgeLabelT]]] =
               None,
               instrSeq: Optional[List[InstrIT]] = None,
               loc: Optional[Loc] = None
  ) -> None:
    self.name = name
    self.paramNames = paramNames
    self.sig = FuncSig(returnType, paramTypes, variadic)
    self.basicBlocks = basicBlocks if basicBlocks else dict()
    self.bbEdges = bbEdges if bbEdges else []
    self.instrSeq = instrSeq
    if instrSeq:
      basicBlocks, bbEdges = self.genBasicBlocks(instrSeq)
    self.loc = loc

  def hasBody(self) -> bool: return bool(self.basicBlocks)

  @staticmethod
  def genBasicBlocks(instrSeq: List[InstrIT]
  ) -> Tuple[Dict[BasicBlockIdT, List[InstrIT]],
             List[Tuple[BasicBlockIdT, BasicBlockIdT, EdgeLabelT]]]:
    """Divides list of instructions into basic blocks.
    Assumption: every target except the first instruction must have a label.
    """

    if not instrSeq: return dict(), set()

    leaders: Set[int] = {0} # first instr is leader
    bbEdges: List[Tuple[BasicBlockIdT, BasicBlockIdT], EdgeLabelT] = []
    bbMap: Dict[BasicBlockIdT, List[InstrIT]] = {}
    labelRenaming: Dict[LabelNameT, BasicBlockIdT] = {}
    currBbId = 0

    # STEP 0: Record all target labels
    validTargets: Set[LabelNameT] = set()
    for insn in instrSeq:
      if isinstance(insn, GotoI):
        validTargets.add(insn.label)
      if isinstance(insn, CondI):
        validTargets.add(insn.trueLabel)
        validTargets.add(insn.falseLabel)

    # STEP 1: Record all leaders

    # put at least one instruction after the last LabelI
    if isinstance(instrSeq[-1], LabelI):
      instrSeq.append(NopI())

    currLabelSet: Set[LabelNameT] = set()
    for index, insn in enumerate(instrSeq):
      if isinstance(insn, LabelI):
        if insn.label not in validTargets: continue
        currLabelSet.add(insn.label)
        # assuming there is a next instruction
        if isinstance(instrSeq[index+1], LabelI):
          # don't add this instruction
          continue
        leaders.add(index+1) # add the non-label instr as target
        currBbId += 1
        for label in currLabelSet:
          labelRenaming[label] = currBbId
        currLabelSet.clear()

    print(labelRenaming)

    instrs = []
    bbId = -1 # start block id
    print(instrSeq)
    for insn in instrSeq:
      if not isinstance(insn, (LabelI, GotoI, CondI, ReturnI)):
        instrs.append(insn)
        continue

      if isinstance(insn, LabelI):
        if insn.label not in validTargets: continue
        bbId = labelRenaming[insn.label] # here bbId is set
        continue

      if isinstance(insn, GotoI):
        bbEdge = (bbId, labelRenaming[insn.label], UnCondEdge)
        bbEdges.append(bbEdge)
        bbMap[bbId] = instrs

      if isinstance(insn, CondI):
        bbEdge = (bbId, labelRenaming[insn.trueLabel], TrueEdge)
        bbEdges.append(bbEdge)
        bbEdge = (bbId, labelRenaming[insn.falseLabel], FalseEdge)
        bbEdges.append(bbEdge)

        instrs.append(insn)
        bbMap[bbId] = instrs

      if isinstance(insn, ReturnI):
        bbEdge = (bbId, 0, UnCondEdge)
        bbEdges.append(bbEdge)

        instrs.append(insn)
        bbMap[bbId] = instrs

      bbId = 0
      instrs = []

    if instrs: bbMap[bbId] = instrs
    bbMap[0] = [NopI()]  # end bb

    # Connect all leaf bbs to end bb with unconditional edge
    bbIdsWithoutEdge: Set[BasicBlockIdT] = set()
    for bbEdge in bbEdges:
      bbIdsWithoutEdge.add(bbEdge[0])

    for bbId in range(1, currBbId+1):
      if bbId not in bbIdsWithoutEdge:
        bbEdge = (bbId, 0, UnCondEdge) # connect to end bb
        bbEdges.append(bbEdge)

    return bbMap, bbEdges

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

  def genDotBbLabel(self,
                    bbId: BasicBlockIdT
  ) -> str:
    """Generate BB label to be used in printing dot graph."""
    bbLabel: str = ""
    if bbId == -1:
      bbLabel = "START"
    elif bbId == 0:
      bbLabel = "END"
    else:
      bbLabel = f"BB{bbId}"
    return bbLabel

  def genDotGraph(self) -> str:
    """Generates a basic block level CFG for dot program."""
    ret = None
    with io.StringIO() as sio:
      sio.write("digraph {\n  node [shape=box]\n")
      for bbId, insnSeq in self.basicBlocks.items():
        insnStrs = [str(insn) for insn in insnSeq]

        bbLabel: str = self.genDotBbLabel(bbId)
        insnStrs.insert(0, "[" + bbLabel + "]")

        bbContent = "\\l".join(insnStrs)
        content = f"  {bbLabel} [label=\"{bbContent}\\l\"];\n"
        sio.write(content)

      for bbIdFrom, bbIdTo, edgeLabel in self.bbEdges:
        fromLabel = self.genDotBbLabel(bbIdFrom)
        toLabel = self.genDotBbLabel(bbIdTo)

        suffix = ""
        if edgeLabel == TrueEdge:
          suffix = " [color=green, penwidth=2]"
        elif edgeLabel == FalseEdge:
          suffix = " [color=red, penwidth=2]"

        content = f"  {fromLabel} -> {toLabel}{suffix};\n"
        sio.write(content)

      sio.write("}\n")
      ret = sio.getvalue()
    return ret


import span.ir.expr as expr
if __name__ == "__main__":
  instrs = [
    instr.AssignI(expr.VarE("v:main:x"), expr.LitE(10)),
    instr.CondI(expr.VarE("v:main:x"), "True", "False"),
    instr.LabelI("True"),
    instr.LabelI("False"),
  ]
  print(Func.genBasicBlocks(instrs))
