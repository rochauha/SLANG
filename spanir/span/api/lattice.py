#!/usr/bin/env python3

# MIT License
# Copyright (c) 2019 Anshuman Dhuliya

"""Defines the base lattice class."""

import logging
_log = logging.getLogger(__name__)

from typing import Tuple, Any, Optional

from span.util.messages import TOP_BOT_BOTH

import span.ir.types as types

# This class cannot subclass LatticeT (since LatticeT uses it),
# but it is a proper lattice.
class ChangeL:
  """
  Lattice with two elements only.
  top: change = False, signifies NoChange in value.
  bot: change = True, signifies Change in value.
  """
  _top: 'ChangeL' = None
  _bot: 'ChangeL' = None

  def __init__(self,
               change: bool = False
  ) -> None:
    self._change = change

  @property
  def bot(self) -> bool: return self._change

  @property
  def top(self) -> bool: return not self._change

  @classmethod
  def getTop(cls):
    top = cls._top
    if top is not None: return top

    top = ChangeL(False)
    cls._top = top
    return top

  @classmethod
  def getBot(cls):
    bot = cls._bot
    if bot is not None: return bot

    bot = ChangeL(True)
    cls._bot = bot
    return bot

  @classmethod
  def make(cls, change: bool):
    """Create object of this class (avoids creation of redundant objects)."""
    if change: return cls.getBot()
    return cls.getTop()

  def __eq__(self,
             other: 'ChangeL'
  ) -> bool:
    if self._change == other._change: return True
    return False

  def meet(self,
           other: 'ChangeL'
  ) -> Tuple['ChangeL', 'ChangeL']:
    """change == True is bot."""
    if self.bot: return self, NoChange
    if other.bot: return other, Changed
    return self, NoChange

  def __bool__(self) -> bool: return self._change

  def __str__(self):
    if self._change: return "Changed"
    return "NoChange"

  def __repr__(self): return self.__str__()

NoChange = ChangeL.getTop()
Changed = ChangeL.getBot()

class LatticeLT(types.AnyT):
  """Base class for all Lattice except for lattice.ChangeL."""
  def __init__(self,
               top: bool = False,
               bot: bool = False
  ) -> None:
    if bot and top:
      _log.error(TOP_BOT_BOTH)
      raise ValueError(TOP_BOT_BOTH)
    self.top = top
    self.bot = bot

  def meet(self, other: 'LatticeLT') -> Tuple['LatticeLT', ChangeL]:
    """Calculates glb of the self and the other data flow value.

    Default implementation, assuming only top and bot exist (binary lattice).

    Args:
      other: the data flow value to calculate `meet` with.

    Returns:
      (Lattice, Changed): glb of self and dfv, and True if glb != self
    """
    if self.bot: return self, NoChange
    if other.bot: return other, Changed
    return self, NoChange

  def getCopy(self) -> 'LatticeLT':
    """Return a copy of this lattice element."""
    raise NotImplementedError()

  def __lt__(self, other: 'LatticeLT') -> bool:
    """Emulates non-strict weaker-than partial order,

    Default implementation, assuming only top and bot exist (binary lattice).

    Salient properties:
    if: x <= y and y <= x, then x and y are equal.
    if: not x <= y and not y <= x, then x and y are incomparable.
    for all other cases,
    not x <= y should-be-equal-to y <= x.
    """
    if self.bot: return True
    if other.bot: return False
    return True  # both are top

  def __eq__(self, other: 'LatticeLT'):
    """returns True if equal, False if not equal or incomparable.

    Default implementation, assuming only top and bot exist (binary lattice).
    """
    if self.top and other.top: return True
    if self.bot and other.bot: return True
    return False

  def __gt__(self, other):
    """Never use `>` or `>=` operator. Don't Override this method."""
    raise NotImplementedError()

class DataLT(LatticeLT):
  """The abstract Lattice type for analyses.

  One should always subclass this to form lattice.
  Directly creating objects of this class will lead to a TypeError().
  """
  def __init__(self,
               funcName: types.FuncNameT,
               val: Optional[Any] = None,
               top: bool = False,
               bot: bool = False
  ) -> None:
    if self.__class__.__name__.endswith("T"): raise TypeError()
    super().__init__(top, bot)
    self.funcName = funcName
    self.val = val

  def meet(self, other: 'DataLT') -> Tuple['DataLT', ChangeL]:
    """Calculates glb of the self and the other data flow value.

    Note: It never modifies the 'self' or the 'other' data flow value.

    Args:
      other: the data flow value to calculate `meet` with.

    Returns:
      (Lattice, ChangeL): glb of self and dfv, and 'Changed' if glb != self
    """
    raise NotImplementedError()

  def __lt__(self, other): raise NotImplementedError()

  def __eq__(self, other): raise NotImplementedError()

  def getCopy(self) -> 'DataLT': raise NotImplementedError()

  def __str__(self):
    if self.top: return "Top"
    if self.bot: return "Bot"
    return f"DataLT({self.val})"

  def __repr__(self): return self.__str__()

