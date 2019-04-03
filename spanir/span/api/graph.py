#!/usr/bin/env python3

# MIT License
# Copyright (c) 2019 Anshuman Dhuliya

"""Program abstraction as control flow graph, call graph etc."""

import logging
_log = logging.getLogger(__name__)
from typing import List, Dict, Set, Optional, Tuple
import io

from span.util.logger import LS
import span.ir.instr as instr
from span.ir.types import EdgeLabelT, FalseEdge, TrueEdge, UnCondEdge, BasicBlockIdT
import span.ir.types as types
import span.ir.obj as obj

from span.util.messages import START_BB_ID_NOT_MINUS_ONE, END_BB_ID_NOT_ZERO
import span.util.util as util

# Type names to make code self documenting
CfgNodeId = int
CfgEdgeId = int
MinHeightT = int

class CfgEdge:
  """A directed edge (with label) between two CfgNodes."""
  def __init__(self,
               src: 'CfgNode',
               dest: 'CfgNode',
               label: EdgeLabelT = UnCondEdge,
  ) -> None:
    self.src = src
    self.dest = dest
    self.label = label

  def __eq__(self, other: 'CfgEdge'):
    return self.src == other.src and self.dest == other.dest

  def __hash__(self): return self.src.id + self.dest.id

  def __str__(self): return self.__repr__()

  def __repr__(self):
    return f"CfgEdge({self.src}, {self.dest}, {self.label!r})"

class CfgNode(object):
  """A cfg statement node (which contains one and only one instruction).
  """
  def __init__(self,
               insn: instr.InstrIT,
               predEdges: Optional[List[CfgEdge]] = None,
               succEdges: Optional[List[CfgEdge]] = None,
  ) -> None:
    # The min height of this node from the end node.
    # i.e. min no. of edges between this and end node.
    # Its used for worklist generation.
    # it eventually stores a unique id (within a func)
    self.id: int = 0x7FFFFFFF  # init to infinite
    self.insn = insn
    self.predEdges = predEdges if predEdges else []
    self.succEdges = succEdges if succEdges else []

  def addPred(self, edge: CfgEdge): self.predEdges.append(edge)

  def addSucc(self, edge: CfgEdge): self.succEdges.append(edge)

  def __eq__(self, other: 'CfgNode') -> bool: return self.id == other.id

  def __hash__(self) -> int: return self.id

  def __str__(self):
    predIds = []
    for predEdge in self.predEdges:
      predIds.append(predEdge.src.id)
    succIds = []
    for succEdge in self.succEdges:
      succIds.append(succEdge.dest.id)
    return f"Node {self.id}: ({self.insn}, pred={predIds}, succ={succIds})"

  def __repr__(self): return self.__str__()

class BbEdge:
  """A directed edge (with label) between two BB (Basic Blocks)."""
  def __init__(self,
               src: 'BB',
               dest: 'BB',
               label: EdgeLabelT = UnCondEdge,
  ) -> None:
    self.src = src
    self.dest = dest
    self.label = label

  def __eq__(self, other: 'BbEdge'):
    return self.src == other.src and self.dest == other.dest

  def __hash__(self): return self.src.id + self.dest.id

  def __str__(self): return self.__repr__()

  def __repr__(self):
    return f"BBEdge({self.src}, {self.dest}, {self.label!r})"

class BB:
  """A Basic Block."""
  def __init__(self,
               id: BasicBlockIdT = 0,
               instrSeq: List[instr.InstrIT] = None,
               predEdges: Optional[List[BbEdge]] = None,  # predecessor basic blocks
               succEdges: Optional[List[BbEdge]] = None,  # successor basic blocks
               ) -> None:
    self.id = id  # id is user defined (unique within func)
    self.instrSeq = instrSeq
    self.cfgNodeSeq: List[CfgNode] = self.genCfgNodeSeq(instrSeq)
    self.predEdges = predEdges if predEdges else []
    self.succEdges = succEdges if succEdges else []

    self.firstCfgNode = self.cfgNodeSeq[0]
    self.lastCfgNode = self.cfgNodeSeq[-1]

  def genCfgNodeSeq(self,
                    instrSeq: List[instr.InstrIT]
  ) -> List[CfgNode]:
    """Convert sequence of instructions to sequentially connected CfgNodes."""
    if not instrSeq: return [CfgNode(instr.NopI())]

    # If only one instruction, then its simple.
    if len(instrSeq) == 1: return [CfgNode(instrSeq[0])]

    # For two or more instructions:
    prev = CfgNode(instrSeq[0])
    cfgNodeSeq: List[CfgNode] = [prev]
    for insn in instrSeq[1:]:
      curr = CfgNode(insn)
      edge = CfgEdge(prev, curr, UnCondEdge)
      curr.addPred(edge)
      prev.addSucc(edge)
      cfgNodeSeq.append(curr)
      prev = curr

    return cfgNodeSeq

  def addPred(self, edge: BbEdge): self.predEdges.append(edge)

  def addSucc(self, edge: BbEdge): self.succEdges.append(edge)

  def __str__(self): return self.__repr__()

  def __repr__(self):
    return f"BB({self.instrSeq})"

class Cfg(object):
  """A Cfg (body of a function)"""
  def __init__(self,
               funcName: types.FuncNameT,
               inputBbMap: Dict[BasicBlockIdT, List[instr.InstrIT]],
               inputBbEdges: List[Tuple[BasicBlockIdT, BasicBlockIdT, EdgeLabelT]]
               ) -> None:
    self.funcName = funcName
    self.inputBbMap = inputBbMap
    self.inputBbEdges = inputBbEdges

    self.startBb: BB = None
    self.endBb: BB = None

    self.start: CfgNode = None
    self.end: CfgNode = None

    self.bbMap: Dict[BasicBlockIdT, BB] = dict()
    self.nodeMap: Dict[CfgNodeId, CfgNode] = dict()
    self.revPostOrder: List[CfgNode] = []

    # fills the variables above correctly.
    self.buildCfgStructure(inputBbMap, inputBbEdges)

  def buildCfgStructure(self,
                        inputBbMap: Dict[BasicBlockIdT, List[instr.InstrIT]],
                        inputBbEdges: List[Tuple[BasicBlockIdT, BasicBlockIdT, EdgeLabelT]]
                        ) -> None:
    """Builds the complete Cfg structure."""
    if not inputBbMap: return

    # STEP 1: Create BBs in their dict.
    for bbId, instrSeq in inputBbMap.items():
      self.bbMap[bbId] = BB(id=bbId, instrSeq=instrSeq)

    if -1 not in self.bbMap:
      _log.error(START_BB_ID_NOT_MINUS_ONE)

    if 0 not in self.bbMap:
      _log.error(END_BB_ID_NOT_ZERO)

    self.startBb = self.bbMap[-1]
    self.endBb = self.bbMap[0]
    self.start = self.startBb.firstCfgNode
    self.end = self.endBb.lastCfgNode

    # STEP 2: Based on bbMap and inputBbEdges, interconnect CfgNodes of BBs.
    self.connectNodes(self.bbMap, inputBbEdges)

    # STEP 3: Find the reverse post order sequence of CFG Nodes
    self.revPostOrder = self.calcRevPostOrder()

    # STEP 4: Number the nodes in reverse post order and add to dict
    newId = 0
    for node in self.revPostOrder:
      newId += 1
      node.id = newId
      self.nodeMap[newId] = node

  def connectNodes(self,
                   bbMap: Dict[BasicBlockIdT, BB],
                   inputBbEdges: List[Tuple[BasicBlockIdT, BasicBlockIdT, EdgeLabelT]]
                   ) -> None:
    """Interconnects basic blocks and cfg nodes."""
    for startBbId, endBbId, edgeLabel in inputBbEdges:
      #STEP 2.1: Interconnect BBs with read ids.
      startBb = bbMap[startBbId]
      endBb = bbMap[endBbId]
      bbEdge = BbEdge(startBb, endBb, edgeLabel)
      startBb.addSucc(bbEdge)
      endBb.addPred(bbEdge)

      #STEP 2.2: Connect CfgNodes across basic blocks.
      startNode = startBb.lastCfgNode
      endNode = endBb.firstCfgNode
      cfgEdge = CfgEdge(startNode, endNode, edgeLabel)
      startNode.addSucc(cfgEdge)
      endNode.addPred(cfgEdge)

  def calcMinHeights(self, currNode: CfgNode):
    """Calculates and allocates min_height of each node."""
    newPredHeight = currNode.id + 1
    for predEdge in currNode.predEdges:
      pred = predEdge.src
      currPredHeight = pred.id
      if currPredHeight > newPredHeight:
        pred.id = newPredHeight
        self.calcMinHeights(pred)

  def calcRevPostOrder(self) -> List[CfgNode]:
    self.end.id = 0
    self.calcMinHeights(self.end)
    done = {id(self.start)}
    sequence = []
    worklist = [(self.start.id, self.start)]
    return self.genRevPostOrderSeq(sequence, done, worklist)

  def genRevPostOrderSeq(self,
                         seq: List[CfgNode],
                         done: Set[CfgNodeId],
                         worklist: List[Tuple[MinHeightT, CfgNode]]
  ) -> List[CfgNode]:
    if not worklist: return seq
    _, node = worklist.pop() # get node with max height
    seq.append(node)
    for succEdge in node.succEdges:
      destNode = succEdge.dest
      if id(destNode) not in done:
        destMinHeight = destNode.id
        tup = (destMinHeight, destNode)
        done.add(id(destNode))
        worklist.append(tup)
    worklist.sort(key=lambda x: x[0])
    return self.genRevPostOrderSeq(seq, done, worklist)

  def genDotGraph(self) -> str:
    """ Generates Dot graph of itself. """
    if not self.inputBbMap: return "digraph{}"
    ret = None
    with io.StringIO() as sio:
      sio.write("digraph {\n  node [shape=box]\n")
      for nodeId, node in self.nodeMap.items():
        suffix = ""
        if len(node.succEdges) == 0 or len(node.predEdges) == 0:
          suffix = ", color=blue, penwidth=4"
        content = f"  n{nodeId} [label=\"{node.insn}\"{suffix}];\n"
        sio.write(content)
      sio.write("\n")

      for nodeId, node in self.nodeMap.items():
        for succ in node.succEdges:
          suffix = ""
          if succ.label == types.FalseEdge:
            suffix = "[color=red, penwidth=2]"
          elif succ.label == types.TrueEdge:
            suffix = "[color=green, penwidth=2]"
          content = f"  n{nodeId} -> n{succ.dest.id} {suffix};\n"
          sio.write(content)
      sio.write("}\n")
      ret = sio.getvalue()
    return ret

  def __str__(self):
    sorted_nids = sorted(self.nodeMap.keys())
    with io.StringIO() as sio:
      sio.write("Cfg(")
      sio.write("\n  RevPostOrder:" + str(self.revPostOrder))
      for nid in sorted_nids:
        sio.write("\n  ")
        sio.write(str(self.nodeMap[nid]))
      sio.write(")")
      ret = sio.getvalue()
    return ret

  def __repr__(self): return self.__str__()

class FeasibleEdges:
  def __init__(self,
               cfg: Cfg
  ) -> None:
    self.cfg = cfg
    self.fEdges: Set[CfgEdge] = set()

  def initFeasibility(self) -> List[CfgNode]:
    """Assuming start node as feasible, marks initial set of feasible edges.

    All initial UnCondEdges chains are marked feasible.
    """
    feasibleNodes = []
    for succEdge in self.cfg.start.succEdges:
      if succEdge.label == UnCondEdge:
        nodes = self.setFeasible(succEdge)
        feasibleNodes.extend(nodes)
    return feasibleNodes

  def setFeasible(self,
                  cfgEdge: CfgEdge
  ) -> List[CfgNode]:
    """Marks the given edge, and all subsequent UnCondEdges chains as feasible.

    Returns list of nodes id of nodes, that may become reachable,
    due to the freshly marked incoming feasible edges.
    """
    feasibleNodes: List[CfgNode] = []
    if cfgEdge in self.fEdges:
      # edge already present, hence must have been already set
      return feasibleNodes
    feasibleNodes.append(cfgEdge.dest)
    if LS: _log.debug("New_Feasible_Edge: %s.", cfgEdge)

    self.fEdges.add(cfgEdge)
    toNode = cfgEdge.dest
    for edge in toNode.succEdges:
      if edge.label == UnCondEdge:
        nodes = self.setFeasible(edge)
        feasibleNodes.extend(nodes)
    return feasibleNodes

  def isFeasibleEdge(self,
                     cfgEdge: CfgEdge
  ) -> bool:
    """Returns true if edge is feasible."""
    if cfgEdge in self.fEdges: return True
    return False

  def setAllSuccEdgesFeasible(self,
                              node: CfgNode
  ) -> List[CfgNode]:
    """Sets all succ edges of node_id as feasible.

    All subsequent UnCondEdges chains are marked feasible.
    """
    feasibleNodes = []
    for edge in node.succEdges:
      nodes = self.setFeasible(edge)
      feasibleNodes.extend(nodes)
    return feasibleNodes

  def setFalseEdgeFeasible(self,
                           node: CfgNode
  ) -> List[CfgNode]:
    """Sets all succ edges of node, with label FalseEdge as feasible.

    All subsequent UnCondEdges chains are marked feasible.
    """
    feasibleNodes: List[CfgNode] = []
    for edge in node.succEdges:
      if edge.label == FalseEdge:
        nodes = self.setFeasible(edge)
        feasibleNodes.extend(nodes)
    return feasibleNodes

  def setTrueEdgeFeasible(self,
                          node: CfgNode
  ) -> List[CfgNode]:
    """Sets all succ edges of node, with label TrueEdge as feasible.

    All subsequent UnCondEdges chains are marked feasible.
    """
    feasibleNodes: List[CfgNode] = []
    for edge in node.succEdges:
      if edge.label == TrueEdge:
        nodes = self.setFeasible(edge)
        feasibleNodes.extend(nodes)
    return feasibleNodes

  def isFeasibleNode(self, node: CfgNode) -> bool:
    """Return true if node has a feasible pred edge."""
    if node == self.cfg.start: return True

    for predEdge in node.predEdges:
      if predEdge in self.fEdges:
        return True  # reachable
    return False

  def __str__(self):
    return f"Feasible Edges: {self.fEdges}."

  def __repr__(self): return self.__str__()

# stores function name to its cfg mapping
# func_cfg_map: Dict[types.FuncNameT, Cfg] = dict()

class FuncNode(object):
  def __init__(self,
               func: obj.Func,
  ) -> None:
    self.func = func
    self.cfg = Cfg(func.name, func.basicBlocks, func.bbEdges)

class CallGraph(object):
  """Call graph of the given translation unit."""
  def __init__(self,
               mainFunc: types.FuncNameT,
               callGraph: Dict[types.FuncNameT, types.FuncNameT]
  ) -> None:
    self.mainFunc = mainFunc
    self.callGraph = callGraph

