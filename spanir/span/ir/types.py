#!/usr/bin/env python3

# MIT License
# Copyright (c) 2019 Anshuman Dhuliya

"""All value types available in the IR.
This module is imported by almost all
other modules in the source. Hence this
module shouldn't import any other source
modules except for the utility modules.
"""

# TODO: make all Type objects immutable. Till then assume immutable.

import logging
_log = logging.getLogger(__name__)
from typing import TypeVar, List, Optional, Tuple, Dict, Set

from span.util.util import LS, AS
from span.util.messages import PTR_INDLEV_INVALID

################################################
#BOUND START: useful_types
################################################

VarNameT = str
FuncNameT = str
LabelNameT = str
RecordNameT = str
StructNameT = str
UnionNameT = str
FieldNameT = str  # a structure field name
MemberNameT = str  # a structure member name

TypeCodeT = int

OpSymbolT = str
OpNameT = str

NumericT = TypeVar('NumericT', int, float)
LitT = TypeVar('LitT', int, float, str)

CfgNodeIdT = int
BasicBlockIdT = int
EdgeLabelT = str

# edge labels
FalseEdge: EdgeLabelT = "FalseEdge"  # False edge
TrueEdge: EdgeLabelT = "TrueEdge"  # True edge
UnCondEdge: EdgeLabelT = "UnCondEdge"  # Unconditional edge

# source location line:col given by user (only used to communicate back)
# this is a 64 bit integer
SourceLocationT = int

################################################
#BOUND END  : useful_types
################################################

################################################
#BOUND START: type_codes
################################################

# the order and ascending sequence is important
VOID_TC: TypeCodeT        = 0

INT_START_TC: TypeCodeT   = 10  # start int type codes
INT1_TC: TypeCodeT        = 10  # bool
INT8_TC: TypeCodeT        = 11  # char
INT16_TC: TypeCodeT       = 12  # short
INT32_TC: TypeCodeT       = 13  # int
INT64_TC: TypeCodeT       = 14  # long long
INT128_TC: TypeCodeT      = 15  # ??
INT_END_TC: TypeCodeT     = 19  # end int type codes

UINT_START_TC: TypeCodeT  = 20  # start unsigned int type codes
UINT8_TC: TypeCodeT       = 20  # unsigned char
UINT16_TC: TypeCodeT      = 21  # unsigned short
UINT32_TC: TypeCodeT      = 22  # unsigned int
UINT64_TC: TypeCodeT      = 23  # unsigned long long
UINT128_TC: TypeCodeT     = 24  # ??
UINT_END_TC: TypeCodeT    = 30  # end unsigned int type codes

FLOAT_START_TC: TypeCodeT = 50  # start float type codes
FLOAT16_TC: TypeCodeT     = 50  # ??
FLOAT32_TC: TypeCodeT     = 51  # float
FLOAT64_TC: TypeCodeT     = 52  # double
FLOAT80_TC: TypeCodeT     = 53  # ??
FLOAT128_TC: TypeCodeT    = 54  # ??
FLOAT_END_TC: TypeCodeT   = 60  # start float type codes

PTR_TC: TypeCodeT         = 100  # pointer type code
ARR_TC: TypeCodeT         = 101  # array type code
CONST_ARR_TC: TypeCodeT   = 102  # const size array type code
VAR_ARR_TC: TypeCodeT     = 103  # variable array type code
INCPL_ARR_TC: TypeCodeT   = 104  # array type code

FUNC_TC: TypeCodeT        = 200  # function type code
FUNC_SIG_TC: TypeCodeT    = 201
STRUCT_TC: TypeCodeT      = 300  # structure type code
UNION_TC: TypeCodeT       = 400  # union type code

################################################
#BOUND END  : type_codes
################################################

class AnyT(object):
  """Base class for all static types.

  This class and all sub-classes suffixed with `T`,
  should not be instantiated.
  All classes suffixed with 'T' are only used for static and dynamic type
  checking.
  """
  def __init__(self) -> None:
    raise TypeError("Classes suffixed with 'T' shouldn't be instantiated."
                    "\nThese classes are exclusively used for static and "
                    "dynamic type checking.")

  def __eq__(self, other):
    if not isinstance(other, self.__class__):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    return True

  def __hash__(self): return hash(self.__class__.__name__)

  def __str__(self): return f"AnyT: {self.__class__.__name__}"

  def __repr__(self): return self.__str__()

class NilT(AnyT):
  def __init__(self): super().__init__()

class _Nil(NilT):
  def __init__(self): pass  # don't call super().__init__()

  def __eq__(self, other):
    if not isinstance(other, NilT):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    return True # all objects are equal

Nil = _Nil()

class Loc(AnyT):
  """Location type : line, col."""
  def __init__(self,
               line: int = 0,
               col: int = 0
               ) -> None:
    self.line = line
    self.col = col

  def __eq__(self,
             other: 'Loc'
  ) -> bool:
    if not isinstance(other, Loc):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.line == other.line:
      if LS: _log.warning("LineNum Differs: %s, %s", self, other)
      return False
    if not self.col == other.col:
      if LS: _log.warning("ColNum Differs: %s, %s", self, other)
      return False
    return True

  def __str__(self): return f"Loc({self.line},{self.col})"

  def __repr__(self): return self.__str__()

class Type(AnyT):
  """Class to represent types and super class for all compound types."""
  def __init__(self,
               typeCode: TypeCodeT
  ) -> None: # don't call super().__init__()
    self.typeCode = typeCode

  def isInteger(self):
    return INT1_TC <= self.typeCode <= UINT128_TC

  def isUnsigned(self):
    return UINT8_TC <= self.typeCode <= UINT128_TC

  def isFloat(self):
    return FLOAT16_TC <= self.typeCode <= FLOAT128_TC

  def isNumeric(self):
    return INT1_TC <= self.typeCode <= FLOAT128_TC

  def isPointer(self):
    return self.typeCode == PTR_TC

  def isFunc(self):
    return self.typeCode == FUNC_TC

  def isStuct(self):
    return self.typeCode == STRUCT_TC

  def isVoid(self):
    return self.typeCode == VOID_TC

  def __eq__(self,
             other: 'Type'
  ) -> bool:
    if not isinstance(other, Type):
      if LS: _log.warning("%s, %s are incomparable", self, other)
      return False
    if not self.typeCode == other.typeCode:
      if LS: _log.warning("Types Differ: %s, %s", self, other)
      return False
    return True

  def __hash__(self) -> int: return self.typeCode

  def bitSize(self) -> int:
    """Returns size in bits for builtin types.
    For other types, see respective classes.
    """
    size = 0
    tc = self.typeCode

    if tc == INT1_TC:         size = 1
    elif tc == INT8_TC:       size = 8
    elif tc == INT16_TC:      size = 16
    elif tc == INT32_TC:      size = 32
    elif tc == INT64_TC:      size = 64
    elif tc == INT128_TC:     size = 128
    elif tc == UINT8_TC:      size = 8
    elif tc == UINT16_TC:     size = 16
    elif tc == UINT32_TC:     size = 32
    elif tc == UINT64_TC:     size = 64
    elif tc == UINT128_TC:    size = 128
    elif tc == FLOAT16_TC:    size = 16
    elif tc == FLOAT32_TC:    size = 32
    elif tc == FLOAT64_TC:    size = 64
    elif tc == FLOAT80_TC:    size = 80
    elif tc == FLOAT128_TC:   size = 128
    elif tc == PTR_TC:        size = 64  # assumes 64bit machine

    return size

  def __str__(self) -> str:
    ss = ""
    tc = self.typeCode

    if   tc == VOID_TC:       ss = "VOID"
    elif tc == INT1_TC:       ss = "INT1"
    elif tc == INT8_TC:       ss = "INT8"
    elif tc == INT16_TC:      ss = "INT16"
    elif tc == INT32_TC:      ss = "INT32"
    elif tc == INT64_TC:      ss = "INT64"
    elif tc == INT128_TC:     ss = "INT128"
    elif tc == UINT8_TC:      ss = "UINT8"
    elif tc == UINT16_TC:     ss = "UINT16"
    elif tc == UINT32_TC:     ss = "UINT32"
    elif tc == UINT64_TC:     ss = "UINT64"
    elif tc == UINT128_TC:    ss = "UINT128"
    elif tc == FLOAT16_TC:    ss = "FLOAT16"
    elif tc == FLOAT32_TC:    ss = "FLOAT32"
    elif tc == FLOAT64_TC:    ss = "FLOAT64"
    elif tc == FLOAT80_TC:    ss = "FLOAT80"
    elif tc == FLOAT128_TC:   ss = "FLOAT128"
    elif tc == PTR_TC:        ss = "PTR"
    elif tc == FUNC_TC:       ss = "FUNC"
    elif tc == FUNC_SIG_TC:   ss = "FUNC_SIG"
    elif tc == STRUCT_TC:     ss = "STRUCT"
    elif tc == UNION_TC:      ss = "UNION"

    return ss

  def __repr__(self) -> str: return self.__str__()

################################################
#BOUND START: basic_type_objects
################################################

Void = Type(VOID_TC)

Int1 = Type(INT1_TC)
Int8 = Type(INT8_TC)
Int16 = Type(INT16_TC)
Int32 = Type(INT32_TC)
Int64 = Type(INT64_TC)
Int128 = Type(INT128_TC)

UInt8 = Type(UINT8_TC)
UInt16 = Type(UINT16_TC)
UInt32 = Type(UINT32_TC)
UInt64 = Type(UINT64_TC)
UInt128 = Type(UINT128_TC)

Float16 = Type(FLOAT16_TC)
Float32 = Type(FLOAT32_TC)
Float64 = Type(FLOAT64_TC)
Float80 = Type(FLOAT80_TC)
Float128 = Type(FLOAT128_TC)

# for convenience
Int = Int32
Char = UInt8
Float = Float32
Double = Float64

################################################
#BOUND END  : basic_type_objects
################################################

class Ptr(Type):
  """Concrete Pointer type.

  Instantiate this class to denote pointer types.
  E.g. types.Prt(types.Char, 2) is a ptr-to-ptr-to-char
  """
  def __init__(self,
               to: Type,
               indir: int = 1
  ) -> None:
    super().__init__(PTR_TC)
    if indir < 1:
      if LS: _log.error(PTR_INDLEV_INVALID)
      assert False, PTR_INDLEV_INVALID
    # indirection level to the object
    self.indir = indir
    # type of the object pointed to
    self.to = to

    # correct a recursive pointer
    while isinstance(self.to, Ptr):
      self.indir += 1
      self.to = self.to.to

  def getPointeeType(self) -> Type:
    if self.indir > 1:
      return Ptr(self.to, self.indir-1)
    return self.to

  def __eq__(self,
             other: 'Ptr'
  ) -> bool:
    if not isinstance(other, Ptr):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.typeCode == other.typeCode:
      if LS: _log.warning("Types Differ: %s, %s", self, other)
      return False
    if not self.indir == other.indir:
      if LS: _log.warning("IndLev Differ: %s, %s", self, other)
      return False
    if not self.to == other.to:
      if LS: _log.warning("DestType Differs: %s, %s", self, other)
      return False
    return True

  def __hash__(self): return hash(self.to) * self.indir

  def __str__(self): return self.__repr__()

  def __repr__(self):
    return f"types.Ptr({self.to}, {self.indir})"

class ArrayT(Type):
  """A superclass for all arrays.
  Not to be instantiated (note the suffix `T`)
  """
  def __init__(self,
               of: Type,
               typeCode: TypeCodeT = ARR_TC,
  ) -> None:
    super().__init__(typeCode)
    self.of = of

  def elementType(self) -> Type:
    """Returns the type of elements this array holds."""
    if isinstance(self.of, ArrayT):
      return self.of.elementType()
    return self.of

class ConstSizeArray(ArrayT):
  """Concrete array type.

  Instantiate this class to denote array types.
  E.g. types.Array(types.Char, [2,2]) is a 2x2 array of chars
  """
  def __init__(self,
               of: Type,
               size: Optional[List[int]],
  ) -> None:
    super().__init__(of=of, typeCode=CONST_ARR_TC)
    self.size = size

  def bitSize(self) -> int:
    """Returns size in bits of this array."""
    size = self.of.bitSize()
    size = self.size * size
    return size

  def __eq__(self,
             other: 'ConstSizeArray'
  ) -> bool:
    if not isinstance(other, ConstSizeArray):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.typeCode == other.typeCode:
      if LS: _log.warning("Types Differ: %s, %s", self, other)
      return False
    if not self.size == other.size:
      if LS: _log.warning("Dimensions Differ: %s, %s", self, other)
      return False
    if not self.of == other.of:
      if LS: _log.warning("DestType Differs: %s, %s", self, other)
      return False
    return True

  def __hash__(self): return self.typeCode + (hash(self.of) * len(self.size))

  def __str__(self): return self.__repr__()

  def __repr__(self): return f"types.Array({self.of}, {self.size})"

class VarArray(ArrayT):
  """an array with variable size: e.g. int arr[x*20+y];"""
  def __init__(self,
               of: Type,
  ) -> None:
    super().__init__(of=of, typeCode=VAR_ARR_TC)

  def bitSize(self):
    """Returns size in bits of this array."""
    if LS: _log.warning("Bit size asked !!!")
    return 0 # No size of VarArray

  def __eq__(self,
             other: 'VarArray'
  ) -> bool:
    if not isinstance(other, VarArray):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.typeCode == other.typeCode:
      if LS: _log.warning("Types Differ: %s, %s", self, other)
      return False
    if not self.of == other.of:
      if LS: _log.warning("DestType Differs: %s, %s", self, other)
      return False
    return True

  def __hash__(self): return hash(self.of) * self.typeCode

  def __str__(self): return self.__repr__()

  def __repr__(self): return f"types.VarArray({self.of})"

class IncompleteArray(ArrayT):
  """An array with no size: e.g. int arr[];"""
  def __init__(self,
               of: Type,
  ) -> None:
    super().__init__(of=of, typeCode=INCPL_ARR_TC)
    self.of = of

  def bitSize(self):
    """Returns size in bits of this array."""
    if LS: _log.warning("Bit size asked !!!")
    return 0 # No size of IncompleteArray

  def __eq__(self,
             other: 'IncompleteArray'
  ) -> bool:
    if not isinstance(other, IncompleteArray):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.typeCode == other.typeCode:
      if LS: _log.warning("Types Differ: %s, %s", self, other)
      return False
    if not self.of == other.of:
      if LS: _log.warning("DestType Differs: %s, %s", self, other)
      return False
    return True

  def __hash__(self): return hash(self.of) * self.typeCode

  def __str__(self): return self.__repr__()

  def __repr__(self): return f"types.IncompleteArray({self.of})"

class FuncSig(Type):
  """A function signature (useful for function pointer types)."""
  def __init__(self,
               returnType: Type,
               paramTypes: Optional[List[Type]] = None,
               variadic: bool = False
  ) -> None:
    super().__init__(FUNC_SIG_TC)
    self.returnType = returnType
    self.paramTypes = paramTypes if paramTypes else []
    self.variadic = variadic

  def bitSize(self):
    """Returns size in bits of this function signature."""
    if LS: _log.warning("Bit size asked !!!")
    return 0 # No size of FuncSig

  def __eq__(self,
             other: 'FuncSig'
  ) -> bool:
    if not isinstance(other, FuncSig):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.typeCode == other.typeCode:
      if LS: _log.warning("Types Differ: %s, %s", self, other)
      return False
    if not self.returnType == other.returnType:
      if LS: _log.warning("ReturnType Differs: %s, %s", self, other)
      return False
    if not self.paramTypes == other.paramTypes:
      if LS: _log.warning("ParamTypes Differ: %s, %s", self.paramTypes, other.paramTypes)
      return False
    if not self.variadic == other.variadic:
      if LS: _log.warning("Variadicness Differs: %s, %s", self, other)
      return False
    return True

  def __hash__(self):
    hsh = hash(self.returnType)
    increment = 170
    for tp in self.paramTypes:
      hsh = hsh ^ (hash(tp) + increment)
      increment += 17
    return hsh

class RecordT(Type):
  """A record type (base class to Struct and Union types)
  Anonymous records are also given a unique name."""

  def __init__(self,
               name: RecordNameT,
               fields: List[Tuple[FieldNameT, Type]] = None,
               loc: Optional[Loc] = None,
               typeCode: TypeCodeT = None,
  ) -> None:
    super().__init__(typeCode)
    self.name = name
    self.fields = fields
    self.loc = loc
    self.typeToFieldsMap: Dict[Type, Set[FieldNameT]] = {}

  def getFieldType(self,
                   fieldName: FieldNameT
  ) -> Type:
    for fName, fType in self.fields:
      if fName == fieldName:
        return fType
    if LS: _log.error("Unknown field: '%s' in record: %s", fieldName, self)
    return Void

  def getFieldsOfType(self,
                      givenType: Type,
                      prefix: VarNameT = None,
  ) -> Set[FieldNameT]:
    """Set of field names of the given type (recursive).
    The optional prefix prepends: "<prefix>." to the
    field names. Thus if prefix="x" and the record
    has field "f", it returns {"x.f"}.
    """

    if givenType in self.typeToFieldsMap:
      fieldNames = self.typeToFieldsMap[givenType]
    else:
      fieldNames: Set[FieldNameT] = set()
      for field in self.fields:
        fieldType = field[1]
        if fieldType == givenType:
          fieldName = field[0]
          fieldNames.add(fieldName)
        elif isinstance(fieldType, RecordT):
          innerFields = fieldType.getFieldsOfType(givenType)
          for f in innerFields:
            fieldNames.add(f"{fieldName}.{f}")
      self.typeToFieldsMap[givenType] = fieldNames

    if prefix:
      prefixedFields = set()
      for fieldName in fieldNames:
        prefixedFields.add(f"{prefix}.{fieldName}")
      fieldNames = prefixedFields

    return fieldNames # Should never be None

  def bitSize(self):
    """Returns size in bits of this record (virtual function)."""
    raise NotImplementedError()

  def __hash__(self):
    return hash(self.name) + STRUCT_TC

  def __str__(self):
    if self.typeCode == STRUCT_TC:
      return f"Struct('{self.name}')"
    elif self.typeCode == UNION_TC:
      return f"Union('{self.name}')"
    else:
      if LS: _log.error("Record is neither a struct nor a union!")

class Struct(RecordT):
  """A structure type.
  Anonymous structs are also given a unique name."""

  def __init__(self,
               name: StructNameT,
               fields: List[Tuple[str, Type]] = None,
               loc: Optional[Loc] = None,
  ) -> None:
    super().__init__(name, fields, loc, STRUCT_TC)
    self.name = name
    self.fields = fields
    self.loc = loc

  def bitSize(self):
    """Returns size in bits of this structure."""
    size = 0
    for fieldName, fieldType in self.fields:
      size += fieldType.bitSize()
    return size

  def __eq__(self,
             other: 'Struct'
  ) -> bool:
    if not isinstance(other, Struct):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.typeCode == other.typeCode:
      if LS: _log.warning("Types Differ: %s, %s", self, other)
      return False
    if not self.name == other.name:
      if LS: _log.warning("StructName Differs: %s, %s", self, other)
      return False
    if not self.fields == other.fields:
      if LS: _log.warning("Fields Differs: %s, %s", self.fields, other.fields)
      return False
    return True

class Union(RecordT):
  """A union type.
  Anonymous unions are also given a unique name."""

  def __init__(self,
               name: UnionNameT,
               fields: List[Tuple[str, Type]] = None,
               loc: Optional[Loc] = None,
  ) -> None:
    super().__init__(name, fields, loc, UNION_TC)

  def bitSize(self) -> int:
    """Returns size in bits of this union."""
    size = 0
    for fieldName, fieldType in self.fields:
      fieldBitSize = fieldType.bitSize()
      size = size if size > fieldBitSize else fieldBitSize
    return size

  def __eq__(self,
             other: 'Union'
  ) -> bool:
    if not isinstance(other, Union):
      if LS: _log.warning("%s, %s are incomparable.", self, other)
      return False
    if not self.typeCode == other.typeCode:
      if LS: _log.warning("Types Differ: %s, %s", self, other)
      return False
    if not self.fields == other.fields:
      if LS: _log.warning("Fields Differ: %s, %s", self, other)
      return False
    if not self.name == other.name:
      if LS: _log.warning("UnionName Differs: %s, %s", self, other)
      return False
    return True

