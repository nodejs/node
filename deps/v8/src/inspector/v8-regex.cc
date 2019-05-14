// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/inspector/v8-regex.h"

#include <limits.h>

#include "src/inspector/string-util.h"
#include "src/inspector/v8-inspector-impl.h"

#include "include/v8-inspector.h"

namespace v8_inspector {

V8Regex::V8Regex(V8InspectorImpl* inspector, const String16& pattern,
                 bool caseSensitive, bool multiline)
    : m_inspector(inspector) {
  v8::Isolate* isolate = m_inspector->isolate();
  v8::HandleScope handleScope(isolate);
  v8::Local<v8::Context> context = m_inspector->regexContext();
  v8::Context::Scope contextScope(context);
  v8::TryCatch tryCatch(isolate);

  unsigned flags = v8::RegExp::kNone;
  if (!caseSensitive) flags |= v8::RegExp::kIgnoreCase;
  if (multiline) flags |= v8::RegExp::kMultiline;

  v8::Local<v8::RegExp> regex;
  if (v8::RegExp::New(context, toV8String(isolate, pattern),
                      static_cast<v8::RegExp::Flags>(flags))
          .ToLocal(&regex))
    m_regex.Reset(isolate, regex);
  else if (tryCatch.HasCaught())
    m_errorMessage = toProtocolString(isolate, tryCatch.Message()->Get());
  else
    m_errorMessage = "Internal error";
}

int V8Regex::match(const String16& string, int startFrom,
                   int* matchLength) const {
  if (matchLength) *matchLength = 0;

  if (m_regex.IsEmpty() || string.isEmpty()) return -1;

  // v8 strings are limited to int.
  if (string.length() > INT_MAX) return -1;

  v8::Isolate* isolate = m_inspector->isolate();
  v8::HandleScope handleScope(isolate);
  v8::Local<v8::Context> context = m_inspector->regexContext();
  v8::Context::Scope contextScope(context);
  v8::MicrotasksScope microtasks(isolate,
                                 v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::TryCatch tryCatch(isolate);

  v8::Local<v8::RegExp> regex = m_regex.Get(isolate);
  v8::Local<v8::Value> exec;
  if (!regex->Get(context, toV8StringInternalized(isolate, "exec"))
           .ToLocal(&exec))
    return -1;
  v8::Local<v8::Value> argv[] = {
      toV8String(isolate, string.substring(startFrom))};
  v8::Local<v8::Value> returnValue;
  if (!exec.As<v8::Function>()
           ->Call(context, regex, arraysize(argv), argv)
           .ToLocal(&returnValue))
    return -1;

  // RegExp#exec returns null if there's no match, otherwise it returns an
  // Array of strings with the first being the whole match string and others
  // being subgroups. The Array also has some random properties tacked on like
  // "index" which is the offset of the match.
  //
  // https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/RegExp/exec

  DCHECK(!returnValue.IsEmpty());
  if (!returnValue->IsArray()) return -1;

  v8::Local<v8::Array> result = returnValue.As<v8::Array>();
  v8::Local<v8::Value> matchOffset;
  if (!result->Get(context, toV8StringInternalized(isolate, "index"))
           .ToLocal(&matchOffset))
    return -1;
  if (matchLength) {
    v8::Local<v8::Value> match;
    if (!result->Get(context, 0).ToLocal(&match)) return -1;
    *matchLength = match.As<v8::String>()->Length();
  }

  return matchOffset.As<v8::Int32>()->Value() + startFrom;
}

}  // namespace v8_inspector
