#!/usr/bin/env python3

# MIT License
# Copyright (c) 2019 Anshuman Dhuliya

"""Adds logging to the project.

How to use?
  STEP 1: Only during application initialization,
    |  import <app-name>.logger
    |  logger.initLogger(app_name="app-name")

  STEP 2: For each module in the application,
    |  import logging
    |  _log = logging.getLogger(__name__)

Logging levels available:
    |  CRITICAL    50 logging.critical()
    |  ERROR       40 logging.error()
    |  WARNING     30 logging.warning()
    |  INFO        20 logging.info()
    |  DEBUG       10 logging.debug()
    |  NOTSET      0
"""

import os
import os.path as osp
import logging
from typing import Optional

from logging.handlers import RotatingFileHandler

_log: Optional[logging.Logger] = None

# THE LOGGER SWITCH (LS): if set to false, all guarded (by LS) logging shuts down.
# Creating an explicit Logger Switch is runtime efficient
# It benefits with ~20% speedup wrt to default logging disable mechanism.
ON:  bool = True
OFF: bool = False
LS:  bool = ON
#LS:  bool = OFF

# Edit these configuration variables:

DEFAULT_APP_NAME: str = "app"
LOGS_DIR: str = ".itsoflife/local/logs/{}-logs"
LOG_FILE_NAME: str = "{}.log"

ABS_LOG_FILE_NAME: str = ""  #initialized at runtime

LOG_FORMAT_1: str = (">>> %(asctime)s : %(levelname)8s : %(filename)s\n"
              "    Line %(lineno)4s : %(funcName)s()\n"
              "%(message)s\n")

LOG_FORMAT_2: str = ("   [%(asctime)s : %(levelname)8s : %(name)s"
              "    Line %(lineno)4s : %(funcName)s()]\n"
              "%(message)s")

MAX_FILE_SIZE: int = 1 << 24  # in bytes 1 << 24 = 16 MB
BACKUP_COUNT: int = 5  # 5 x 16MB = 80 MB logs + one extra current 16 MB logfile.

_root_logger: Optional[logging.Logger] = None
_initialized: bool = False

class LogLevels:
  """Logging Levels.

  Notes:
    CRITICAL > ERROR > WARNING > INFO > DEBUG > NOTSET
    For example, setting logging level to INFO,
    will enable INFO and DEBUG only.
  """
  CRITICAL = logging.CRITICAL
  ERROR = logging.ERROR
  WARNING = logging.WARNING
  INFO = logging.INFO
  DEBUG = logging.DEBUG
  NOTSET = logging.NOTSET

def create_dir(dirpath):
  """Creates dir. Relative paths use user's home.

  Args:
    dirpath: an absolute or relative path

  Returns:
    str: absolute path of the directory or None.

  """
  if osp.isabs(dirpath):
    abs_path = dirpath
  else:
    user_home = os.getenv("HOME", None).strip()
    if user_home:
      abs_path = osp.join(user_home, dirpath)
    else:
      logging.error("Unable to create dir '{}'. Env variable 'HOME' empty /not "
            "available.".format(dirpath))
      return None

  try:
    os.makedirs(abs_path, exist_ok=True)
  except Exception as e:
    logging.error("Error creating directory {},\n{}".format(abs_path, e))
    return None

  return abs_path

def init_logger(file_name:str=None,
                app_name:str=DEFAULT_APP_NAME,
                log_level:int=LogLevels.INFO,  # default logging level
                log_format:str=LOG_FORMAT_2,
                max_file_size:int=1<<24,
                backup_count=BACKUP_COUNT) -> bool:
  """Initializes the logging system.

  Args:
    file_name:
    app_name: one word app name (without space/ special chars)
    log_level:
    log_format:
    max_file_size: in bytes
    backup_count:
    
  Returns:
    bool: True if logging setup correctly.
  """
  global _initialized, _root_logger, _log, ABS_LOG_FILE_NAME
  if _initialized: return True

  # create log file dir
  if file_name:
    dir_path = osp.dirname(file_name)
    abs_path = create_dir(dir_path)
  else:
    dir_path = LOGS_DIR.format(app_name)
    abs_path = create_dir(dir_path)
    file_name = LOG_FILE_NAME.format(app_name)

  if not abs_path:
    logging.error("%s: Cannot create logging dir: %s", app_name, dir_path)
    return False

  abs_file_name = osp.join(abs_path, file_name)
  ABS_LOG_FILE_NAME = abs_file_name

  logging.info("{}: logs enabled: setting up logging system.".format(app_name))

  # set up root logger
  _root_logger = logging.getLogger()
  _root_logger.setLevel(log_level)

  handler = RotatingFileHandler(abs_file_name,
                                maxBytes=max_file_size,
                                backupCount=backup_count)
  handler.setFormatter(logging.Formatter(log_format))

  _root_logger.handlers = []  # remove all previous handlers
  _root_logger.addHandler(handler)

  _root_logger.info("%s: Initialized with format : %s",
                    app_name,
                    repr(log_format))

  _log = logging.getLogger(__name__)

  _initialized = True
  return True

def disable():
  """Disables all logging.

  Disables all logging except CRITICAL.
  """
  global _initialized, _root_logger
  if _initialized:
    _root_logger.setLevel(LogLevels.CRITICAL)
    _log.info("logging level changed to %s", LogLevels.CRITICAL)
    return True
  return False

def enable(log_level=LogLevels.INFO):
  """Enables log_level severity and above.

  Args:
    log_level: severity level to enable from (and above).
  """
  global _initialized, _root_logger
  if _initialized:
    _root_logger.setLevel(log_level)
    _log.info("logging level changed to %s", log_level)
    return True
  return False

def set_level(log_level=LogLevels.INFO):
  """Set _root_logger's log level, if already initialized

  Returns:
    bool: True if log level changed, false otherwise.
  """
  global _root_logger
  if _root_logger:
    _root_logger.setLevel(log_level)
    _log.info("logging level changed to %s", log_level)
    return True  # changed log level
  else:
    return False  # not set

if __name__ == "__main__":
  init_logger()


