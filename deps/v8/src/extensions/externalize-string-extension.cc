// Copyright 2010 the V8 project authors. All rights reserved.
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

#include "externalize-string-extension.h"

namespace v8 {
namespace internal {

template <typename Char, typename Base>
class SimpleStringResource : public Base {
 public:
  // Takes ownership of |data|.
  SimpleStringResource(Char* data, size_t length)
      : data_(data),
        length_(length) {}

  virtual ~SimpleStringResource() { delete[] data_; }

  virtual const Char* data() const { return data_; }

  virtual size_t length() const { return length_; }

 private:
  Char* const data_;
  const size_t length_;
};


typedef SimpleStringResource<char, v8::String::ExternalAsciiStringResource>
    SimpleAsciiStringResource;
typedef SimpleStringResource<uc16, v8::String::ExternalStringResource>
    SimpleTwoByteStringResource;


const char* const ExternalizeStringExtension::kSource =
    "native function externalizeString();"
    "native function isAsciiString();";


v8::Handle<v8::FunctionTemplate> ExternalizeStringExtension::GetNativeFunction(
    v8::Handle<v8::String> str) {
  if (strcmp(*v8::String::AsciiValue(str), "externalizeString") == 0) {
    return v8::FunctionTemplate::New(ExternalizeStringExtension::Externalize);
  } else {
    ASSERT(strcmp(*v8::String::AsciiValue(str), "isAsciiString") == 0);
    return v8::FunctionTemplate::New(ExternalizeStringExtension::IsAscii);
  }
}


v8::Handle<v8::Value> ExternalizeStringExtension::Externalize(
    const v8::Arguments& args) {
  if (args.Length() < 1 || !args[0]->IsString()) {
    return v8::ThrowException(v8::String::New(
        "First parameter to externalizeString() must be a string."));
  }
  bool force_two_byte = false;
  if (args.Length() >= 2) {
    if (args[1]->IsBoolean()) {
      force_two_byte = args[1]->BooleanValue();
    } else {
      return v8::ThrowException(v8::String::New(
          "Second parameter to externalizeString() must be a boolean."));
    }
  }
  bool result = false;
  Handle<String> string = Utils::OpenHandle(*args[0].As<v8::String>());
  if (string->IsExternalString()) {
    return v8::ThrowException(v8::String::New(
        "externalizeString() can't externalize twice."));
  }
  if (string->IsOneByteRepresentation() && !force_two_byte) {
    char* data = new char[string->length()];
    String::WriteToFlat(*string, data, 0, string->length());
    SimpleAsciiStringResource* resource = new SimpleAsciiStringResource(
        data, string->length());
    result = string->MakeExternal(resource);
    if (result && !string->IsSymbol()) {
      HEAP->external_string_table()->AddString(*string);
    }
    if (!result) delete resource;
  } else {
    uc16* data = new uc16[string->length()];
    String::WriteToFlat(*string, data, 0, string->length());
    SimpleTwoByteStringResource* resource = new SimpleTwoByteStringResource(
        data, string->length());
    result = string->MakeExternal(resource);
    if (result && !string->IsSymbol()) {
      HEAP->external_string_table()->AddString(*string);
    }
    if (!result) delete resource;
  }
  if (!result) {
    return v8::ThrowException(v8::String::New("externalizeString() failed."));
  }
  return v8::Undefined();
}


v8::Handle<v8::Value> ExternalizeStringExtension::IsAscii(
    const v8::Arguments& args) {
  if (args.Length() != 1 || !args[0]->IsString()) {
    return v8::ThrowException(v8::String::New(
        "isAsciiString() requires a single string argument."));
  }
  return
      Utils::OpenHandle(*args[0].As<v8::String>())->IsOneByteRepresentation() ?
      v8::True() : v8::False();
}


void ExternalizeStringExtension::Register() {
  static ExternalizeStringExtension externalize_extension;
  static v8::DeclareExtension declaration(&externalize_extension);
}

} }  // namespace v8::internal
