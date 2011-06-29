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

#include "src/extensions/experimental/i18n-extension.h"

#include "src/extensions/experimental/break-iterator.h"
#include "src/extensions/experimental/collator.h"
#include "src/extensions/experimental/datetime-format.h"
#include "src/extensions/experimental/i18n-locale.h"
#include "src/extensions/experimental/i18n-natives.h"
#include "src/extensions/experimental/number-format.h"

namespace v8 {
namespace internal {

I18NExtension* I18NExtension::extension_ = NULL;

I18NExtension::I18NExtension()
    : v8::Extension("v8/i18n", I18Natives::GetScriptSource()) {
}

v8::Handle<v8::FunctionTemplate> I18NExtension::GetNativeFunction(
    v8::Handle<v8::String> name) {
  if (name->Equals(v8::String::New("NativeJSLocale"))) {
    return v8::FunctionTemplate::New(I18NLocale::JSLocale);
  } else if (name->Equals(v8::String::New("NativeJSBreakIterator"))) {
    return v8::FunctionTemplate::New(BreakIterator::JSBreakIterator);
  } else if (name->Equals(v8::String::New("NativeJSCollator"))) {
    return v8::FunctionTemplate::New(Collator::JSCollator);
  } else if (name->Equals(v8::String::New("NativeJSDateTimeFormat"))) {
    return v8::FunctionTemplate::New(DateTimeFormat::JSDateTimeFormat);
  } else if (name->Equals(v8::String::New("NativeJSNumberFormat"))) {
    return v8::FunctionTemplate::New(NumberFormat::JSNumberFormat);
  }

  return v8::Handle<v8::FunctionTemplate>();
}

I18NExtension* I18NExtension::get() {
  if (!extension_) {
    extension_ = new I18NExtension();
  }
  return extension_;
}

void I18NExtension::Register() {
  static v8::DeclareExtension i18n_extension_declaration(I18NExtension::get());
}

} }  // namespace v8::internal
