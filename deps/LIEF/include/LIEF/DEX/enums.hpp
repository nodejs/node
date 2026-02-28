/* Copyright 2017 - 2025 R. Thomas
 * Copyright 2017 - 2025 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef LIEF_DEX_ENUMS_H
#define LIEF_DEX_ENUMS_H

namespace LIEF {
namespace DEX {

enum ACCESS_FLAGS {
  ACC_UNKNOWN               = 0x0,
  ACC_PUBLIC                = 0x1,
  ACC_PRIVATE               = 0x2,
  ACC_PROTECTED             = 0x4,
  ACC_STATIC                = 0x8,
  ACC_FINAL                 = 0x10,
  ACC_SYNCHRONIZED          = 0x20,
  ACC_VOLATILE              = 0x40,
  ACC_BRIDGE                = 0x40,
  ACC_TRANSIENT             = 0x80,
  ACC_VARARGS               = 0x80,
  ACC_NATIVE                = 0x100,
  ACC_INTERFACE             = 0x200,
  ACC_ABSTRACT              = 0x400,
  ACC_STRICT                = 0x800,
  ACC_SYNTHETIC             = 0x1000,
  ACC_ANNOTATION            = 0x2000,
  ACC_ENUM                  = 0x4000,
  ACC_CONSTRUCTOR           = 0x10000,
  ACC_DECLARED_SYNCHRONIZED = 0x20000
};


enum METHOD_TYPES {
  METHOD_UNKNOWN      = 0x00,
  METHOD_VIRTUAL      = 0x01,
  METHOD_DIRECT       = 0x02, // Deprecated

  METHOD_EXTERN       = 0x03,
  METHOD_CTOR         = 0x04,
  METHOD_STATIC       = 0x05,
  METHOD_STATIC_CTOR  = 0x06,
};

static const ACCESS_FLAGS access_flags_list[] = {
  ACCESS_FLAGS::ACC_UNKNOWN,
  ACCESS_FLAGS::ACC_PUBLIC,
  ACCESS_FLAGS::ACC_PRIVATE,
  ACCESS_FLAGS::ACC_PROTECTED,
  ACCESS_FLAGS::ACC_STATIC,
  ACCESS_FLAGS::ACC_FINAL,
  ACCESS_FLAGS::ACC_SYNCHRONIZED,
  ACCESS_FLAGS::ACC_VOLATILE,
  ACCESS_FLAGS::ACC_BRIDGE,
  ACCESS_FLAGS::ACC_TRANSIENT,
  ACCESS_FLAGS::ACC_VARARGS,
  ACCESS_FLAGS::ACC_NATIVE,
  ACCESS_FLAGS::ACC_INTERFACE,
  ACCESS_FLAGS::ACC_ABSTRACT,
  ACCESS_FLAGS::ACC_STRICT,
  ACCESS_FLAGS::ACC_SYNTHETIC,
  ACCESS_FLAGS::ACC_ANNOTATION,
  ACCESS_FLAGS::ACC_ENUM,
  ACCESS_FLAGS::ACC_CONSTRUCTOR,
  ACCESS_FLAGS::ACC_DECLARED_SYNCHRONIZED,
};

}
}
#endif
