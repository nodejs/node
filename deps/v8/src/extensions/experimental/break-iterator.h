// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_EXTENSIONS_EXPERIMENTAL_BREAK_ITERATOR_H_
#define V8_EXTENSIONS_EXPERIMENTAL_BREAK_ITERATOR_H_

#include "include/v8.h"

#include "unicode/uversion.h"

namespace U_ICU_NAMESPACE {
class BreakIterator;
class UnicodeString;
}

namespace v8 {
namespace internal {

class BreakIterator {
 public:
  static v8::Handle<v8::Value> JSBreakIterator(const v8::Arguments& args);

  // Helper methods for various bindings.

  // Unpacks break iterator object from corresponding JavaScript object.
  static icu::BreakIterator* UnpackBreakIterator(v8::Handle<v8::Object> obj);

  // Deletes the old value and sets the adopted text in
  // corresponding JavaScript object.
  static icu::UnicodeString* ResetAdoptedText(v8::Handle<v8::Object> obj,
                                              v8::Handle<v8::Value> text_value);

  // Release memory we allocated for the BreakIterator once the JS object that
  // holds the pointer gets garbage collected.
  static void DeleteBreakIterator(v8::Persistent<v8::Value> object,
                                  void* param);

  // Assigns new text to the iterator.
  static v8::Handle<v8::Value> BreakIteratorAdoptText(
      const v8::Arguments& args);

  // Moves iterator to the beginning of the string and returns new position.
  static v8::Handle<v8::Value> BreakIteratorFirst(const v8::Arguments& args);

  // Moves iterator to the next position and returns it.
  static v8::Handle<v8::Value> BreakIteratorNext(const v8::Arguments& args);

  // Returns current iterator's current position.
  static v8::Handle<v8::Value> BreakIteratorCurrent(
      const v8::Arguments& args);

  // Returns type of the item from current position.
  // This call is only valid for word break iterators. Others just return 0.
  static v8::Handle<v8::Value> BreakIteratorBreakType(
      const v8::Arguments& args);

 private:
  BreakIterator() {}

  static v8::Persistent<v8::FunctionTemplate> break_iterator_template_;
};

} }  // namespace v8::internal

#endif  // V8_EXTENSIONS_EXPERIMENTAL_BREAK_ITERATOR_H_
