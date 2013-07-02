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

#ifndef V8_EXTENSIONS_I18N_NUMBER_FORMAT_H_
#define V8_EXTENSIONS_I18N_NUMBER_FORMAT_H_

#include "unicode/uversion.h"
#include "v8.h"

namespace U_ICU_NAMESPACE {
class DecimalFormat;
}

namespace v8_i18n {

class NumberFormat {
 public:
  static void JSCreateNumberFormat(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Helper methods for various bindings.

  // Unpacks date format object from corresponding JavaScript object.
  static icu::DecimalFormat* UnpackNumberFormat(v8::Handle<v8::Object> obj);

  // Release memory we allocated for the NumberFormat once the JS object that
  // holds the pointer gets garbage collected.
  static void DeleteNumberFormat(v8::Isolate* isolate,
                                 v8::Persistent<v8::Object>* object,
                                 void* param);

  // Formats number and returns corresponding string.
  static void JSInternalFormat(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Parses a string and returns a number.
  static void JSInternalParse(const v8::FunctionCallbackInfo<v8::Value>& args);

 private:
  NumberFormat();
};

}  // namespace v8_i18n

#endif  // V8_EXTENSIONS_I18N_NUMBER_FORMAT_H_
