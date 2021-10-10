// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_EXPERIMENTAL_EXPERIMENTAL_COMPILER_H_
#define V8_REGEXP_EXPERIMENTAL_EXPERIMENTAL_COMPILER_H_

#include "src/regexp/experimental/experimental-bytecode.h"
#include "src/regexp/regexp-ast.h"
#include "src/regexp/regexp-flags.h"
#include "src/zone/zone-list.h"

namespace v8 {
namespace internal {

class ExperimentalRegExpCompiler final : public AllStatic {
 public:
  // Checks whether a given RegExpTree can be compiled into an experimental
  // bytecode program.  This mostly amounts to the absence of back references,
  // but see the definition.
  // TODO(mbid,v8:10765): Currently more things are not handled, e.g. some
  // quantifiers and unicode.
  static bool CanBeHandled(RegExpTree* tree, RegExpFlags flags,
                           int capture_count);
  // Compile regexp into a bytecode program.  The regexp must be handlable by
  // the experimental engine; see`CanBeHandled`.  The program is returned as a
  // ZoneList backed by the same Zone that is used in the RegExpTree argument.
  static ZoneList<RegExpInstruction> Compile(RegExpTree* tree,
                                             RegExpFlags flags, Zone* zone);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_EXPERIMENTAL_EXPERIMENTAL_COMPILER_H_
