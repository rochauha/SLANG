#!/usr/bin/env python3

# MIT License
# Copyright (c) 2019 Anshuman Dhuliya

"""Project wide utility functions."""

import os
import os.path as osp
from io import StringIO
import string
import random
from typing import Optional

import logging
_log = logging.getLogger(__name__)

globalCounter: int = 0
NAME_SEP = ":"

################################################
# BOUND START: SystemWideSwitches
################################################

"""A system wide feature switchs
The switches are used to dynamically enable or disable specific features.
Use as follows:
  from span.util.util import LS, US, AS
"""

# by default all switches are false
LS = AS = US = False # IMPORTANT

# logger switch (enables the logging system)
# its good to enable while developing
LS: bool = True # just comment this line to make it False

# dfv update switch (enforces monotonic updates)
# its good to disable when deploying
US: bool = True # just comment this line to make it False

# assertion switch (enables deeper correctness checking, like monotonicity)
# its good to enable while developing
AS: bool = True # just comment this line to make it False

################################################
# BOUND END  : SystemWideSwitches
################################################

def createDir(dirpath, exist_ok=True):
  """Creates dir. Relative paths use current directory.

  Args:
    dirpath: an absolute or relative path

  Returns:
    str: absolute path of the directory or None.
  """
  if osp.isabs(dirpath):
    abs_path = dirpath
  else:
    cwd = os.getcwd()
    abs_path = osp.join(cwd, dirpath)

  _log.debug("CreatingDirectory: %s", abs_path)

  try:
    os.makedirs(abs_path, exist_ok=exist_ok)
  except Exception as e:
    _log.error("Error CreatingDirectory: {},\n{}".format(abs_path, e))
    return None

  return abs_path

def simplifyName(name: str):
  """Given a name 'v:main:b' it returns just 'b'"""
  return name.split(NAME_SEP)[-1]

def getUniqueId() -> int:
  """Returns a unique integer id (increments by 1)."""
  # use of simple function and a global var is runtime efficient.
  global globalCounter
  globalCounter += 1
  return globalCounter

def readFromFile(fileName: str) -> str:
  """Returns the complete content of the given file."""
  sio = StringIO()
  with open(fileName) as f:
    return f.read()

def writeToFile(fileName: str, content: str):
  """Writes content to the given file."""
  with open(fileName, "w") as f:
    f.write(content)
  return None

def appendToFile(fileName: str, content: str):
  """Writes content to the given file."""
  with open(fileName, "a") as f:
    f.write(content)
  return None

def randomString(length: int = 10,
                 digits: bool = True,
                 caps: bool = True,
                 small: bool = True,
) -> Optional[str]:
  """Returns a random string of given length."""
  if not (digits or caps or small): return None

  randDigits = random.choices(string.digits, k=length)
  randCaps = random.choices(string.ascii_uppercase, k=length)
  randSmall = random.choices(string.ascii_lowercase, k=length)

  collect = []
  if digits:
    collect = randDigits
  if caps:
    collect.extend(randCaps)
  if small:
    collect.extend(randSmall)

  random.shuffle(collect)

  return "".join(collect[:length])

