// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple interpreter for the Irregexp byte code.

#ifndef V8_REGEXP_INTERPRETER_IRREGEXP_H_
#define V8_REGEXP_INTERPRETER_IRREGEXP_H_

#include "src/regexp/jsregexp.h"

namespace v8 {
namespace internal {

class V8_EXPORT_PRIVATE IrregexpInterpreter {
 public:
  enum Result { RETRY = -2, EXCEPTION = -1, FAILURE = 0, SUCCESS = 1 };
  STATIC_ASSERT(EXCEPTION == static_cast<int>(RegExpImpl::RE_EXCEPTION));
  STATIC_ASSERT(FAILURE == static_cast<int>(RegExpImpl::RE_FAILURE));
  STATIC_ASSERT(SUCCESS == static_cast<int>(RegExpImpl::RE_SUCCESS));

  // The caller is responsible for initializing registers before each call.
  static Result Match(Isolate* isolate, Handle<ByteArray> code_array,
                      Handle<String> subject_string, int* registers,
                      int start_position);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_INTERPRETER_IRREGEXP_H_
