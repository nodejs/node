// Copyright 2013 the V8 project authors. All rights reserved.
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
// limitations under the License.

#include "i18n-extension.h"

#include "break-iterator.h"
#include "collator.h"
#include "natives.h"
#include "number-format.h"

using v8::internal::I18NNatives;

namespace v8_i18n {

Extension::Extension()
    : v8::Extension("v8/i18n",
                    reinterpret_cast<const char*>(
                        I18NNatives::GetScriptsSource().start()),
                    0,
                    0,
                    I18NNatives::GetScriptsSource().length()) {}

v8::Handle<v8::FunctionTemplate> Extension::GetNativeFunction(
    v8::Handle<v8::String> name) {
  // Number format and parse.
  if (name->Equals(v8::String::New("NativeJSCreateNumberFormat"))) {
    return v8::FunctionTemplate::New(NumberFormat::JSCreateNumberFormat);
  } else if (name->Equals(v8::String::New("NativeJSInternalNumberFormat"))) {
    return v8::FunctionTemplate::New(NumberFormat::JSInternalFormat);
  } else if (name->Equals(v8::String::New("NativeJSInternalNumberParse"))) {
    return v8::FunctionTemplate::New(NumberFormat::JSInternalParse);
  }

  // Collator.
  if (name->Equals(v8::String::New("NativeJSCreateCollator"))) {
    return v8::FunctionTemplate::New(Collator::JSCreateCollator);
  } else if (name->Equals(v8::String::New("NativeJSInternalCompare"))) {
    return v8::FunctionTemplate::New(Collator::JSInternalCompare);
  }

  // Break iterator.
  if (name->Equals(v8::String::New("NativeJSCreateBreakIterator"))) {
    return v8::FunctionTemplate::New(BreakIterator::JSCreateBreakIterator);
  } else if (name->Equals(v8::String::New("NativeJSBreakIteratorAdoptText"))) {
    return v8::FunctionTemplate::New(
        BreakIterator::JSInternalBreakIteratorAdoptText);
  } else if (name->Equals(v8::String::New("NativeJSBreakIteratorFirst"))) {
    return v8::FunctionTemplate::New(
        BreakIterator::JSInternalBreakIteratorFirst);
  } else if (name->Equals(v8::String::New("NativeJSBreakIteratorNext"))) {
    return v8::FunctionTemplate::New(
        BreakIterator::JSInternalBreakIteratorNext);
  } else if (name->Equals(v8::String::New("NativeJSBreakIteratorCurrent"))) {
    return v8::FunctionTemplate::New(
        BreakIterator::JSInternalBreakIteratorCurrent);
  } else if (name->Equals(v8::String::New("NativeJSBreakIteratorBreakType"))) {
    return v8::FunctionTemplate::New(
        BreakIterator::JSInternalBreakIteratorBreakType);
  }

  return v8::Handle<v8::FunctionTemplate>();
}


void Extension::Register() {
  static Extension i18n_extension;
  static v8::DeclareExtension extension_declaration(&i18n_extension);
}

}  // namespace v8_i18n
