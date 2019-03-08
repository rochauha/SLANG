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

import span.ir.types as types
import span.ir.expr as expr
import span.ir.instr as instr
import span.ir.obj as obj
import span.ir.tunit as irTUnit
import span.util.util as util

usage = """
USAGE:

  ./main.py file1 file2

It successfully exits with 'SUCCESS,' if file1 and file2 match.
In case of error it throws error and one can look up the log file,
for more information on cause of the error.
"""

if __name__ == "__main__":
  print("RotatingLogFile:", logger.ABS_LOG_FILE_NAME)
  if len(sys.argv) != 3:
    print(usage)
    exit(1)

  fileName1 = sys.argv[1]
  fileName2 = sys.argv[2]

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

  _log.error("SPAN_IR_FINISHED!")


