// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple interpreter for the Irregexp byte code.

#ifndef V8_REGEXP_INTERPRETER_IRREGEXP_H_
#define V8_REGEXP_INTERPRETER_IRREGEXP_H_

#include "src/regexp/jsregexp.h"

namespace v8 {
namespace internal {


class IrregexpInterpreter {
 public:
  static RegExpImpl::IrregexpResult Match(Isolate* isolate,
                                          Handle<ByteArray> code,
                                          Handle<String> subject,
                                          int* captures,
                                          int start_position);
};


}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_INTERPRETER_IRREGEXP_H_
