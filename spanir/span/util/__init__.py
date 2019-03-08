#!/usr/bin/env python3

# MIT License
# Copyright (c) 2019 Anshuman Dhuliya

"""Utilities for SPAN's internal use.

This package should never be dependent on SPAN internals.
i.e. no imports from SPAN internals in any module here.
This avoids circular dependency.

The content of the __eq__() methods in most classes in this package
is written to support the automated testing of the IR.

See `spanir` project.
"""
