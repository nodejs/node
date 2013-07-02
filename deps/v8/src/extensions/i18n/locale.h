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

#ifndef V8_EXTENSIONS_I18N_SRC_LOCALE_H_
#define V8_EXTENSIONS_I18N_SRC_LOCALE_H_

#include "unicode/uversion.h"
#include "v8.h"

namespace v8_i18n {

// Canonicalizes the BCP47 language tag using BCP47 rules.
// Returns 'invalid-tag' in case input was not well formed.
void JSCanonicalizeLanguageTag(const v8::FunctionCallbackInfo<v8::Value>& args);

// Returns a list of available locales for collator, date or number formatter.
void JSAvailableLocalesOf(const v8::FunctionCallbackInfo<v8::Value>& args);

// Returns default ICU locale.
void JSGetDefaultICULocale(const v8::FunctionCallbackInfo<v8::Value>& args);

// Returns an array of objects, that have maximized and base names of inputs.
// Unicode extensions are dropped from both.
// Input: ['zh-TW-u-nu-thai', 'sr']
// Output: [{maximized: 'zh-Hant-TW', base: 'zh-TW'},
//          {maximized: 'sr-Cyrl-RS', base: 'sr'}]
void JSGetLanguageTagVariants(const v8::FunctionCallbackInfo<v8::Value>& args);

}  // namespace v8_i18n

#endif  // V8_EXTENSIONS_I18N_LOCALE_H_
