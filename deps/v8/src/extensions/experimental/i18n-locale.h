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

#ifndef V8_EXTENSIONS_EXPERIMENTAL_I18N_LOCALE_H_
#define V8_EXTENSIONS_EXPERIMENTAL_I18N_LOCALE_H_

#include "include/v8.h"

namespace v8 {
namespace internal {

class I18NLocale {
 public:
  I18NLocale() {}

  // Implementations of window.Locale methods.
  static v8::Handle<v8::Value> JSLocale(const v8::Arguments& args);

  // Infers region id given the locale id, or uses user specified region id.
  // Result is canonicalized.
  // Returns status of ICU operation (maximizing locale or get region call).
  static bool GetBestMatchForRegionID(
      const char* locale_id, v8::Handle<v8::Value> regions, char* result);

 private:
  // Key name for localeID parameter.
  static const char* const kLocaleID;
  // Key name for regionID parameter.
  static const char* const kRegionID;
  // Key name for the icuLocaleID result.
  static const char* const kICULocaleID;
};

} }  // namespace v8::internal

#endif  // V8_EXTENSIONS_EXPERIMENTAL_I18N_LOCALE_H_
