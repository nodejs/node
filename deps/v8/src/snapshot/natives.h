// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_NATIVES_H_
#define V8_SNAPSHOT_NATIVES_H_

#include "include/v8.h"
#include "src/objects.h"
#include "src/vector.h"

namespace v8 { class StartupData; }  // Forward declaration.

namespace v8 {
namespace internal {

enum NativeType {
  EXTRAS,
  TEST
};

// Extra handling for V8_EXPORT_PRIVATE in combination with USING_V8_SHARED
// since definition of methods of classes marked as dllimport is not allowed.
template <NativeType type>
#ifdef USING_V8_SHARED
class NativesCollection {
#else
class V8_EXPORT_PRIVATE NativesCollection {
#endif  // USING_V8_SHARED

 public:
  // The following methods are implemented in js2c-generated code:

  // Number of built-in scripts.
  static int GetBuiltinsCount();
  static int GetIndex(const char* name);
  static Vector<const char> GetScriptSource(int index);
  static Vector<const char> GetScriptName(int index);
  static Vector<const char> GetScriptsSource();
};

typedef NativesCollection<EXTRAS> ExtraNatives;


#ifdef V8_USE_EXTERNAL_STARTUP_DATA
// Used for reading the natives at runtime. Implementation in natives-empty.cc
void SetNativesFromFile(StartupData* natives_blob);
void ReadNatives();
void DisposeNatives();
#endif

class NativesExternalStringResource final
    : public v8::String::ExternalOneByteStringResource {
 public:
  NativesExternalStringResource(NativeType type, int index);

  const char* data() const override { return data_; }
  size_t length() const override { return length_; }

  v8::String::ExternalOneByteStringResource* EncodeForSerialization() const {
    DCHECK(type_ == EXTRAS);
    intptr_t val = (index_ << 1) | 1;
    val = val << kSystemPointerSizeLog2;  // Pointer align.
    return reinterpret_cast<v8::String::ExternalOneByteStringResource*>(val);
  }

  // Decode from serialization.
  static NativesExternalStringResource* DecodeForDeserialization(
      const v8::String::ExternalOneByteStringResource* encoded) {
    intptr_t val =
        reinterpret_cast<intptr_t>(encoded) >> kSystemPointerSizeLog2;
    DCHECK(val & 1);
    int index = static_cast<int>(val >> 1);
    return new NativesExternalStringResource(EXTRAS, index);
  }

 private:
  const char* data_;
  size_t length_;
  NativeType type_;
  int index_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_NATIVES_H_
