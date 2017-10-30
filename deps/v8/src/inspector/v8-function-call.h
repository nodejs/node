/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef V8_INSPECTOR_V8FUNCTIONCALL_H_
#define V8_INSPECTOR_V8FUNCTIONCALL_H_

#include "src/inspector/string-16.h"

#include "include/v8.h"

namespace v8_inspector {

class V8InspectorImpl;

class V8FunctionCall {
 public:
  V8FunctionCall(V8InspectorImpl*, v8::Local<v8::Context>, v8::Local<v8::Value>,
                 const String16& name);

  void appendArgument(v8::Local<v8::Value>);
  void appendArgument(const String16&);
  void appendArgument(int);
  void appendArgument(bool);

  v8::Local<v8::Value> call(bool& hadException, bool reportExceptions = true);
  v8::Local<v8::Value> callWithoutExceptionHandling();

 protected:
  V8InspectorImpl* m_inspector;
  v8::Local<v8::Context> m_context;
  std::vector<v8::Local<v8::Value>> m_arguments;
  v8::Local<v8::String> m_name;
  v8::Local<v8::Value> m_value;
};

}  // namespace v8_inspector

#endif  // V8_INSPECTOR_V8FUNCTIONCALL_H_
