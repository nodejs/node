// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/extensions/externalize-string-extension.h"

#include "include/v8-template.h"
#include "src/api/api-inl.h"
#include "src/base/strings.h"
#include "src/execution/isolate.h"
#include "src/handles/handles.h"
#include "src/heap/heap-layout-inl.h"
#include "src/objects/heap-object-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

template <typename Char, typename Base>
class SimpleStringResource : public Base {
 public:
  // Takes ownership of |data|.
  SimpleStringResource(Char* data, size_t length)
      : data_(data),
        length_(length) {}

  ~SimpleStringResource() override { delete[] data_; }

  const Char* data() const override { return data_; }

  size_t length() const override { return length_; }

 private:
  Char* const data_;
  const size_t length_;
};

using SimpleOneByteStringResource =
    SimpleStringResource<char, v8::String::ExternalOneByteStringResource>;
using SimpleTwoByteStringResource =
    SimpleStringResource<base::uc16, v8::String::ExternalStringResource>;

static constexpr int kMinOneByteLength =
    kExternalPointerSlotSize - kTaggedSize + 1;
static constexpr int kMinTwoByteLength =
    (kExternalPointerSlotSize - kTaggedSize) / sizeof(base::uc16) + 1;
static constexpr int kMinOneByteCachedLength =
    2 * kExternalPointerSlotSize - kTaggedSize + 1;
static constexpr int kMinTwoByteCachedLength =
    (2 * kExternalPointerSlotSize - kTaggedSize) / sizeof(base::uc16) + 1;

// static
const char* ExternalizeStringExtension::BuildSource(char* buf, size_t size) {
  base::SNPrintF(base::VectorOf(buf, size),
                 "native function externalizeString();"
                 "native function createExternalizableString();"
                 "native function createExternalizableTwoByteString();"
                 "native function isOneByteString();"
                 "let kExternalStringMinOneByteLength = %d;"
                 "let kExternalStringMinTwoByteLength = %d;"
                 "let kExternalStringMinOneByteCachedLength = %d;"
                 "let kExternalStringMinTwoByteCachedLength = %d;",
                 kMinOneByteLength, kMinTwoByteLength, kMinOneByteCachedLength,
                 kMinTwoByteCachedLength);
  return buf;
}
v8::Local<v8::FunctionTemplate>
ExternalizeStringExtension::GetNativeFunctionTemplate(
    v8::Isolate* isolate, v8::Local<v8::String> str) {
  if (strcmp(*v8::String::Utf8Value(isolate, str), "externalizeString") == 0) {
    return v8::FunctionTemplate::New(isolate,
                                     ExternalizeStringExtension::Externalize);
  } else if (strcmp(*v8::String::Utf8Value(isolate, str),
                    "createExternalizableString") == 0) {
    return v8::FunctionTemplate::New(
        isolate, ExternalizeStringExtension::CreateExternalizableString);
  } else if (strcmp(*v8::String::Utf8Value(isolate, str),
                    "createExternalizableTwoByteString") == 0) {
    return v8::FunctionTemplate::New(
        isolate, ExternalizeStringExtension::CreateExternalizableTwoByteString);
  } else {
    DCHECK_EQ(strcmp(*v8::String::Utf8Value(isolate, str), "isOneByteString"),
              0);
    return v8::FunctionTemplate::New(isolate,
                                     ExternalizeStringExtension::IsOneByte);
  }
}

namespace {

bool HasExternalForwardingIndex(DirectHandle<String> string) {
  if (!string->IsShared()) return false;
  uint32_t raw_hash = string->raw_hash_field(kAcquireLoad);
  return Name::IsExternalForwardingIndex(raw_hash);
}

}  // namespace

void ExternalizeStringExtension::Externalize(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  DCHECK(ValidateCallbackInfo(info));
  if (info.Length() < 1 || !info[0]->IsString()) {
    info.GetIsolate()->ThrowError(
        "First parameter to externalizeString() must be a string.");
    return;
  }
  bool result = false;
  DirectHandle<String> string =
      Utils::OpenDirectHandle(*info[0].As<v8::String>());
  const bool externalize_as_one_byte = string->IsOneByteRepresentation();
  if (!string->SupportsExternalization(
          externalize_as_one_byte ? v8::String::Encoding::ONE_BYTE_ENCODING
                                  : v8::String::Encoding::TWO_BYTE_ENCODING)) {
    // If the string is shared, testing with the combination of
    // --shared-string-table and --isolate in d8 may result in races to
    // externalize the same string. If GC is stressed in addition, this test
    // might fail as the string was already externalized (marked for
    // externalization by another thread and externalized during GC).
    if (!StringShape(*string).IsExternal()) {
      info.GetIsolate()->ThrowError("string does not support externalization.");
    }
    return;
  }
  if (externalize_as_one_byte) {
    uint8_t* data = new uint8_t[string->length()];
    String::WriteToFlat(*string, data, 0, string->length());
    SimpleOneByteStringResource* resource = new SimpleOneByteStringResource(
        reinterpret_cast<char*>(data), string->length());
    result = Utils::ToLocal(string)->MakeExternal(info.GetIsolate(), resource);
    if (!result) delete resource;
  } else {
    base::uc16* data = new base::uc16[string->length()];
    String::WriteToFlat(*string, data, 0, string->length());
    SimpleTwoByteStringResource* resource = new SimpleTwoByteStringResource(
        data, string->length());
    result = Utils::ToLocal(string)->MakeExternal(info.GetIsolate(), resource);
    if (!result) delete resource;
  }
  // If the string is shared, testing with the combination of
  // --shared-string-table and --isolate in d8 may result in races to
  // externalize the same string. Those races manifest as externalization
  // sometimes failing if another thread won and already forwarded the string to
  // the external resource. Don't consider those races as failures.
  if (!result && !HasExternalForwardingIndex(string)) {
    info.GetIsolate()->ThrowError("externalizeString() failed.");
    return;
  }
}

namespace {

MaybeDirectHandle<String> CopyConsStringToOld(Isolate* isolate,
                                              DirectHandle<ConsString> string,
                                              v8::String::Encoding encoding) {
  DirectHandle<String> first = handle(string->first(), isolate);
  DirectHandle<String> second = handle(string->second(), isolate);
  if (encoding == v8::String::Encoding::TWO_BYTE_ENCODING &&
      first->IsOneByteRepresentation() && second->IsOneByteRepresentation()) {
    isolate->Throw(*isolate->factory()->NewStringFromAsciiChecked(
        "Cannot create externalizable two-byte string from one-byte "
        "ConsString. Create at least one part of the ConsString with "
        "createExternalizableTwoByteString()"));
    return kNullMaybeHandle;
  }
  return isolate->factory()->NewConsString(first, second, AllocationType::kOld);
}

MaybeDirectHandle<String> CreateExternalizableString(
    v8::Isolate* isolate, DirectHandle<String> string,
    v8::String::Encoding encoding) {
  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  DCHECK_IMPLIES(encoding == v8::String::Encoding::ONE_BYTE_ENCODING,
                 string->IsOneByteRepresentation());
  if (string->SupportsExternalization(encoding)) {
    return string;
  }
  // Return the string if it is already externalized.
  if (StringShape(*string).IsExternal()) {
    return string;
  }

  // Read-only strings are never externalizable. Don't try to copy them as
  // some parts of the code might rely on some strings being in RO space (i.e.
  // empty string).
  if (HeapLayout::InReadOnlySpace(*string)) {
    isolate->ThrowError("Read-only strings cannot be externalized.");
    return kNullMaybeHandle;
  }
#ifdef V8_COMPRESS_POINTERS
  // Small strings may not be in-place externalizable.
  if (string->Size() < static_cast<int>(sizeof(UncachedExternalString))) {
    isolate->ThrowError("String is too short to be externalized.");
    return kNullMaybeHandle;
  }
#endif

  // Special handling for ConsStrings, as the ConsString -> ExternalString
  // migration is special for GC (Tagged pointers to Untagged pointers).
  // Skip if the ConsString is flat, as we won't be guaranteed a string in old
  // space in that case. Note that this is also true for non-canonicalized
  // ConsStrings that TurboFan might create (the first part is empty), so we
  // explicitly check for that case as well.
  if (IsConsString(*string, i_isolate) && !string->IsFlat() &&
      Cast<ConsString>(string)->first()->length() != 0) {
    DirectHandle<String> result;
    if (CopyConsStringToOld(i_isolate, Cast<ConsString>(string), encoding)
            .ToHandle(&result)) {
      DCHECK(result->SupportsExternalization(encoding));
      return result;
    } else {
      return kNullMaybeHandle;
    }
  }
  // All other strings can be implicitly flattened.
  if (encoding == v8::String::ONE_BYTE_ENCODING) {
    MaybeDirectHandle<SeqOneByteString> maybe_result =
        i_isolate->factory()->NewRawOneByteString(string->length(),
                                                  AllocationType::kOld);
    DirectHandle<SeqOneByteString> result;
    if (maybe_result.ToHandle(&result)) {
      DisallowGarbageCollection no_gc;
      String::WriteToFlat(*string, result->GetChars(no_gc), 0,
                          string->length());
      DCHECK(result->SupportsExternalization(encoding));
      return result;
    }
  } else {
    MaybeDirectHandle<SeqTwoByteString> maybe_result =
        i_isolate->factory()->NewRawTwoByteString(string->length(),
                                                  AllocationType::kOld);
    DirectHandle<SeqTwoByteString> result;
    if (maybe_result.ToHandle(&result)) {
      DisallowGarbageCollection no_gc;
      String::WriteToFlat(*string, result->GetChars(no_gc), 0,
                          string->length());
      DCHECK(result->SupportsExternalization(encoding));
      return result;
    }
  }
  isolate->ThrowError("Unable to create string");
  return kNullMaybeHandle;
}

}  // namespace

void ExternalizeStringExtension::CreateExternalizableString(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  DCHECK(ValidateCallbackInfo(info));
  if (info.Length() < 1 || !info[0]->IsString()) {
    info.GetIsolate()->ThrowError(
        "First parameter to createExternalizableString() must be a string.");
    return;
  }
  v8::Isolate* isolate = info.GetIsolate();
  DirectHandle<String> string =
      Utils::OpenDirectHandle(*info[0].As<v8::String>());
  v8::String::Encoding encoding = string->IsOneByteRepresentation()
                                      ? v8::String::Encoding::ONE_BYTE_ENCODING
                                      : v8::String::Encoding::TWO_BYTE_ENCODING;
  MaybeDirectHandle<String> maybe_result =
      i::CreateExternalizableString(isolate, string, encoding);
  DirectHandle<String> result;
  if (maybe_result.ToHandle(&result)) {
    DCHECK(!isolate->HasPendingException());
    info.GetReturnValue().Set(Utils::ToLocal(result));
  } else {
    DCHECK(isolate->HasPendingException());
  }
}

void ExternalizeStringExtension::CreateExternalizableTwoByteString(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  DCHECK(ValidateCallbackInfo(info));
  if (info.Length() < 1 || !info[0]->IsString()) {
    info.GetIsolate()->ThrowError(
        "First parameter to createExternalizableString() must be a string.");
    return;
  }
  v8::Isolate* isolate = info.GetIsolate();
  DirectHandle<String> string =
      Utils::OpenDirectHandle(*info[0].As<v8::String>());
  MaybeDirectHandle<String> maybe_result = i::CreateExternalizableString(
      isolate, string, v8::String::Encoding::TWO_BYTE_ENCODING);
  DirectHandle<String> result;
  if (maybe_result.ToHandle(&result)) {
    DCHECK(!isolate->HasPendingException());
    info.GetReturnValue().Set(Utils::ToLocal(result));
  } else {
    DCHECK(isolate->HasPendingException());
  }
}

void ExternalizeStringExtension::IsOneByte(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  DCHECK(ValidateCallbackInfo(info));
  if (info.Length() != 1 || !info[0]->IsString()) {
    info.GetIsolate()->ThrowError(
        "isOneByteString() requires a single string argument.");
    return;
  }
  bool is_one_byte = Utils::OpenDirectHandle(*info[0].As<v8::String>())
                         ->IsOneByteRepresentation();
  info.GetReturnValue().Set(is_one_byte);
}

}  // namespace internal
}  // namespace v8
