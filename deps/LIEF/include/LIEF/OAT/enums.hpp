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
#ifndef LIEF_OAT_ENUMS_H
#define LIEF_OAT_ENUMS_H

namespace LIEF {
namespace OAT {

enum OAT_CLASS_TYPES {
  OAT_CLASS_ALL_COMPILED  = 0, /// OatClass is followed by an OatMethodOffsets for each method.
  OAT_CLASS_SOME_COMPILED = 1, /// A bitmap of which OatMethodOffsets are present follows the OatClass.
  OAT_CLASS_NONE_COMPILED = 2, /// All methods are interpreted so no OatMethodOffsets are necessary.
};

// From art/runtime/mirror/class.h
enum OAT_CLASS_STATUS {
  STATUS_RETIRED                       = -2,  // Retired, should not be used. Use the newly cloned one instead.
  STATUS_ERROR                         = -1,
  STATUS_NOTREADY                      = 0,
  STATUS_IDX                           = 1,  // Loaded, DEX idx in super_class_type_idx_ and interfaces_type_idx_.
  STATUS_LOADED                        = 2,  // DEX idx values resolved.
  STATUS_RESOLVING                     = 3,  // Just cloned from temporary class object.
  STATUS_RESOLVED                      = 4,  // Part of linking.
  STATUS_VERIFYING                     = 5,  // In the process of being verified.
  STATUS_RETRY_VERIFICATION_AT_RUNTIME = 6,  // Compile time verification failed, retry at runtime.
  STATUS_VERIFYING_AT_RUNTIME          = 7,  // Retrying verification at runtime.
  STATUS_VERIFIED                      = 8,  // Logically part of linking; done pre-init.
  STATUS_INITIALIZING                  = 9,  // Class init in progress.
  STATUS_INITIALIZED                   = 10, // Ready to go.
};

enum HEADER_KEYS {
  KEY_IMAGE_LOCATION     = 0,
  KEY_DEX2OAT_CMD_LINE   = 1,
  KEY_DEX2OAT_HOST       = 2,
  KEY_PIC                = 3,
  KEY_HAS_PATCH_INFO     = 4,
  KEY_DEBUGGABLE         = 5,
  KEY_NATIVE_DEBUGGABLE  = 6,
  KEY_COMPILER_FILTER    = 7,
  KEY_CLASS_PATH         = 8,
  KEY_BOOT_CLASS_PATH    = 9,
  KEY_CONCURRENT_COPYING = 10,
  KE_COMPILATION_REASON  = 11,
};

enum INSTRUCTION_SETS {
  INST_SET_NONE    = 0,
  INST_SET_ARM     = 1,
  INST_SET_ARM_64  = 2,
  INST_SET_THUMB2  = 3,
  INST_SET_X86     = 4,
  INST_SET_X86_64  = 5,
  INST_SET_MIPS    = 6,
  INST_SET_MIPS_64 = 7,
};



static const HEADER_KEYS header_keys_list[] = {
  HEADER_KEYS::KEY_IMAGE_LOCATION     ,
  HEADER_KEYS::KEY_DEX2OAT_CMD_LINE   ,
  HEADER_KEYS::KEY_DEX2OAT_HOST       ,
  HEADER_KEYS::KEY_PIC                ,
  HEADER_KEYS::KEY_HAS_PATCH_INFO     ,
  HEADER_KEYS::KEY_DEBUGGABLE         ,
  HEADER_KEYS::KEY_NATIVE_DEBUGGABLE  ,
  HEADER_KEYS::KEY_COMPILER_FILTER    ,
  HEADER_KEYS::KEY_CLASS_PATH         ,
  HEADER_KEYS::KEY_BOOT_CLASS_PATH    ,
  HEADER_KEYS::KEY_CONCURRENT_COPYING ,
  HEADER_KEYS::KE_COMPILATION_REASON  ,
};

}
}
#endif
