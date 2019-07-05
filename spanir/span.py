#!/usr/bin/env python3

# MIT License
# Copyright (c) 2019 Anshuman Dhuliya

"""
This is the main module that invokes the whole span system.
"""

import span.util.logger as logger
import logging
_log: logging.Logger = None

import time
import sys
import subprocess as subp
import re

def initialize():
  global _log
  # FIXME: (@PRODUCTION) change logging level to INFO.
  logger.initLogger(appName="span", logLevel=logger.LogLevels.DEBUG)
  _log = logging.getLogger(__name__)

if __name__ == "__main__":
  initialize() #IMPORTANT: initialize logs first
  _log.error("\n\nSPAN_STARTED!\n\n")

# redundant imports are here to eval the input span ir file
import span.ir.types as types # IMPORTANT
import span.ir.op as op # IMPORTANT
import span.ir.expr as expr # IMPORTANT
import span.ir.instr as instr # IMPORTANT
import span.ir.obj as obj # IMPORTANT
import span.ir.tunit as irTUnit # IMPORTANT
from span.ir.types import Loc # IMPORTANT
import span.ir.graph as graph # IMPORTANT
import span.ir.ir as ir

import span.util.util as util

# Set this variable to True only if,
#   * you are intersted in SPAN IR only (view/print its dot graph etc)
#   * you dont want to run analyses/diagnoses on it.
ONLY_IR_SUPPORT = False

try:
  import span.sys.host as host
  import span.sys.clients as clients
  import span.sys.diagnosis as sysDiagnosis
  import span.api.diagnosis as apiDiagnosis
  sysDiagnosis.init()
except ImportError:
  ONLY_IR_SUPPORT = True
  print("Only IR support available!!!")

usage = """

  # prints this message
  span help

  # generates file test.c.spanir
  span c2spanir test.c
  
  # generates file test.c.hooplir
  span spanir2hooplir test.c.spanir
  
  # diagnoses test.c.spanir and generates test.c.spanreport
  span diagnose DeadStoreR test.c.spanir
  
  # analyze and print result to standard output
  # other analyses are synergistically added
  span analyze +LiveVarsA test.c.spanir
  
  # analyze and print result to standard output
  # other analyses are synergistically added
  # + prefix runs the analysis, - prefix diables analysis
  # the first + analysis is the first to run.
  span analyze +LiveVarsA+PointsToA-ConstA test.c.spanir
  
  # analyze and print result to standard output
  # other analyses are NOT added synergistically
  span analyze /+LiveVarsA/ test.c.spanir
  
  # generate the dot files for each function
  span view dot test.c.spanir
  
  # generate the node level dot files for each function
  span view dotnode test.c.spanir
"""

installDotMsg = """
xdg-open cannot locate program to view graphviz dot files.
Install xdot with,
  sudo apt-get update
  sudo apt-get install xdot
"""

commands = {"help", "view", "c2spanir", "diagnose", "analyze", "spanir2hooplir"}

def printRegisteredAnalyses():
  if ONLY_IR_SUPPORT: return onlyIrSupportAvailable("Analysis")

  for anName, anClass in clients.analyses.items():
    doc = anClass.__doc__ if anClass.__doc__ else "No Description"
    doc = doc[:50]
    if len(doc) == 50: doc += "..."
    print(f"  '{anName}': {doc}")

def printRegisteredDiagnoses():
  if ONLY_IR_SUPPORT: return onlyIrSupportAvailable("Diagnosis")

  for diName, diClass in sysDiagnosis.allDiagnoses.items():
    doc = diClass.__doc__ if diClass.__doc__ else "No Description"
    doc = doc[:50]
    if len(doc) == 50: doc += "..."
    print(f"  '{diName}': {doc}")

def printUsageAndExit(exitCode: int = 0) -> None:
  print(usage)
  print()
  print("Analyses one can invoke (analysis names end with 'A'):")
  print("\nDiagnoses one can invoke (diagnoses names end with 'D'):")

  printRegisteredAnalyses()
  print()
  printRegisteredDiagnoses()

  exit(exitCode)

def checkArguments():
  """Basic check of the given command line arguments."""
  if not 3 <= len(sys.argv) <= 4:
    # check the minimum requirement
    printUsageAndExit(1)
  else:
    command = sys.argv[1]
    if command not in commands:
      printUsageAndExit(2)
    else:
      if command == "c2spanir":
        if not len(sys.argv) == 3:
          printUsageAndExit(3)
      elif command == "diagnose" or\
          command == "analyze" or command == "view":
        if not len(sys.argv) == 4:
          printUsageAndExit(4)

def c2spanir(cFileName: str = None) -> int:
  """Converts the C file to SPAN IR
  e.g. takes hello.c and produces hello.c.spanir
  """
  if not cFileName:
    cFileName = sys.argv[2]
  cmd = f"clang --analyze -Xanalyzer -analyzer-checker=debug.SlangGenAst {cFileName}"

  print("running> ", cmd)
  completed = subp.run(cmd, shell=True)
  print("Return Code:", completed.returncode)
  if completed.returncode != 0:
    print("SPAN: ERROR.")
    return completed.returncode
  return 0

def diagnoseSpanIr() -> None:
  """Runs the given diagnosis on the spanir file for each function."""
  diName = sys.argv[2] # diagnosis name
  fileName = sys.argv[3]

  fileName = convertIfCFile(fileName)

  spanir = util.readFromFile(fileName)
  currTUnit = eval(spanir)

  reports = []
  for objName, irObj in currTUnit.allObjs.items():
    if isinstance(irObj, obj.Func):
      func: obj.Func = irObj
      if func.basicBlocks: # if function is not empty
        report = sysDiagnosis.runDiagnosis(diName, func)
        if report:
          reports.extend(report)

  # dump the span reports in the designated file
  apiDiagnosis.dumpReports(currTUnit.name, reports)

  # now run scan-build to visualize the reports
  if not reports:
    print("No report generated.")
    exit(0)

  fileName = ".".join(fileName.split(".")[:-1]) # remove .spanir extension
  cmd = f"scan-build -V -enable-checker debug.slangbug clang -c -std=c99 {fileName}"
  completed = subp.run(cmd, shell=True)
  print("Return Code:", completed.returncode)
  if completed.returncode != 0:
    print("SPAN: ERROR.")

def analyzeSpanIr() -> None:
  """Runs the given analysis on the spanir file for each function."""
  anNameExpr = sys.argv[2] # analysis name expression
  fileName = sys.argv[3]

  fileName = convertIfCFile(fileName)

  avoidAnalyses = []
  mainAnalysis = None
  otherAnalyses = []
  analysisCount = 1024 # a large number

  if anNameExpr[0] == "/" and anNameExpr[-1] == "/":
    analysisCount = 0
    if anNameExpr[0] == "/" and anNameExpr[-1] != "/" or\
      anNameExpr[0] != "/" and anNameExpr[-1] == "/":
      printUsageAndExit(20)
    anNameExpr = anNameExpr[1:-1]

  givenAnalyses = re.findall(r"[+-]\w+", anNameExpr)

  for anName in givenAnalyses:
    if anName[0] == "+":
      analysisCount += 1
      if not mainAnalysis:
        mainAnalysis = anName[1:]
      else:
        otherAnalyses.append(anName[1:])
    elif anName[0] == "-":
      avoidAnalyses.append(anName[1:])
    else:
      printUsageAndExit(30)

  spanir = util.readFromFile(fileName)
  currTUnit = eval(spanir)

  # for index in range(1,10000):
  #   start = time.time()
  for objName, irObj in currTUnit.allObjs.items():
    if isinstance(irObj, obj.Func):
      func: obj.Func = irObj
      if func.basicBlocks: # if function is not empty
        syn1 = host.Host(func, mainAnalysis, otherAnalyses, avoidAnalyses, analysisCount)
        syn1.analyze() # do the analysis
        print("\nResults for function:", func.name)
        print("========================================")
        syn1.printResult() # print the result of each analysis
    #   end = time.time()
    #   print(f"StepTimeTaken({index}):", (end-start) * 1000, "millisec.", file=sys.stderr)

def viewDotFile():
  """Generates the dot files to view the IR better."""
  format = sys.argv[2]
  fileName = sys.argv[3]

  fileName = convertIfCFile(fileName)

  spanir = util.readFromFile(fileName)
  currTUnit = eval(spanir)

  #if OPTIMIZE: irTUnit.OptimizeTUnit.optimizeO3(tUnit)

  show = True
  dotGraph: str = ""
  for objName, irObj in currTUnit.allObjs.items():
    if isinstance(irObj, obj.Func):
      func: obj.Func = irObj
      if func.basicBlocks: # if function has body
        if format == "dot":
          #dotGraph = funcObj.genDotGraph()
          dotGraph = func.cfg.genBbDotGraph()
          suffix = "bb"
        elif format == "dotnode":
          dotGraph = func.cfg.genDotGraph()
          suffix = "node"
        else:
          printUsageAndExit(10)

        dotFileName = fileName + "." + func.name.split(":")[-1]
        dotFileName += f".{suffix}.dot"
        util.writeToFile(dotFileName, dotGraph)

        if show:
          cmd = f"xdg-open {dotFileName} &"
          completed = subp.run(cmd, shell=True)
          print("Return Code:", completed.returncode)
          if completed.returncode != 0:
            print(installDotMsg)
            show = False

def spanir2hooplir():
  """Converts spanir to a custom hooplir."""
  fileName = sys.argv[2]

  spanir = util.readFromFile(fileName)
  currTUnit = eval(spanir)

  for objName, irObj in currTUnit.allObjs.items():
    if isinstance(irObj, obj.Func):
      func: obj.Func = irObj
      print(f"\n--Function: {objName} (START)")
      print(f"--Args: {func.paramNames}\n")
      hooplIr = func.toHooplIr()
      print(hooplIr)
      print(f"\n--Function: {objName} (END)")
      print()

def convertIfCFile(fileName: str) -> str:
  if fileName.endswith(".c"):
    c2spanir(fileName)
    return f"{fileName}.spanir"
  return fileName

def onlyIrSupportAvailable(msg: str) -> None:
  if ONLY_IR_SUPPORT:
    print(f"Only IR Support available! {msg} not possible.")
    return None

# mainentry
if __name__ == "__main__":
  print("RotatingLogFile: file://", logger.ABS_LOG_FILE_NAME, sep="")
  start = time.time()

  checkArguments()

  command = sys.argv[1]
  if command == "help":
    printUsageAndExit(0)
  elif command == "view":
    viewDotFile()
  elif command == "spanir2hooplir":
    spanir2hooplir()

  ###################################################
  # ONLY_IR_SUPPORT check
  ###################################################
  if ONLY_IR_SUPPORT:
    onlyIrSupportAvailable("Conversion")
    exit(1) # Don't go any further.
  ###################################################

  if command == "c2spanir":
    c2spanir()
  elif command == "diagnose":
    diagnoseSpanIr()
  elif command == "analyze":
    analyzeSpanIr()
  else:
    printUsageAndExit(11)

  end = time.time()
  print("TimeTaken:", (end-start) * 1000, "millisec.", file=sys.stderr)

_log.error("FINISHED!")

