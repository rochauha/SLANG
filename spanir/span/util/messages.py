#!/usr/bin/env python3

# MIT License
# Copyright (c) 2019 Anshuman Dhuliya

"""Project wide messages.."""

CONTROL_HERE_ERROR = "Control should not reach here."

START_BB_ID_NOT_MINUS_ONE = (
  "Start BB id is not -1 in the given input Dict["
  "BasicBlockId, BB]."
)

END_BB_ID_NOT_ZERO = (
  "End BB id is not 0 in the given input Dict[BasicBlockId, BB]."
  "\nThis is required if BB count is greater than one."
)

PTR_INDLEV_INVALID = "Indirection level of pointer is less than 1!"
TOP_BOT_BOTH = "Are you saying its Top and Bot at the same time?!!!"

SHOULD_BE_ONLY_ONE_EDGE = "There should be only one edge here."
