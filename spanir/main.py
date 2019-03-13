#!/usr/bin/env python3

# MIT License
# Copyright (c) 2019 Anshuman Dhuliya

"""
IMPORTANT: Redundant imports are important here.
"""

import sys

import span.util.logger as logger
import logging
_log: logging.Logger = None
import time

def initialize():
  global _log
  # FIXME: (@PRODUCTION) change logging level to INFO.
  logger.init_logger(app_name="spanir", log_level=logger.LogLevels.DEBUG)
  _log = logging.getLogger(__name__)
  # when logging is disabled.

if __name__ == "__main__":
  #IMPORTANT: initialize logs first
  initialize()
  _log.error("\n\nSPAN_IR_STARTED!\n\n")

# EDIT------AFTER-------THIS-------LINE

import span.ir.op as op
import span.ir.types as types
import span.ir.expr as expr
import span.ir.instr as instr
import span.ir.obj as obj
import span.ir.tunit as irTUnit
import span.util.util as util
import span.api.graph as graph

usage = """
USAGE:

  ./main.py validate file
  ./main.py match file1 file2
  ./main.py dot file

In case of error it throws error and one can look up the log file,
for more information on cause of the error.

'dot' generates the dot file with CFG for each function present.
"""

def checkArgs() -> str:
  if not len(sys.argv) >= 3:
    print(usage)
    exit(1)

  operation = sys.argv[1]
  if operation == "match":
    if len(sys.argv) != 4:
      print(usage)
      exit(3)
  elif operation == "validate":
    if len(sys.argv) != 3:
      print(usage)
      exit(4)
  elif operation == "dot":
    if len(sys.argv) != 3:
      print(usage)
      exit(5)
  else:
    print(usage)
    exit(6)

  return operation

def match():
  fileName1 = sys.argv[2]
  fileName2 = sys.argv[3]
  fileContent1 = util.getFileContent(fileName1)
  fileContent2 = util.getFileContent(fileName2)

  start = time.time()

  ir1 = eval(fileContent1)
  ir2 = eval(fileContent2)

  exitCode = 0
  # work here
  if ir1 != ir2:
    exitCode = 2

  end = time.time()
  print("TimeTaken (ir-load-and-compare):", (end-start) * 1000, "millisec.")

  if exitCode != 0:
    print("NOT A MATCH.")
  else:
    print("A MATCH.")
  exit(exitCode)

def validate():
  print("TODO: Validation functionality.")

def genDotGraph():
  fileName = sys.argv[2]
  fileContent = util.getFileContent(fileName)
  tUnit = eval(fileContent)

  for objName, obj in tUnit.allObjs.items():
    if objName.startswith("f:"):
      cfg = graph.Cfg(obj.name, obj.basicBlocks, obj.bbEdges)
      dotGraph = cfg.genDotGraph()
      print("START: DOTGRAPH for", objName)
      print(dotGraph)
      print("END  : DOTGRAPH for", objName)
    print("\n\n\n")

if __name__ == "__main__":
  print("RotatingLogFile:", logger.ABS_LOG_FILE_NAME)
  operation = checkArgs()

  if operation == "match":
    match()
  elif operation == "validate":
    validate()
  elif operation == "dot":
    genDotGraph()

  _log.error("SPAN_IR_FINISHED!")


