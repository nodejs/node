// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple interpreter for the Irregexp byte code.

#ifndef V8_REGEXP_REGEXP_INTERPRETER_H_
#define V8_REGEXP_REGEXP_INTERPRETER_H_

#include "src/regexp/regexp.h"

namespace v8 {
namespace internal {

class V8_EXPORT_PRIVATE IrregexpInterpreter {
 public:
  enum Result {
    FAILURE = RegExp::kInternalRegExpFailure,
    SUCCESS = RegExp::kInternalRegExpSuccess,
    EXCEPTION = RegExp::kInternalRegExpException,
    RETRY = RegExp::kInternalRegExpRetry,
  };

  // The caller is responsible for initializing registers before each call.
  static Result Match(Isolate* isolate, Handle<ByteArray> code_array,
                      Handle<String> subject_string, int* registers,
                      int start_position);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_INTERPRETER_H_
