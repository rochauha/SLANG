#!/usr/bin/env python3

# MIT License
# Copyright (c) 2019 Anshuman Dhuliya

"""Project wide utility functions."""

import os
import os.path as osp
from io import StringIO

import logging
_log = logging.getLogger(__name__)

global_counter: int = 0
NAME_SEP = ":"

# An custom Assertion Switch.
# Note: Set to False once the system is production ready.
AS = True
#AS = False

def create_dir(dirpath, exist_ok=True):
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

  _log.debug("creating directory %s", abs_path)

  try:
    os.makedirs(abs_path, exist_ok=exist_ok)
  except Exception as e:
    _log.error("Error creating directory {},\n{}".format(abs_path, e))
    return None

  return abs_path

def simplifyName(name: str):
  """Given a name 'v:main:b' it returns just 'b'"""
  names = name.split(NAME_SEP)
  return names[-1]

def getUniqueId() -> int:
  """Returns a unique integer id (increments by 1)."""
  # use of simple function and a global var is runtime efficient.
  global global_counter
  global_counter += 1
  return global_counter

def getFileContent(fileName: str) -> str:
  sio = StringIO()
  with open(fileName) as f:
    return f.read()
  return None

