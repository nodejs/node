// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if V8_TARGET_ARCH_S390

#include "src/s390/constants-s390.h"

namespace v8 {
namespace internal {

Instruction::OpcodeFormatType Instruction::OpcodeFormatTable[] = {
    // Based on Figure B-3 in z/Architecture Principles of
    // Operation.
    TWO_BYTE_OPCODE,           // 0x00
    TWO_BYTE_OPCODE,           // 0x01
    TWO_BYTE_DISJOINT_OPCODE,  // 0x02
    TWO_BYTE_DISJOINT_OPCODE,  // 0x03
    ONE_BYTE_OPCODE,           // 0x04
    ONE_BYTE_OPCODE,           // 0x05
    ONE_BYTE_OPCODE,           // 0x06
    ONE_BYTE_OPCODE,           // 0x07
    ONE_BYTE_OPCODE,           // 0x08
    ONE_BYTE_OPCODE,           // 0x09
    ONE_BYTE_OPCODE,           // 0x0a
    ONE_BYTE_OPCODE,           // 0x0b
    ONE_BYTE_OPCODE,           // 0x0c
    ONE_BYTE_OPCODE,           // 0x0d
    ONE_BYTE_OPCODE,           // 0x0e
    ONE_BYTE_OPCODE,           // 0x0f
    ONE_BYTE_OPCODE,           // 0x10
    ONE_BYTE_OPCODE,           // 0x11
    ONE_BYTE_OPCODE,           // 0x12
    ONE_BYTE_OPCODE,           // 0x13
    ONE_BYTE_OPCODE,           // 0x14
    ONE_BYTE_OPCODE,           // 0x15
    ONE_BYTE_OPCODE,           // 0x16
    ONE_BYTE_OPCODE,           // 0x17
    ONE_BYTE_OPCODE,           // 0x18
    ONE_BYTE_OPCODE,           // 0x19
    ONE_BYTE_OPCODE,           // 0x1a
    ONE_BYTE_OPCODE,           // 0x1b
    ONE_BYTE_OPCODE,           // 0x1c
    ONE_BYTE_OPCODE,           // 0x1d
    ONE_BYTE_OPCODE,           // 0x1e
    ONE_BYTE_OPCODE,           // 0x1f
    ONE_BYTE_OPCODE,           // 0x20
    ONE_BYTE_OPCODE,           // 0x21
    ONE_BYTE_OPCODE,           // 0x22
    ONE_BYTE_OPCODE,           // 0x23
    ONE_BYTE_OPCODE,           // 0x24
    ONE_BYTE_OPCODE,           // 0x25
    ONE_BYTE_OPCODE,           // 0x26
    ONE_BYTE_OPCODE,           // 0x27
    ONE_BYTE_OPCODE,           // 0x28
    ONE_BYTE_OPCODE,           // 0x29
    ONE_BYTE_OPCODE,           // 0x2a
    ONE_BYTE_OPCODE,           // 0x2b
    ONE_BYTE_OPCODE,           // 0x2c
    ONE_BYTE_OPCODE,           // 0x2d
    ONE_BYTE_OPCODE,           // 0x2e
    ONE_BYTE_OPCODE,           // 0x2f
    ONE_BYTE_OPCODE,           // 0x30
    ONE_BYTE_OPCODE,           // 0x31
    ONE_BYTE_OPCODE,           // 0x32
    ONE_BYTE_OPCODE,           // 0x33
    ONE_BYTE_OPCODE,           // 0x34
    ONE_BYTE_OPCODE,           // 0x35
    ONE_BYTE_OPCODE,           // 0x36
    ONE_BYTE_OPCODE,           // 0x37
    ONE_BYTE_OPCODE,           // 0x38
    ONE_BYTE_OPCODE,           // 0x39
    ONE_BYTE_OPCODE,           // 0x3a
    ONE_BYTE_OPCODE,           // 0x3b
    ONE_BYTE_OPCODE,           // 0x3c
    ONE_BYTE_OPCODE,           // 0x3d
    ONE_BYTE_OPCODE,           // 0x3e
    ONE_BYTE_OPCODE,           // 0x3f
    ONE_BYTE_OPCODE,           // 0x40
    ONE_BYTE_OPCODE,           // 0x41
    ONE_BYTE_OPCODE,           // 0x42
    ONE_BYTE_OPCODE,           // 0x43
    ONE_BYTE_OPCODE,           // 0x44
    ONE_BYTE_OPCODE,           // 0x45
    ONE_BYTE_OPCODE,           // 0x46
    ONE_BYTE_OPCODE,           // 0x47
    ONE_BYTE_OPCODE,           // 0x48
    ONE_BYTE_OPCODE,           // 0x49
    ONE_BYTE_OPCODE,           // 0x4a
    ONE_BYTE_OPCODE,           // 0x4b
    ONE_BYTE_OPCODE,           // 0x4c
    ONE_BYTE_OPCODE,           // 0x4d
    ONE_BYTE_OPCODE,           // 0x4e
    ONE_BYTE_OPCODE,           // 0x4f
    ONE_BYTE_OPCODE,           // 0x50
    ONE_BYTE_OPCODE,           // 0x51
    ONE_BYTE_OPCODE,           // 0x52
    ONE_BYTE_OPCODE,           // 0x53
    ONE_BYTE_OPCODE,           // 0x54
    ONE_BYTE_OPCODE,           // 0x55
    ONE_BYTE_OPCODE,           // 0x56
    ONE_BYTE_OPCODE,           // 0x57
    ONE_BYTE_OPCODE,           // 0x58
    ONE_BYTE_OPCODE,           // 0x59
    ONE_BYTE_OPCODE,           // 0x5a
    ONE_BYTE_OPCODE,           // 0x5b
    ONE_BYTE_OPCODE,           // 0x5c
    ONE_BYTE_OPCODE,           // 0x5d
    ONE_BYTE_OPCODE,           // 0x5e
    ONE_BYTE_OPCODE,           // 0x5f
    ONE_BYTE_OPCODE,           // 0x60
    ONE_BYTE_OPCODE,           // 0x61
    ONE_BYTE_OPCODE,           // 0x62
    ONE_BYTE_OPCODE,           // 0x63
    ONE_BYTE_OPCODE,           // 0x64
    ONE_BYTE_OPCODE,           // 0x65
    ONE_BYTE_OPCODE,           // 0x66
    ONE_BYTE_OPCODE,           // 0x67
    ONE_BYTE_OPCODE,           // 0x68
    ONE_BYTE_OPCODE,           // 0x69
    ONE_BYTE_OPCODE,           // 0x6a
    ONE_BYTE_OPCODE,           // 0x6b
    ONE_BYTE_OPCODE,           // 0x6c
    ONE_BYTE_OPCODE,           // 0x6d
    ONE_BYTE_OPCODE,           // 0x6e
    ONE_BYTE_OPCODE,           // 0x6f
    ONE_BYTE_OPCODE,           // 0x70
    ONE_BYTE_OPCODE,           // 0x71
    ONE_BYTE_OPCODE,           // 0x72
    ONE_BYTE_OPCODE,           // 0x73
    ONE_BYTE_OPCODE,           // 0x74
    ONE_BYTE_OPCODE,           // 0x75
    ONE_BYTE_OPCODE,           // 0x76
    ONE_BYTE_OPCODE,           // 0x77
    ONE_BYTE_OPCODE,           // 0x78
    ONE_BYTE_OPCODE,           // 0x79
    ONE_BYTE_OPCODE,           // 0x7a
    ONE_BYTE_OPCODE,           // 0x7b
    ONE_BYTE_OPCODE,           // 0x7c
    ONE_BYTE_OPCODE,           // 0x7d
    ONE_BYTE_OPCODE,           // 0x7e
    ONE_BYTE_OPCODE,           // 0x7f
    ONE_BYTE_OPCODE,           // 0x80
    ONE_BYTE_OPCODE,           // 0x81
    ONE_BYTE_OPCODE,           // 0x82
    ONE_BYTE_OPCODE,           // 0x83
    ONE_BYTE_OPCODE,           // 0x84
    ONE_BYTE_OPCODE,           // 0x85
    ONE_BYTE_OPCODE,           // 0x86
    ONE_BYTE_OPCODE,           // 0x87
    ONE_BYTE_OPCODE,           // 0x88
    ONE_BYTE_OPCODE,           // 0x89
    ONE_BYTE_OPCODE,           // 0x8a
    ONE_BYTE_OPCODE,           // 0x8b
    ONE_BYTE_OPCODE,           // 0x8c
    ONE_BYTE_OPCODE,           // 0x8d
    ONE_BYTE_OPCODE,           // 0x8e
    ONE_BYTE_OPCODE,           // 0x8f
    ONE_BYTE_OPCODE,           // 0x90
    ONE_BYTE_OPCODE,           // 0x91
    ONE_BYTE_OPCODE,           // 0x92
    ONE_BYTE_OPCODE,           // 0x93
    ONE_BYTE_OPCODE,           // 0x94
    ONE_BYTE_OPCODE,           // 0x95
    ONE_BYTE_OPCODE,           // 0x96
    ONE_BYTE_OPCODE,           // 0x97
    ONE_BYTE_OPCODE,           // 0x98
    ONE_BYTE_OPCODE,           // 0x99
    ONE_BYTE_OPCODE,           // 0x9a
    ONE_BYTE_OPCODE,           // 0x9b
    TWO_BYTE_DISJOINT_OPCODE,  // 0x9c
    TWO_BYTE_DISJOINT_OPCODE,  // 0x9d
    TWO_BYTE_DISJOINT_OPCODE,  // 0x9e
    TWO_BYTE_DISJOINT_OPCODE,  // 0x9f
    TWO_BYTE_DISJOINT_OPCODE,  // 0xa0
    TWO_BYTE_DISJOINT_OPCODE,  // 0xa1
    TWO_BYTE_DISJOINT_OPCODE,  // 0xa2
    TWO_BYTE_DISJOINT_OPCODE,  // 0xa3
    TWO_BYTE_DISJOINT_OPCODE,  // 0xa4
    THREE_NIBBLE_OPCODE,       // 0xa5
    TWO_BYTE_DISJOINT_OPCODE,  // 0xa6
    THREE_NIBBLE_OPCODE,       // 0xa7
    ONE_BYTE_OPCODE,           // 0xa8
    ONE_BYTE_OPCODE,           // 0xa9
    ONE_BYTE_OPCODE,           // 0xaa
    ONE_BYTE_OPCODE,           // 0xab
    ONE_BYTE_OPCODE,           // 0xac
    ONE_BYTE_OPCODE,           // 0xad
    ONE_BYTE_OPCODE,           // 0xae
    ONE_BYTE_OPCODE,           // 0xaf
    ONE_BYTE_OPCODE,           // 0xb0
    ONE_BYTE_OPCODE,           // 0xb1
    TWO_BYTE_OPCODE,           // 0xb2
    TWO_BYTE_OPCODE,           // 0xb3
    TWO_BYTE_DISJOINT_OPCODE,  // 0xb4
    TWO_BYTE_DISJOINT_OPCODE,  // 0xb5
    TWO_BYTE_DISJOINT_OPCODE,  // 0xb6
    TWO_BYTE_DISJOINT_OPCODE,  // 0xb7
    TWO_BYTE_DISJOINT_OPCODE,  // 0xb8
    TWO_BYTE_OPCODE,           // 0xb9
    ONE_BYTE_OPCODE,           // 0xba
    ONE_BYTE_OPCODE,           // 0xbb
    ONE_BYTE_OPCODE,           // 0xbc
    ONE_BYTE_OPCODE,           // 0xbd
    ONE_BYTE_OPCODE,           // 0xbe
    ONE_BYTE_OPCODE,           // 0xbf
    THREE_NIBBLE_OPCODE,       // 0xc0
    THREE_NIBBLE_OPCODE,       // 0xc1
    THREE_NIBBLE_OPCODE,       // 0xc2
    THREE_NIBBLE_OPCODE,       // 0xc3
    THREE_NIBBLE_OPCODE,       // 0xc4
    THREE_NIBBLE_OPCODE,       // 0xc5
    THREE_NIBBLE_OPCODE,       // 0xc6
    ONE_BYTE_OPCODE,           // 0xc7
    THREE_NIBBLE_OPCODE,       // 0xc8
    THREE_NIBBLE_OPCODE,       // 0xc9
    THREE_NIBBLE_OPCODE,       // 0xca
    THREE_NIBBLE_OPCODE,       // 0xcb
    THREE_NIBBLE_OPCODE,       // 0xcc
    TWO_BYTE_DISJOINT_OPCODE,  // 0xcd
    TWO_BYTE_DISJOINT_OPCODE,  // 0xce
    TWO_BYTE_DISJOINT_OPCODE,  // 0xcf
    ONE_BYTE_OPCODE,           // 0xd0
    ONE_BYTE_OPCODE,           // 0xd1
    ONE_BYTE_OPCODE,           // 0xd2
    ONE_BYTE_OPCODE,           // 0xd3
    ONE_BYTE_OPCODE,           // 0xd4
    ONE_BYTE_OPCODE,           // 0xd5
    ONE_BYTE_OPCODE,           // 0xd6
    ONE_BYTE_OPCODE,           // 0xd7
    ONE_BYTE_OPCODE,           // 0xd8
    ONE_BYTE_OPCODE,           // 0xd9
    ONE_BYTE_OPCODE,           // 0xda
    ONE_BYTE_OPCODE,           // 0xdb
    ONE_BYTE_OPCODE,           // 0xdc
    ONE_BYTE_OPCODE,           // 0xdd
    ONE_BYTE_OPCODE,           // 0xde
    ONE_BYTE_OPCODE,           // 0xdf
    ONE_BYTE_OPCODE,           // 0xe0
    ONE_BYTE_OPCODE,           // 0xe1
    ONE_BYTE_OPCODE,           // 0xe2
    TWO_BYTE_DISJOINT_OPCODE,  // 0xe3
    TWO_BYTE_DISJOINT_OPCODE,  // 0xe4
    TWO_BYTE_OPCODE,           // 0xe5
    TWO_BYTE_DISJOINT_OPCODE,  // 0xe6
    TWO_BYTE_DISJOINT_OPCODE,  // 0xe7
    ONE_BYTE_OPCODE,           // 0xe8
    ONE_BYTE_OPCODE,           // 0xe9
    ONE_BYTE_OPCODE,           // 0xea
    TWO_BYTE_DISJOINT_OPCODE,  // 0xeb
    TWO_BYTE_DISJOINT_OPCODE,  // 0xec
    TWO_BYTE_DISJOINT_OPCODE,  // 0xed
    ONE_BYTE_OPCODE,           // 0xee
    ONE_BYTE_OPCODE,           // 0xef
    ONE_BYTE_OPCODE,           // 0xf0
    ONE_BYTE_OPCODE,           // 0xf1
    ONE_BYTE_OPCODE,           // 0xf2
    ONE_BYTE_OPCODE,           // 0xf3
    ONE_BYTE_OPCODE,           // 0xf4
    ONE_BYTE_OPCODE,           // 0xf5
    ONE_BYTE_OPCODE,           // 0xf6
    ONE_BYTE_OPCODE,           // 0xf7
    ONE_BYTE_OPCODE,           // 0xf8
    ONE_BYTE_OPCODE,           // 0xf9
    ONE_BYTE_OPCODE,           // 0xfa
    ONE_BYTE_OPCODE,           // 0xfb
    ONE_BYTE_OPCODE,           // 0xfc
    ONE_BYTE_OPCODE,           // 0xfd
    TWO_BYTE_DISJOINT_OPCODE,  // 0xfe
    TWO_BYTE_DISJOINT_OPCODE,  // 0xff
};

// These register names are defined in a way to match the native disassembler
// formatting. See for example the command "objdump -d <binary file>".
const char* Registers::names_[kNumRegisters] = {
    "r0", "r1", "r2",  "r3", "r4", "r5",  "r6",  "r7",
    "r8", "r9", "r10", "fp", "ip", "r13", "r14", "sp"};

const char* DoubleRegisters::names_[kNumDoubleRegisters] = {
    "f0", "f1", "f2",  "f3",  "f4",  "f5",  "f6",  "f7",
    "f8", "f9", "f10", "f11", "f12", "f13", "f14", "f15"};

int DoubleRegisters::Number(const char* name) {
  for (int i = 0; i < kNumDoubleRegisters; i++) {
    if (strcmp(names_[i], name) == 0) {
      return i;
    }
  }

  // No register with the requested name found.
  return kNoRegister;
}

int Registers::Number(const char* name) {
  // Look through the canonical names.
  for (int i = 0; i < kNumRegisters; i++) {
    if (strcmp(names_[i], name) == 0) {
      return i;
    }
  }

  // No register with the requested name found.
  return kNoRegister;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_TARGET_ARCH_S390
