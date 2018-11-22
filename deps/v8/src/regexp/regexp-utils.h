// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_REGEXP_REGEXP_UTILS_H_
#define V8_REGEXP_REGEXP_UTILS_H_

#include "src/objects.h"

namespace v8 {
namespace internal {

class RegExpMatchInfo;

// Helper methods for C++ regexp builtins.
class RegExpUtils : public AllStatic {
 public:
  // Last match info accessors.
  static Handle<String> GenericCaptureGetter(Isolate* isolate,
                                             Handle<RegExpMatchInfo> match_info,
                                             int capture, bool* ok = nullptr);

  // Last index (RegExp.lastIndex) accessors.
  static V8_WARN_UNUSED_RESULT MaybeHandle<Object> SetLastIndex(
      Isolate* isolate, Handle<JSReceiver> regexp, uint64_t value);
  static V8_WARN_UNUSED_RESULT MaybeHandle<Object> GetLastIndex(
      Isolate* isolate, Handle<JSReceiver> recv);

  // ES#sec-regexpexec Runtime Semantics: RegExpExec ( R, S )
  static V8_WARN_UNUSED_RESULT MaybeHandle<Object> RegExpExec(
      Isolate* isolate, Handle<JSReceiver> regexp, Handle<String> string,
      Handle<Object> exec);

  // ES#sec-isregexp IsRegExp ( argument )
  // Includes checking of the match property.
  static Maybe<bool> IsRegExp(Isolate* isolate, Handle<Object> object);

  // Checks whether the given object is an unmodified JSRegExp instance.
  // Neither the object's map, nor its prototype's map, nor any relevant
  // method on the prototype may be modified.
  static bool IsUnmodifiedRegExp(Isolate* isolate, Handle<Object> obj);

  // ES#sec-advancestringindex
  // AdvanceStringIndex ( S, index, unicode )
  static uint64_t AdvanceStringIndex(Handle<String> string, uint64_t index,
                                     bool unicode);
  static V8_WARN_UNUSED_RESULT MaybeHandle<Object> SetAdvancedStringIndex(
      Isolate* isolate, Handle<JSReceiver> regexp, Handle<String> string,
      bool unicode);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_REGEXP_REGEXP_UTILS_H_
