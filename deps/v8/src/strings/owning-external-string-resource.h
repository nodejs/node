// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_STRINGS_OWNING_EXTERNAL_STRING_RESOURCE_H_
#define V8_STRINGS_OWNING_EXTERNAL_STRING_RESOURCE_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string_view>

#include "include/v8-primitive.h"
#include "src/objects/string.h"
#include "src/objects/tagged.h"

#ifdef V8_ENABLE_MEMORY_CORRUPTION_API
#include "src/init/isolate-group.h"
#include "src/sandbox/external-strings-cage.h"
#endif  // V8_ENABLE_MEMORY_CORRUPTION_API

namespace v8::internal {

// A simple implementation of an external string resource that uniquely owns the
// string buffer.
//
// In fuzzing builds with memory corruption API, allocates the buffer from
// ExternalStringsCage.

template <typename CharT, typename StdCharT, typename FlatStringCharT,
          typename Base>
class OwningExternalStringResourceImpl : public Base {
 public:
  static_assert(sizeof(CharT) == sizeof(StdCharT));
  static_assert(sizeof(CharT) == sizeof(FlatStringCharT));

  explicit OwningExternalStringResourceImpl(
      std::basic_string_view<StdCharT> source)
      : length_(source.size()), storage_(CreateUninitializedStorage(length_)) {
    source.copy(storage_.get(), length_);
    SealIfSupported();
  }

  explicit OwningExternalStringResourceImpl(Tagged<String> source)
      : length_(source->length()),
        storage_(CreateUninitializedStorage(length_)) {
    String::WriteToFlat(source,
                        reinterpret_cast<FlatStringCharT*>(storage_.get()), 0,
                        static_cast<uint32_t>(length_));
    SealIfSupported();
  }

  OwningExternalStringResourceImpl(const OwningExternalStringResourceImpl&) =
      delete;
  OwningExternalStringResourceImpl& operator=(
      const OwningExternalStringResourceImpl&) = delete;

  ~OwningExternalStringResourceImpl() override = default;

  const CharT* data() const override { return storage_.get(); }
  size_t length() const override { return length_; }

 private:
#ifdef V8_ENABLE_MEMORY_CORRUPTION_API
  using Storage = std::unique_ptr<CharT[], ExternalStringsCage::Deleter<CharT>>;
#else   // V8_ENABLE_MEMORY_CORRUPTION_API
  using Storage = std::unique_ptr<CharT[]>;
#endif  // V8_ENABLE_MEMORY_CORRUPTION_API

  static Storage CreateUninitializedStorage(size_t length) {
#ifdef V8_ENABLE_MEMORY_CORRUPTION_API
    return IsolateGroup::current()->external_strings_cage()->Allocate<CharT>(
        length);
#else   // V8_ENABLE_MEMORY_CORRUPTION_API
    return std::make_unique_for_overwrite<CharT[]>(length);
#endif  // V8_ENABLE_MEMORY_CORRUPTION_API
  }

  void SealIfSupported() {
#ifdef V8_ENABLE_MEMORY_CORRUPTION_API
    IsolateGroup::current()->external_strings_cage()->Seal(storage_.get(),
                                                           length_);
#endif  // V8_ENABLE_MEMORY_CORRUPTION_API
  }

  const size_t length_;
  const Storage storage_;
};

using OwningExternalOneByteStringResource =
    OwningExternalStringResourceImpl<char, char, uint8_t,
                                     v8::String::ExternalOneByteStringResource>;
using OwningExternalStringResource =
    OwningExternalStringResourceImpl<base::uc16, char16_t, base::uc16,
                                     v8::String::ExternalStringResource>;

}  // namespace v8::internal

#endif  // V8_STRINGS_OWNING_EXTERNAL_STRING_RESOURCE_H_
