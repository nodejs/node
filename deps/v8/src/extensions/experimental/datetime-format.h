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

#ifndef V8_EXTENSIONS_EXPERIMENTAL_DATETIME_FORMAT_H_
#define V8_EXTENSIONS_EXPERIMENTAL_DATETIME_FORMAT_H_

#include "include/v8.h"

#include "unicode/uversion.h"

namespace U_ICU_NAMESPACE {
class SimpleDateFormat;
}

namespace v8 {
namespace internal {

class DateTimeFormat {
 public:
  static v8::Handle<v8::Value> JSDateTimeFormat(const v8::Arguments& args);

  // Helper methods for various bindings.

  // Unpacks date format object from corresponding JavaScript object.
  static icu::SimpleDateFormat* UnpackDateTimeFormat(
      v8::Handle<v8::Object> obj);

  // Release memory we allocated for the DateFormat once the JS object that
  // holds the pointer gets garbage collected.
  static void DeleteDateTimeFormat(v8::Persistent<v8::Value> object,
                                   void* param);

  // Formats date and returns corresponding string.
  static v8::Handle<v8::Value> Format(const v8::Arguments& args);

  // All date time symbol methods below return stand-alone names in
  // either narrow, abbreviated or wide width.

  // Get list of months.
  static v8::Handle<v8::Value> GetMonths(const v8::Arguments& args);

  // Get list of weekdays.
  static v8::Handle<v8::Value> GetWeekdays(const v8::Arguments& args);

  // Get list of eras.
  static v8::Handle<v8::Value> GetEras(const v8::Arguments& args);

  // Get list of day periods.
  static v8::Handle<v8::Value> GetAmPm(const v8::Arguments& args);

 private:
  DateTimeFormat();

  static v8::Persistent<v8::FunctionTemplate> datetime_format_template_;
};

} }  // namespace v8::internal

#endif  // V8_EXTENSIONS_EXPERIMENTAL_DATETIME_FORMAT_H_
