// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A simple interpreter for the Irregexp byte code.

#ifndef V8_INTERPRETER_IRREGEXP_H_
#define V8_INTERPRETER_IRREGEXP_H_

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


} }  // namespace v8::internal

#endif  // V8_INTERPRETER_IRREGEXP_H_
