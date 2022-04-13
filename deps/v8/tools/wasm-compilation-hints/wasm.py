#!/usr/bin/env python

# Copyright 2019 the V8 project authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be found
# in the LICENSE file.

import io
import math
import struct
import sys

CUSTOM_SECTION_ID = 0
FUNCTION_SECTION_ID = 3

def peek_uint8(fin):
  bs = fin.peek(1)[:1]
  if len(bs) != 1:
    return None, bs
  return ord(bs[0]), bs

def read_uint8(fin):
  value, bs = peek_uint8(fin)
  fin.read(len(bs))
  return value, bs

def peek_uint32(fin):
  bs = fin.peek(4)[:4]
  if len(bs) != 4:
    return None, bs
  return ord(bs[0]) | ord(bs[1]) << 8 | ord(bs[2]) << 16 | ord(bs[3]) << 24, bs

def read_uint32(fin):
  value, bs = peek_uint32(fin)
  fin.read(len(bs))
  return value, bs

def peek_varuintN(fin):
  value = 0
  shift = 0
  n = 1
  while True:
    bs = fin.peek(n)[:n]
    if len(bs) < n:
      return None, bs
    b = ord(bs[-1])
    value |= (b & 0x7F) << shift;
    if (b & 0x80) == 0x00:
      return value, bs
    shift += 7;
    n += 1

def read_varuintN(fin):
  value, bs = peek_varuintN(fin)
  fin.read(len(bs))
  return value, bs

def to_varuintN(value):
  bs = ""
  while True:
    b = value & 0x7F
    value >>= 7
    if (value != 0x00):
      b |= 0x80
    bs += chr(b)
    if value == 0x00:
      return bs

def write_varuintN(value, fout):
  bs = to_varuintN(value)
  fout.write(bs)
  return bs

def peek_magic_number(fin, expected_magic_number=0x6d736100):
  magic_number, bs = peek_uint32(fin)
  assert magic_number == expected_magic_number, "unexpected magic number"
  return magic_number, bs

def read_magic_number(fin, expected_magic_number=0x6d736100):
  magic_number, bs = peek_magic_number(fin, expected_magic_number)
  fin.read(len(bs))
  return magic_number, bs

def peek_version(fin, expected_version=1):
  version, bs = peek_uint32(fin)
  assert version == expected_version, "unexpected version"
  return version, bs

def read_version(fin, expected_version=1):
  version, bs = peek_version(fin, expected_version)
  fin.read(len(bs))
  return version, bs

def write_custom_section(fout, section_name_bs, payload_bs):
  section_name_length_bs = to_varuintN(len(section_name_bs))
  payload_length_bs = to_varuintN(len(section_name_bs) \
      + len(section_name_length_bs) + len(payload_bs))
  section_id_bs = to_varuintN(CUSTOM_SECTION_ID)
  fout.write(section_id_bs)
  fout.write(payload_length_bs)
  fout.write(section_name_length_bs)
  fout.write(section_name_bs)
  fout.write(payload_bs)

def write_compilation_hints_section(fout, hints_bs):
  num_compilation_hints_bs = to_varuintN(len(hints_bs))
  section_name_bs = b"compilationHints"
  payload_bs = num_compilation_hints_bs + hints_bs
  write_custom_section(fout, section_name_bs, payload_bs)
